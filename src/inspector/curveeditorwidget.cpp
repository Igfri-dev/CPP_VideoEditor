#include "curveeditorwidget.h"
#include <QPainterPath>
#include <QToolTip>
#include <QHBoxLayout>
#include <cmath>

CurveEditorWidget::CurveEditorWidget(CurveType type, const QString &title, QWidget *parent)
    : QWidget(parent)
    , m_type(type)
    , m_curve(type)
    , m_title(title)
{
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    if (m_title.isEmpty()) {
        if (m_type == CurveType::ColorSpectrum) {
            m_title = "🎨 Intensidad por Color (Gradiente)";
        } else if (m_type == CurveType::Luma) {
            m_title = "☀️ Brillo y Luminosidad (Curva de Tonos)";
        } else if (m_type == CurveType::Red) {
            m_title = "🔴 Canal Rojo (R)";
        } else if (m_type == CurveType::Green) {
            m_title = "🟢 Canal Verde (G)";
        } else if (m_type == CurveType::Blue) {
            m_title = "🔵 Canal Azul (B)";
        }
    }

    // Reset button in top-right
    m_resetBtn = new QPushButton("↺", this);
    m_resetBtn->setToolTip("Restablecer curva a valores originales");
    m_resetBtn->setFixedSize(22, 20);
    m_resetBtn->setCursor(Qt::PointingHandCursor);
    m_resetBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #21262d; color: #8b949e; border: 1px solid #30363d; border-radius: 3px; font-weight: bold; font-size: 11px;"
        "}"
        "QPushButton:hover { background-color: #30363d; color: #ffffff; border-color: #58a6ff; }"
    );
    connect(m_resetBtn, &QPushButton::clicked, this, &CurveEditorWidget::resetToDefault);
}

void CurveEditorWidget::setCurveType(CurveType type)
{
    m_type = type;
    m_curve = ColorCurve(type);
    update();
}

void CurveEditorWidget::setCurve(const ColorCurve &curve)
{
    m_curve = curve;
    m_type = curve.type();
    update();
}

void CurveEditorWidget::resetToDefault()
{
    if (m_type == CurveType::ColorSpectrum) {
        m_curve = ColorCurve::defaultColorSpectrum();
    } else if (m_type == CurveType::Luma) {
        m_curve = ColorCurve::defaultLuma();
    } else {
        m_curve = ColorCurve::defaultChannel(m_type);
    }
    update();
    emit curveChanged(m_curve);
    emit curveChangeCommitted(m_curve);
}

QSize CurveEditorWidget::sizeHint() const
{
    return QSize(280, 175);
}

QSize CurveEditorWidget::minimumSizeHint() const
{
    return QSize(220, 150);
}

QRectF CurveEditorWidget::plotAreaRect() const
{
    const double marginLeft = 34.0;
    const double marginRight = 12.0;
    const double marginTop = 24.0;
    const double marginBottom = 22.0;
    return QRectF(marginLeft, marginTop,
                  qMax(10.0, width() - marginLeft - marginRight),
                  qMax(10.0, height() - marginTop - marginBottom));
}

QRectF CurveEditorWidget::gradientBarRect() const
{
    QRectF plot = plotAreaRect();
    return QRectF(plot.left(), plot.bottom() + 4.0, plot.width(), 12.0);
}

QPointF CurveEditorWidget::normalizedToScreen(const ColorCurvePoint &pt) const
{
    QRectF plot = plotAreaRect();
    double px = plot.left() + pt.x * plot.width();
    double py = plot.bottom() - pt.y * plot.height();
    return QPointF(px, py);
}

ColorCurvePoint CurveEditorWidget::screenToNormalized(const QPointF &screenPos) const
{
    QRectF plot = plotAreaRect();
    double nx = (screenPos.x() - plot.left()) / plot.width();
    double ny = (plot.bottom() - screenPos.y()) / plot.height();
    return ColorCurvePoint(qBound(0.0, nx, 1.0), qBound(0.0, ny, 1.0));
}

int CurveEditorWidget::findPointNear(const QPointF &screenPos, double radius) const
{
    double bestDist = radius;
    int bestIdx = -1;
    for (int i = 0; i < m_curve.pointCount(); ++i) {
        QPointF pScr = normalizedToScreen(m_curve.point(i));
        double d = std::hypot(pScr.x() - screenPos.x(), pScr.y() - screenPos.y());
        if (d < bestDist) {
            bestDist = d;
            bestIdx = i;
        }
    }
    return bestIdx;
}

