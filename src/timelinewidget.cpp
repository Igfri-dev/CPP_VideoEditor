#include "timelinewidget.h"
#include "engine/waveformgenerator.h"
#include "engine/videoframedecoder.h"
#include "medialibrary/mediaitem.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMenu>
#include <QToolTip>
#include <QInputDialog>
#include <QColorDialog>
#include <QLinearGradient>
#include <QtMath>
#include <algorithm>

TimelineWidget::TimelineWidget(TimelineModel *model, QWidget *parent)
    : QWidget(parent)
    , m_model(model)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAcceptDrops(true);

    m_hScrollBar = new QScrollBar(Qt::Horizontal, this);
    m_hScrollBar->setStyleSheet(
        "QScrollBar:horizontal {"
        "  background: #14171d; height: 10px; margin: 0px;"
        "}"
        "QScrollBar::handle:horizontal {"
        "  background: #30363d; min-width: 20px; border-radius: 4px;"
        "}"
        "QScrollBar::handle:horizontal:hover { background: #58a6ff; }"
    );
    connect(m_hScrollBar, &QScrollBar::valueChanged, this, &TimelineWidget::onScrollValueChanged);

    m_vScrollBar = new QScrollBar(Qt::Vertical, this);
    m_vScrollBar->setStyleSheet(
        "QScrollBar:vertical {"
        "  background: #14171d; width: 10px; margin: 0px;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: #30363d; min-height: 20px; border-radius: 4px;"
        "}"
        "QScrollBar::handle:vertical:hover { background: #58a6ff; }"
    );
    connect(m_vScrollBar, &QScrollBar::valueChanged, this, &TimelineWidget::onVScrollValueChanged);

    if (m_model) {
        connect(m_model, &TimelineModel::timelineChanged, this, &TimelineWidget::onModelChanged);
        connect(m_model, &TimelineModel::durationChanged, this, &TimelineWidget::onModelChanged);
    }

    updateScrollBars();
}

TimelineWidget::~TimelineWidget()
{
}

void TimelineWidget::setPixelsPerSecond(double pps)
{
    m_pixelsPerSecond = qBound(10.0, pps, 500.0);
    invalidateFilmstripCache();
    updateScrollBars();
    update();
}

void TimelineWidget::zoomIn()
{
    setPixelsPerSecond(m_pixelsPerSecond * 1.25);
}

void TimelineWidget::zoomOut()
{
    setPixelsPerSecond(m_pixelsPerSecond / 1.25);
}

void TimelineWidget::zoomToFit()
{
    if (!m_model) return;
    int availableWidth = width() - trackHeaderWidth() - 20;
    if (availableWidth <= 50) return;
    double durationSec = m_model->totalDurationMs() / 1000.0;
    if (durationSec > 0.1) {
        setPixelsPerSecond(availableWidth / durationSec);
    }
}

void TimelineWidget::setPlayheadPosition(qint64 positionMs)
{
    m_playheadPositionMs = positionMs;
    // Auto scroll if playhead moves out of view during playback
    int playheadX = timeMsToPixel(m_playheadPositionMs);
    int visibleWidth = width() - trackHeaderWidth();
    if (playheadX > m_scrollX + visibleWidth - 50) {
        m_hScrollBar->setValue(playheadX - 100);
    } else if (playheadX < m_scrollX) {
        m_hScrollBar->setValue(qMax(0, playheadX - 50));
    }
    update();
}

void TimelineWidget::setSelectedClipId(qint64 clipId)
{
    m_selectedClipId = clipId;
    m_selectedClipIds.clear();
    if (clipId > 0) {
        m_selectedClipIds.insert(clipId);
    }
    emit selectionChanged(m_selectedClipIds);
    emit clipSelected(m_selectedClipId);
    update();
}

void TimelineWidget::setSelectedClipIds(const QSet<qint64> &ids)
{
    m_selectedClipIds = ids;
    if (m_selectedClipIds.isEmpty()) {
        m_selectedClipId = -1;
    } else if (!m_selectedClipIds.contains(m_selectedClipId)) {
        m_selectedClipId = *m_selectedClipIds.begin();
    }
    emit selectionChanged(m_selectedClipIds);
    emit clipSelected(m_selectedClipId);
    update();
}

void TimelineWidget::selectClip(qint64 clipId, bool toggle)
{
    if (clipId <= 0) return;
    if (toggle) {
        if (m_selectedClipIds.contains(clipId)) {
            m_selectedClipIds.remove(clipId);
            if (m_selectedClipId == clipId) {
                m_selectedClipId = m_selectedClipIds.isEmpty() ? -1 : *m_selectedClipIds.begin();
            }
        } else {
            m_selectedClipIds.insert(clipId);
            m_selectedClipId = clipId;
        }
    } else {
        m_selectedClipIds.clear();
        m_selectedClipIds.insert(clipId);
        m_selectedClipId = clipId;
    }
    emit selectionChanged(m_selectedClipIds);
    emit clipSelected(m_selectedClipId);
    update();
}

void TimelineWidget::clearSelection()
{
    m_selectedClipIds.clear();
    m_selectedClipId = -1;
    emit selectionChanged(m_selectedClipIds);
    emit clipSelected(-1);
    update();
}

void TimelineWidget::splitSelectedClip()
{
    splitSelectedClips();
}

void TimelineWidget::splitSelectedClips()
{
    if (!m_model) return;
    if (!m_selectedClipIds.isEmpty()) {
        m_model->splitClips(m_selectedClipIds.values(), m_playheadPositionMs);
    } else if (m_selectedClipId > 0) {
        m_model->splitClip(m_selectedClipId, m_playheadPositionMs);
    } else {
        m_model->splitAllAtPlayhead(m_playheadPositionMs);
    }
}

void TimelineWidget::joinSelectedClips()
{
    if (!m_model) return;
    if (m_selectedClipIds.size() >= 2) {
        m_model->joinClips(m_selectedClipIds.values());
        clearSelection();
    }
}

void TimelineWidget::deleteSelectedClip(bool ripple)
{
    deleteSelectedClips(ripple);
}

void TimelineWidget::deleteSelectedClips(bool ripple)
{
    if (!m_model) return;
    if (!m_selectedClipIds.isEmpty()) {
        m_model->deleteClips(m_selectedClipIds.values(), ripple);
        clearSelection();
    } else if (m_selectedClipId > 0) {
        m_model->deleteClip(m_selectedClipId, ripple);
        clearSelection();
    }
}

void TimelineWidget::duplicateSelectedClip()
{
    if (m_model && m_selectedClipId > 0) {
        qint64 newId = m_model->duplicateClip(m_selectedClipId);
        if (newId > 0) {
            setSelectedClipId(newId);
        }
    }
}

void TimelineWidget::separateSelectedClipAudio()
{
    if (m_model && m_selectedClipId > 0) {
        m_model->separateAudio(m_selectedClipId);
    }
}

void TimelineWidget::undo()
{
    if (m_model) {
        m_model->undo();
        QSet<qint64> validIds;
        for (qint64 id : m_selectedClipIds) {
            if (m_model->findClip(id)) validIds.insert(id);
        }
        setSelectedClipIds(validIds);
    }
}

void TimelineWidget::redo()
{
    if (m_model) {
        m_model->redo();
        QSet<qint64> validIds;
        for (qint64 id : m_selectedClipIds) {
            if (m_model->findClip(id)) validIds.insert(id);
        }
        setSelectedClipIds(validIds);
    }
}

void TimelineWidget::selectAll()
{
    if (m_model) {
        QSet<qint64> allIds;
        for (const auto &t : m_model->videoTracks()) {
            for (const auto &c : t.clips()) allIds.insert(c.id());
        }
        for (const auto &t : m_model->audioTracks()) {
            for (const auto &c : t.clips()) allIds.insert(c.id());
        }
        setSelectedClipIds(allIds);
    }
}

void TimelineWidget::addVideoTrack()
{
    if (m_model) {
        m_model->addTrack(TrackType::Video);
    }
}

