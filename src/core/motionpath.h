#pragma once

#include <QPointF>
#include <QVector>
#include <QString>
#include <cmath>

enum class MotionPreset {
    None,            // Sin desplazamiento
    LeftToRight,     // Izquierda a Derecha
    RightToLeft,     // Derecha a Izquierda
    TopToBottom,     // Arriba a Abajo
    BottomToTop,     // Abajo a Arriba
    DiagonalTLBR,    // Diagonal (Arriba-Izquierda a Abajo-Derecha)
    DiagonalBLTR,    // Diagonal (Abajo-Izquierda a Arriba-Derecha)
    Custom           // A mano alzada / Personalizado
};

enum class MotionEasing {
    Linear,
    EaseInOut,
    EaseIn,
    EaseOut
};

struct MotionWaypoint {
    QPointF pos;             // Coordenadas relativas al centro del lienzo (posX, posY)
    bool isCurved = false;   // ¿Posee curvatura Bézier activa?
    QPointF handleIn;        // Offset del tirador entrante (tangente)
    QPointF handleOut;       // Offset del tirador saliente (tangente)

    MotionWaypoint() = default;
    MotionWaypoint(const QPointF &p, bool curved = false, const QPointF &hIn = QPointF(0, 0), const QPointF &hOut = QPointF(0, 0))
        : pos(p), isCurved(curved), handleIn(hIn), handleOut(hOut) {}

    bool operator==(const MotionWaypoint &other) const {
        return pos == other.pos && isCurved == other.isCurved &&
               handleIn == other.handleIn && handleOut == other.handleOut;
    }
    bool operator!=(const MotionWaypoint &other) const {
        return !(*this == other);
    }
};

class MotionPath {
public:
    MotionPath();

    bool isEnabled() const { return m_preset != MotionPreset::None && m_waypoints.size() >= 2; }
    MotionPreset preset() const { return m_preset; }
    void setPreset(MotionPreset p, const QPointF &basePos = QPointF(0, 0));

    MotionEasing easing() const { return m_easing; }
    void setEasing(MotionEasing e) { m_easing = e; }

    const QVector<MotionWaypoint>& waypoints() const { return m_waypoints; }
    QVector<MotionWaypoint>& waypoints() { return m_waypoints; }
    void setWaypoints(const QVector<MotionWaypoint> &pts) { m_waypoints = pts; }

    int waypointCount() const { return m_waypoints.size(); }
    MotionWaypoint waypoint(int index) const;
    void setWaypoint(int index, const MotionWaypoint &wp);
    void addWaypoint(const MotionWaypoint &wp);
    void insertWaypoint(int index, const MotionWaypoint &wp);
    void removeWaypoint(int index);
    void clearWaypoints();

    QPointF startPoint() const;
    void setStartPoint(const QPointF &p);
    QPointF endPoint() const;
    void setEndPoint(const QPointF &p);

    // Evalúa la posición en la trayectoria en tiempo normalizado u en [0.0, 1.0]
    QPointF evaluate(double u) const;

    // Evalúa la posición según el tiempo de la línea de tiempo
    QPointF evaluateAtTime(qint64 timelineMs, qint64 clipInMs, qint64 clipDurationMs) const;

    // Calcula automáticamente tiradores Bézier suaves estilo pluma vectorial
    void autoSmoothHandles(double tension = 0.33);

    // Invierte el sentido del desplazamiento
    void reverse();

    bool operator==(const MotionPath &other) const;
    bool operator!=(const MotionPath &other) const { return !(*this == other); }

private:
    MotionPreset m_preset = MotionPreset::None;
    MotionEasing m_easing = MotionEasing::EaseInOut;
    QVector<MotionWaypoint> m_waypoints;
};

QString motionPresetToString(MotionPreset preset);
MotionPreset stringToMotionPreset(const QString &str);
QString motionPresetDisplayName(MotionPreset preset);

QString motionEasingToString(MotionEasing easing);
MotionEasing stringToMotionEasing(const QString &str);
QString motionEasingDisplayName(MotionEasing easing);
