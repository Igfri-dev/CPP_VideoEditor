#pragma once

#include <QWidget>
#include <QImage>
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>

enum class ScopeMode {
    Waveform = 0,
    RGBParade = 1,
    Vectorscope = 2,
    Histogram = 3,
    AllScopes = 4
};

QString scopeModeToString(ScopeMode mode);

class VideoScopesWidget : public QWidget {
    Q_OBJECT
public:
    explicit VideoScopesWidget(QWidget *parent = nullptr);

    ScopeMode scopeMode() const { return m_mode; }
    void setScopeMode(ScopeMode mode);

    void updateFrame(const QImage &frame);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void drawWaveform(QPainter &p, const QRect &rect, const QImage &img, float gain);
    void drawRGBParade(QPainter &p, const QRect &rect, const QImage &img, float gain);
    void drawVectorscope(QPainter &p, const QRect &rect, const QImage &img, float gain);
    void drawHistogram(QPainter &p, const QRect &rect, const QImage &img);

    ScopeMode m_mode = ScopeMode::AllScopes;
    QImage m_currentFrame;
    float m_intensityGain = 1.2f;

    QComboBox *m_modeCombo = nullptr;
    QSlider *m_gainSlider = nullptr;
};
