// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "Expression.h"
#include <QHash>
#include <QRegularExpression>
#include <cmath>
#include <stdexcept>

namespace {

const QHash<QString, double> &unitScales() {
    static const QHash<QString, double> units = {
        {"mm", 1.0}, {"cm", 10.0}, {"m", 1000.0}, {"um", 0.001},
        {"in", 25.4}, {"ft", 304.8}, {"yd", 914.4},
        {"deg", 1.0}, {"rad", 180.0 / M_PI},
    };
    return units;
}

const QHash<QString, double> &constants() {
    static const QHash<QString, double> c = { {"PI", M_PI}, {"pi", M_PI}, {"E", M_E} };
    return c;
}

bool isFunction(const QString &name) {
    static const QStringList fns = {"sin", "cos", "tan", "asin", "acos", "atan", "sqrt", "abs",
                                    "min", "max", "pow", "floor", "ceil", "round", "exp", "ln", "log"};
    return fns.contains(name);
}

double applyFunction(const QString &fn, const QList<double> &a) {
    auto need = [&](int n) {
        if (a.size() != n) throw std::runtime_error(QString("%1() expects %2 argument(s)").arg(fn).arg(n).toStdString());
    };
    // Trig functions take and return degrees, matching Fusion's angle unit
    const double d2r = M_PI / 180.0;
    if (fn == "sin")   { need(1); return std::sin(a[0] * d2r); }
    if (fn == "cos")   { need(1); return std::cos(a[0] * d2r); }
    if (fn == "tan")   { need(1); return std::tan(a[0] * d2r); }
    if (fn == "asin")  { need(1); return std::asin(a[0]) / d2r; }
    if (fn == "acos")  { need(1); return std::acos(a[0]) / d2r; }
    if (fn == "atan")  { need(1); return std::atan(a[0]) / d2r; }
    if (fn == "sqrt")  { need(1); return std::sqrt(a[0]); }
    if (fn == "abs")   { need(1); return std::fabs(a[0]); }
    if (fn == "floor") { need(1); return std::floor(a[0]); }
    if (fn == "ceil")  { need(1); return std::ceil(a[0]); }
    if (fn == "round") { need(1); return std::round(a[0]); }
    if (fn == "exp")   { need(1); return std::exp(a[0]); }
    if (fn == "ln")    { need(1); return std::log(a[0]); }
    if (fn == "log")   { need(1); return std::log10(a[0]); }
    if (fn == "pow")   { need(2); return std::pow(a[0], a[1]); }
    if (fn == "min" || fn == "max") {
        if (a.isEmpty()) throw std::runtime_error("min/max need arguments");
        double r = a[0];
        for (double v : a) r = fn == "min" ? std::min(r, v) : std::max(r, v);
        return r;
    }
    throw std::runtime_error(("Unknown function " + fn).toStdString());
}

class Parser {
public:
    Parser(const QString &s, const Expression::Resolver &r) : m_s(s), m_resolve(r) {}

    double parse() {
        double v = expr();
        skipWs();
        if (m_pos < m_s.size())
            throw std::runtime_error(QString("Unexpected '%1'").arg(m_s.mid(m_pos, 1)).toStdString());
        return v;
    }

private:
    void skipWs() { while (m_pos < m_s.size() && m_s[m_pos].isSpace()) ++m_pos; }
    bool accept(QChar c) {
        skipWs();
        if (m_pos < m_s.size() && m_s[m_pos] == c) { ++m_pos; return true; }
        return false;
    }

    double expr() {
        double v = term();
        for (;;) {
            if (accept('+')) v += term();
            else if (accept('-')) v -= term();
            else return v;
        }
    }

    double term() {
        double v = power();
        for (;;) {
            if (accept('*')) v *= power();
            else if (accept('/')) {
                double d = power();
                if (d == 0.0) throw std::runtime_error("Division by zero");
                v /= d;
            } else return v;
        }
    }

    double power() {
        double base = unary();
        if (accept('^')) return std::pow(base, power());
        return base;
    }

