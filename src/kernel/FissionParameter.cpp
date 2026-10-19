// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "FissionParameter.h"

FissionParameter::FissionParameter(const QString &name, const QString &expression,
                                   const QString &unit, QObject *parent)
    : QObject(parent), m_name(name), m_expression(expression), m_unit(unit) {}

void FissionParameter::setName(const QString &name) {
    if (m_name != name) { m_name = name; emit changed(); }
}

void FissionParameter::setExpression(const QString &expr) {
    if (m_expression != expr) { m_expression = expr; emit changed(); }
}

void FissionParameter::setUnit(const QString &unit) {
    if (m_unit != unit) { m_unit = unit; emit changed(); }
}

void FissionParameter::setComment(const QString &comment) {
    if (m_comment != comment) { m_comment = comment; emit changed(); }
}

void FissionParameter::setEvaluated(double value, const QString &error) {
    if (m_value != value || m_error != error) {
        m_value = value;
        m_error = error;
        emit valueChanged();
    }
}

QJsonObject FissionParameter::toJson() const {
    QJsonObject obj;
    obj["name"] = m_name;
    obj["expression"] = m_expression;
    obj["unit"] = m_unit;
    obj["comment"] = m_comment;
    return obj;
}

void FissionParameter::fromJson(const QJsonObject &obj) {
    m_name = obj["name"].toString("param");
    m_expression = obj["expression"].toString("0");
    m_unit = obj["unit"].toString("mm");
    m_comment = obj["comment"].toString();
}
