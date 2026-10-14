#include "videoscopeswidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QtMath>
#include <vector>

QString scopeModeToString(ScopeMode mode)
{
    switch (mode) {
    case ScopeMode::Waveform: return "Waveform (Luma Y)";
    case ScopeMode::RGBParade: return "RGB Parade";
    case ScopeMode::Vectorscope: return "Vectorscope";
    case ScopeMode::Histogram: return "Histograma RGB";
    case ScopeMode::AllScopes: return "Todos los Scopes (2x2)";
    }
    return "Todos los Scopes";
}

VideoScopesWidget::VideoScopesWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(320, 240);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(4);

    QHBoxLayout *topBar = new QHBoxLayout();
    QLabel *title = new QLabel("📊 Video Scopes", this);
    title->setStyleSheet("color: #58a6ff; font-weight: bold; font-size: 11px;");
    topBar->addWidget(title);

    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem("Todos los Scopes (2x2)", static_cast<int>(ScopeMode::AllScopes));
    m_modeCombo->addItem("Waveform (Luma Y)", static_cast<int>(ScopeMode::Waveform));
    m_modeCombo->addItem("RGB Parade", static_cast<int>(ScopeMode::RGBParade));
    m_modeCombo->addItem("Vectorscope", static_cast<int>(ScopeMode::Vectorscope));
    m_modeCombo->addItem("Histograma", static_cast<int>(ScopeMode::Histogram));
    m_modeCombo->setStyleSheet(
        "QComboBox { background-color: #21262d; color: #c9d1d9; border: 1px solid #30363d; border-radius: 3px; padding: 2px 6px; font-size: 11px; }"
    );
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (idx >= 0) {
            m_mode = static_cast<ScopeMode>(m_modeCombo->itemData(idx).toInt());
            update();
        }
    });
    topBar->addWidget(m_modeCombo);

    topBar->addStretch();

    QLabel *gainLbl = new QLabel("Ganancia:", this);
    gainLbl->setStyleSheet("color: #8b949e; font-size: 10px;");
    topBar->addWidget(gainLbl);

    m_gainSlider = new QSlider(Qt::Horizontal, this);
    m_gainSlider->setRange(50, 300);
    m_gainSlider->setValue(120);
    m_gainSlider->setFixedWidth(70);
    connect(m_gainSlider, &QSlider::valueChanged, this, [this](int val) {
        m_intensityGain = val / 100.0f;
        update();
    });
    topBar->addWidget(m_gainSlider);

    mainLayout->addLayout(topBar);
    mainLayout->addStretch();
}

QSize VideoScopesWidget::sizeHint() const
{
    return QSize(420, 320);
}

QSize VideoScopesWidget::minimumSizeHint() const
{
    return QSize(300, 220);
}

void VideoScopesWidget::setScopeMode(ScopeMode mode)
{
    m_mode = mode;
    int idx = m_modeCombo->findData(static_cast<int>(mode));
    if (idx >= 0) m_modeCombo->setCurrentIndex(idx);
    update();
}

void VideoScopesWidget::updateFrame(const QImage &frame)
{
    m_currentFrame = frame;
    update();
}

void VideoScopesWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Dark backdrop
    p.fillRect(rect(), QColor("#0d1117"));

    QRect contentRect = rect().adjusted(6, 34, -6, -6);
    if (contentRect.width() <= 10 || contentRect.height() <= 10) return;

    if (m_mode == ScopeMode::Waveform) {
        drawWaveform(p, contentRect, m_currentFrame, m_intensityGain);
    } else if (m_mode == ScopeMode::RGBParade) {
        drawRGBParade(p, contentRect, m_currentFrame, m_intensityGain);
    } else if (m_mode == ScopeMode::Vectorscope) {
        drawVectorscope(p, contentRect, m_currentFrame, m_intensityGain);
    } else if (m_mode == ScopeMode::Histogram) {
        drawHistogram(p, contentRect, m_currentFrame);
    } else {
        // AllScopes: 2x2 Grid
        int hw = contentRect.width() / 2;
        int hh = contentRect.height() / 2;
        QRect rTL(contentRect.left(), contentRect.top(), hw - 2, hh - 2);
        QRect rTR(contentRect.left() + hw + 2, contentRect.top(), hw - 2, hh - 2);
        QRect rBL(contentRect.left(), contentRect.top() + hh + 2, hw - 2, hh - 2);
        QRect rBR(contentRect.left() + hw + 2, contentRect.top() + hh + 2, hw - 2, hh - 2);

        drawWaveform(p, rTL, m_currentFrame, m_intensityGain);
        drawRGBParade(p, rTR, m_currentFrame, m_intensityGain);
        drawVectorscope(p, rBL, m_currentFrame, m_intensityGain);
        drawHistogram(p, rBR, m_currentFrame);
    }
}

void VideoScopesWidget::drawWaveform(QPainter &p, const QRect &r, const QImage &img, float gain)
{
    p.save();
    p.fillRect(r, QColor("#161b22"));
    p.setPen(QPen(QColor("#30363d"), 1));
    p.drawRect(r);

    // Graticule IRE lines (0, 20, 40, 60, 80, 100)
    QFont f = p.font();
    f.setPixelSize(9);
    p.setFont(f);

    for (int ire = 0; ire <= 100; ire += 20) {
        int y = r.bottom() - static_cast<int>((ire / 100.0) * r.height());
        p.setPen(QPen(QColor(48, 54, 61, (ire == 0 || ire == 100) ? 200 : 100), 1, Qt::DashLine));
        p.drawLine(r.left(), y, r.right(), y);
        p.setPen(QColor("#8b949e"));
        p.drawText(r.left() + 4, y - 2, QString::number(ire));
    }

    p.setPen(QColor("#58a6ff"));
    p.drawText(r.right() - 65, r.top() + 14, "Waveform (Y)");

    if (!img.isNull() && img.width() > 0 && img.height() > 0) {
        // Fast subsampled column scan
        int stepX = std::max(1, img.width() / r.width());
        int stepY = std::max(1, img.height() / 160);

        QColor phosphor(0, 255, 128, std::clamp(static_cast<int>(35 * gain), 10, 255));
        p.setPen(phosphor);

        for (int x = 0; x < r.width(); ++x) {
            int srcX = std::min(img.width() - 1, x * stepX);
            int screenX = r.left() + x;

            for (int sy = 0; sy < img.height(); sy += stepY) {
                QRgb pix = img.pixel(srcX, sy);
                float yVal = (0.2126f * qRed(pix) + 0.7152f * qGreen(pix) + 0.0722f * qBlue(pix)) / 255.0f;
                int screenY = r.bottom() - static_cast<int>(yVal * (r.height() - 4)) - 2;
                p.drawPoint(screenX, screenY);
            }
        }
    }

    p.restore();
}

