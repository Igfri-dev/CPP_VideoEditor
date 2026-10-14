#pragma once

#include <QString>
#include <QVector>
#include <QPointF>
#include <QColor>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QtMath>
#include <algorithm>
#include <cmath>

enum class KeyframeInterpolation {
    Linear = 0,
    EaseIn = 1,
    EaseOut = 2,
    EaseInOut = 3,
    Hold = 4,
    Bezier = 5
};

inline QString keyframeInterpolationToString(KeyframeInterpolation interp)
{
    switch (interp) {
    case KeyframeInterpolation::Linear: return "Linear";
    case KeyframeInterpolation::EaseIn: return "EaseIn";
    case KeyframeInterpolation::EaseOut: return "EaseOut";
    case KeyframeInterpolation::EaseInOut: return "EaseInOut";
    case KeyframeInterpolation::Hold: return "Hold";
    case KeyframeInterpolation::Bezier: return "Bezier";
    }
    return "Linear";
}

inline KeyframeInterpolation stringToKeyframeInterpolation(const QString &str)
{
    QString s = str.trimmed();
    if (s.compare("EaseIn", Qt::CaseInsensitive) == 0 || s.compare("Ease In", Qt::CaseInsensitive) == 0)
        return KeyframeInterpolation::EaseIn;
    if (s.compare("EaseOut", Qt::CaseInsensitive) == 0 || s.compare("Ease Out", Qt::CaseInsensitive) == 0)
        return KeyframeInterpolation::EaseOut;
    if (s.compare("EaseInOut", Qt::CaseInsensitive) == 0 || s.compare("Ease In/Out", Qt::CaseInsensitive) == 0)
        return KeyframeInterpolation::EaseInOut;
    if (s.compare("Hold", Qt::CaseInsensitive) == 0)
        return KeyframeInterpolation::Hold;
    if (s.compare("Bezier", Qt::CaseInsensitive) == 0)
        return KeyframeInterpolation::Bezier;
    return KeyframeInterpolation::Linear;
}

inline QString keyframeInterpolationDisplayName(KeyframeInterpolation interp)
{
    switch (interp) {
    case KeyframeInterpolation::Linear: return "Lineal (Linear)";
    case KeyframeInterpolation::EaseIn: return "Aceleración (Ease In)";
    case KeyframeInterpolation::EaseOut: return "Desaceleración (Ease Out)";
    case KeyframeInterpolation::EaseInOut: return "Suave S (Ease In/Out)";
    case KeyframeInterpolation::Hold: return "Mantener (Hold)";
    case KeyframeInterpolation::Bezier: return "Curva Bézier";
    }
    return "Lineal";
}

template<typename T>
struct Keyframe {
    qint64 timeMs = 0; // Relative to clip start: 0 .. durationMs
    T value{};
    KeyframeInterpolation interpolation = KeyframeInterpolation::Linear;
    QPointF handleIn = QPointF(-0.33, 0.0);  // Incoming tangent handle offset (for Bezier)
    QPointF handleOut = QPointF(0.33, 0.0);  // Outgoing tangent handle offset (for Bezier)

    Keyframe() = default;
    Keyframe(qint64 t, const T &val, KeyframeInterpolation interp = KeyframeInterpolation::Linear)
        : timeMs(t), value(val), interpolation(interp) {}
    Keyframe(qint64 t, const T &val, KeyframeInterpolation interp, const QPointF &hIn, const QPointF &hOut)
        : timeMs(t), value(val), interpolation(interp), handleIn(hIn), handleOut(hOut) {}

    bool operator==(const Keyframe<T> &other) const {
        return timeMs == other.timeMs &&
               value == other.value &&
               interpolation == other.interpolation &&
               handleIn == other.handleIn &&
               handleOut == other.handleOut;
    }
    bool operator!=(const Keyframe<T> &other) const {
        return !(*this == other);
    }
};

