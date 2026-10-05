#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <QPainter>
#include <QLinearGradient>
#include "core/timelinemodel.h"
#include "engine/audioengine.h"
#include "snapping/canvassnappingengine.h"

class AudioMeterWidget : public QWidget {
    Q_OBJECT
public:
    explicit AudioMeterWidget(QWidget *parent = nullptr) : QWidget(parent) {
        setFixedSize(85, 26);
        setToolTip("Vúmetro de Audio Estéreo (dB)");
        m_decayTimer.setInterval(30);
        connect(&m_decayTimer, &QTimer::timeout, this, &AudioMeterWidget::onDecayTick);
        m_decayTimer.start();
    }

    void setLevels(double left, double right) {
        if (left >= m_leftLevel) m_leftLevel = left;
        if (right >= m_rightLevel) m_rightLevel = right;
        if (left >= m_leftPeak) m_leftPeak = left;
        if (right >= m_rightPeak) m_rightPeak = right;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        // Background
        p.fillRect(rect(), QColor("#161b22"));
        p.setPen(QColor("#30363d"));
        p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 3, 3);

        auto drawBar = [&](int y, double level, double peak, const QString &label) {
            p.setPen(QColor("#8b949e"));
            QFont f = p.font();
            f.setPixelSize(8);
            f.setBold(true);
            p.setFont(f);
            p.drawText(4, y + 8, label);

            int barX = 16;
            int barW = width() - barX - 6;
            int barH = 7;
            QRect barBg(barX, y + 1, barW, barH);
            p.fillRect(barBg, QColor("#0d1117"));

            int fillW = static_cast<int>(qBound(0.0, level, 1.0) * barW);
            if (fillW > 0) {
                QLinearGradient g(barX, 0, barX + barW, 0);
                g.setColorAt(0.0, QColor("#2ea043"));   // Green
                g.setColorAt(0.7, QColor("#d29922"));   // Yellow
                g.setColorAt(0.9, QColor("#f85149"));   // Clip red
                p.fillRect(QRect(barX, y + 1, fillW, barH), g);
            }

            if (peak > 0.05) {
                int px = barX + static_cast<int>(qBound(0.0, peak, 1.0) * barW) - 1;
                px = qBound(barX, px, barX + barW - 1);
                p.setPen(peak >= 0.9 ? QColor("#ff7b72") : QColor("#e3b341"));
                p.drawLine(px, y + 1, px, y + 1 + barH);
            }
        };

        drawBar(3, m_leftLevel, m_leftPeak, "L");
        drawBar(14, m_rightLevel, m_rightPeak, "R");
    }

private slots:
    void onDecayTick() {
        bool changed = false;
        double decayRate = 0.04;
        if (m_leftLevel > 0.0) {
            m_leftLevel = qMax(0.0, m_leftLevel - decayRate);
            changed = true;
        }
        if (m_rightLevel > 0.0) {
            m_rightLevel = qMax(0.0, m_rightLevel - decayRate);
            changed = true;
        }
        if (m_leftPeak > 0.0) {
            m_leftPeak = qMax(0.0, m_leftPeak - decayRate * 0.4);
            changed = true;
        }
        if (m_rightPeak > 0.0) {
            m_rightPeak = qMax(0.0, m_rightPeak - decayRate * 0.4);
            changed = true;
        }
        if (changed) update();
    }

private:
    double m_leftLevel = 0.0;
    double m_rightLevel = 0.0;
    double m_leftPeak = 0.0;
    double m_rightPeak = 0.0;
    QTimer m_decayTimer;
};

class MonitorWidget : public QWidget {
    Q_OBJECT
public:
    explicit MonitorWidget(TimelineModel *model, QWidget *parent = nullptr);

    void setFrame(const QImage &frame);
    void setSelectedClipId(qint64 clipId);
    void setCurrentPositionMs(qint64 posMs);
    qint64 selectedClipId() const { return m_selectedClipId; }

    Snapping::SnapSettings& snapSettings() { return m_snapSettings; }
    const Snapping::SnapSettings& snapSettings() const { return m_snapSettings; }
    void setSnappingEnabled(bool enabled) { m_snapSettings.enabled = enabled; update(); }
    bool isSnappingEnabled() const { return m_snapSettings.enabled; }
    void setShowSafeAreas(bool show) { m_showSafeAreas = show; update(); }
    bool showSafeAreas() const { return m_showSafeAreas; }
    void setIsPlaying(bool playing);
    bool isPlaying() const { return m_isPlaying; }

signals:
    void clipTransformChanged(qint64 clipId);
    void clipSelected(qint64 clipId);
    void filesDropped(const QStringList &filePaths);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    enum class GizmoAction {
        None,
        Move,
        ResizeTL,
        ResizeTR,
        ResizeBR,
        ResizeBL,
        ResizeLeft,
        ResizeRight,
        Rotate,
        DragMotionPoint,
        DragMotionHandleIn,
        DragMotionHandleOut
    };

