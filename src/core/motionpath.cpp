#include "motionpath.h"
#include <QtMath>
#include <algorithm>

MotionPath::MotionPath()
    : m_preset(MotionPreset::None), m_easing(MotionEasing::EaseInOut)
{
}

void MotionPath::setPreset(MotionPreset p, const QPointF &basePos)
{
    m_preset = p;
    m_waypoints.clear();

    switch (p) {
    case MotionPreset::None:
        break;

    case MotionPreset::LeftToRight:
        m_waypoints.append(MotionWaypoint(QPointF(basePos.x() - 600.0, basePos.y()), false));
        m_waypoints.append(MotionWaypoint(QPointF(basePos.x() + 600.0, basePos.y()), false));
        break;

    case MotionPreset::RightToLeft:
        m_waypoints.append(MotionWaypoint(QPointF(basePos.x() + 600.0, basePos.y()), false));
        m_waypoints.append(MotionWaypoint(QPointF(basePos.x() - 600.0, basePos.y()), false));
        break;

    case MotionPreset::TopToBottom:
        m_waypoints.append(MotionWaypoint(QPointF(basePos.x(), basePos.y() - 350.0), false));
        m_waypoints.append(MotionWaypoint(QPointF(basePos.x(), basePos.y() + 350.0), false));
        break;

    case MotionPreset::BottomToTop:
        m_waypoints.append(MotionWaypoint(QPointF(basePos.x(), basePos.y() + 350.0), false));
        m_waypoints.append(MotionWaypoint(QPointF(basePos.x(), basePos.y() - 350.0), false));
        break;

    case MotionPreset::DiagonalTLBR:
        m_waypoints.append(MotionWaypoint(QPointF(basePos.x() - 600.0, basePos.y() - 350.0), false));
        m_waypoints.append(MotionWaypoint(QPointF(basePos.x() + 600.0, basePos.y() + 350.0), false));
        break;

    case MotionPreset::DiagonalBLTR:
        m_waypoints.append(MotionWaypoint(QPointF(basePos.x() - 600.0, basePos.y() + 350.0), false));
        m_waypoints.append(MotionWaypoint(QPointF(basePos.x() + 600.0, basePos.y() - 350.0), false));
        break;

    case MotionPreset::Custom:
        m_waypoints.append(MotionWaypoint(basePos - QPointF(300.0, 0.0), true, QPointF(0, 0), QPointF(100.0, -60.0)));
        m_waypoints.append(MotionWaypoint(basePos + QPointF(300.0, 0.0), true, QPointF(-100.0, 60.0), QPointF(0, 0)));
        break;
    }
}

MotionWaypoint MotionPath::waypoint(int index) const
{
    if (index >= 0 && index < m_waypoints.size()) {
        return m_waypoints[index];
    }
    return MotionWaypoint();
}

void MotionPath::setWaypoint(int index, const MotionWaypoint &wp)
{
    if (index >= 0 && index < m_waypoints.size()) {
        m_waypoints[index] = wp;
    }
}

void MotionPath::addWaypoint(const MotionWaypoint &wp)
{
    m_waypoints.append(wp);
    if (m_preset == MotionPreset::None) {
        m_preset = MotionPreset::Custom;
    }
}

void MotionPath::insertWaypoint(int index, const MotionWaypoint &wp)
{
    if (index < 0) {
        m_waypoints.prepend(wp);
    } else if (index >= m_waypoints.size()) {
        m_waypoints.append(wp);
    } else {
        m_waypoints.insert(index, wp);
    }
    if (m_preset != MotionPreset::Custom) {
        m_preset = MotionPreset::Custom;
    }
}

void MotionPath::removeWaypoint(int index)
{
    if (index >= 0 && index < m_waypoints.size()) {
        m_waypoints.removeAt(index);
    }
    if (m_waypoints.size() < 2) {
        m_preset = MotionPreset::None;
    }
}

void MotionPath::clearWaypoints()
{
    m_waypoints.clear();
    m_preset = MotionPreset::None;
}

QPointF MotionPath::startPoint() const
{
    return m_waypoints.isEmpty() ? QPointF(0, 0) : m_waypoints.first().pos;
}

