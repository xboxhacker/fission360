// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QIcon>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSurfaceFormat>
#include <QTimer>
#include <algorithm>
#include <cstdio>
#include <OSD.hxx>

#include "api/CommandRegistry.h"
#include "api/FissionApplication.h"
#include "api/McpServer.h"
#include "api/ProjectBrowser.h"
#include "api/ScriptHost.h"
#include "app/SelfTest.h"
#include "app/ShellIntegration.h"
#include "app/UiTestDriver.h"
#include "kernel/FissionBody.h"
#include "kernel/FissionDocument.h"
#include "kernel/FissionFeature.h"
#include "kernel/FissionParameter.h"
#include "kernel/FissionTimeline.h"
#include "kernel/drawing/DrawingManager.h"
#include "kernel/drawing/DrawingDocument.h"
#include "kernel/drawing/DrawingSheet.h"
#include "kernel/drawing/DrawingView.h"
#include "kernel/drawing/DrawingSketch.h"
#include "kernel/render/AppearanceLibrary.h"
#include "kernel/render/Material.h"
#include "kernel/render/RenderAssetStore.h"
#include "kernel/render/RenderEnvironmentLibrary.h"
#include "kernel/render/RenderSettings.h"
#include "kernel/mcmaster/McMasterCarrClient.h"
#include "ui/BRepRenderer.h"
#include "ui/SelectionManager.h"
#include "ui/ViewportController.h"

// FISSION_LOG=<file>: Qt / QML messages are appended there (a GUI build has no console to show them)
static FILE *g_log = nullptr;
static void logMessage(QtMsgType type, const QMessageLogContext &, const QString &msg) {
    static const char *const kinds[] = {"debug", "warning", "critical", "fatal", "info"};
    std::fprintf(g_log, "[%s] %s\n", kinds[std::min<int>(type, 4)], qPrintable(msg));
    std::fflush(g_log);
}