void TimelineWidget::addAudioTrack()
{
    if (m_model) {
        m_model->addTrack(TrackType::Audio);
    }
}

qint64 TimelineWidget::pixelToTimeMs(int x) const
{
    int adjustedX = x - trackHeaderWidth() + m_scrollX;
    if (adjustedX <= 0) return 0;
    return qRound64((adjustedX / m_pixelsPerSecond) * 1000.0);
}

int TimelineWidget::timeMsToPixel(qint64 ms) const
{
    return qRound((ms / 1000.0) * m_pixelsPerSecond);
}

int TimelineWidget::contentWidth() const
{
    if (!m_model) return width();
    return timeMsToPixel(m_model->totalDurationMs()) + 200;
}

int TimelineWidget::contentHeight() const
{
    if (!m_model) return height();
    int vCount = m_model->videoTracks().size();
    int aCount = m_model->audioTracks().size();
    int total = (vCount + aCount) * (trackHeight() + trackGap()) + dividerHeight() + 24;
    return total;
}

void TimelineWidget::updateScrollBars()
{
    if (!m_hScrollBar || !m_vScrollBar) return;

    int totalH = contentHeight();
    int visibleH = qMax(1, height() - rulerHeight() - 10);
    int maxScrollY = qMax(0, totalH - visibleH);
    m_vScrollBar->setRange(0, maxScrollY);
    m_vScrollBar->setPageStep(visibleH);
    if (maxScrollY == 0) m_scrollY = 0;

    bool vScrollVisible = (maxScrollY > 0);
    m_vScrollBar->setVisible(vScrollVisible);
    int vScrollW = vScrollVisible ? 10 : 0;

    int totalW = contentWidth();
    int visibleW = qMax(1, width() - trackHeaderWidth() - vScrollW);
    int maxScrollX = qMax(0, totalW - visibleW);
    m_hScrollBar->setRange(0, maxScrollX);
    m_hScrollBar->setPageStep(visibleW);
    if (maxScrollX == 0) m_scrollX = 0;

    bool hScrollVisible = (maxScrollX > 0);
    m_hScrollBar->setVisible(hScrollVisible);
    int hScrollH = hScrollVisible ? 10 : 0;

    m_vScrollBar->setGeometry(width() - 10, rulerHeight(), 10, height() - rulerHeight() - hScrollH);
    m_hScrollBar->setGeometry(trackHeaderWidth(), height() - 10, width() - trackHeaderWidth() - vScrollW, 10);
}

void TimelineWidget::setScrollX(int x)
{
    if (m_hScrollBar) {
        m_hScrollBar->setValue(x);
    } else {
        m_scrollX = x;
        update();
    }
}

void TimelineWidget::setScrollY(int y)
{
    if (m_vScrollBar) {
        m_vScrollBar->setValue(y);
    } else {
        m_scrollY = y;
        update();
    }
}

void TimelineWidget::onScrollValueChanged(int value)
{
    m_scrollX = value;
    update();
}

void TimelineWidget::onVScrollValueChanged(int value)
{
    m_scrollY = value;
    update();
}

void TimelineWidget::setIsPlaying(bool playing)
{
    m_isPlaying = playing;
}

void TimelineWidget::invalidateFilmstripCache(qint64 clipId)
{
    if (clipId < 0) {
        m_filmstripCache.clear();
    } else {
        m_filmstripCache.remove(clipId);
    }
}

void TimelineWidget::onModelChanged()
{
    invalidateFilmstripCache();
    updateScrollBars();
    update();
}

void TimelineWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateScrollBars();
}

QRect TimelineWidget::trackRect(qint64 trackId) const
{
    if (!m_model) return QRect();

    int vScrollW = (m_vScrollBar && m_vScrollBar->isVisible()) ? 10 : 0;
    int y = rulerHeight() + 4 - m_scrollY;
    for (const TimelineTrack &t : m_model->videoTracks()) {
        if (t.id() == trackId) {
            return QRect(0, y, width() - vScrollW, trackHeight());
        }
        y += trackHeight() + trackGap();
    }

    y += dividerHeight();

    for (const TimelineTrack &t : m_model->audioTracks()) {
        if (t.id() == trackId) {
            return QRect(0, y, width() - vScrollW, trackHeight());
        }
        y += trackHeight() + trackGap();
    }

    return QRect();
}

qint64 TimelineWidget::trackIdAtY(int y) const
{
    if (!m_model || y < rulerHeight()) return -1;

    int curY = rulerHeight() + 4 - m_scrollY;
    for (const TimelineTrack &t : m_model->videoTracks()) {
        if (y >= curY && y <= curY + trackHeight()) {
            return t.id();
        }
        curY += trackHeight() + trackGap();
    }

    curY += dividerHeight();

    for (const TimelineTrack &t : m_model->audioTracks()) {
        if (y >= curY && y <= curY + trackHeight()) {
            return t.id();
        }
        curY += trackHeight() + trackGap();
    }

    return -1;
}

QRect TimelineWidget::clipRect(const TimelineClip &clip, const QRect &tRect) const
{
    int x1 = trackHeaderWidth() + timeMsToPixel(clip.timelineInMs()) - m_scrollX;
    int x2 = trackHeaderWidth() + timeMsToPixel(clip.timelineOutMs()) - m_scrollX;
    int w = qMax(8, x2 - x1);
    return QRect(x1, tRect.top() + 3, w, tRect.height() - 6);
}

TimelineWidget::ClipHitResult TimelineWidget::hitTestClip(const QPoint &pos) const
{
    ClipHitResult res;
    if (!m_model || pos.x() < trackHeaderWidth()) return res;

    qint64 tId = trackIdAtY(pos.y());
    if (tId <= 0) return res;

    const TimelineTrack *track = m_model->findTrack(tId);
    if (!track) return res;

    QRect tRect = trackRect(tId);
    for (const TimelineClip &clip : track->clips()) {
        QRect cRect = clipRect(clip, tRect);
        if (cRect.contains(pos)) {
            res.clipId = clip.id();
            res.trackId = tId;
            // Check edge proximity (within 8px of edge)
            if (pos.x() <= cRect.left() + 8) {
                res.isLeftEdge = true;
            } else if (pos.x() >= cRect.right() - 8) {
                res.isRightEdge = true;
            }
            return res;
        }
    }

    return res;
}

qint64 TimelineWidget::snapTime(qint64 timeMs, qint64 ignoreClipId, bool bypassSnap)
{
    if (!m_model) return timeMs;

    Snapping::TimelineSnapSettings settings = m_snapSettings;
    if (bypassSnap) {
        settings.enabled = false;
    }

    Snapping::TimelineSnapResult res = m_timelineSnapEngine.snapTime(
        timeMs, m_model, settings, m_pixelsPerSecond, ignoreClipId, m_playheadPositionMs
    );

    m_hasActiveSnapIndicator = res.snapped;
    if (res.snapped) {
        m_activeSnapIndicatorTimeMs = res.timeMs;
        return res.timeMs;
    }

    if (m_snapSettings.enabled && !bypassSnap && m_model) {
        qint64 snapThresholdMs = static_cast<qint64>((m_snapSettings.thresholdPixels / m_pixelsPerSecond) * 1000.0);
        for (const TimelineMarker &m : m_model->markers()) {
            qint64 dist = std::abs(timeMs - m.timeMs);
            if (dist <= snapThresholdMs) {
                m_hasActiveSnapIndicator = true;
                m_activeSnapIndicatorTimeMs = m.timeMs;
                return m.timeMs;
            }
        }
    }

    return res.timeMs;
}

void TimelineWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    if (!m_model) return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Background
    p.fillRect(rect(), QColor("#14171d"));

    int vScrollW = (m_vScrollBar && m_vScrollBar->isVisible()) ? 10 : 0;
    int hScrollH = (m_hScrollBar && m_hScrollBar->isVisible()) ? 10 : 0;

    // 1. Time Ruler at top (always fixed at top)
    QRect rulerRect(0, 0, width() - vScrollW, rulerHeight());
    p.fillRect(rulerRect, QColor("#1a1e26"));
    p.setPen(QColor("#30363d"));
    p.drawLine(0, rulerHeight(), width() - vScrollW, rulerHeight());

    // Draw ruler tick marks
    p.setPen(QColor("#8b949e"));
    QFont f = p.font();
    f.setPixelSize(10);
    p.setFont(f);

    // Intervals based on zoom level
    int stepSec = 1;
    if (m_pixelsPerSecond < 20) stepSec = 5;
    if (m_pixelsPerSecond < 10) stepSec = 10;
    if (m_pixelsPerSecond > 100) stepSec = 1;

    qint64 totalMs = m_model->totalDurationMs();
    int maxSec = (totalMs / 1000) + 10;

    for (int s = 0; s <= maxSec; s += stepSec) {
        int x = trackHeaderWidth() + timeMsToPixel(s * 1000LL) - m_scrollX;
        if (x < trackHeaderWidth() || x > width() - vScrollW) continue;

        bool major = (s % (stepSec * 5) == 0);
        int tickH = major ? 12 : 6;
        p.drawLine(x, rulerHeight() - tickH, x, rulerHeight());

        if (major) {
            int mins = s / 60;
            int secs = s % 60;
            QString text = QString("%1:%2").arg(mins, 2, 10, QChar('0')).arg(secs, 2, 10, QChar('0'));
            p.drawText(x + 4, rulerHeight() - 8, text);
        }
    }

    // Draw Markers on Ruler and vertical guide lines
    if (m_model) {
        for (const TimelineMarker &m : m_model->markers()) {
            int mx = trackHeaderWidth() + timeMsToPixel(m.timeMs) - m_scrollX;
            if (mx < trackHeaderWidth() || mx > width() - vScrollW) continue;

            // Dotted guide line across visible tracks
            p.setPen(QPen(m.color, 1, Qt::DashLine));
            p.drawLine(mx, rulerHeight(), mx, height() - hScrollH);

            // Diamond badge on ruler
            QPolygon diamond;
            diamond << QPoint(mx, rulerHeight() - 15)
                    << QPoint(mx + 6, rulerHeight() - 9)
                    << QPoint(mx, rulerHeight() - 3)
                    << QPoint(mx - 6, rulerHeight() - 9);

            p.setBrush(m.color);
            p.setPen(QPen(Qt::white, 1));
            p.drawPolygon(diamond);

            // Marker name text above or beside
            QFont mf = p.font();
            mf.setBold(true);
            mf.setPixelSize(9);
            p.setFont(mf);
            p.setPen(Qt::white);
            p.drawText(mx + 8, rulerHeight() - 6, m.name);
        }
    }

    // 2. Render Tracks (Clipped to viewport below ruler and above bottom scrollbar)
    QRect tracksViewport(0, rulerHeight() + 1, width() - vScrollW, height() - rulerHeight() - hScrollH - 1);
    p.setClipRect(tracksViewport);

    auto drawTracks = [this, &p, vScrollW](const QVector<TimelineTrack> &tracks, int startY) -> int {
        int y = startY;
        for (const TimelineTrack &track : tracks) {
            QRect tRect(0, y, width() - vScrollW, trackHeight());

            // Only draw if within visible vertical viewport
            if (tRect.bottom() >= rulerHeight() && tRect.top() <= height()) {
                // Track header
                QRect hRect(0, y, trackHeaderWidth(), trackHeight());
                p.fillRect(hRect, QColor("#1a202c"));
                p.setPen(QColor("#30363d"));
                p.drawRect(hRect);

                // Track name
                p.setPen(track.type() == TrackType::Video ? QColor("#58a6ff") : QColor("#d29922"));
                QFont tf = p.font();
                tf.setBold(true);
                tf.setPixelSize(12);
                p.setFont(tf);
                QString trackTitle = track.name();
                if (track.type() == TrackType::Video && m_model) {
                    trackTitle += QString(" [Z:%1]").arg(m_model->trackZIndex(track.id()));
                }
                p.drawText(hRect.adjusted(8, 6, -8, -6), Qt::AlignTop | Qt::AlignLeft, trackTitle);

                // Interactive Buttons in Track Header
                QFont bf = p.font();
                bf.setBold(true);
                bf.setPixelSize(10);
                p.setFont(bf);

                if (track.type() == TrackType::Video) {
                    // Eye / Visibility Button
                    QRect eyeBtn(8, y + 26, 44, 20);
                    p.setBrush(track.isVisible() ? QColor("#238636") : QColor("#21262d"));
                    p.setPen(QColor("#30363d"));
                    p.drawRoundedRect(eyeBtn, 3, 3);
                    p.setPen(track.isVisible() ? Qt::white : QColor("#8b949e"));
                    p.drawText(eyeBtn, Qt::AlignCenter, track.isVisible() ? "👁 Ver" : "✕ Ocul");

                    // Lock Button
                    QRect lockBtn(56, y + 26, 44, 20);
                    p.setBrush(track.isLocked() ? QColor("#d29922") : QColor("#21262d"));
                    p.setPen(QColor("#30363d"));
                    p.drawRoundedRect(lockBtn, 3, 3);
                    p.setPen(track.isLocked() ? QColor("#0d1117") : QColor("#8b949e"));
                    p.drawText(lockBtn, Qt::AlignCenter, track.isLocked() ? "🔒 Bloq" : "🔓 Libre");
                } else {
                    // Mute [M] Button
                    QRect mBtn(8, y + 26, 28, 20);
                    p.setBrush(track.isMuted() ? QColor("#f85149") : QColor("#21262d"));
                    p.setPen(QColor("#30363d"));
                    p.drawRoundedRect(mBtn, 3, 3);
                    p.setPen(track.isMuted() ? Qt::white : QColor("#8b949e"));
                    p.drawText(mBtn, Qt::AlignCenter, "M");

                    // Solo [S] Button
                    QRect sBtn(40, y + 26, 28, 20);
                    p.setBrush(track.isSolo() ? QColor("#d29922") : QColor("#21262d"));
                    p.setPen(QColor("#30363d"));
                    p.drawRoundedRect(sBtn, 3, 3);
                    p.setPen(track.isSolo() ? QColor("#0d1117") : QColor("#8b949e"));
                    p.drawText(sBtn, Qt::AlignCenter, "S");

                    // Lock Button
                    QRect lockBtn(72, y + 26, 28, 20);
                    p.setBrush(track.isLocked() ? QColor("#d29922") : QColor("#21262d"));
                    p.setPen(QColor("#30363d"));
                    p.drawRoundedRect(lockBtn, 3, 3);
                    p.setPen(track.isLocked() ? QColor("#0d1117") : QColor("#8b949e"));
                    p.drawText(lockBtn, Qt::AlignCenter, track.isLocked() ? "🔒" : "🔓");
                }

                // Track lane
                QRect lRect(trackHeaderWidth(), y, width() - trackHeaderWidth() - vScrollW, trackHeight());
                p.fillRect(lRect, QColor("#161b22"));
                p.setPen(QColor("#21262d"));
                p.drawRect(lRect);

                // Draw Clips in this track
                for (const TimelineClip &clip : track.clips()) {
                    QRect cRect = clipRect(clip, tRect);
                    if (cRect.right() < trackHeaderWidth() || cRect.left() > width() - vScrollW) continue;

                    // Clip body
                    QColor baseColor = clip.color();
                    if (track.isMuted() || !track.isVisible()) {
                        baseColor = baseColor.darker(150);
                    }

                    p.setBrush(baseColor);
                    bool isSelected = m_selectedClipIds.contains(clip.id()) || (clip.id() == m_selectedClipId);
                    p.setPen(isSelected ? QPen(QColor("#ffdf5d"), 2) : QPen(QColor("#11161f"), 1));
                    p.drawRoundedRect(cRect, 4, 4);

                    // Video & Image Filmstrip Thumbnails
                    if ((clip.type() == ClipType::Video || clip.type() == ClipType::Image) && !clip.filePath().isEmpty()) {
                        int thumbW = 56;
                        int thumbH = cRect.height() - 4;
                        if (cRect.width() >= 24 && thumbH > 10) {
                            auto it = m_filmstripCache.find(clip.id());
                            bool cacheValid = (it != m_filmstripCache.end() &&
                                               it->width == cRect.width() &&
                                               it->height == thumbH &&
                                               it->sourceInMs == clip.sourceInMs() &&
                                               it->durationMs == clip.durationMs() &&
                                               qAbs(it->speed - clip.speed()) < 0.001);

                            if (cacheValid) {
                                p.drawPixmap(cRect.left(), cRect.top() + 2, it->pixmap);
                            } else if (!m_isPlaying) {
                                int numThumbs = qBound(1, cRect.width() / thumbW, 20);
                                QPixmap stripPix(cRect.width(), thumbH);
                                stripPix.fill(Qt::transparent);
                                QPainter sp(&stripPix);

                                for (int ti = 0; ti < numThumbs; ++ti) {
                                    int tx = ti * thumbW;
                                    double progress = (numThumbs > 1) ? static_cast<double>(ti) / (numThumbs - 1) : 0.0;
                                    qint64 frameSourceMs = clip.sourceInMs() + qRound64(progress * clip.durationMs() * clip.speed());

                                    QImage thumb = VideoFrameDecoder::instance().getFrame(clip.filePath(), frameSourceMs, QSize(thumbW, thumbH));
                                    if (!thumb.isNull()) {
                                        sp.drawImage(QRect(tx, 0, thumbW, thumbH), thumb);
                                    }
                                }

                                // Dark gradient overlay at the top for title legibility
                                QLinearGradient grad(0, 0, 0, thumbH);
                                grad.setColorAt(0.0, QColor(0, 0, 0, 190));
                                grad.setColorAt(0.45, QColor(0, 0, 0, 110));
                                grad.setColorAt(1.0, QColor(0, 0, 0, 140));
                                sp.fillRect(QRect(0, 0, cRect.width(), thumbH), grad);
                                sp.end();

                                FilmstripCacheItem item;
                                item.width = cRect.width();
                                item.height = thumbH;
                                item.sourceInMs = clip.sourceInMs();
                                item.durationMs = clip.durationMs();
                                item.speed = clip.speed();
                                item.pixmap = stripPix;
                                m_filmstripCache[clip.id()] = item;

                                p.drawPixmap(cRect.left(), cRect.top() + 2, stripPix);
                            }
                        }
                    }

                    // Audio Waveform rendering
                    if (clip.type() == ClipType::Audio) {
                        QVector<float> peaks = WaveformGenerator::instance().getWaveform(clip.filePath(), 20);
                        if (!peaks.isEmpty()) {
                            p.setPen(QPen(QColor(255, 255, 255, 120), 1));
                            int centerY = cRect.center().y();
                            int maxAmp = (cRect.height() / 2) - 4;

                            for (int px = cRect.left() + 2; px < cRect.right() - 2; px += 2) {
                                qint64 timelineMs = pixelToTimeMs(px);
                                qint64 offsetTimeline = timelineMs - clip.timelineInMs();
                                if (offsetTimeline < 0 || offsetTimeline > clip.durationMs()) continue;
                                qint64 sourceMs = clip.sourceInMs() + qRound64(offsetTimeline * clip.speed());
                                int peakIdx = static_cast<int>((sourceMs / 1000.0) * 20.0);
                                if (peakIdx >= 0 && peakIdx < peaks.size()) {
                                    int amp = static_cast<int>(peaks[peakIdx] * maxAmp);
                                    p.drawLine(px, centerY - amp, px, centerY + amp);
                                }
                            }
                        }
                    }

                    // Fade In / Fade Out Triangles
                    if (clip.fadeInMs() > 0) {
                        int fadeW = timeMsToPixel(clip.fadeInMs());
                        QPolygon fadePoly;
                        fadePoly << cRect.topLeft() << QPoint(cRect.left() + fadeW, cRect.top()) << cRect.bottomLeft();
                        p.setBrush(QColor(0, 0, 0, 100));
                        p.setPen(Qt::NoPen);
                        p.drawPolygon(fadePoly);
                    }
                    if (clip.fadeOutMs() > 0) {
                        int fadeW = timeMsToPixel(clip.fadeOutMs());
                        QPolygon fadePoly;
                        fadePoly << QPoint(cRect.right() - fadeW, cRect.top()) << cRect.topRight() << cRect.bottomRight();
                        p.setBrush(QColor(0, 0, 0, 100));
                        p.setPen(Qt::NoPen);
                        p.drawPolygon(fadePoly);
                    }

                    // Text
                    p.setPen(Qt::white);
                    QFont cf = p.font();
                    cf.setBold(true);
                    cf.setPixelSize(10);
                    p.setFont(cf);

                    QString label = clip.name();
                    if (clip.type() == ClipType::Image) {
                        label = "🖼️ " + label;
                    } else if (clip.type() == ClipType::Text) {
                        label = "🔤 " + label;
                    }
                    if (clip.isLinked()) {
                        label = "🔗 " + label;
                    }
                    if (clip.transitionIn() != TransitionType::None || clip.transitionOut() != TransitionType::None) {
                        label += " 🔄";
                    }
                    if (clip.type() != ClipType::Audio && m_model) {
                        int z = m_model->clipZIndex(clip.id());
                        if (z > 0) {
                            label += QString(" [Z:%1]").arg(z);
                        }
                    }
                    if (!qFuzzyCompare(clip.speed(), 1.0)) {
                        label += QString(" (%1x)").arg(clip.speed(), 0, 'f', 1);
                    }
                    if (qAbs(clip.scaleX() - 1.0) > 0.05 || qAbs(clip.rotation()) > 1.0 || qAbs(clip.posX()) > 5.0 || qAbs(clip.posY()) > 5.0) {
                        label += " 📐";
                    }
                    p.drawText(cRect.adjusted(6, 4, -6, -4), Qt::AlignTop | Qt::AlignLeft, label);
                }
            }

            y += trackHeight() + trackGap();
        }
        return y;
    };

    int curY = rulerHeight() + 4 - m_scrollY;
    curY = drawTracks(m_model->videoTracks(), curY);

    // Divider between Video and Audio
    QRect divRect(0, curY, width() - vScrollW, dividerHeight());
    if (divRect.bottom() >= rulerHeight() && divRect.top() <= height()) {
        p.fillRect(divRect, QColor("#0d1117"));
        p.setPen(QColor("#30363d"));
        p.drawLine(0, curY + dividerHeight() / 2, width() - vScrollW, curY + dividerHeight() / 2);
        p.setPen(QColor("#8b949e"));
        QFont df = p.font();
        df.setPixelSize(8);
        p.setFont(df);
        p.drawText(divRect.adjusted(8, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, "PISTAS DE AUDIO");
    }
    curY += dividerHeight();

    drawTracks(m_model->audioTracks(), curY);

    // Marquee selection rectangle
    if (m_dragMode == DragMode::MarqueeSelect && !m_marqueeRect.isNull()) {
        p.setPen(QPen(QColor("#58a6ff"), 1, Qt::DashLine));
        p.setBrush(QColor(88, 166, 255, 40));
        p.drawRect(m_marqueeRect);
    }

    // Shaded area for region past total duration in tracks viewport
    qint64 totalDurMs = m_model->totalDurationMs();
    int endX = trackHeaderWidth() + timeMsToPixel(totalDurMs) - m_scrollX;
    if (endX < width() - vScrollW) {
        int shadeX = qMax(trackHeaderWidth(), endX);
        int shadeW = width() - vScrollW - shadeX;
        if (shadeW > 0) {
            p.fillRect(QRect(shadeX, rulerHeight() + 1, shadeW, height() - rulerHeight() - hScrollH - 1),
                       QColor(0, 0, 0, 115));
        }
    }

    // Reset clipping for Playhead, Duration and Snap indicators
    p.setClipping(false);

    // 3. End of Video Duration Marker (Vertical line + draggable handle on ruler)
    if (endX >= trackHeaderWidth() && endX <= width() - vScrollW) {
        // Shading on ruler past end of video
        int rulerShadeW = width() - vScrollW - endX;
        if (rulerShadeW > 0) {
            p.fillRect(QRect(endX, 0, rulerShadeW, rulerHeight()), QColor(0, 0, 0, 75));
        }

        // Vertical boundary line
        p.setPen(QPen(QColor("#f0883e"), 2, Qt::SolidLine));
        p.drawLine(endX, 0, endX, height() - hScrollH);

        // End Handle on the ruler
        int mins = (totalDurMs / 1000) / 60;
        double secs = (totalDurMs % 60000) / 1000.0;
        QString durText = QString("%1:%2").arg(mins, 2, 10, QChar('0')).arg(secs, 4, 'f', 1, QChar('0'));
        QString endLabel = QString("FIN %1").arg(durText);

        QFont endFont = p.font();
        endFont.setPixelSize(9);
        endFont.setBold(true);
        p.setFont(endFont);

        int badgeW = 76;
        int badgeH = rulerHeight() - 4;
        QRect endBadgeRect(endX, 2, badgeW, badgeH);

        // Draw bracket / tag
        p.setBrush(QColor("#f0883e"));
        p.setPen(QColor("#d29922"));
        p.drawRoundedRect(endBadgeRect, 3, 3);

        p.setPen(QColor("#0d1117"));
        p.drawText(endBadgeRect, Qt::AlignCenter, endLabel);
    }

    // Floating badge when dragging timeline duration
    if (m_isDraggingDuration) {
        int mins = (m_activeDurationDragMs / 1000) / 60;
        double secs = (m_activeDurationDragMs % 60000) / 1000.0;
        QString liveText = QString("⏱ Duración: %1:%2 (%3s)")
                               .arg(mins, 2, 10, QChar('0'))
                               .arg(secs, 4, 'f', 1, QChar('0'))
                               .arg(m_activeDurationDragMs / 1000.0, 0, 'f', 1);

        QFont badgeFont = p.font();
        badgeFont.setPixelSize(11);
        badgeFont.setBold(true);
        p.setFont(badgeFont);

        int dragX = trackHeaderWidth() + timeMsToPixel(m_activeDurationDragMs) - m_scrollX;
        QRect dragBadgeRect(dragX - 85, rulerHeight() + 8, 170, 24);
        p.setBrush(QColor("#1f242c"));
        p.setPen(QPen(QColor("#f0883e"), 2));
        p.drawRoundedRect(dragBadgeRect, 4, 4);

        p.setPen(QColor("#ffffff"));
        p.drawText(dragBadgeRect, Qt::AlignCenter, liveText);
    }

    // 4. Playhead (Vertical needle and head handle)
    int playheadX = trackHeaderWidth() + timeMsToPixel(m_playheadPositionMs) - m_scrollX;
    if (playheadX >= trackHeaderWidth() && playheadX <= width() - vScrollW) {
        // Red / golden needle
        p.setPen(QPen(QColor("#f85149"), 2));
        p.drawLine(playheadX, rulerHeight(), playheadX, height() - hScrollH);

        // Top triangle handle
        QPolygon head;
        head << QPoint(playheadX - 6, 0)
             << QPoint(playheadX + 6, 0)
             << QPoint(playheadX + 6, rulerHeight() - 6)
             << QPoint(playheadX, rulerHeight())
             << QPoint(playheadX - 6, rulerHeight() - 6);
        p.setBrush(QColor("#f85149"));
        p.setPen(QColor("#ff7b72"));
        p.drawPolygon(head);
    }

    // 5. Timeline Snapping indicator line
    if (m_hasActiveSnapIndicator) {
        int snapX = trackHeaderWidth() + timeMsToPixel(m_activeSnapIndicatorTimeMs) - m_scrollX;
        if (snapX >= trackHeaderWidth() && snapX <= width() - vScrollW) {
            p.setPen(QPen(QColor("#00d2ff"), 1.5, Qt::DashLine));
            p.drawLine(snapX, rulerHeight(), snapX, height() - hScrollH);

            p.setBrush(QColor("#00d2ff"));
            p.setPen(Qt::NoPen);
            p.drawEllipse(QPoint(snapX, rulerHeight() + 4), 3, 3);
        }
    }
}