void MotionPath::setStartPoint(const QPointF &p)
{
    if (m_waypoints.isEmpty()) {
        m_waypoints.append(MotionWaypoint(p));
        m_waypoints.append(MotionWaypoint(p + QPointF(200, 0)));
    } else {
        m_waypoints.first().pos = p;
    }
}

QPointF MotionPath::endPoint() const
{
    return m_waypoints.isEmpty() ? QPointF(0, 0) : m_waypoints.last().pos;
}

void MotionPath::setEndPoint(const QPointF &p)
{
    if (m_waypoints.isEmpty()) {
        m_waypoints.append(MotionWaypoint(p - QPointF(200, 0)));
        m_waypoints.append(MotionWaypoint(p));
    } else if (m_waypoints.size() == 1) {
        m_waypoints.append(MotionWaypoint(p));
    } else {
        m_waypoints.last().pos = p;
    }
}

QPointF MotionPath::evaluate(double u) const
{
    if (m_waypoints.isEmpty()) {
        return QPointF(0, 0);
    }
    if (m_waypoints.size() == 1) {
        return m_waypoints.first().pos;
    }

    // Clamp normalized time
    u = qBound(0.0, u, 1.0);

    // Apply temporal easing
    double e = u;
    switch (m_easing) {
    case MotionEasing::Linear:
        e = u;
        break;
    case MotionEasing::EaseIn:
        e = u * u;
        break;
    case MotionEasing::EaseOut:
        e = 1.0 - (1.0 - u) * (1.0 - u);
        break;
    case MotionEasing::EaseInOut:
        e = (u < 0.5) ? (2.0 * u * u) : (1.0 - std::pow(-2.0 * u + 2.0, 2) / 2.0);
        break;
    }

    e = qBound(0.0, e, 1.0);

    const int numSegments = m_waypoints.size() - 1;
    const double segLen = 1.0 / numSegments;

    int segIndex = qBound(0, static_cast<int>(e / segLen), numSegments - 1);
    double localS = (e - segIndex * segLen) / segLen;
    localS = qBound(0.0, localS, 1.0);

    const MotionWaypoint &pA = m_waypoints[segIndex];
    const MotionWaypoint &pB = m_waypoints[segIndex + 1];

    if (pA.isCurved || pB.isCurved || !pA.handleOut.isNull() || !pB.handleIn.isNull()) {
        // Cubic Bézier interpolation
        QPointF p0 = pA.pos;
        QPointF p1 = pA.pos + pA.handleOut;
        QPointF p2 = pB.pos + pB.handleIn;
        QPointF p3 = pB.pos;

        double omt = 1.0 - localS;
        double omt2 = omt * omt;
        double omt3 = omt2 * omt;
        double s2 = localS * localS;
        double s3 = s2 * localS;

        return omt3 * p0 + 3.0 * omt2 * localS * p1 + 3.0 * omt * s2 * p2 + s3 * p3;
    } else {
        // Linear segment
        return (1.0 - localS) * pA.pos + localS * pB.pos;
    }
}

QPointF MotionPath::evaluateAtTime(qint64 timelineMs, qint64 clipInMs, qint64 clipDurationMs) const
{
    if (clipDurationMs <= 0) {
        return startPoint();
    }
    double u = static_cast<double>(timelineMs - clipInMs) / static_cast<double>(clipDurationMs);
    return evaluate(u);
}

void MotionPath::autoSmoothHandles(double tension)
{
    const int n = m_waypoints.size();
    if (n < 2) return;

    for (int i = 0; i < n; ++i) {
        m_waypoints[i].isCurved = true;
        if (i == 0) {
            QPointF dir = m_waypoints[1].pos - m_waypoints[0].pos;
            m_waypoints[i].handleOut = dir * tension;
            m_waypoints[i].handleIn = QPointF(0, 0);
        } else if (i == n - 1) {
            QPointF dir = m_waypoints[n - 1].pos - m_waypoints[n - 2].pos;
            m_waypoints[i].handleIn = -dir * tension;
            m_waypoints[i].handleOut = QPointF(0, 0);
        } else {
            QPointF dir = (m_waypoints[i + 1].pos - m_waypoints[i - 1].pos) / 2.0;
            m_waypoints[i].handleOut = dir * tension;
            m_waypoints[i].handleIn = -dir * tension;
        }
    }
    m_preset = MotionPreset::Custom;
}