void VideoScopesWidget::drawRGBParade(QPainter &p, const QRect &r, const QImage &img, float gain)
{
    p.save();
    p.fillRect(r, QColor("#161b22"));
    p.setPen(QPen(QColor("#30363d"), 1));
    p.drawRect(r);

    int subW = r.width() / 3;
    QRect rR(r.left(), r.top(), subW, r.height());
    QRect rG(r.left() + subW, r.top(), subW, r.height());
    QRect rB(r.left() + subW * 2, r.top(), r.width() - subW * 2, r.height());

    // Separators
    p.setPen(QPen(QColor("#30363d"), 1));
    p.drawLine(rG.left(), r.top(), rG.left(), r.bottom());
    p.drawLine(rB.left(), r.top(), rB.left(), r.bottom());

    QFont f = p.font();
    f.setPixelSize(9);
    p.setFont(f);
    p.setPen(QColor("#f85149")); p.drawText(rR.left() + 4, r.top() + 12, "RED");
    p.setPen(QColor("#56d364")); p.drawText(rG.left() + 4, r.top() + 12, "GREEN");
    p.setPen(QColor("#58a6ff")); p.drawText(rB.left() + 4, r.top() + 12, "BLUE");

    if (!img.isNull() && img.width() > 0 && img.height() > 0) {
        int stepX = std::max(1, img.width() / subW);
        int stepY = std::max(1, img.height() / 120);

        int alpha = std::clamp(static_cast<int>(40 * gain), 10, 255);
        QColor colR(255, 60, 60, alpha);
        QColor colG(50, 255, 100, alpha);
        QColor colB(60, 150, 255, alpha);

        for (int x = 0; x < subW; ++x) {
            int srcX = std::min(img.width() - 1, x * stepX);

            for (int sy = 0; sy < img.height(); sy += stepY) {
                QRgb pix = img.pixel(srcX, sy);
                int pyR = r.bottom() - static_cast<int>((qRed(pix) / 255.0) * (r.height() - 6)) - 3;
                int pyG = r.bottom() - static_cast<int>((qGreen(pix) / 255.0) * (r.height() - 6)) - 3;
                int pyB = r.bottom() - static_cast<int>((qBlue(pix) / 255.0) * (r.height() - 6)) - 3;

                p.setPen(colR); p.drawPoint(rR.left() + x, pyR);
                p.setPen(colG); p.drawPoint(rG.left() + x, pyG);
                p.setPen(colB); p.drawPoint(rB.left() + x, pyB);
            }
        }
    }

    p.restore();
}

void VideoScopesWidget::drawVectorscope(QPainter &p, const QRect &r, const QImage &img, float gain)
{
    p.save();
    p.fillRect(r, QColor("#161b22"));
    p.setPen(QPen(QColor("#30363d"), 1));
    p.drawRect(r);

    int cx = r.center().x();
    int cy = r.center().y();
    int radius = std::min(r.width(), r.height()) / 2 - 14;
    if (radius <= 10) { p.restore(); return; }

    // Outer circle & 75% target circle
    p.setPen(QPen(QColor("#30363d"), 1));
    p.drawEllipse(QPoint(cx, cy), radius, radius);
    p.setPen(QPen(QColor(48, 54, 61, 140), 1, Qt::DashLine));
    p.drawEllipse(QPoint(cx, cy), static_cast<int>(radius * 0.75), static_cast<int>(radius * 0.75));

    // Center crosshairs
    p.drawLine(cx - 8, cy, cx + 8, cy);
    p.drawLine(cx, cy - 8, cx, cy + 8);

    // Skin tone axis line (I-bar, ~123 degrees / upper-left)
    double skinAngleRad = qDegreesToRadians(123.0);
    int sx = cx + static_cast<int>(std::cos(skinAngleRad) * radius * 0.95);
    int sy = cy - static_cast<int>(std::sin(skinAngleRad) * radius * 0.95);
    p.setPen(QPen(QColor(240, 136, 62, 180), 1.5, Qt::DashLine));
    p.drawLine(cx, cy, sx, sy);

    QFont f = p.font();
    f.setPixelSize(8);
    p.setFont(f);
    p.setPen(QColor("#f0883e"));
    p.drawText(sx - 10, sy - 2, "Skin");

    // Standard 6 color targets at 75% saturation: R, Mg, B, Cy, G, Yl
    struct Target { const char* label; double angleDeg; QColor col; };
    static const Target targets[] = {
        { "R",  104.0, QColor(255, 80, 80) },
        { "Mg",  61.0, QColor(255, 80, 255) },
        { "B",  347.0, QColor(80, 150, 255) },
        { "Cy", 284.0, QColor(80, 255, 255) },
        { "G",  241.0, QColor(80, 255, 80) },
        { "Yl", 167.0, QColor(255, 255, 80) }
    };

    double r75 = radius * 0.75;
    for (const auto &tg : targets) {
        double rad = qDegreesToRadians(tg.angleDeg);
        int tx = cx + static_cast<int>(std::cos(rad) * r75);
        int ty = cy - static_cast<int>(std::sin(rad) * r75);
        p.setPen(QPen(tg.col, 1));
        p.drawRect(tx - 3, ty - 3, 6, 6);
        p.drawText(tx + 5, ty + 3, tg.label);
    }

    // Trace pixels onto vectorscope
    if (!img.isNull() && img.width() > 0 && img.height() > 0) {
        int step = std::max(1, (img.width() * img.height()) / 12000);
        int alpha = std::clamp(static_cast<int>(35 * gain), 10, 255);
        QColor phosphor(0, 230, 180, alpha);
        p.setPen(phosphor);

        const int totalPix = img.width() * img.height();
        for (int i = 0; i < totalPix; i += step) {
            int px = i % img.width();
            int py = i / img.width();
            QRgb col = img.pixel(px, py);

            float rF = qRed(col) / 255.0f;
            float gF = qGreen(col) / 255.0f;
            float bF = qBlue(col) / 255.0f;

            float Cb = -0.168736f * rF - 0.331264f * gF + 0.500000f * bF;
            float Cr =  0.500000f * rF - 0.418688f * gF - 0.081312f * bF;

            int dotX = cx + static_cast<int>(Cb * radius * 2.0);
            int dotY = cy - static_cast<int>(Cr * radius * 2.0);
            p.drawPoint(dotX, dotY);
        }
    }

    p.restore();
}