int main(int argc, char *argv[]) {
    if (qEnvironmentVariableIsSet("FISSION_LOG")) {
        g_log = std::fopen(qgetenv("FISSION_LOG").constData(), "a");
        if (g_log) qInstallMessageHandler(logMessage);
    }
    bool mcpMode = false;
    for (int i = 1; i < argc; ++i) {
        const QByteArray arg(argv[i]);
        if (arg == "--screenshot" || arg == "--uitest" || arg == "--uiperf") qputenv("QSG_RENDER_LOOP", "basic");
        if (arg == "--mcp") mcpMode = true;
    }

    // Headless MCP: no window, no WebEngine — CommandRegistry over stdio JSON-RPC
    if (mcpMode) {
        QCoreApplication app(argc, argv);
        app.setApplicationName("Fission 360");
        app.setApplicationVersion(FISSION_VERSION);
        app.setOrganizationName("FissionCAD");
        OSD::SetSignal(false);

        QCommandLineParser parser;
        parser.setApplicationDescription("Fission 360 MCP server (stdio)");
        parser.addHelpOption();
        QCommandLineOption mcpOpt("mcp", "Run as a Model Context Protocol server on stdio.");
        QCommandLineOption inOpt("in", "Open a design before serving.", "file");
        QCommandLineOption allowPath("allow-path", "Allow agent file access under DIR (repeatable).", "dir");
        QCommandLineOption allowAny("allow-any-path", "Disable MCP path sandbox (not recommended).");
        parser.addOptions({mcpOpt, inOpt, allowPath, allowAny});
        parser.process(app);

        FissionApplication *fissionApp = FissionApplication::instance();
        CommandRegistry::instance()->registerBuiltins(fissionApp);

        McpServer::Options opts;
        opts.openPath = parser.value(inOpt);
        opts.allowPaths = parser.values(allowPath);
        opts.allowAnyPath = parser.isSet(allowAny);
        return McpServer::run(fissionApp, opts);
    }

    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setDepthBufferSize(24);
    fmt.setStencilBufferSize(8);
    QSurfaceFormat::setDefaultFormat(fmt);

    // Access violations and FP faults inside OCCT become Standard_Failure exceptions (needs /EHa) so a bad
    // imported shape fails one feature instead of closing the app
    OSD::SetSignal(false);
    // WebEngine (Chromium, ~150 MB) is only for the McMaster-Carr browser. The exe does not link it: its QML
    // plugin loads it the first time that dialog opens. All it needs from startup is shared GL contexts
    // (plus the OpenGL graphics API set above) before the application object exists.
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QGuiApplication app(argc, argv);
    app.setApplicationName("Fission 360");
    app.setApplicationVersion(FISSION_VERSION);
    app.setOrganizationName("FissionCAD");
    app.setWindowIcon(QIcon(":/icons/fission360.png"));

    QCommandLineParser parser;
    parser.setApplicationDescription("Fission 360 parametric CAD");
    parser.addHelpOption();
    QCommandLineOption selfTest("selftest", "Run the modeling kernel regression tests and exit.");
    QCommandLineOption demo("demo", "Start with the demo bracket loaded.");
    QCommandLineOption open("open", "Open a .f360 design.", "file");
    QCommandLineOption screenshot("screenshot", "Save a window screenshot to <file> and exit.", "file");
    QCommandLineOption delay("delay", "Milliseconds to wait before --screenshot (default 2500).", "ms", "2500");
    QCommandLineOption uiTestOption("uitest", "Run the scripted UI walkthrough, saving screenshots to <dir>.", "dir");
    QCommandLineOption meshBench("meshbench", "Time Insert Mesh and Convert Mesh on an STL / OBJ file and exit.", "file");
    QCommandLineOption stepBench("stepbench", "Import a STEP / IGES file, extrude every face and exit.", "file");
    QCommandLineOption toolBench("toolbench", "Run every solid tool on an imported STEP / IGES / .f360 and exit.", "file");
    QCommandLineOption uiPerf("uiperf", "Time inserting, picking and converting an STL / OBJ file in the real window.", "file");
    QCommandLineOption registerType("register", "Register the .f360 file type, icon and Explorer thumbnails for this user and exit.");
    QCommandLineOption unregisterType("unregister", "Remove the .f360 file type registration for this user and exit.");
    parser.addOptions({selfTest, demo, open, screenshot, delay, uiTestOption, meshBench, stepBench, toolBench, uiPerf, registerType,
                       unregisterType});
    parser.addPositionalArgument("design", "A .f360 design to open (what Explorer passes on double-click).", "[design.f360]");
    parser.process(app);

    if (parser.isSet(selfTest)) return SelfTest::run() == 0 ? 0 : 1;
    if (parser.isSet(meshBench)) return SelfTest::meshBench(parser.value(meshBench));
    if (parser.isSet(stepBench)) return SelfTest::stepBench(parser.value(stepBench));
    if (parser.isSet(toolBench)) return SelfTest::toolBench(parser.value(toolBench));
    if (parser.isSet(registerType) || parser.isSet(unregisterType)) {
        const bool ok = parser.isSet(registerType) ? ShellIntegration::registerFileType(true) : ShellIntegration::unregisterFileType();
        std::printf("%s\n%s\n", ok ? "done" : "FAILED", qPrintable(ShellIntegration::status()));
        return ok ? 0 : 1;
    }
    const bool scripted = parser.isSet(uiTestOption) || parser.isSet(uiPerf) || parser.isSet(screenshot);
    // Defer shell registration until after the first window frame so cold start stays snappy
    if (!scripted) {
        QTimer::singleShot(0, [] { ShellIntegration::registerFileType(); });
    }

    FissionApplication *fissionApp = FissionApplication::instance();
    CommandRegistry::instance()->registerBuiltins(fissionApp);
    const QStringList designs = parser.positionalArguments();
    if (parser.isSet(open)) fissionApp->openDocument(parser.value(open));
    else if (!designs.isEmpty()) for (const QString &d : designs) fissionApp->openDocument(d);
    else if (parser.isSet(demo)) fissionApp->openDemo();

    QQuickStyle::setStyle("Basic");

    qmlRegisterType<BRepRenderer>("Fission.Render", 1, 0, "BRepRenderer");
    qmlRegisterType<ViewportController>("Fission.Render", 1, 0, "ViewportController");
    qmlRegisterType<Appearance>("Fission.Render", 1, 0, "Appearance");
    qmlRegisterType<RenderSettings>("Fission.Render", 1, 0, "RenderSettings");
    qmlRegisterSingletonInstance("Fission.Render", 1, 0, "AppearanceLibrary", AppearanceLibrary::instance());
    qmlRegisterSingletonInstance("Fission.Render", 1, 0, "RenderEnvironmentLibrary",
                                 RenderEnvironmentLibrary::instance());
    qmlRegisterType<SelectionManager>("Fission.UI", 1, 0, "SelectionManager");
    qmlRegisterUncreatableType<FissionDocument>("Fission.Kernel", 1, 0, "FissionDocument", "Owned by fissionApp");
    qmlRegisterUncreatableType<FissionFeature>("Fission.Kernel", 1, 0, "FissionFeature", "Use doc.addFeature()");
    qmlRegisterUncreatableType<FissionTimeline>("Fission.Kernel", 1, 0, "FissionTimeline", "Owned by a document");
    qmlRegisterUncreatableType<FissionBody>("Fission.Kernel", 1, 0, "FissionBody", "Owned by a document");
    qmlRegisterUncreatableType<FissionParameter>("Fission.Kernel", 1, 0, "FissionParameter", "Use doc.addParameter()");
    qmlRegisterType<DrawingManager>("Fission.Drawing", 1, 0, "DrawingManager");
    qmlRegisterUncreatableType<DrawingDocument>("Fission.Drawing", 1, 0, "DrawingDocument", "Owned by DrawingManager");
    qmlRegisterUncreatableType<DrawingSheet>("Fission.Drawing", 1, 0, "DrawingSheet", "Owned by a DrawingDocument");
    qmlRegisterUncreatableType<DrawingView>("Fission.Drawing", 1, 0, "DrawingView", "Owned by a DrawingSheet");
    qmlRegisterUncreatableType<DrawingSketch>("Fission.Drawing", 1, 0, "DrawingSketch", "Owned by a DrawingSheet");

    const QString perfFile = parser.value(uiPerf);
    UiTestDriver uiTest(perfFile.isEmpty() ? parser.value(uiTestOption) : QDir::temp().filePath("f360perf"), perfFile);

    ProjectBrowser projects;
    if (parser.isSet(uiTestOption) || !perfFile.isEmpty())
        projects.setSessionRoot(QDir(uiTest.dir()).filePath("projects"));

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("fissionApp", fissionApp);
    engine.rootContext()->setContextProperty("fissionCommands", CommandRegistry::instance());
    engine.rootContext()->setContextProperty("projects", &projects);
    engine.rootContext()->setContextProperty("fissionScripts", new ScriptHost(&engine, &engine));
    engine.rootContext()->setContextProperty("uiTest", &uiTest);
    engine.rootContext()->setContextProperty("appearanceLibrary", AppearanceLibrary::instance());
    engine.rootContext()->setContextProperty("renderEnvironments", RenderEnvironmentLibrary::instance());
    engine.rootContext()->setContextProperty("renderAssets", RenderAssetStore::instance());
    engine.rootContext()->setContextProperty("mcmaster", McMasterCarrClient::instance());
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(2); }, Qt::QueuedConnection);
    engine.load(QUrl("qrc:/qml/main.qml"));
    if (engine.rootObjects().isEmpty()) return 2;
    uiTest.setWindow(qobject_cast<QQuickWindow *>(engine.rootObjects().first()));

    if (parser.isSet(screenshot)) {
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        const QString file = parser.value(screenshot);
        QTimer::singleShot(parser.value(delay).toInt(), &app, [window, file] {
            const bool ok = window && window->grabWindow().save(file);
            std::printf("screenshot %s: %s\n", ok ? "saved" : "FAILED", qPrintable(file));
            std::fflush(stdout);
            QCoreApplication::exit(ok ? 0 : 3);
        });
    }
    return app.exec();
}