    double unary() {
        if (accept('-')) return -unary();
        if (accept('+')) return unary();
        return withUnit(primary());
    }

    // A unit directly after a value scales it: "5 mm", "(a+b) in", "30deg"
    double withUnit(double v) {
        skipWs();
        int save = m_pos;
        QString id = identifier();
        if (!id.isEmpty() && unitScales().contains(id)) return v * unitScales().value(id);
        m_pos = save;
        return v;
    }

    QString identifier() {
        skipWs();
        int start = m_pos;
        if (m_pos < m_s.size() && (m_s[m_pos].isLetter() || m_s[m_pos] == '_')) {
            while (m_pos < m_s.size() && (m_s[m_pos].isLetterOrNumber() || m_s[m_pos] == '_')) ++m_pos;
        }
        return m_s.mid(start, m_pos - start);
    }

    double primary() {
        skipWs();
        if (m_pos >= m_s.size()) throw std::runtime_error("Unexpected end of expression");
        QChar c = m_s[m_pos];
        if (c == '(') {
            ++m_pos;
            double v = expr();
            if (!accept(')')) throw std::runtime_error("Missing ')'");
            return v;
        }
        if (c.isDigit() || c == '.') {
            int start = m_pos;
            while (m_pos < m_s.size() && (m_s[m_pos].isDigit() || m_s[m_pos] == '.')) ++m_pos;
            if (m_pos < m_s.size() && (m_s[m_pos] == 'e' || m_s[m_pos] == 'E')) {
                int save = m_pos++;
                if (m_pos < m_s.size() && (m_s[m_pos] == '+' || m_s[m_pos] == '-')) ++m_pos;
                if (m_pos < m_s.size() && m_s[m_pos].isDigit()) {
                    while (m_pos < m_s.size() && m_s[m_pos].isDigit()) ++m_pos;
                } else {
                    m_pos = save;
                }
            }
            bool ok = false;
            double v = m_s.mid(start, m_pos - start).toDouble(&ok);
            if (!ok) throw std::runtime_error("Invalid number");
            return v;
        }
        QString id = identifier();
        if (id.isEmpty()) throw std::runtime_error(QString("Unexpected '%1'").arg(c).toStdString());
        if (isFunction(id)) {
            if (!accept('(')) throw std::runtime_error(("Expected '(' after " + id).toStdString());
            QList<double> args;
            if (!accept(')')) {
                do { args.append(expr()); } while (accept(','));
                if (!accept(')')) throw std::runtime_error("Missing ')'");
            }
            return applyFunction(id, args);
        }
        if (constants().contains(id)) return constants().value(id);
        if (m_resolve) {
            bool ok = false;
            double v = m_resolve(id, ok);
            if (ok) return v;
        }
        throw std::runtime_error(("Unknown parameter '" + id + "'").toStdString());
    }

    QString m_s;
    int m_pos = 0;
    Expression::Resolver m_resolve;
};

} // namespace

double Expression::evaluate(const QString &expr, const Resolver &resolver, QString *error) {
    if (error) error->clear();
    try {
        if (expr.trimmed().isEmpty()) throw std::runtime_error("Empty expression");
        return Parser(expr, resolver).parse();
    } catch (const std::exception &e) {
        if (error) *error = QString::fromUtf8(e.what());
        return 0.0;
    }
}

double Expression::evaluate(const QString &expr, QString *error) {
    return evaluate(expr, Resolver(), error);
}

QStringList Expression::identifiers(const QString &expr) {
    static const QRegularExpression re("[A-Za-z_][A-Za-z0-9_]*");
    QStringList out;
    auto it = re.globalMatch(expr);
    while (it.hasNext()) {
        const QString id = it.next().captured(0);
        if (unitScales().contains(id) || constants().contains(id) || isFunction(id)) continue;
        if (!out.contains(id)) out.append(id);
    }
    return out;
}

bool Expression::isValidName(const QString &name) {
    static const QRegularExpression re("^[A-Za-z_][A-Za-z0-9_]*$");
    return re.match(name).hasMatch() && !unitScales().contains(name)
        && !constants().contains(name) && !isFunction(name);
}