namespace KeyframeMath {

// Normalized cubic Bézier evaluation: solves B_x(t) = u and evaluates B_y(t)
inline double evaluateCubicBezier(double u, const QPointF &hIn, const QPointF &hOut)
{
    u = qBound(0.0, u, 1.0);
    if (u <= 0.0) return 0.0;
    if (u >= 1.0) return 1.0;

    // Control points: P0 = (0,0), P1 = (p1x, p1y), P2 = (p2x, p2y), P3 = (1,1)
    double p1x = qBound(0.0, hOut.x(), 1.0);
    double p1y = hOut.y();
    double p2x = qBound(0.0, 1.0 + hIn.x(), 1.0);
    double p2y = 1.0 + hIn.y();

    // B_x(t) = 3*(1-t)^2*t*p1x + 3*(1-t)*t^2*p2x + t^3
    auto bx = [p1x, p2x](double t) -> double {
        double mt = 1.0 - t;
        return 3.0 * mt * mt * t * p1x + 3.0 * mt * t * t * p2x + t * t * t;
    };

    auto bxPrime = [p1x, p2x](double t) -> double {
        double mt = 1.0 - t;
        return 3.0 * mt * mt * p1x + 6.0 * mt * t * (p2x - p1x) + 3.0 * t * t * (1.0 - p2x);
    };

    // Newton-Raphson iteration with bisection fallback
    double t = u;
    for (int i = 0; i < 8; ++i) {
        double x = bx(t) - u;
        if (std::abs(x) < 1e-5) break;
        double d = bxPrime(t);
        if (std::abs(d) < 1e-6) break;
        t = qBound(0.0, t - x / d, 1.0);
    }

    // Evaluate B_y(t)
    double mt = 1.0 - t;
    return 3.0 * mt * mt * t * p1y + 3.0 * mt * t * t * p2y + t * t * t;
}

inline double ease(KeyframeInterpolation mode, double u, const QPointF &hIn = QPointF(-0.33, 0.0), const QPointF &hOut = QPointF(0.33, 0.0))
{
    u = qBound(0.0, u, 1.0);
    switch (mode) {
    case KeyframeInterpolation::Linear:
        return u;
    case KeyframeInterpolation::Hold:
        return (u >= 1.0) ? 1.0 : 0.0;
    case KeyframeInterpolation::EaseIn:
        // Quadratic ease in
        return u * u;
    case KeyframeInterpolation::EaseOut:
        // Quadratic ease out
        return u * (2.0 - u);
    case KeyframeInterpolation::EaseInOut:
        // Smoothstep cubic ease in-out
        return u * u * (3.0 - 2.0 * u);
    case KeyframeInterpolation::Bezier:
        return evaluateCubicBezier(u, hIn, hOut);
    }
    return u;
}

template<typename T>
inline T interpolateValue(const T &v0, const T &v1, double factor)
{
    return static_cast<T>(v0 + (v1 - v0) * factor);
}

template<>
inline int interpolateValue<int>(const int &v0, const int &v1, double factor)
{
    return static_cast<int>(std::round(v0 + (v1 - v0) * factor));
}

template<>
inline qint64 interpolateValue<qint64>(const qint64 &v0, const qint64 &v1, double factor)
{
    return static_cast<qint64>(std::round(v0 + (v1 - v0) * factor));
}

template<>
inline QPointF interpolateValue<QPointF>(const QPointF &v0, const QPointF &v1, double factor)
{
    return QPointF(v0.x() + (v1.x() - v0.x()) * factor,
                   v0.y() + (v1.y() - v0.y()) * factor);
}

template<>
inline QColor interpolateValue<QColor>(const QColor &v0, const QColor &v1, double factor)
{
    int r = qBound(0, static_cast<int>(std::round(v0.red() + (v1.red() - v0.red()) * factor)), 255);
    int g = qBound(0, static_cast<int>(std::round(v0.green() + (v1.green() - v0.green()) * factor)), 255);
    int b = qBound(0, static_cast<int>(std::round(v0.blue() + (v1.blue() - v0.blue()) * factor)), 255);
    int a = qBound(0, static_cast<int>(std::round(v0.alpha() + (v1.alpha() - v0.alpha()) * factor)), 255);
    return QColor(r, g, b, a);
}

} // namespace KeyframeMath

template<typename T>
class AnimatedProperty {
public:
    AnimatedProperty() = default;
    explicit AnimatedProperty(const T &defaultBase) : m_baseValue(defaultBase) {}

    bool isAnimated() const { return !m_keyframes.isEmpty(); }
    int keyframeCount() const { return m_keyframes.size(); }
    const QVector<Keyframe<T>>& keyframes() const { return m_keyframes; }
    QVector<Keyframe<T>>& keyframes() { return m_keyframes; }

    void setKeyframes(const QVector<Keyframe<T>> &kfs) {
        m_keyframes = kfs;
        std::sort(m_keyframes.begin(), m_keyframes.end(), [](const Keyframe<T> &a, const Keyframe<T> &b) {
            return a.timeMs < b.timeMs;
        });
    }

    const T& baseValue() const { return m_baseValue; }
    void setBaseValue(const T &val) { m_baseValue = val; }

    void clearKeyframes() { m_keyframes.clear(); }

    void setKeyframe(qint64 timeMs, const T &value, KeyframeInterpolation interp = KeyframeInterpolation::Linear) {
        for (int i = 0; i < m_keyframes.size(); ++i) {
            if (m_keyframes[i].timeMs == timeMs) {
                m_keyframes[i].value = value;
                m_keyframes[i].interpolation = interp;
                return;
            }
            if (m_keyframes[i].timeMs > timeMs) {
                m_keyframes.insert(i, Keyframe<T>(timeMs, value, interp));
                return;
            }
        }
        m_keyframes.append(Keyframe<T>(timeMs, value, interp));
    }

