// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include "FissionFeature.h"

// props:
//   faces     : [face refs] removed to open the shell (empty = closed hollow body)
//   body      : body index used when faces is empty
//   thickness : expr       direction : "inside" | "outside"
class FissionShell : public FissionFeature {
    Q_OBJECT
public:
    explicit FissionShell(QObject *parent = nullptr) : FissionFeature(parent) {}
    QString type() const override { return QStringLiteral("Shell"); }
    QVariantMap defaults() const override {
        return {{"faces", QVariantList()}, {"body", 0}, {"thickness", "2 mm"}, {"direction", "inside"}};
    }
    QStringList expressionKeys() const override { return {"thickness"}; }

protected:
    bool compute(RebuildContext &ctx) override;
};