void TimelineWidget::mousePressEvent(QMouseEvent *event)
{
    setFocus();
    m_dragStartPos = event->pos();

    // Check click on track header controls
    if (event->pos().x() < trackHeaderWidth()) {
        qint64 tId = trackIdAtY(event->pos().y());
        if (TimelineTrack *track = m_model->findTrack(tId)) {
            if (event->button() == Qt::RightButton) {
                showTrackContextMenu(tId, event->globalPosition().toPoint());
                return;
            }

            QRect tRect = trackRect(tId);
            int lx = event->pos().x();
            int ly = event->pos().y();

            if (track->type() == TrackType::Video) {
                QRect eyeBtn(8, tRect.top() + 26, 44, 20);
                QRect lockBtn(56, tRect.top() + 26, 44, 20);
                if (eyeBtn.contains(lx, ly)) {
                    track->setVisible(!track->isVisible());
                    m_model->notifyChange();
                    update();
                    return;
                } else if (lockBtn.contains(lx, ly)) {
                    track->setLocked(!track->isLocked());
                    m_model->notifyChange();
                    update();
                    return;
                }
            } else {
                QRect mBtn(8, tRect.top() + 26, 28, 20);
                QRect sBtn(40, tRect.top() + 26, 28, 20);
                QRect lockBtn(72, tRect.top() + 26, 28, 20);
                if (mBtn.contains(lx, ly)) {
                    track->setMuted(!track->isMuted());
                    m_model->notifyChange();
                    update();
                    return;
                } else if (sBtn.contains(lx, ly)) {
                    track->setSolo(!track->isSolo());
                    m_model->notifyChange();
                    update();
                    return;
                } else if (lockBtn.contains(lx, ly)) {
                    track->setLocked(!track->isLocked());
                    m_model->notifyChange();
                    update();
                    return;
                }
            }
        }
        return;
    }

    // Check click on End of Video Duration Marker
    int endMarkerX = trackHeaderWidth() + timeMsToPixel(m_model->totalDurationMs()) - m_scrollX;
    bool nearEndMarker = (qAbs(event->pos().x() - endMarkerX) <= 8 || 
                         (event->pos().x() >= endMarkerX && event->pos().x() <= endMarkerX + 76 && event->pos().y() <= rulerHeight()));

    if (nearEndMarker && event->button() == Qt::LeftButton) {
        m_dragMode = DragMode::DragTimelineDuration;
        m_dragStartPos = event->pos();
        m_isDraggingDuration = true;
        m_activeDurationDragMs = m_model->totalDurationMs();
        update();
        return;
    }

    // Ruler click -> check click on existing marker or scrub playhead
    if (event->pos().y() <= rulerHeight()) {
        if (m_model) {
            for (const TimelineMarker &m : m_model->markers()) {
                int mx = trackHeaderWidth() + timeMsToPixel(m.timeMs) - m_scrollX;
                if (qAbs(event->pos().x() - mx) <= 8) {
                    if (event->button() == Qt::RightButton) {
                        showMarkerContextMenu(m.id, event->globalPosition().toPoint());
                        return;
                    }
                    setPlayheadPosition(m.timeMs);
                    emit playheadSeekRequested(m.timeMs);
                    return;
                }
            }
        }

        m_dragMode = DragMode::ScrubPlayhead;
        qint64 t = pixelToTimeMs(event->pos().x());
        setPlayheadPosition(t);
        emit playheadSeekRequested(t);
        return;
    }

    // Check click on a clip
    ClipHitResult hit = hitTestClip(event->pos());
    bool isMultiMod = (event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier | Qt::MetaModifier));

    if (hit.clipId > 0) {
        if (event->button() == Qt::RightButton) {
            if (!m_selectedClipIds.contains(hit.clipId)) {
                selectClip(hit.clipId, false);
            }
            showClipContextMenu(hit.clipId, event->globalPosition().toPoint());
            return;
        }

        if (isMultiMod) {
            selectClip(hit.clipId, true);
        } else {
            if (!m_selectedClipIds.contains(hit.clipId)) {
                selectClip(hit.clipId, false);
            }
        }

        TimelineClip *clip = m_model->findClip(hit.clipId);
        if (clip) {
            m_dragClipId = hit.clipId;
            m_dragTrackId = hit.trackId;
            m_dragInitialTimelineInMs = clip->timelineInMs();
            m_dragInitialTimelineOutMs = clip->timelineOutMs();

            m_dragMultiInitialInMs.clear();
            for (qint64 id : m_selectedClipIds) {
                if (TimelineClip *c = m_model->findClip(id)) {
                    m_dragMultiInitialInMs[id] = c->timelineInMs();
                }
            }

            if (hit.isLeftEdge) {
                m_dragMode = DragMode::TrimClipLeft;
            } else if (hit.isRightEdge) {
                m_dragMode = DragMode::TrimClipRight;
            } else {
                m_dragMode = DragMode::MoveClip;
            }
        }
        update();
        return;
    }

    // Empty space click -> scrub playhead & initiate rubberband selection
    if (event->button() == Qt::LeftButton) {
        if (!isMultiMod) {
            clearSelection();
        }
        m_dragMode = DragMode::MarqueeSelect;
        m_marqueeRect = QRect(m_dragStartPos, m_dragStartPos);

        qint64 t = pixelToTimeMs(event->pos().x());
        setPlayheadPosition(t);
        emit playheadSeekRequested(t);
        update();
    } else if (event->button() == Qt::RightButton) {
        qint64 tMs = pixelToTimeMs(event->pos().x());
        qint64 tId = trackIdAtY(event->pos().y());
        showEmptyContextMenu(tId, tMs, event->globalPosition().toPoint());
    }
}

