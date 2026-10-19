// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include <QString>

enum class FeatureOperation { Join, Cut, Intersect, NewBody };

inline FeatureOperation operationFromString(const QString &s) {
    if (s == QLatin1String("cut")) return FeatureOperation::Cut;
    if (s == QLatin1String("intersect")) return FeatureOperation::Intersect;
    if (s == QLatin1String("newBody")) return FeatureOperation::NewBody;
    return FeatureOperation::Join;
}