QString CurveEditorWidget::colorNameFromHue(double hueNorm) const
{
    int deg = static_cast<int>(qBound(0.0, hueNorm, 1.0) * 360.0 + 0.5) % 360;
    if (deg >= 345 || deg < 15) return QString("Rojo (%1°)").arg(deg);
    if (deg >= 15 && deg < 45) return QString("Naranja (%1°)").arg(deg);
    if (deg >= 45 && deg < 75) return QString("Amarillo (%1°)").arg(deg);
    if (deg >= 75 && deg < 105) return QString("Lima (%1°)").arg(deg);
    if (deg >= 105 && deg < 145) return QString("Verde (%1°)").arg(deg);
    if (deg >= 145 && deg < 170) return QString("Turquesa (%1°)").arg(deg);
    if (deg >= 170 && deg < 205) return QString("Cian (%1°)").arg(deg);
    if (deg >= 205 && deg < 255) return QString("Azul (%1°)").arg(deg);
    if (deg >= 255 && deg < 285) return QString("Violeta (%1°)").arg(deg);
    if (deg >= 285 && deg < 320) return QString("Magenta (%1°)").arg(deg);
    return QString("Rosa (%1°)").arg(deg);
}

QString CurveEditorWidget::formatPointTooltip(int index, const ColorCurvePoint &pt) const
{
    Q_UNUSED(index);
    if (m_type == CurveType::ColorSpectrum) {
        int intensityPct = qRound(pt.y * 200.0);
        return QString("%1 • Intensidad: %2%").arg(colorNameFromHue(pt.x)).arg(intensityPct);
    } else {
        int inVal = qRound(pt.x * 255.0);
        int outVal = qRound(pt.y * 255.0);
        return QString("Entrada: %1 • Salida: %2").arg(inVal).arg(outVal);
    }
}

void CurveEditorWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Position reset button at top right
    m_resetBtn->move(width() - m_resetBtn->width() - 4, 3);

    // 1. Title bar text
    p.setPen(QColor("#c9d1d9"));
    QFont titleFont = font();
    titleFont.setPointSize(10);
    titleFont.setBold(true);
    p.setFont(titleFont);
    p.drawText(6, 16, m_title);

    // Status readout or instruction when hovering
    QFont statusFont = font();
    statusFont.setPointSize(9);
    statusFont.setBold(false);
    p.setFont(statusFont);

    if (m_draggedPointIndex >= 0 && m_draggedPointIndex < m_curve.pointCount()) {
        p.setPen(QColor("#58a6ff"));
        QString status = formatPointTooltip(m_draggedPointIndex, m_curve.point(m_draggedPointIndex));
        p.drawText(QRectF(width() - 210, 2, 175, 18), Qt::AlignRight | Qt::AlignVCenter, status);
    } else if (m_hoveredPointIndex >= 0 && m_hoveredPointIndex < m_curve.pointCount()) {
        p.setPen(QColor("#8b949e"));
        QString status = formatPointTooltip(m_hoveredPointIndex, m_curve.point(m_hoveredPointIndex));
        p.drawText(QRectF(width() - 210, 2, 175, 18), Qt::AlignRight | Qt::AlignVCenter, status);
    }

    QRectF plot = plotAreaRect();

    // 2. Draw Plot Area Background & Border
    p.setPen(QPen(QColor("#30363d"), 1.0));
    p.setBrush(QColor("#0d1117"));
    p.drawRoundedRect(plot, 3.0, 3.0);

    // 3. Grid Lines (25%, 50%, 75%)
    p.setPen(QPen(QColor("#21262d"), 1.0, Qt::DashLine));
    for (int i = 1; i <= 3; ++i) {
        double gx = plot.left() + (plot.width() * (i * 0.25));
        double gy = plot.top() + (plot.height() * (i * 0.25));
        p.drawLine(QPointF(gx, plot.top()), QPointF(gx, plot.bottom()));
        p.drawLine(QPointF(plot.left(), gy), QPointF(plot.right(), gy));
    }

    // 4. Baseline reference
    if (m_type == CurveType::ColorSpectrum) {
        // Neutral intensity baseline at Y = 0.5 (100% normal intensity)
        double midY = plot.top() + plot.height() * 0.5;
        p.setPen(QPen(QColor("#484f58"), 1.2, Qt::DashLine));
        p.drawLine(QPointF(plot.left(), midY), QPointF(plot.right(), midY));
    } else {
        // Diagonal linear identity baseline (0,0) to (1,1)
        p.setPen(QPen(QColor("#484f58"), 1.2, Qt::DashLine));
        p.drawLine(plot.bottomLeft(), plot.topRight());
    }

    // 5. Y-Axis Labels
    p.setPen(QColor("#6e7681"));
    QFont axisFont = font();
    axisFont.setPointSize(8);
    p.setFont(axisFont);

    if (m_type == CurveType::ColorSpectrum) {
        p.drawText(QRectF(0, plot.top() - 6, 30, 14), Qt::AlignRight | Qt::AlignVCenter, "200%");
        p.drawText(QRectF(0, plot.top() + plot.height() * 0.5 - 7, 30, 14), Qt::AlignRight | Qt::AlignVCenter, "100%");
        p.drawText(QRectF(0, plot.bottom() - 8, 30, 14), Qt::AlignRight | Qt::AlignVCenter, "0%");
    } else {
        p.drawText(QRectF(0, plot.top() - 6, 30, 14), Qt::AlignRight | Qt::AlignVCenter, "255");
        p.drawText(QRectF(0, plot.top() + plot.height() * 0.5 - 7, 30, 14), Qt::AlignRight | Qt::AlignVCenter, "128");
        p.drawText(QRectF(0, plot.bottom() - 8, 30, 14), Qt::AlignRight | Qt::AlignVCenter, "0");
    }

    // 6. X-Axis Bottom Gradient Bar
    QRectF gradRect = gradientBarRect();
    QLinearGradient xGrad(gradRect.left(), 0, gradRect.right(), 0);

    if (m_type == CurveType::ColorSpectrum) {
        xGrad.setColorAt(0.00, QColor(255, 0, 0));
        xGrad.setColorAt(0.17, QColor(255, 255, 0));
        xGrad.setColorAt(0.33, QColor(0, 255, 0));
        xGrad.setColorAt(0.50, QColor(0, 255, 255));
        xGrad.setColorAt(0.67, QColor(0, 0, 255));
        xGrad.setColorAt(0.83, QColor(255, 0, 255));
        xGrad.setColorAt(1.00, QColor(255, 0, 0));
    } else if (m_type == CurveType::Luma) {
        xGrad.setColorAt(0.0, QColor(0, 0, 0));
        xGrad.setColorAt(1.0, QColor(255, 255, 255));
    } else if (m_type == CurveType::Red) {
        xGrad.setColorAt(0.0, QColor(0, 0, 0));
        xGrad.setColorAt(1.0, QColor(248, 81, 73));
    } else if (m_type == CurveType::Green) {
        xGrad.setColorAt(0.0, QColor(0, 0, 0));
        xGrad.setColorAt(1.0, QColor(46, 160, 67));
    } else if (m_type == CurveType::Blue) {
        xGrad.setColorAt(0.0, QColor(0, 0, 0));
        xGrad.setColorAt(1.0, QColor(56, 139, 253));
    }

    p.setPen(QPen(QColor("#30363d"), 1.0));
    p.setBrush(xGrad);
    p.drawRoundedRect(gradRect, 2.0, 2.0);

    // 7. Continuous Function Curve
    QPainterPath curvePath;
    QPainterPath fillPath;
    const int steps = qMax(40, static_cast<int>(plot.width() / 2.0));

    double startX = plot.left();
    double startNormX = 0.0;
    double startNormY = m_curve.evaluate(startNormX);
    QPointF startPt(startX, plot.bottom() - startNormY * plot.height());

    curvePath.moveTo(startPt);
    fillPath.moveTo(plot.left(), plot.bottom());
    fillPath.lineTo(startPt);

    for (int s = 1; s <= steps; ++s) {
        double normX = static_cast<double>(s) / steps;
        double normY = m_curve.evaluate(normX);
        double sx = plot.left() + normX * plot.width();
        double sy = plot.bottom() - normY * plot.height();
        curvePath.lineTo(sx, sy);
        fillPath.lineTo(sx, sy);
    }

    fillPath.lineTo(plot.right(), plot.bottom());
    fillPath.closeSubpath();

    // Fill under curve
    QColor fillColor;
    QColor strokeColor;
    if (m_type == CurveType::ColorSpectrum) {
        fillColor = QColor(56, 139, 253, 30);
        strokeColor = QColor("#58a6ff");
    } else if (m_type == CurveType::Luma) {
        fillColor = QColor(240, 246, 252, 25);
        strokeColor = QColor("#f0f6fc");
    } else if (m_type == CurveType::Red) {
        fillColor = QColor(248, 81, 73, 30);
        strokeColor = QColor("#f85149");
    } else if (m_type == CurveType::Green) {
        fillColor = QColor(46, 160, 67, 30);
        strokeColor = QColor("#2ea043");
    } else {
        fillColor = QColor(56, 139, 253, 30);
        strokeColor = QColor("#388bfd");
    }

    p.setPen(Qt::NoPen);
    p.setBrush(fillColor);
    p.drawPath(fillPath);

    p.setPen(QPen(strokeColor, 2.2));
    p.setBrush(Qt::NoBrush);
    p.drawPath(curvePath);

    // 8. Render Points
    for (int i = 0; i < m_curve.pointCount(); ++i) {
        ColorCurvePoint pt = m_curve.point(i);
        QPointF pScr = normalizedToScreen(pt);

        bool isDragged = (i == m_draggedPointIndex);
        bool isHovered = (i == m_hoveredPointIndex);

        if (isDragged || isHovered) {
            // Glow halo ring
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(88, 166, 255, 80));
            p.drawEllipse(pScr, 8.0, 8.0);
        }

        // Inner point circle
        double r = (isDragged || isHovered) ? 5.5 : 4.0;
        p.setPen(QPen(strokeColor, 1.8));
        p.setBrush(QColor("#ffffff"));
        p.drawEllipse(pScr, r, r);
    }
}

