// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "SelectionManager.h"

bool SelectionManager::sameRef(const QVariantMap &a, const QVariantMap &b) {
    for (const char *key : {"kind", "body", "index", "sketch", "id", "entity"})
        if (a.value(key) != b.value(key)) return false;
    return true;
}

int SelectionManager::indexOf(const QVariantMap &ref) const {
    for (int i = 0; i < m_items.size(); ++i)
        if (sameRef(m_items[i].toMap(), ref)) return i;
    return -1;
}

void SelectionManager::setFilter(const QString &f) {
    if (m_filter != f) { m_filter = f; emit filterChanged(); }
}

void SelectionManager::select(const QVariantMap &ref, bool additive) {
    if (!additive) m_items.clear();
    if (!ref.isEmpty() && indexOf(ref) < 0) m_items.append(ref);
    emit selectionChanged();
}

void SelectionManager::toggle(const QVariantMap &ref) {
    const int i = indexOf(ref);
    if (i >= 0) m_items.removeAt(i);
    else m_items.append(ref);
    emit selectionChanged();
}

void SelectionManager::clear() {
    if (m_items.isEmpty()) return;
    m_items.clear();
    emit selectionChanged();
}

void SelectionManager::setItems(const QVariantList &items) {
    m_items = items;
    emit selectionChanged();
}

QVariantList SelectionManager::ofKind(const QString &kind) const {
    QVariantList out;
    for (const QVariant &v : m_items)
        if (v.toMap().value("kind").toString() == kind) out.append(v);
    return out;
}

QString SelectionManager::summary() const {
    if (m_items.isEmpty()) return QStringLiteral("Nothing selected");
    QMap<QString, int> counts;
    for (const QVariant &v : m_items) counts[v.toMap().value("kind").toString()]++;
    QStringList parts;
    for (auto it = counts.begin(); it != counts.end(); ++it)
        parts.append(QString("%1 %2%3").arg(it.value()).arg(it.key()).arg(it.value() > 1 ? "s" : ""));
    return parts.join(", ") + " selected";
}