void VideoScopesWidget::drawHistogram(QPainter &p, const QRect &r, const QImage &img)
{
    p.save();
    p.fillRect(r, QColor("#161b22"));
    p.setPen(QPen(QColor("#30363d"), 1));
    p.drawRect(r);

    QFont f = p.font();
    f.setPixelSize(9);
    p.setFont(f);
    p.setPen(QColor("#8b949e"));
    p.drawText(r.left() + 4, r.top() + 12, "Histograma RGB");

    if (!img.isNull() && img.width() > 0 && img.height() > 0) {
        int histR[256] = {0};
        int histG[256] = {0};
        int histB[256] = {0};

        int step = std::max(1, (img.width() * img.height()) / 25000);
        int total = 0;
        const int totalPix = img.width() * img.height();
        for (int i = 0; i < totalPix; i += step) {
            int px = i % img.width();
            int py = i / img.width();
            QRgb col = img.pixel(px, py);
            histR[qRed(col)]++;
            histG[qGreen(col)]++;
            histB[qBlue(col)]++;
            total++;
        }

        int maxVal = 1;
        for (int i = 1; i < 255; ++i) { // Ignore clipped endpoints for scaling
            maxVal = std::max({maxVal, histR[i], histG[i], histB[i]});
        }

        int plotH = r.height() - 20;
        int plotW = r.width() - 8;
        int plotB = r.bottom() - 4;
        int plotL = r.left() + 4;

        for (int i = 0; i < 256; ++i) {
            int x = plotL + static_cast<int>((i / 255.0) * plotW);
            int hR = static_cast<int>((static_cast<double>(histR[i]) / maxVal) * plotH);
            int hG = static_cast<int>((static_cast<double>(histG[i]) / maxVal) * plotH);
            int hB = static_cast<int>((static_cast<double>(histB[i]) / maxVal) * plotH);

            p.setPen(QColor(255, 60, 60, 120));
            p.drawLine(x, plotB, x, plotB - std::min(plotH, hR));

            p.setPen(QColor(50, 255, 100, 120));
            p.drawLine(x, plotB, x, plotB - std::min(plotH, hG));

            p.setPen(QColor(60, 150, 255, 120));
            p.drawLine(x, plotB, x, plotB - std::min(plotH, hB));
        }
    }

    p.restore();
}