void CurveEditorWidget::mousePressEvent(QMouseEvent *event)
{
    QRectF plot = plotAreaRect();
    if (!plot.contains(event->pos()) && event->button() == Qt::LeftButton) {
        int nearIdx = findPointNear(event->pos(), 12.0);
        if (nearIdx < 0) {
            QWidget::mousePressEvent(event);
            return;
        }
    }

    int clickedPoint = findPointNear(event->pos(), 10.0);

    if (event->button() == Qt::RightButton) {
        // Right-click deletes point if not endpoint
        if (clickedPoint > 0 && clickedPoint < m_curve.pointCount() - 1) {
            m_curve.removePoint(clickedPoint);
            m_hoveredPointIndex = -1;
            m_draggedPointIndex = -1;
            update();
            emit curveChanged(m_curve);
            emit curveChangeCommitted(m_curve);
            return;
        }
    } else if (event->button() == Qt::LeftButton) {
        if (clickedPoint >= 0) {
            m_draggedPointIndex = clickedPoint;
        } else if (plot.contains(event->pos())) {
            // Click on graph adds new point
            ColorCurvePoint np = screenToNormalized(event->pos());
            int newIdx = m_curve.addPoint(np.x, np.y);
            m_draggedPointIndex = newIdx;
            emit curveChanged(m_curve);
            emit curveChangeCommitted(m_curve);
        }
        setCursor(Qt::ClosedHandCursor);
        update();
    }
}

void CurveEditorWidget::mouseMoveEvent(QMouseEvent *event)
{
    m_lastMousePos = event->pos();
    QRectF plot = plotAreaRect();
    m_isHoveringPlot = plot.contains(event->pos());

    if (m_draggedPointIndex >= 0) {
        ColorCurvePoint np = screenToNormalized(event->pos());
        m_curve.movePoint(m_draggedPointIndex, np.x, np.y);
        emit curveChanged(m_curve);
        update();
    } else {
        int prevHover = m_hoveredPointIndex;
        m_hoveredPointIndex = findPointNear(event->pos(), 9.0);

        if (m_hoveredPointIndex >= 0) {
            setCursor(Qt::PointingHandCursor);
            setToolTip(formatPointTooltip(m_hoveredPointIndex, m_curve.point(m_hoveredPointIndex)) + "\n[Arrastra para mover • Clic derecho para eliminar]");
        } else if (m_isHoveringPlot) {
            setCursor(Qt::CrossCursor);
            setToolTip("Haz clic en cualquier posición del gráfico para añadir un nuevo punto");
        } else {
            setCursor(Qt::ArrowCursor);
            setToolTip(QString());
        }

        if (prevHover != m_hoveredPointIndex) {
            update();
        }
    }
}

void CurveEditorWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_draggedPointIndex >= 0) {
        m_draggedPointIndex = -1;
        setCursor(m_hoveredPointIndex >= 0 ? Qt::PointingHandCursor : (m_isHoveringPlot ? Qt::CrossCursor : Qt::ArrowCursor));
        update();
        emit curveChangeCommitted(m_curve);
    }
}

void CurveEditorWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        int pointIdx = findPointNear(event->pos(), 9.0);
        if (pointIdx > 0 && pointIdx < m_curve.pointCount() - 1) {
            m_curve.removePoint(pointIdx);
            m_hoveredPointIndex = -1;
            m_draggedPointIndex = -1;
            update();
            emit curveChanged(m_curve);
            emit curveChangeCommitted(m_curve);
        }
    }
}

void CurveEditorWidget::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    m_hoveredPointIndex = -1;
    m_isHoveringPlot = false;
    update();
}