void TimelineWidget::mouseMoveEvent(QMouseEvent *event)
{
    // Update cursor depending on hover over end marker or clip edges
    if (m_dragMode == DragMode::None) {
        int endMarkerX = trackHeaderWidth() + timeMsToPixel(m_model ? m_model->totalDurationMs() : 10000) - m_scrollX;
        bool nearEndMarker = (qAbs(event->pos().x() - endMarkerX) <= 8 || 
                             (event->pos().x() >= endMarkerX && event->pos().x() <= endMarkerX + 76 && event->pos().y() <= rulerHeight()));
        if (nearEndMarker) {
            setCursor(Qt::SizeHorCursor);
            setToolTip(QString("Fin de video: %1 s (Arrastra para elegir duración, doble clic para auto-ajustar)")
                           .arg((m_model ? m_model->totalDurationMs() : 10000) / 1000.0, 0, 'f', 1));
            return;
        }

        ClipHitResult hit = hitTestClip(event->pos());
        if (hit.clipId > 0 && (hit.isLeftEdge || hit.isRightEdge)) {
            setCursor(Qt::SizeHorCursor);
        } else if (hit.clipId > 0) {
            setCursor(Qt::SizeAllCursor);
        } else {
            setCursor(Qt::ArrowCursor);
            setToolTip(QString());
        }
        return;
    }

    if (m_dragMode == DragMode::DragTimelineDuration) {
        bool isAltPressed = (event->modifiers() & Qt::AltModifier);
        qint64 rawMs = pixelToTimeMs(event->pos().x());
        qint64 targetMs = snapTime(rawMs, -1, isAltPressed);
        targetMs = qMax<qint64>(500, targetMs);
        m_model->setCustomDurationMs(targetMs);
        m_isDraggingDuration = true;
        m_activeDurationDragMs = targetMs;
        updateScrollBars();
        update();
        return;
    }

    if (m_dragMode == DragMode::ScrubPlayhead) {
        qint64 t = pixelToTimeMs(event->pos().x());
        setPlayheadPosition(t);
        emit playheadSeekRequested(t);
        return;
    }

    if (m_dragMode == DragMode::MarqueeSelect) {
        m_marqueeRect = QRect(m_dragStartPos, event->pos()).normalized();
        update();
        return;
    }

    bool isAltPressed = (event->modifiers() & Qt::AltModifier);

    if (m_dragMode == DragMode::TrimClipLeft) {
        qint64 deltaMs = pixelToTimeMs(event->pos().x()) - pixelToTimeMs(m_dragStartPos.x());
        qint64 targetIn = snapTime(m_dragInitialTimelineInMs + deltaMs, m_dragClipId, isAltPressed);
        m_model->trimClip(m_dragClipId, true, targetIn);
        if (TimelineClip *c = m_model->findClip(m_dragClipId)) {
            qint64 dur = c->durationMs();
            qint64 dMs = (m_dragInitialTimelineOutMs - m_dragInitialTimelineInMs) - dur;
            QToolTip::showText(event->globalPosition().toPoint(), QString("Recorte In: %1s (Δ %2%3s)")
                               .arg(dur / 1000.0, 0, 'f', 2)
                               .arg(dMs >= 0 ? "-" : "+")
                               .arg(qAbs(dMs) / 1000.0, 0, 'f', 2), this);
        }
        update();
        return;
    }

    if (m_dragMode == DragMode::TrimClipRight) {
        qint64 deltaMs = pixelToTimeMs(event->pos().x()) - pixelToTimeMs(m_dragStartPos.x());
        qint64 targetOut = snapTime(m_dragInitialTimelineOutMs + deltaMs, m_dragClipId, isAltPressed);
        m_model->trimClip(m_dragClipId, false, targetOut);
        if (TimelineClip *c = m_model->findClip(m_dragClipId)) {
            qint64 dur = c->durationMs();
            qint64 dMs = dur - (m_dragInitialTimelineOutMs - m_dragInitialTimelineInMs);
            QToolTip::showText(event->globalPosition().toPoint(), QString("Recorte Out: %1s (Δ %2%3s)")
                               .arg(dur / 1000.0, 0, 'f', 2)
                               .arg(dMs >= 0 ? "+" : "")
                               .arg(dMs / 1000.0, 0, 'f', 2), this);
        }
        update();
        return;
    }

    if (m_dragMode == DragMode::MoveClip) {
        qint64 deltaMs = pixelToTimeMs(event->pos().x()) - pixelToTimeMs(m_dragStartPos.x());
        qint64 targetIn = snapTime(m_dragInitialTimelineInMs + deltaMs, m_dragClipId, isAltPressed);
        deltaMs = targetIn - m_dragInitialTimelineInMs;

        if (m_selectedClipIds.size() > 1 && m_dragMultiInitialInMs.size() > 1) {
            // Check lower bound so no clip moves before 0
            for (auto it = m_dragMultiInitialInMs.begin(); it != m_dragMultiInitialInMs.end(); ++it) {
                if (it.value() + deltaMs < 0) {
                    deltaMs = -it.value();
                }
            }
            for (auto it = m_dragMultiInitialInMs.begin(); it != m_dragMultiInitialInMs.end(); ++it) {
                qint64 cid = it.key();
                qint64 cTrackId = -1;
                for (const auto &tr : m_model->videoTracks()) {
                    if (tr.findClip(cid)) { cTrackId = tr.id(); break; }
                }
                if (cTrackId <= 0) {
                    for (const auto &tr : m_model->audioTracks()) {
                        if (tr.findClip(cid)) { cTrackId = tr.id(); break; }
                    }
                }
                if (cTrackId > 0) {
                    m_model->moveClip(cid, cTrackId, it.value() + deltaMs);
                }
            }
        } else {
            qint64 targetTrackId = trackIdAtY(event->pos().y());
            if (targetTrackId <= 0) targetTrackId = m_dragTrackId;
            m_model->moveClip(m_dragClipId, targetTrackId, targetIn);
        }
        update();
        return;
    }
}

