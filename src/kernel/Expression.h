// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include <QString>
#include <QStringList>
#include <functional>

// Evaluates Fusion-style parameter expressions: "width * 2 + 5 mm", "30 deg", "sqrt(a^2 + b^2)".
// Lengths are normalized to millimetres, angles to degrees.
class Expression {
public:
    // Returns the value of a named parameter; sets ok=false if the name is unknown.
    using Resolver = std::function<double(const QString &name, bool &ok)>;

    static double evaluate(const QString &expr, const Resolver &resolver, QString *error = nullptr);
    static double evaluate(const QString &expr, QString *error = nullptr);

    // Identifiers referenced by an expression (excluding units, functions and constants).
    static QStringList identifiers(const QString &expr);

    static bool isValidName(const QString &name);
};
