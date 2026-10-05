#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QMouseEvent>
#include <QPainter>
#include "../core/colorcurve.h"

class CurveEditorWidget : public QWidget {
    Q_OBJECT

public:
    explicit CurveEditorWidget(CurveType type = CurveType::ColorSpectrum,
                              const QString &title = QString(),
                              QWidget *parent = nullptr);

    CurveType curveType() const { return m_type; }
    void setCurveType(CurveType type);

    const ColorCurve& curve() const { return m_curve; }
    void setCurve(const ColorCurve &curve);

    void resetToDefault();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void curveChanged(const ColorCurve &curve);
    void curveChangeCommitted(const ColorCurve &curve);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QRectF plotAreaRect() const;
    QRectF gradientBarRect() const;
    QPointF normalizedToScreen(const ColorCurvePoint &pt) const;
    ColorCurvePoint screenToNormalized(const QPointF &screenPos) const;
    int findPointNear(const QPointF &screenPos, double radius = 10.0) const;
    QString formatPointTooltip(int index, const ColorCurvePoint &pt) const;
    QString colorNameFromHue(double hueNorm) const;

    CurveType m_type;
    ColorCurve m_curve;
    QString m_title;

    int m_draggedPointIndex = -1;
    int m_hoveredPointIndex = -1;
    QPointF m_lastMousePos;
    bool m_isHoveringPlot = false;

    QPushButton *m_resetBtn = nullptr;
    QLabel *m_titleLabel = nullptr;
};