    void setKeyframeWithHandles(qint64 timeMs, const T &value, KeyframeInterpolation interp,
                                const QPointF &handleIn, const QPointF &handleOut) {
        for (int i = 0; i < m_keyframes.size(); ++i) {
            if (m_keyframes[i].timeMs == timeMs) {
                m_keyframes[i].value = value;
                m_keyframes[i].interpolation = interp;
                m_keyframes[i].handleIn = handleIn;
                m_keyframes[i].handleOut = handleOut;
                return;
            }
            if (m_keyframes[i].timeMs > timeMs) {
                m_keyframes.insert(i, Keyframe<T>(timeMs, value, interp, handleIn, handleOut));
                return;
            }
        }
        m_keyframes.append(Keyframe<T>(timeMs, value, interp, handleIn, handleOut));
    }

    bool removeKeyframeAt(qint64 timeMs, qint64 toleranceMs = 20) {
        for (int i = 0; i < m_keyframes.size(); ++i) {
            if (std::abs(m_keyframes[i].timeMs - timeMs) <= toleranceMs) {
                m_keyframes.removeAt(i);
                return true;
            }
        }
        return false;
    }

    bool removeKeyframeByIndex(int index) {
        if (index >= 0 && index < m_keyframes.size()) {
            m_keyframes.removeAt(index);
            return true;
        }
        return false;
    }

    bool hasKeyframeAt(qint64 timeMs, qint64 toleranceMs = 20) const {
        return findKeyframeIndex(timeMs, toleranceMs) >= 0;
    }

    int findKeyframeIndex(qint64 timeMs, qint64 toleranceMs = 20) const {
        int bestIdx = -1;
        qint64 bestDiff = toleranceMs + 1;
        for (int i = 0; i < m_keyframes.size(); ++i) {
            qint64 diff = std::abs(m_keyframes[i].timeMs - timeMs);
            if (diff <= toleranceMs && diff < bestDiff) {
                bestDiff = diff;
                bestIdx = i;
            }
        }
        return bestIdx;
    }

    Keyframe<T> keyframeAt(qint64 timeMs, qint64 toleranceMs = 20) const {
        int idx = findKeyframeIndex(timeMs, toleranceMs);
        if (idx >= 0 && idx < m_keyframes.size()) {
            return m_keyframes[idx];
        }
        return Keyframe<T>(timeMs, m_baseValue);
    }

    bool setInterpolationAt(qint64 timeMs, KeyframeInterpolation interp, qint64 toleranceMs = 20) {
        int idx = findKeyframeIndex(timeMs, toleranceMs);
        if (idx >= 0 && idx < m_keyframes.size()) {
            m_keyframes[idx].interpolation = interp;
            return true;
        }
        return false;
    }

    qint64 previousKeyframeTime(qint64 timeMs, qint64 toleranceMs = 10) const {
        qint64 prevTime = -1;
        for (const auto &kf : m_keyframes) {
            if (kf.timeMs < timeMs - toleranceMs) {
                prevTime = kf.timeMs;
            } else {
                break;
            }
        }
        return prevTime;
    }

    qint64 nextKeyframeTime(qint64 timeMs, qint64 toleranceMs = 10) const {
        for (const auto &kf : m_keyframes) {
            if (kf.timeMs > timeMs + toleranceMs) {
                return kf.timeMs;
            }
        }
        return -1;
    }

    T valueAt(qint64 timeMs) const {
        if (m_keyframes.isEmpty()) {
            return m_baseValue;
        }
        if (m_keyframes.size() == 1 || timeMs <= m_keyframes.first().timeMs) {
            return m_keyframes.first().value;
        }
        if (timeMs >= m_keyframes.last().timeMs) {
            return m_keyframes.last().value;
        }

        // Binary search for interval
        auto it = std::upper_bound(m_keyframes.begin(), m_keyframes.end(), timeMs,
                                   [](qint64 t, const Keyframe<T> &kf) {
            return t < kf.timeMs;
        });

        int idx1 = std::distance(m_keyframes.begin(), it);
        int idx0 = idx1 - 1;

        const auto &k0 = m_keyframes[idx0];
        const auto &k1 = m_keyframes[idx1];

        qint64 span = k1.timeMs - k0.timeMs;
        if (span <= 0) return k1.value;

        double u = static_cast<double>(timeMs - k0.timeMs) / static_cast<double>(span);
        double factor = KeyframeMath::ease(k0.interpolation, u, k0.handleIn, k0.handleOut);

        return KeyframeMath::interpolateValue(k0.value, k1.value, factor);
    }

