// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
// Kept out of GeomUtils.h: Message_ProgressIndicator pulls in windows.h, whose near / far macros break
// ordinary code that includes GeomUtils.h.
#include <Message_ProgressIndicator.hxx>
#include <QElapsedTimer>
#include <algorithm>

namespace GeomUtils {

// Progress indicator that asks an OCCT algorithm to stop once `ms` have passed. OCCT only checks it between
// its own steps, so pair it with runWithTimeLimit when one step can run for minutes.
class TimeLimit : public Message_ProgressIndicator {
public:
    explicit TimeLimit(qint64 ms) : m_ms(ms) { m_timer.start(); }
    Standard_Boolean UserBreak() override { return expired(); }
    void Show(const Message_ProgressScope &, const Standard_Boolean) override {}
    bool expired() const { return m_timer.elapsed() > m_ms; }
    qint64 remaining() const { return std::max<qint64>(0, m_ms - m_timer.elapsed()); }

private:
    QElapsedTimer m_timer;
    qint64 m_ms;
};

}