    struct GizmoGeometry {
        QRect displayRect;
        QPointF centerScreen;
        QPointF pTL;
        QPointF pTR;
        QPointF pBR;
        QPointF pBL;
        QPointF pTC;
        QPointF pML;
        QPointF pMR;
        QPointF pRot;
        double scaleScreenX = 1.0;
        double scaleScreenY = 1.0;
        bool isTextClip = false;
        bool isValid = false;
    };

    GizmoGeometry calculateGizmoGeometry() const;
    GizmoAction hitTest(const QPointF &pos, const GizmoGeometry &geom) const;
    QPointF canvasToScreen(const QPointF &canvasOffset, const QRect &dRect) const;
    QPointF screenToCanvas(const QPointF &screenPoint, const QRect &dRect) const;

    TimelineModel *m_model = nullptr;
    QImage m_currentFrame;
    qint64 m_selectedClipId = -1;
    qint64 m_currentPositionMs = 0;

    int m_activeMotionPointIndex = -1;
    MotionPath m_initialMotionPath;

    GizmoAction m_currentAction = GizmoAction::None;
    QPointF m_initialMousePos;
    double m_initialClipPosX = 0.0;
    double m_initialClipPosY = 0.0;
    double m_initialMouseDist = 1.0;
    double m_initialClipScaleX = 1.0;
    double m_initialClipScaleY = 1.0;
    double m_initialMouseAngle = 0.0;
    double m_initialClipRotation = 0.0;
    double m_initialTextBoxWidth = 0.0;
    bool m_isResizingWidth = false;
    int m_activeWidthValue = 0;

    Snapping::CanvasSnappingEngine m_snapEngine;
    Snapping::SnapSettings m_snapSettings;
    std::vector<Snapping::SmartGuide> m_activeGuides;
    bool m_showSafeAreas = false;
    bool m_isRotating = false;
    bool m_activeRotationSnap = false;
    double m_activeRotationAngle = 0.0;
    QString m_activeRotationLabel;

    bool m_isPlaying = false;
    qint64 m_cachedGhostClipId = -1;
    qint64 m_cachedGhostSourceInMs = -1;
    QImage m_cachedGhostImg;
};

class PreviewWidget : public QWidget {
    Q_OBJECT

public:
    explicit PreviewWidget(TimelineModel *model, AudioEngine *audioEngine, QWidget *parent = nullptr);
    ~PreviewWidget() override;

    void updatePreview();
    void setPosition(qint64 positionMs);
    qint64 currentPosition() const { return m_currentPositionMs; }

    bool isPlaying() const { return m_isPlaying; }
    void setSelectedClipId(qint64 clipId);

    Snapping::SnapSettings& snapSettings() { return m_monitorWidget->snapSettings(); }
    const Snapping::SnapSettings& snapSettings() const { return m_monitorWidget->snapSettings(); }
    void setSnappingEnabled(bool enabled) { m_monitorWidget->setSnappingEnabled(enabled); }
    bool isSnappingEnabled() const { return m_monitorWidget->isSnappingEnabled(); }
    void setShowSafeAreas(bool show) { m_monitorWidget->setShowSafeAreas(show); }
    bool showSafeAreas() const { return m_monitorWidget->showSafeAreas(); }

public slots:
    void play();
    void pause();
    void togglePlayPause();
    void stepForward();
    void stepBackward();
    void jumpToStart();
    void jumpToEnd();
    void setMasterVolume(int volumePercent);

signals:
    void playheadMoved(qint64 positionMs);
    void playbackStateChanged(bool isPlaying);
    void clipSelected(qint64 clipId);
    void clipTransformChanged(qint64 clipId);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onVideoTimerTick();
    void onAudioPositionAdvanced(qint64 positionMs);
    void onMonitorFilesDropped(const QStringList &filePaths);

private:
    void renderCurrentFrame();
    void updateTimecodeLabel();
    QString formatTimecode(qint64 ms) const;

    TimelineModel *m_model = nullptr;
    AudioEngine *m_audioEngine = nullptr;

    qint64 m_currentPositionMs = 0;
    qint64 m_lastRenderedFrameIndex = -1;
    bool m_isPlaying = false;
    bool m_isLooping = false;
    QTimer m_videoTimer;

    MonitorWidget *m_monitorWidget = nullptr;
    QLabel *m_timecodeLabel = nullptr;

    QPushButton *m_jumpStartBtn = nullptr;
    QPushButton *m_stepBackBtn = nullptr;
    QPushButton *m_playPauseBtn = nullptr;
    QPushButton *m_stepForwardBtn = nullptr;
    QPushButton *m_jumpEndBtn = nullptr;
    QPushButton *m_loopBtn = nullptr;
    QSlider *m_volumeSlider = nullptr;
    AudioMeterWidget *m_audioMeter = nullptr;
};