    bool operator==(const AnimatedProperty<T> &other) const {
        return m_baseValue == other.m_baseValue && m_keyframes == other.m_keyframes;
    }
    bool operator!=(const AnimatedProperty<T> &other) const {
        return !(*this == other);
    }

private:
    T m_baseValue{};
    QVector<Keyframe<T>> m_keyframes;
};

// Serialization helpers
namespace KeyframeSerialization {

inline QJsonValue valueToJson(float v) { return QJsonValue(static_cast<double>(v)); }
inline QJsonValue valueToJson(double v) { return QJsonValue(v); }
inline QJsonValue valueToJson(int v) { return QJsonValue(v); }
inline QJsonValue valueToJson(qint64 v) { return QJsonValue(v); }

inline QJsonValue valueToJson(const QPointF &p) {
    QJsonObject obj;
    obj["x"] = p.x();
    obj["y"] = p.y();
    return obj;
}

inline QJsonValue valueToJson(const QColor &c) {
    return QJsonValue(c.name(QColor::HexArgb));
}

template<typename T>
inline T valueFromJson(const QJsonValue &val, const T &defaultVal = T{});

template<>
inline float valueFromJson<float>(const QJsonValue &val, const float &defaultVal) {
    return static_cast<float>(val.toDouble(defaultVal));
}

template<>
inline double valueFromJson<double>(const QJsonValue &val, const double &defaultVal) {
    return val.toDouble(defaultVal);
}

template<>
inline int valueFromJson<int>(const QJsonValue &val, const int &defaultVal) {
    return val.toInt(defaultVal);
}

template<>
inline qint64 valueFromJson<qint64>(const QJsonValue &val, const qint64 &defaultVal) {
    return val.toVariant().toLongLong();
}

template<>
inline QPointF valueFromJson<QPointF>(const QJsonValue &val, const QPointF &defaultVal) {
    if (!val.isObject()) return defaultVal;
    QJsonObject obj = val.toObject();
    return QPointF(obj.value("x").toDouble(defaultVal.x()),
                   obj.value("y").toDouble(defaultVal.y()));
}

template<>
inline QColor valueFromJson<QColor>(const QJsonValue &val, const QColor &defaultVal) {
    if (!val.isString()) return defaultVal;
    return QColor(val.toString());
}

template<typename T>
inline QJsonObject keyframeToJson(const Keyframe<T> &kf) {
    QJsonObject obj;
    obj["timeMs"] = kf.timeMs;
    obj["value"] = valueToJson(kf.value);
    obj["interpolation"] = keyframeInterpolationToString(kf.interpolation);
    if (kf.interpolation == KeyframeInterpolation::Bezier) {
        obj["hxIn"] = kf.handleIn.x();
        obj["hyIn"] = kf.handleIn.y();
        obj["hxOut"] = kf.handleOut.x();
        obj["hyOut"] = kf.handleOut.y();
    }
    return obj;
}

template<typename T>
inline Keyframe<T> keyframeFromJson(const QJsonObject &obj) {
    Keyframe<T> kf;
    kf.timeMs = obj.value("timeMs").toVariant().toLongLong();
    kf.value = valueFromJson<T>(obj.value("value"));
    kf.interpolation = stringToKeyframeInterpolation(obj.value("interpolation").toString("Linear"));
    if (obj.contains("hxIn")) {
        kf.handleIn = QPointF(obj.value("hxIn").toDouble(-0.33), obj.value("hyIn").toDouble(0.0));
    }
    if (obj.contains("hxOut")) {
        kf.handleOut = QPointF(obj.value("hxOut").toDouble(0.33), obj.value("hyOut").toDouble(0.0));
    }
    return kf;
}

template<typename T>
inline QJsonObject animatedPropertyToJson(const AnimatedProperty<T> &prop) {
    QJsonObject obj;
    obj["baseValue"] = valueToJson(prop.baseValue());
    if (prop.isAnimated()) {
        QJsonArray arr;
        for (const auto &kf : prop.keyframes()) {
            arr.append(keyframeToJson(kf));
        }
        obj["keyframes"] = arr;
    }
    return obj;
}

template<typename T>
inline AnimatedProperty<T> animatedPropertyFromJson(const QJsonObject &obj, const T &defaultBase = T{}) {
    AnimatedProperty<T> prop(defaultBase);
    if (obj.contains("baseValue")) {
        prop.setBaseValue(valueFromJson<T>(obj.value("baseValue"), defaultBase));
    }
    if (obj.contains("keyframes")) {
        QJsonArray arr = obj.value("keyframes").toArray();
        QVector<Keyframe<T>> kfs;
        kfs.reserve(arr.size());
        for (const auto &val : arr) {
            kfs.append(keyframeFromJson<T>(val.toObject()));
        }
        prop.setKeyframes(kfs);
    }
    return prop;
}

} // namespace KeyframeSerialization
