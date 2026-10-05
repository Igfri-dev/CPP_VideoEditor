#include "colorcurve.h"
#include <algorithm>
#include <cmath>

static const ColorCurvePoint s_dummyPoint(0.0, 0.0);

ColorCurve::ColorCurve(CurveType type)
    : m_type(type)
{
    if (m_type == CurveType::ColorSpectrum) {
        m_points = {
            ColorCurvePoint(0.00, 0.5),
            ColorCurvePoint(0.17, 0.5),
            ColorCurvePoint(0.33, 0.5),
            ColorCurvePoint(0.50, 0.5),
            ColorCurvePoint(0.67, 0.5),
            ColorCurvePoint(0.83, 0.5),
            ColorCurvePoint(1.00, 0.5)
        };
    } else if (m_type == CurveType::Luma) {
        m_points = {
            ColorCurvePoint(0.0, 0.0),
            ColorCurvePoint(0.5, 0.5),
            ColorCurvePoint(1.0, 1.0)
        };
    } else {
        m_points = {
            ColorCurvePoint(0.0, 0.0),
            ColorCurvePoint(1.0, 1.0)
        };
    }
}

ColorCurve::ColorCurve(CurveType type, const QVector<ColorCurvePoint> &points)
    : m_type(type), m_points(points)
{
    sortPoints();
    clampEndpoints();
}

const ColorCurvePoint& ColorCurve::point(int index) const
{
    if (index >= 0 && index < m_points.size()) {
        return m_points[index];
    }
    return s_dummyPoint;
}

int ColorCurve::addPoint(double x, double y)
{
    double cx = qBound(0.0, x, 1.0);
    double cy = qBound(0.0, y, 1.0);

    // If a point already exists very close in X, just update its Y
    for (int i = 0; i < m_points.size(); ++i) {
        if (qAbs(m_points[i].x - cx) < 0.015) {
            m_points[i].y = cy;
            return i;
        }
    }

    m_points.append(ColorCurvePoint(cx, cy));
    sortPoints();
    clampEndpoints();

    for (int i = 0; i < m_points.size(); ++i) {
        if (qAbs(m_points[i].x - cx) < 1e-4) {
            return i;
        }
    }
    return -1;
}

bool ColorCurve::movePoint(int index, double newX, double newY)
{
    if (index < 0 || index >= m_points.size()) {
        return false;
    }

    double cy = qBound(0.0, newY, 1.0);

    // Endpoints are pinned to X=0.0 and X=1.0, but their Y can move freely
    if (index == 0) {
        m_points[0].x = 0.0;
        m_points[0].y = cy;
        return true;
    }
    if (index == m_points.size() - 1) {
        m_points[index].x = 1.0;
        m_points[index].y = cy;
        return true;
    }

    // Intermediate points: maintain order within neighbor boundaries
    double minX = m_points[index - 1].x + 0.01;
    double maxX = m_points[index + 1].x - 0.01;
    if (minX > maxX) {
        double mid = (m_points[index - 1].x + m_points[index + 1].x) / 2.0;
        minX = mid;
        maxX = mid;
    }
    double cx = qBound(minX, newX, maxX);

    m_points[index].x = cx;
    m_points[index].y = cy;
    return true;
}

bool ColorCurve::removePoint(int index)
{
    // Do not remove endpoints, and ensure at least 2 points remain
    if (index <= 0 || index >= m_points.size() - 1 || m_points.size() <= 2) {
        return false;
    }
    m_points.removeAt(index);
    return true;
}

void ColorCurve::clearAndSetPoints(const QVector<ColorCurvePoint> &pts)
{
    m_points = pts;
    sortPoints();
    clampEndpoints();
}

