// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include <QAbstractListModel>
#include <QList>
#include "FissionFeature.h"

// Ordered feature history. Features at index >= marker are rolled back (not computed).
class FissionTimeline : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int marker READ marker WRITE setMarker NOTIFY markerChanged)

public:
    enum Roles { FeatureRole = Qt::UserRole + 1, NameRole, TypeRole, SuppressedRole, ErrorRole, ActiveRole, IdRole };

    explicit FissionTimeline(QObject *parent = nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_features.size(); }
    int marker() const { return m_marker; }
    void setMarker(int marker);

    Q_INVOKABLE FissionFeature *at(int index) const;
    Q_INVOKABLE int indexOf(FissionFeature *feature) const { return m_features.indexOf(feature); }
    const QList<FissionFeature *> &features() const { return m_features; }

    void insert(int index, FissionFeature *feature);
    FissionFeature *take(int index);
    void move(int from, int to);
    void clear();
    void refresh(int index);   // re-emit roles after a status change

signals:
    void countChanged();
    void markerChanged();

private:
    QList<FissionFeature *> m_features;
    int m_marker = 0;
};
