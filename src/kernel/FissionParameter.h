// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include <QObject>
#include <QString>
#include <QJsonObject>

// A user parameter from the Parameters table, e.g. width = "40 mm".
class FissionParameter : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY changed)
    Q_PROPERTY(QString expression READ expression WRITE setExpression NOTIFY changed)
    Q_PROPERTY(QString unit READ unit WRITE setUnit NOTIFY changed)
    Q_PROPERTY(QString comment READ comment WRITE setComment NOTIFY changed)
    Q_PROPERTY(double value READ value NOTIFY valueChanged)
    Q_PROPERTY(QString error READ error NOTIFY valueChanged)

public:
    explicit FissionParameter(const QString &name = QStringLiteral("param"),
                              const QString &expression = QStringLiteral("0"),
                              const QString &unit = QStringLiteral("mm"),
                              QObject *parent = nullptr);

    QString name() const { return m_name; }
    void setName(const QString &name);
    QString expression() const { return m_expression; }
    void setExpression(const QString &expr);
    QString unit() const { return m_unit; }
    void setUnit(const QString &unit);
    QString comment() const { return m_comment; }
    void setComment(const QString &comment);

    double value() const { return m_value; }
    QString error() const { return m_error; }
    void setEvaluated(double value, const QString &error);

    QJsonObject toJson() const;
    void fromJson(const QJsonObject &obj);

signals:
    void changed();
    void valueChanged();

private:
    QString m_name;
    QString m_expression;
    QString m_unit;
    QString m_comment;
    double m_value = 0.0;
    QString m_error;
};