double ColorCurve::evaluate(double x) const
{
    const int n = m_points.size();
    if (n == 0) {
        return x;
    }
    if (n == 1) {
        return m_points[0].y;
    }

    double clampedX = qBound(0.0, x, 1.0);

    if (clampedX <= m_points.first().x) {
        return m_points.first().y;
    }
    if (clampedX >= m_points.last().x) {
        return m_points.last().y;
    }

    if (n == 2) {
        double dx = m_points[1].x - m_points[0].x;
        if (dx < 1e-9) return m_points[0].y;
        double t = (clampedX - m_points[0].x) / dx;
        return qBound(0.0, m_points[0].y + t * (m_points[1].y - m_points[0].y), 1.0);
    }

    // Step 1: Compute secant slopes
    QVector<double> deltas(n - 1);
    for (int k = 0; k < n - 1; ++k) {
        double dx = m_points[k + 1].x - m_points[k].x;
        deltas[k] = (dx > 1e-9) ? (m_points[k + 1].y - m_points[k].y) / dx : 0.0;
    }

    // Step 2: Initialize tangents d_k
    QVector<double> d(n);
    d[0] = deltas[0];
    d[n - 1] = deltas[n - 2];
    for (int k = 1; k < n - 1; ++k) {
        if (deltas[k - 1] * deltas[k] <= 0.0) {
            d[k] = 0.0;
        } else {
            d[k] = (deltas[k - 1] + deltas[k]) / 2.0;
        }
    }

    // Step 3: Fritsch-Carlson monotonicity constraint
    for (int k = 0; k < n - 1; ++k) {
        if (std::abs(deltas[k]) < 1e-9) {
            d[k] = 0.0;
            d[k + 1] = 0.0;
        } else {
            double alpha = d[k] / deltas[k];
            double beta = d[k + 1] / deltas[k];
            if (alpha < 0.0) d[k] = 0.0;
            if (beta < 0.0) d[k + 1] = 0.0;
            double hypotSq = alpha * alpha + beta * beta;
            if (hypotSq > 9.0) {
                double tau = 3.0 / std::sqrt(hypotSq);
                d[k] = tau * alpha * deltas[k];
                d[k + 1] = tau * beta * deltas[k];
            }
        }
    }

    // Step 4: Locate segment containing clampedX
    int seg = 0;
    for (int k = 0; k < n - 1; ++k) {
        if (clampedX >= m_points[k].x && clampedX <= m_points[k + 1].x) {
            seg = k;
            break;
        }
    }

    double h = m_points[seg + 1].x - m_points[seg].x;
    if (h < 1e-9) {
        return m_points[seg].y;
    }

    double t = (clampedX - m_points[seg].x) / h;
    double t2 = t * t;
    double t3 = t2 * t;

    double h00 = 2.0 * t3 - 3.0 * t2 + 1.0;
    double h10 = t3 - 2.0 * t2 + t;
    double h01 = -2.0 * t3 + 3.0 * t2;
    double h11 = t3 - t2;

    double y = h00 * m_points[seg].y +
               h10 * h * d[seg] +
               h01 * m_points[seg + 1].y +
               h11 * h * d[seg + 1];

    return qBound(0.0, y, 1.0);
}

void ColorCurve::buildLut256(uint8_t *lut) const
{
    if (!lut) return;
    for (int i = 0; i < 256; ++i) {
        double val = evaluate(i / 255.0);
        lut[i] = static_cast<uint8_t>(qBound(0.0, val * 255.0 + 0.5, 255.0));
    }
}

bool ColorCurve::isIdentity() const
{
    if (m_type == CurveType::ColorSpectrum) {
        for (const auto &p : m_points) {
            if (qAbs(p.y - 0.5) > 0.02) {
                return false;
            }
        }
        return true;
    }

    // Luma / RGB channel curves: identity is y = x
    for (const auto &p : m_points) {
        if (qAbs(p.y - p.x) > 0.02) {
            return false;
        }
    }
    return true;
}

ColorCurve ColorCurve::defaultLuma()
{
    return ColorCurve(CurveType::Luma);
}

ColorCurve ColorCurve::defaultColorSpectrum()
{
    return ColorCurve(CurveType::ColorSpectrum);
}

ColorCurve ColorCurve::defaultChannel(CurveType type)
{
    return ColorCurve(type);
}

bool ColorCurve::operator==(const ColorCurve &o) const
{
    if (m_type != o.m_type || m_points.size() != o.m_points.size()) {
        return false;
    }
    for (int i = 0; i < m_points.size(); ++i) {
        if (m_points[i] != o.m_points[i]) {
            return false;
        }
    }
    return true;
}

void ColorCurve::sortPoints()
{
    std::sort(m_points.begin(), m_points.end(), [](const ColorCurvePoint &a, const ColorCurvePoint &b) {
        return a.x < b.x;
    });
}

void ColorCurve::clampEndpoints()
{
    if (m_points.isEmpty()) {
        m_points.append(ColorCurvePoint(0.0, 0.0));
        m_points.append(ColorCurvePoint(1.0, 1.0));
        return;
    }
    if (m_points.size() == 1) {
        m_points.append(ColorCurvePoint(1.0, 1.0));
    }
    m_points.first().x = 0.0;
    m_points.last().x = 1.0;
}
