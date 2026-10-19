// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "FissionImport.h"
#include "FissionDocument.h"
#include "FissionProgress.h"
#include "GeomUtils.h"

#include "FissionBody.h"

#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMetaObject>
#include <QPointer>
#include <QSet>
#include <QUuid>
#include <functional>
#include <sstream>
#include <thread>

#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <IGESCAFControl_Reader.hxx>
#include <OSD.hxx>
#include <Interface_Static.hxx>
#include <Quantity_Color.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <TDF_Label.hxx>
#include <TDF_LabelSequence.hxx>
#include <TDataStd_Name.hxx>
#include <TDocStd_Document.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <ShapeFix_Shape.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>

static QString labelName(const TDF_Label &label) {
    Handle(TDataStd_Name) n;
    if (label.IsNull() || !label.FindAttribute(TDataStd_Name::GetID(), n)) return {};
    const TCollection_ExtendedString &s = n->Get();
    return QString::fromUtf16(reinterpret_cast<const char16_t *>(s.ToExtString()), s.Length()).trimmed();
}

static bool labelColor(const Handle(XCAFDoc_ColorTool) &ct, const TDF_Label &label, QColor &out) {
    Quantity_Color c;
    if (!ct->GetColor(label, XCAFDoc_ColorSurf, c) && !ct->GetColor(label, XCAFDoc_ColorGen, c)) return false;
    // XCAF colours are linear RGB; the viewport and files use sRGB
    double r, g, b;
    c.Values(r, g, b, Quantity_TOC_sRGB);
    out = QColor::fromRgbF(float(r), float(g), float(b));
    return true;
}

