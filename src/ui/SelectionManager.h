// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include <QObject>
#include <QVariantList>
#include <QVariantMap>

// Current selection as reference maps produced by BRepRenderer::pick():
//   {kind:"face"|"edge", body, index, count, x,y,z}   {kind:"body", body}
//   {kind:"profile", sketch, index}                     {kind:"feature", id}
class SelectionManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList items READ items NOTIFY selectionChanged)
    Q_PROPERTY(int count READ count NOTIFY selectionChanged)
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)
    Q_PROPERTY(QString summary READ summary NOTIFY selectionChanged)

public:
    explicit SelectionManager(QObject *parent = nullptr) : QObject(parent) {}

    QVariantList items() const { return m_items; }
    int count() const { return m_items.size(); }
    // Comma-separated kinds allowed for picking, e.g. "edge" or "profile,face"; empty = any
    QString filter() const { return m_filter; }
    void setFilter(const QString &f);
    QString summary() const;

    Q_INVOKABLE void select(const QVariantMap &ref, bool additive);
    Q_INVOKABLE void toggle(const QVariantMap &ref);
    Q_INVOKABLE void clear();
    Q_INVOKABLE bool contains(const QVariantMap &ref) const { return indexOf(ref) >= 0; }
    Q_INVOKABLE QVariantList ofKind(const QString &kind) const;
    Q_INVOKABLE void setItems(const QVariantList &items);

    static bool sameRef(const QVariantMap &a, const QVariantMap &b);

signals:
    void selectionChanged();
    void filterChanged();

private:
    int indexOf(const QVariantMap &ref) const;
    QVariantList m_items;
    QString m_filter;
};