void TimelineWidget::mouseReleaseEvent(QMouseEvent *event)
{
    m_hasActiveSnapIndicator = false;

    if (m_dragMode == DragMode::DragTimelineDuration) {
        m_dragMode = DragMode::None;
        m_isDraggingDuration = false;
        setCursor(Qt::ArrowCursor);
        updateScrollBars();
        update();
        return;
    }

    if (m_dragMode == DragMode::MarqueeSelect) {
        if (m_marqueeRect.width() > 3 || m_marqueeRect.height() > 3) {
            bool isMultiMod = (event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier | Qt::MetaModifier));
            QSet<qint64> foundIds = isMultiMod ? m_selectedClipIds : QSet<qint64>();

            auto checkIntersect = [this, &foundIds](const QVector<TimelineTrack> &tracks) {
                for (const TimelineTrack &t : tracks) {
                    QRect tRect = trackRect(t.id());
                    for (const TimelineClip &c : t.clips()) {
                        QRect cRect = clipRect(c, tRect);
                        if (m_marqueeRect.intersects(cRect)) {
                            foundIds.insert(c.id());
                        }
                    }
                }
            };
            checkIntersect(m_model->videoTracks());
            checkIntersect(m_model->audioTracks());
            setSelectedClipIds(foundIds);
        }
        m_marqueeRect = QRect();
    }

    m_dragMode = DragMode::None;
    setCursor(Qt::ArrowCursor);
    QToolTip::hideText();
    update();
}

void TimelineWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (m_model) {
        int endMarkerX = trackHeaderWidth() + timeMsToPixel(m_model->totalDurationMs()) - m_scrollX;
        bool nearEndMarker = (qAbs(event->pos().x() - endMarkerX) <= 12 || 
                             (event->pos().x() >= endMarkerX && event->pos().x() <= endMarkerX + 76 && event->pos().y() <= rulerHeight()));
        if (nearEndMarker) {
            m_model->setCustomDurationMs(0); // Reset to auto-fit
            updateScrollBars();
            update();
            return;
        }
    }
    QWidget::mouseDoubleClickEvent(event);
}

void TimelineWidget::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier || event->modifiers() & Qt::MetaModifier) {
        // Zoom centered around mouse pointer
        if (event->angleDelta().y() > 0) {
            zoomIn();
        } else {
            zoomOut();
        }
        event->accept();
        return;
    }

    bool isShift = (event->modifiers() & Qt::ShiftModifier);

    // If Shift modifier is pressed, scroll horizontally
    if (isShift) {
        int dx = (event->angleDelta().x() != 0) ? event->angleDelta().x() : event->angleDelta().y();
        m_hScrollBar->setValue(m_hScrollBar->value() - dx);
        event->accept();
        return;
    }

    // Horizontal wheel or touchpad horizontal gesture
    if (event->angleDelta().x() != 0) {
        m_hScrollBar->setValue(m_hScrollBar->value() - event->angleDelta().x());
        event->accept();
        return;
    }

    // Vertical wheel: scroll tracks vertically if vertical scrollbar is available
    if (m_vScrollBar && m_vScrollBar->isVisible() && m_vScrollBar->maximum() > 0) {
        m_vScrollBar->setValue(m_vScrollBar->value() - event->angleDelta().y());
    } else {
        // Fallback to horizontal scrolling when tracks fit in the viewport
        m_hScrollBar->setValue(m_hScrollBar->value() - event->angleDelta().y());
    }
    event->accept();
}

void TimelineWidget::keyPressEvent(QKeyEvent *event)
{
    bool isCtrlOrCmd = (event->modifiers() & Qt::ControlModifier) || (event->modifiers() & Qt::MetaModifier);
    bool isShift = (event->modifiers() & Qt::ShiftModifier);

    // Undo / Redo shortcuts
    if (isCtrlOrCmd && event->key() == Qt::Key_Z) {
        if (isShift) {
            redo();
        } else {
            undo();
        }
        event->accept();
        return;
    }

    if (isCtrlOrCmd && event->key() == Qt::Key_Y) {
        redo();
        event->accept();
        return;
    }

    // Join clips shortcut (Ctrl+J / Cmd+J)
    if (isCtrlOrCmd && event->key() == Qt::Key_J) {
        joinSelectedClips();
        event->accept();
        return;
    }

    // Split shortcut (Ctrl+K / Cmd+K)
    if (isCtrlOrCmd && event->key() == Qt::Key_K) {
        splitSelectedClips();
        event->accept();
        return;
    }

    // Duplicate shortcut (Ctrl+D / Cmd+D)
    if (isCtrlOrCmd && event->key() == Qt::Key_D) {
        duplicateSelectedClip();
        event->accept();
        return;
    }

    // Select All shortcut (Ctrl+A / Cmd+A)
    if (isCtrlOrCmd && event->key() == Qt::Key_A) {
        selectAll();
        event->accept();
        return;
    }

    // Delete
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        deleteSelectedClips(isShift);
        event->accept();
        return;
    }

    // Add Marker shortcut (M)
    if (event->key() == Qt::Key_M && !isCtrlOrCmd) {
        if (m_model) {
            m_model->addMarker(m_playheadPositionMs);
            update();
        }
        event->accept();
        return;
    }

    QWidget::keyPressEvent(event);
}

void TimelineWidget::applyFilterToSelectedClips(VisualFilter filter)
{
    if (!m_model) return;
    QList<qint64> targets;
    if (!m_selectedClipIds.isEmpty()) {
        targets = m_selectedClipIds.values();
    } else if (m_selectedClipId > 0) {
        targets.append(m_selectedClipId);
    }
    if (targets.isEmpty()) return;

    m_model->setClipsFilter(targets, filter);
    update();
}

void TimelineWidget::addFilterToSelectedClips(VisualFilter filter)
{
    if (!m_model) return;
    QList<qint64> targets;
    if (!m_selectedClipIds.isEmpty()) {
        targets = m_selectedClipIds.values();
    } else if (m_selectedClipId > 0) {
        targets.append(m_selectedClipId);
    }
    if (targets.isEmpty()) return;

    if (filter == VisualFilter::None) {
        m_model->clearClipsFilters(targets);
    } else {
        m_model->addClipsFilter(targets, filter);
    }
    update();
}

void TimelineWidget::populateEffectsMenu(QMenu *parentMenu, const std::function<void(VisualFilter)> &onSelectFilter)
{
    if (!parentMenu) return;

    QMap<QString, QMenu*> categoryMenus;
    for (const VisualFilterInfo &info : allVisualFilters()) {
        if (info.filter == VisualFilter::None) {
            QAction *actNone = parentMenu->addAction("🚫 " + info.name);
            QObject::connect(actNone, &QAction::triggered, [onSelectFilter]() {
                onSelectFilter(VisualFilter::None);
            });
            parentMenu->addSeparator();
            continue;
        }

        if (!categoryMenus.contains(info.category)) {
            QMenu *catMenu = parentMenu->addMenu(info.category);
            catMenu->setStyleSheet(parentMenu->styleSheet());
            categoryMenus.insert(info.category, catMenu);
        }

        QAction *act = categoryMenus[info.category]->addAction(info.name);
        VisualFilter f = info.filter;
        QObject::connect(act, &QAction::triggered, [onSelectFilter, f]() {
            onSelectFilter(f);
        });
    }
}