void MotionPath::reverse()
{
    std::reverse(m_waypoints.begin(), m_waypoints.end());
    for (auto &wp : m_waypoints) {
        std::swap(wp.handleIn, wp.handleOut);
        wp.handleIn = -wp.handleIn;
        wp.handleOut = -wp.handleOut;
    }
    m_preset = MotionPreset::Custom;
}

bool MotionPath::operator==(const MotionPath &other) const
{
    return m_preset == other.m_preset &&
           m_easing == other.m_easing &&
           m_waypoints == other.m_waypoints;
}

QString motionPresetToString(MotionPreset preset)
{
    switch (preset) {
    case MotionPreset::None: return "None";
    case MotionPreset::LeftToRight: return "LeftToRight";
    case MotionPreset::RightToLeft: return "RightToLeft";
    case MotionPreset::TopToBottom: return "TopToBottom";
    case MotionPreset::BottomToTop: return "BottomToTop";
    case MotionPreset::DiagonalTLBR: return "DiagonalTLBR";
    case MotionPreset::DiagonalBLTR: return "DiagonalBLTR";
    case MotionPreset::Custom: return "Custom";
    }
    return "None";
}

MotionPreset stringToMotionPreset(const QString &str)
{
    QString s = str.trimmed().toLower();
    if (s == "lefttoright" || s == "izquierdaderecha") return MotionPreset::LeftToRight;
    if (s == "righttoleft" || s == "derechaizquierda") return MotionPreset::RightToLeft;
    if (s == "toptobottom" || s == "arribaabajo") return MotionPreset::TopToBottom;
    if (s == "bottomtotop" || s == "abajoarriba") return MotionPreset::BottomToTop;
    if (s == "diagonaltlbr") return MotionPreset::DiagonalTLBR;
    if (s == "diagonalbltr") return MotionPreset::DiagonalBLTR;
    if (s == "custom" || s == "personalizado") return MotionPreset::Custom;
    return MotionPreset::None;
}

QString motionPresetDisplayName(MotionPreset preset)
{
    switch (preset) {
    case MotionPreset::None: return "Sin desplazamiento (Estático)";
    case MotionPreset::LeftToRight: return "➡️ Izquierda a Derecha";
    case MotionPreset::RightToLeft: return "⬅️ Derecha a Izquierda";
    case MotionPreset::TopToBottom: return "⬇️ Arriba a Abajo";
    case MotionPreset::BottomToTop: return "⬆️ Abajo a Arriba";
    case MotionPreset::DiagonalTLBR: return "↘️ Diagonal (NO a SE)";
    case MotionPreset::DiagonalBLTR: return "↗️ Diagonal (SO a NE)";
    case MotionPreset::Custom: return "🎨 Personalizado / Mano Alzada";
    }
    return "Sin desplazamiento";
}

QString motionEasingToString(MotionEasing easing)
{
    switch (easing) {
    case MotionEasing::Linear: return "Linear";
    case MotionEasing::EaseInOut: return "EaseInOut";
    case MotionEasing::EaseIn: return "EaseIn";
    case MotionEasing::EaseOut: return "EaseOut";
    }
    return "EaseInOut";
}

MotionEasing stringToMotionEasing(const QString &str)
{
    QString s = str.trimmed().toLower();
    if (s == "linear" || s == "lineal") return MotionEasing::Linear;
    if (s == "easein") return MotionEasing::EaseIn;
    if (s == "easeout") return MotionEasing::EaseOut;
    return MotionEasing::EaseInOut;
}

QString motionEasingDisplayName(MotionEasing easing)
{
    switch (easing) {
    case MotionEasing::Linear: return "Lineal (Velocidad Constante)";
    case MotionEasing::EaseInOut: return "Suave In-Out (Ease In-Out)";
    case MotionEasing::EaseIn: return "Acelerado (Ease In)";
    case MotionEasing::EaseOut: return "Frenado (Ease Out)";
    }
    return "Suave In-Out";
}
