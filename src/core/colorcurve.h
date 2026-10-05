#pragma once

#include <QVector>
#include <QtGlobal>
#include <cstdint>

enum class CurveType {
    Luma = 0,          // Brillo y luminosidad (X: Entrada 0..1 [Negro a Blanco], Y: Salida 0..1)
    ColorSpectrum = 1, // Espectro de color (X: Matiz 0..1 [Rojo->Amarillo->Verde->Cian->Azul->Magenta->Rojo], Y: Intensidad 0..1 [0.5 = neutro 1.0x])
    Red = 2,           // Canal Rojo
    Green = 3,         // Canal Verde
    Blue = 4           // Canal Azul
};

struct ColorCurvePoint {
    double x = 0.0; // [0.0, 1.0]
    double y = 0.5; // [0.0, 1.0]

    ColorCurvePoint() = default;
    ColorCurvePoint(double px, double py)
        : x(qBound(0.0, px, 1.0)), y(qBound(0.0, py, 1.0)) {}

    bool operator==(const ColorCurvePoint &o) const {
        return qAbs(x - o.x) < 1e-4 && qAbs(y - o.y) < 1e-4;
    }
    bool operator!=(const ColorCurvePoint &o) const {
        return !(*this == o);
    }
};

class ColorCurve {
public:
    explicit ColorCurve(CurveType type = CurveType::Luma);
    ColorCurve(CurveType type, const QVector<ColorCurvePoint> &points);

    CurveType type() const { return m_type; }
    void setType(CurveType type) { m_type = type; }

    int pointCount() const { return m_points.size(); }
    const QVector<ColorCurvePoint>& points() const { return m_points; }
    const ColorCurvePoint& point(int index) const;

    // Point manipulation
    int addPoint(double x, double y);
    bool movePoint(int index, double newX, double newY);
    bool removePoint(int index);
    void clearAndSetPoints(const QVector<ColorCurvePoint> &pts);

    // Curve evaluation using Fritsch-Carlson Monotone Cubic Spline (C1-continuous)
    double evaluate(double x) const;
    void buildLut256(uint8_t *lut) const;

    bool isIdentity() const;

    // Factory methods
    static ColorCurve defaultLuma();
    static ColorCurve defaultColorSpectrum();
    static ColorCurve defaultChannel(CurveType type);

    bool operator==(const ColorCurve &o) const;
    bool operator!=(const ColorCurve &o) const { return !(*this == o); }

private:
    void sortPoints();
    void clampEndpoints();

    CurveType m_type = CurveType::Luma;
    QVector<ColorCurvePoint> m_points;
};