TopoDS_Shape FissionImportCAD::readFile(const QString &path, QString &format, QString &error, QVariantList *parts) {
    const QString ext = QFileInfo(path).suffix().toLower();
    format = (ext == "igs" || ext == "iges") ? QStringLiteral("IGES") : QStringLiteral("STEP");
    if (!QFileInfo::exists(path)) { error = "File not found: " + path; return {}; }
    try {
        Interface_Static::SetCVal("xstep.cascade.unit", "MM");
        Interface_Static::SetIVal("read.stepcaf.subshapes.name", 1);
        const QByteArray native = QFileInfo(path).absoluteFilePath().toLocal8Bit();
        // The XDE readers keep the product structure, names and colours next to the shapes
        Handle(TDocStd_Document) doc;
        XCAFApp_Application::GetApplication()->NewDocument("MDTV-XCAF", doc);
        bool ok = false;
        if (format == "IGES") {
            IGESCAFControl_Reader reader;
            reader.SetColorMode(true);
            reader.SetNameMode(true);
            ok = reader.ReadFile(native.constData()) == IFSelect_RetDone && reader.Transfer(doc);
        } else {
            STEPCAFControl_Reader reader;
            reader.SetColorMode(true);
            reader.SetNameMode(true);
            ok = reader.ReadFile(native.constData()) == IFSelect_RetDone && reader.Transfer(doc);
        }
        if (!ok) {
            error = "Could not read the " + format + " file";
            XCAFApp_Application::GetApplication()->Close(doc);
            return {};
        }
        const Handle(XCAFDoc_ShapeTool) st = XCAFDoc_DocumentTool::ShapeTool(doc->Main());
        const Handle(XCAFDoc_ColorTool) ct = XCAFDoc_DocumentTool::ColorTool(doc->Main());

        // Solids first, in assembly order, each named by its path through the assemblies ("Asm/Sub/Part");
        // loose surfaces follow
        BRep_Builder bb;
        TopoDS_Compound all, loose;
        bb.MakeCompound(all);
        bb.MakeCompound(loose);
        QVariantList found;
        std::function<void(const TDF_Label &, const TopLoc_Location &, const QString &, const QColor &)> walk;
        walk = [&](const TDF_Label &label, const TopLoc_Location &loc, const QString &parent, const QColor &inherited) {
            TDF_Label ref = label;
            TopLoc_Location here = loc;
            QString name = labelName(label);
            QColor color;
            labelColor(ct, label, color);   // a colour on the instance wins over the part's
            if (XCAFDoc_ShapeTool::IsReference(label)) {
                XCAFDoc_ShapeTool::GetReferredShape(label, ref);
                here = loc * XCAFDoc_ShapeTool::GetLocation(label);
                const QString partName = labelName(ref);
                if (name.isEmpty() || name.startsWith("=>") || name.startsWith("NAUO")) name = partName;
                if (!color.isValid()) labelColor(ct, ref, color);
            }
            if (!color.isValid()) color = inherited;
            const QString full = parent.isEmpty() ? name : name.isEmpty() ? parent : parent + "/" + name;
            if (XCAFDoc_ShapeTool::IsAssembly(ref)) {
                TDF_LabelSequence comps;
                XCAFDoc_ShapeTool::GetComponents(ref, comps);
                for (int i = 1; i <= comps.Length(); ++i) walk(comps.Value(i), here, full, color);
                return;
            }
            const TopoDS_Shape base = XCAFDoc_ShapeTool::GetShape(ref);
            const TopoDS_Shape shape = base.Moved(here);
            // Solid names (Fusion writes each body's name on its solid) live on sub-shape labels of the part
            QStringList solidNames;
            for (TopExp_Explorer e(base, TopAbs_SOLID); e.More(); e.Next()) {
                TDF_Label sub;
                solidNames << (st->FindSubShape(ref, e.Current(), sub) ? labelName(sub) : QString());
            }
            const int count = int(solidNames.size());
            int k = 0;
            for (TopExp_Explorer e(shape, TopAbs_SOLID); e.More(); e.Next()) {
                QColor solidColor;
                Quantity_Color qc;
                if (ct->GetColor(e.Current(), XCAFDoc_ColorSurf, qc) || ct->GetColor(e.Current(), XCAFDoc_ColorGen, qc)) {
                    double r, g, b;
                    qc.Values(r, g, b, Quantity_TOC_sRGB);
                    solidColor = QColor::fromRgbF(float(r), float(g), float(b));
                }
                if (!solidColor.isValid()) solidColor = color;
                bb.Add(all, e.Current());
                const QString solidName = solidNames.value(k);
                ++k;
                found << QVariantMap{{"name", count > 1 ? QString("%1 (%2)").arg(full).arg(k) : full},
                                     {"solidName", solidName.startsWith("=>") ? QString() : solidName},
                                     {"component", full.section('/', -1)},
                                     {"color", solidColor.isValid() ? solidColor.name() : QString()}};
            }
            for (TopExp_Explorer e(shape, TopAbs_SHELL, TopAbs_SOLID); e.More(); e.Next()) bb.Add(loose, e.Current());
            for (TopExp_Explorer e(shape, TopAbs_FACE, TopAbs_SHELL); e.More(); e.Next()) bb.Add(loose, e.Current());
        };
        TDF_LabelSequence roots;
        st->GetFreeShapes(roots);
        for (int i = 1; i <= roots.Length(); ++i) walk(roots.Value(i), TopLoc_Location(), QString(), QColor());
        XCAFApp_Application::GetApplication()->Close(doc);
        if (TopExp_Explorer(loose, TopAbs_FACE).More()) bb.Add(all, loose);
        if (!TopExp_Explorer(all, TopAbs_FACE).More()) {
            error = "The file holds no surfaces or solids";
            return {};
        }
        ShapeFix_Shape fix(all);
        fix.Perform();
        const TopoDS_Shape fixed = fix.Shape();
        int solids = 0;
        for (TopExp_Explorer e(fixed, TopAbs_SOLID); e.More(); e.Next()) ++solids;
        if (parts && solids == found.size()) *parts = found;   // repairs that split or merge solids lose the names
        return fixed;
    } catch (const Standard_Failure &e) {
        error = QString("Import failed: %1").arg(e.GetMessageString());
        return {};
    }
}

