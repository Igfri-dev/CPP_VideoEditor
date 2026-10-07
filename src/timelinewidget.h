#pragma once

#include <QWidget>
#include <QScrollBar>
#include <QVector>
#include "core/timelinemodel.h"
#include "snapping/timelinesnappingengine.h"

class TimelineWidget : public QWidget {
    Q_OBJECT

public:
    explicit TimelineWidget(TimelineModel *model, QWidget *parent = nullptr);
    ~TimelineWidget() override;

    void setPlayheadPosition(qint64 positionMs);
    qint64 playheadPosition() const { return m_playheadPositionMs; }

    qint64 selectedClipId() const { return m_selectedClipId; }
    void setSelectedClipId(qint64 clipId);

    const QSet<qint64>& selectedClipIds() const { return m_selectedClipIds; }
    void setSelectedClipIds(const QSet<qint64> &ids);
    void selectClip(qint64 clipId, bool toggle = false);
    void clearSelection();
    bool isClipSelected(qint64 clipId) const { return m_selectedClipIds.contains(clipId); }

    void applyFilterToSelectedClips(VisualFilter filter);
    void addFilterToSelectedClips(VisualFilter filter);
    static void populateEffectsMenu(QMenu *parentMenu, const std::function<void(VisualFilter)> &onSelectFilter);

    Snapping::TimelineSnapSettings& snapSettings() { return m_snapSettings; }
    const Snapping::TimelineSnapSettings& snapSettings() const { return m_snapSettings; }
    void setSnappingEnabled(bool enabled) { m_snapSettings.enabled = enabled; update(); }
    bool isSnappingEnabled() const { return m_snapSettings.enabled; }

    double pixelsPerSecond() const { return m_pixelsPerSecond; }
    void setPixelsPerSecond(double pps);

    int contentHeight() const;
    int scrollY() const { return m_scrollY; }
    void setScrollY(int y);
    int scrollX() const { return m_scrollX; }
    void setScrollX(int x);
    QScrollBar *verticalScrollBar() const { return m_vScrollBar; }
    QScrollBar *horizontalScrollBar() const { return m_hScrollBar; }

    int rulerHeight() const { return 28; }
    int trackHeaderWidth() const { return 110; }
    int trackHeight() const { return 52; }
    int trackGap() const { return 6; }
    int dividerHeight() const { return 12; }

    qint64 pixelToTimeMs(int x) const;
    int timeMsToPixel(qint64 ms) const;

    qint64 trackIdAtY(int y) const;
    QRect trackRect(qint64 trackId) const;
    void updateScrollBars();

public slots:
    void zoomIn();
    void zoomOut();
    void zoomToFit();
    void splitSelectedClip();
    void splitSelectedClips();
    void joinSelectedClips();
    void deleteSelectedClip(bool ripple = false);
    void deleteSelectedClips(bool ripple = false);
    void duplicateSelectedClip();
    void separateSelectedClipAudio();
    void undo();
    void redo();
    void selectAll();
    void addVideoTrack();
    void addAudioTrack();
    void setIsPlaying(bool playing);
    bool isPlaying() const { return m_isPlaying; }
    void invalidateFilmstripCache(qint64 clipId = -1);

signals:
    void playheadSeekRequested(qint64 positionMs);
    void clipSelected(qint64 clipId);
    void selectionChanged(const QSet<qint64> &selectedIds);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onScrollValueChanged(int value);
    void onVScrollValueChanged(int value);
    void onModelChanged();

private:
    enum class DragMode {
        None,
        ScrubPlayhead,
        MoveClip,
        TrimClipLeft,
        TrimClipRight,
        MarqueeSelect,
        DragTimelineDuration
    };

    struct ClipHitResult {
        qint64 clipId = -1;
        qint64 trackId = -1;
        bool isLeftEdge = false;
        bool isRightEdge = false;
    };

    ClipHitResult hitTestClip(const QPoint &pos) const;
    QRect clipRect(const TimelineClip &clip, const QRect &tRect) const;

    int contentWidth() const;

    qint64 snapTime(qint64 timeMs, qint64 ignoreClipId = -1, bool bypassSnap = false);

    Snapping::TimelineSnappingEngine m_timelineSnapEngine;
    Snapping::TimelineSnapSettings m_snapSettings;
    bool m_hasActiveSnapIndicator = false;
    qint64 m_activeSnapIndicatorTimeMs = 0;

    TimelineModel *m_model = nullptr;
    QScrollBar *m_hScrollBar = nullptr;
    QScrollBar *m_vScrollBar = nullptr;

    qint64 m_playheadPositionMs = 0;
    qint64 m_selectedClipId = -1;
    QSet<qint64> m_selectedClipIds;

    double m_pixelsPerSecond = 50.0; // 50 pixels per second
    int m_scrollX = 0;
    int m_scrollY = 0;

    DragMode m_dragMode = DragMode::None;
    QPoint m_dragStartPos;
    QRect m_marqueeRect;

    qint64 m_dragInitialTimelineInMs = 0;
    qint64 m_dragInitialTimelineOutMs = 0;
    qint64 m_dragClipId = -1;
    qint64 m_dragTrackId = -1;
    QMap<qint64, qint64> m_dragMultiInitialInMs;

    bool m_isDraggingDuration = false;
    qint64 m_activeDurationDragMs = 0;

    void showClipContextMenu(qint64 clipId, const QPoint &globalPos);
    void showEmptyContextMenu(qint64 trackId, qint64 timelineMs, const QPoint &globalPos);
    void showTrackContextMenu(qint64 trackId, const QPoint &globalPos);
    void showMarkerContextMenu(qint64 markerId, const QPoint &globalPos);

    struct FilmstripCacheItem {
        int width = 0;
        int height = 0;
        qint64 sourceInMs = -1;
        qint64 durationMs = -1;
        double speed = 1.0;
        QPixmap pixmap;
    };
    QMap<qint64, FilmstripCacheItem> m_filmstripCache;
    bool m_isPlaying = false;
};