void TimelineWidget::showClipContextMenu(qint64 clipId, const QPoint &globalPos)
{
    TimelineClip *clip = m_model->findClip(clipId);
    if (!clip) return;

    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background-color: #161b22; color: #c9d1d9; border: 1px solid #30363d; }"
        "QMenu::item:selected { background-color: #1f293d; color: #58a6ff; }"
    );

    if (m_selectedClipIds.size() >= 2) {
        QAction *actJoin = menu.addAction("🔗 Unir clips seleccionados (Ctrl+J)");
        QAction *actSplit = menu.addAction("✂ Cortar clips seleccionados (Ctrl+K)");

        QMenu *effectsMenu = menu.addMenu("✨ Aplicar Efecto Visual a la Selección");
        effectsMenu->setStyleSheet(menu.styleSheet());
        populateEffectsMenu(effectsMenu, [this](VisualFilter f) {
            applyFilterToSelectedClips(f);
        });

        menu.addSeparator();
        QAction *actDeleteMulti = menu.addAction("🗑 Eliminar clips seleccionados (Supr)");
        QAction *actRippleMulti = menu.addAction("⏪ Eliminar con rizado (Shift+Supr)");

        QAction *selected = menu.exec(globalPos);
        if (!selected) return;

        if (selected == actJoin) {
            joinSelectedClips();
        } else if (selected == actSplit) {
            splitSelectedClips();
        } else if (selected == actDeleteMulti) {
            deleteSelectedClips(false);
        } else if (selected == actRippleMulti) {
            deleteSelectedClips(true);
        }
        return;
    }

    QAction *actSeparateAudio = nullptr;
    if (clip->type() == ClipType::Video || clip->isLinked()) {
        actSeparateAudio = menu.addAction("Separar Audio de Video");
    }

    QAction *actSplit = menu.addAction("Cortar en el cabezal (Split) (Ctrl+K)");
    QAction *actDuplicate = menu.addAction("Duplicar clip (Ctrl+D)");

    QMenu *effectsMenu = menu.addMenu("✨ Aplicar Efecto Visual");
    effectsMenu->setStyleSheet(menu.styleSheet());
    populateEffectsMenu(effectsMenu, [this, clipId](VisualFilter f) {
        if (m_model) {
            m_model->setClipFilter(clipId, f);
            update();
        }
    });

    menu.addSeparator();
    QAction *actDelete = menu.addAction("Eliminar clip (Supr)");
    QAction *actRippleDelete = menu.addAction("Eliminar con rizado (Ripple Delete) (Shift+Supr)");

    QAction *selected = menu.exec(globalPos);
    if (!selected) return;

    if (selected == actSeparateAudio) {
        m_model->separateAudio(clipId);
    } else if (selected == actSplit) {
        m_model->splitClip(clipId, m_playheadPositionMs);
    } else if (selected == actDuplicate) {
        m_model->duplicateClip(clipId);
    } else if (selected == actDelete) {
        deleteSelectedClip(false);
    } else if (selected == actRippleDelete) {
        deleteSelectedClip(true);
    }
}

void TimelineWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void TimelineWidget::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void TimelineWidget::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        qint64 dropTime = pixelToTimeMs(event->position().x());
        qint64 targetTrackId = trackIdAtY(event->position().y());
        qint64 lastClipId = -1;

        for (const QUrl &url : event->mimeData()->urls()) {
            if (url.isLocalFile()) {
                QString path = url.toLocalFile();
                MediaItem item(path);
                qint64 dur = item.durationMs() > 0 ? item.durationMs() : 5000;
                qint64 id = m_model->addMediaClip(path, item.type(), targetTrackId, dropTime, dur, false);
                if (id > 0) {
                    lastClipId = id;
                }
            }
        }
        if (lastClipId > 0) {
            setSelectedClipId(lastClipId);
            emit clipSelected(lastClipId);
        }
        event->acceptProposedAction();
    }
}

void TimelineWidget::showEmptyContextMenu(qint64 trackId, qint64 timelineMs, const QPoint &globalPos)
{
    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background-color: #161b22; color: #c9d1d9; border: 1px solid #30363d; }"
        "QMenu::item:selected { background-color: #1f293d; color: #58a6ff; }"
    );

    QAction *actAddText = menu.addAction("🔤 Añadir Cuadro de Texto aquí (Ctrl+T)");
    menu.addSeparator();
    QAction *actAddVideoTrack = menu.addAction("+ Añadir Pista de Video");
    QAction *actAddAudioTrack = menu.addAction("+ Añadir Pista de Audio");

    QAction *selected = menu.exec(globalPos);
    if (!selected) return;

    if (selected == actAddText) {
        if (m_model) {
            qint64 newId = m_model->addTextClip("Texto de ejemplo", timelineMs, 5000, trackId);
            if (newId > 0) {
                setSelectedClipId(newId);
                emit clipSelected(newId);
            }
        }
    } else if (selected == actAddVideoTrack) {
        addVideoTrack();
    } else if (selected == actAddAudioTrack) {
        addAudioTrack();
    }
}

void TimelineWidget::showTrackContextMenu(qint64 trackId, const QPoint &globalPos)
{
    if (!m_model) return;
    TimelineTrack *track = m_model->findTrack(trackId);
    if (!track) return;

    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background-color: #161b22; color: #c9d1d9; border: 1px solid #30363d; padding: 4px; }"
        "QMenu::item:selected { background-color: #1f293d; color: #58a6ff; }"
    );

    QAction *actRename = menu.addAction("✏️ Renombrar Pista...");
    QAction *actToggleLock = menu.addAction(track->isLocked() ? "🔓 Desbloquear Pista" : "🔒 Bloquear Pista");

    QAction *actAddV = nullptr;
    QAction *actAddA = nullptr;
    if (track->type() == TrackType::Video) {
        actAddV = menu.addAction("➕ Añadir Pista de Video Arriba");
    } else {
        actAddA = menu.addAction("➕ Añadir Pista de Audio Abajo");
    }

    menu.addSeparator();
    QAction *actDelete = menu.addAction("🗑️ Eliminar Pista");

    QAction *chosen = menu.exec(globalPos);
    if (!chosen) return;

    if (chosen == actRename) {
        bool ok = false;
        QString name = QInputDialog::getText(this, "Renombrar Pista", "Nuevo nombre de pista:", QLineEdit::Normal, track->name(), &ok);
        if (ok && !name.trimmed().isEmpty()) {
            m_model->renameTrack(trackId, name.trimmed());
            update();
        }
    } else if (chosen == actToggleLock) {
        track->setLocked(!track->isLocked());
        m_model->notifyChange();
        update();
    } else if (chosen == actAddV) {
        addVideoTrack();
    } else if (chosen == actAddA) {
        addAudioTrack();
    } else if (chosen == actDelete) {
        m_model->removeTrack(trackId);
        update();
    }
}

void TimelineWidget::showMarkerContextMenu(qint64 markerId, const QPoint &globalPos)
{
    if (!m_model) return;
    TimelineMarker *marker = m_model->findMarker(markerId);
    if (!marker) return;

    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background-color: #161b22; color: #c9d1d9; border: 1px solid #30363d; padding: 4px; }"
        "QMenu::item:selected { background-color: #1f293d; color: #58a6ff; }"
    );

    QAction *actRename = menu.addAction("✏️ Cambiar Nombre del Marcador...");
    QAction *actColor = menu.addAction("🎨 Cambiar Color del Marcador...");
    menu.addSeparator();
    QAction *actDelete = menu.addAction("🗑️ Eliminar Marcador");

    QAction *chosen = menu.exec(globalPos);
    if (!chosen) return;

    if (chosen == actRename) {
        bool ok = false;
        QString name = QInputDialog::getText(this, "Nombre del Marcador", "Etiqueta / Nota:", QLineEdit::Normal, marker->name, &ok);
        if (ok) {
            marker->name = name.trimmed();
            m_model->notifyChange();
            update();
        }
    } else if (chosen == actColor) {
        QColor c = QColorDialog::getColor(marker->color, this, "Seleccionar Color de Marcador");
        if (c.isValid()) {
            marker->color = c;
            m_model->notifyChange();
            update();
        }
    } else if (chosen == actDelete) {
        m_model->removeMarker(markerId);
        update();
    }
}