QStringList FissionImportCAD::fusionBodyNames(const QVariantList &parts) {
    QStringList names;
    QHash<QString, int> uses;
    for (const QVariant &v : parts) {
        const QString n = v.toMap().value("solidName").toString();
        names << n;
        if (!n.isEmpty()) ++uses[n];
    }
    // Fusion allows the same body name in different components; keep those apart by component
    QSet<QString> taken;
    for (int i = 0; i < names.size(); ++i) {
        if (names[i].isEmpty()) continue;
        QString n = names[i];
        const QString comp = parts[i].toMap().value("component").toString();
        if (uses.value(n) > 1 && !comp.isEmpty()) n = QString("%1 (%2)").arg(n, comp);
        const QString wanted = n;
        for (int k = 2; taken.contains(n); ++k) n = QString("%1 %2").arg(wanted).arg(k);
        taken.insert(n);
        names[i] = n;
    }
    return names;
}

TopoDS_Shape FissionImportCAD::readStepBase64(const QString &b64, QString &error, QVariantList *parts) {
    error.clear();
    const QByteArray raw = QByteArray::fromBase64(b64.toLatin1());
    if (raw.isEmpty()) {
        error = QStringLiteral("Empty STEP payload");
        return {};
    }
    const QString tmpPath = QDir::temp().filePath(
        QStringLiteral("_f360_snap_%1.stp").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    QFile f(tmpPath);
    if (!f.open(QIODevice::WriteOnly)) {
        error = f.errorString();
        return {};
    }
    f.write(raw);
    f.close();
    QString format;
    const TopoDS_Shape shape = readFile(tmpPath, format, error, parts);
    QFile::remove(tmpPath);
    return shape;
}

QString FissionImportCAD::encode(const TopoDS_Shape &shape) {
    std::ostringstream out;
    BRepTools::Write(shape, out);
    const std::string s = out.str();
    return QString::fromLatin1(qCompress(QByteArray(s.data(), qsizetype(s.size())), 6).toBase64());
}

TopoDS_Shape FissionImportCAD::decode(const QString &data) {
    const QByteArray raw = qUncompress(QByteArray::fromBase64(data.toLatin1()));
    if (raw.isEmpty()) return {};
    std::istringstream in(std::string(raw.constData(), size_t(raw.size())));
    TopoDS_Shape shape;
    BRep_Builder bb;
    try {
        BRepTools::Read(shape, in, bb);
    } catch (const Standard_Failure &) {
        return {};
    }
    return shape;
}

bool FissionImportCAD::compute(RebuildContext &ctx) {
    const TopoDS_Shape shape = decode(str("data"));
    if (shape.IsNull()) return fail("The imported data is missing or damaged");
    const QVariantList parts = list("parts");
    QSet<QString> taken;
    for (const BodyState &b : ctx.bodies) taken.insert(b.name);
    auto uniqueName = [&](const QString &wanted) {
        if (wanted.isEmpty()) return ctx.nextBodyName();
        QString name = wanted;
        for (int k = 2; taken.contains(name); ++k) name = QString("%1 %2").arg(wanted).arg(k);
        taken.insert(name);
        return name;
    };
    int solids = 0;
    for (TopExp_Explorer e(shape, TopAbs_SOLID); e.More(); e.Next()) {
        ctx.bodies.append({uniqueName(parts.value(solids).toMap().value("name").toString()), e.Current(), id()});
        ++solids;
    }
    // Surfaces that are not part of a solid stay together as one surface body
    BRep_Builder bb;
    TopoDS_Compound loose;
    bb.MakeCompound(loose);
    int faces = 0;
    for (TopExp_Explorer e(shape, TopAbs_SHELL, TopAbs_SOLID); e.More(); e.Next()) { bb.Add(loose, e.Current()); ++faces; }
    for (TopExp_Explorer e(shape, TopAbs_FACE, TopAbs_SHELL); e.More(); e.Next()) { bb.Add(loose, e.Current()); ++faces; }
    if (faces) ctx.bodies.append({ctx.nextBodyName(), loose, id()});
    if (!solids && !faces) return fail("The imported data holds no geometry");
    return true;
}

QString FissionDocument::importCAD(const QString &path) {
    FissionProgress::begin(QStringLiteral("importcad"), QStringLiteral("Reading CAD…"));
    QString format, err;
    QVariantList parts;
    const TopoDS_Shape shape = FissionImportCAD::readFile(path, format, err, &parts);
    if (shape.IsNull()) {
        FissionProgress::end(QStringLiteral("importcad"));
        m_lastError = err;
        return QString();
    }
    FissionProgress::report(QStringLiteral("importcad"), 0.7, QStringLiteral("Adding to design…"));
    const int before = bodyCount();
    FissionFeature *f = addFeature("ImportCAD", {{"data", FissionImportCAD::encode(shape)},
                                                 {"fileName", QFileInfo(path).fileName()}, {"format", format},
                                                 {"parts", parts}},
                                   QFileInfo(path).completeBaseName());
    FissionProgress::end(QStringLiteral("importcad"));
    if (!f) { m_lastError = "Could not add the import"; return QString(); }
    if (!f->error().isEmpty()) m_lastError = f->error();
    // The file's colours become the bodies' display colours (saved with the design like any other)
    for (int i = 0; i < parts.size() && before + i < bodyCount(); ++i) {
        const QColor c(parts[i].toMap().value("color").toString());
        if (c.isValid()) body(before + i)->setColor(c);
    }
    return f->id();
}

void FissionDocument::importCADAsync(const QString &path) {
    if (m_importCADBusy) {
        m_lastError = QStringLiteral("An import is already running");
        emit importCADFinished(QString(), m_lastError, 0, 0);
        return;
    }
    m_importCADBusy = true;
    FissionProgress::begin(QStringLiteral("importcad"), QStringLiteral("Reading CAD…"));
    const QPointer<FissionDocument> self(this);
    std::thread([self, path] {
        OSD::SetThreadLocalSignal(OSD_SignalMode_Set, false);
        QString format, err;
        QVariantList parts;
        TopoDS_Shape shape;
        QString data;
        if (self) {
            FissionProgress::report(QStringLiteral("importcad"), 0.15, QStringLiteral("Reading CAD…"));
            shape = FissionImportCAD::readFile(path, format, err, &parts);
            if (!shape.IsNull()) {
                FissionProgress::report(QStringLiteral("importcad"), 0.55, QStringLiteral("Packing geometry…"));
                data = FissionImportCAD::encode(shape);
            }
        } else {
            err = QStringLiteral("Document closed");
        }
        QMetaObject::invokeMethod(qApp, [self, path, format, parts, data, err] {
            if (!self) return;
            self->m_importCADBusy = false;
            if (data.isEmpty()) {
                FissionProgress::end(QStringLiteral("importcad"));
                self->m_lastError = err.isEmpty() ? QStringLiteral("Import failed") : err;
                emit self->importCADFinished(QString(), self->m_lastError, 0, 0);
                return;
            }
            FissionProgress::report(QStringLiteral("importcad"), 0.85, QStringLiteral("Adding to design…"));
            const int before = self->bodyCount();
            FissionFeature *f = self->addFeature("ImportCAD",
                {{"data", data},
                 {"fileName", QFileInfo(path).fileName()},
                 {"format", format},
                 {"parts", parts}},
                QFileInfo(path).completeBaseName());
            FissionProgress::end(QStringLiteral("importcad"));
            if (!f) {
                self->m_lastError = QStringLiteral("Could not add the import");
                emit self->importCADFinished(QString(), self->m_lastError, 0, 0);
                return;
            }
            if (!f->error().isEmpty())
                self->m_lastError = f->error();
            for (int i = 0; i < parts.size() && before + i < self->bodyCount(); ++i) {
                const QColor c(parts[i].toMap().value("color").toString());
                if (c.isValid())
                    self->body(before + i)->setColor(c);
            }
            const int n = self->bodyCount() - before;
            emit self->importCADFinished(f->id(), QString(), before, n);
        }, Qt::QueuedConnection);
    }).detach();
}
