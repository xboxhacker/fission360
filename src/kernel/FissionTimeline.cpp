// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "FissionTimeline.h"

int FissionTimeline::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : m_features.size();
}

QVariant FissionTimeline::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_features.size()) return {};
    FissionFeature *f = m_features[index.row()];
    switch (role) {
        case FeatureRole: return QVariant::fromValue(f);
        case NameRole: return f->name();
        case TypeRole: return f->type();
        case SuppressedRole: return f->isSuppressed();
        case ErrorRole: return f->error();
        case ActiveRole: return index.row() < m_marker;
        case IdRole: return f->id();
    }
    return {};
}

QHash<int, QByteArray> FissionTimeline::roleNames() const {
    return {{FeatureRole, "feature"}, {NameRole, "name"}, {TypeRole, "type"}, {SuppressedRole, "suppressed"},
            {ErrorRole, "error"}, {ActiveRole, "active"}, {IdRole, "featureId"}};
}

void FissionTimeline::setMarker(int marker) {
    marker = std::clamp(marker, 0, int(m_features.size()));
    if (marker == m_marker) return;
    m_marker = marker;
    emit markerChanged();
    if (!m_features.isEmpty()) emit dataChanged(index(0), index(m_features.size() - 1), {ActiveRole});
}

FissionFeature *FissionTimeline::at(int i) const {
    return (i >= 0 && i < m_features.size()) ? m_features[i] : nullptr;
}

void FissionTimeline::insert(int i, FissionFeature *feature) {
    i = std::clamp(i, 0, int(m_features.size()));
    beginInsertRows(QModelIndex(), i, i);
    m_features.insert(i, feature);
    if (i <= m_marker) ++m_marker;   // before endInsertRows so views read the right ActiveRole
    endInsertRows();
    emit countChanged();
    emit markerChanged();
}

FissionFeature *FissionTimeline::take(int i) {
    if (i < 0 || i >= m_features.size()) return nullptr;
    beginRemoveRows(QModelIndex(), i, i);
    FissionFeature *f = m_features.takeAt(i);
    if (i < m_marker) --m_marker;
    endRemoveRows();
    emit countChanged();
    emit markerChanged();
    return f;
}

void FissionTimeline::move(int from, int to) {
    if (from == to || from < 0 || to < 0 || from >= m_features.size() || to >= m_features.size()) return;
    beginMoveRows(QModelIndex(), from, from, QModelIndex(), to > from ? to + 1 : to);
    m_features.move(from, to);
    endMoveRows();
    emit dataChanged(index(0), index(m_features.size() - 1), {ActiveRole});
}

void FissionTimeline::clear() {
    beginResetModel();
    m_features.clear();
    m_marker = 0;
    endResetModel();
    emit countChanged();
    emit markerChanged();
}

void FissionTimeline::refresh(int i) {
    if (i >= 0 && i < m_features.size()) emit dataChanged(index(i), index(i));
}
