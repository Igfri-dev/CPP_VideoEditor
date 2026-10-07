#pragma once

#include <QObject>
#include <QVector>
#include "track.h"
#include "clip.h"
#include "marker.h"
#include <QSize>

#include <QList>

enum class ProjectAspectRatio {
    Landscape_16_9,
    Vertical_9_16,
    Square_1_1,
    Classic_4_3,
    Cinema_21_9
};

QString projectAspectRatioToString(ProjectAspectRatio ratio);
ProjectAspectRatio stringToProjectAspectRatio(const QString &str);
QString projectAspectRatioDisplayName(ProjectAspectRatio ratio);
QSize projectAspectRatioDimensions(ProjectAspectRatio ratio);

struct TimelineSnapshot {
    QVector<TimelineTrack> videoTracks;
    QVector<TimelineTrack> audioTracks;
    QVector<TimelineMarker> markers;
    ProjectAspectRatio aspectRatio = ProjectAspectRatio::Landscape_16_9;
    qint64 nextClipId = 1;
    qint64 nextTrackId = 1;
    qint64 nextMarkerId = 1;
    qint64 customDurationMs = 0;
    ColorAdjustments globalColorAdjustments;
    QString description;
};

class TimelineModel : public QObject {
    Q_OBJECT

public:
    explicit TimelineModel(QObject *parent = nullptr);

    void resetProject();

    // Track management
    const QVector<TimelineTrack>& videoTracks() const { return m_videoTracks; }
    QVector<TimelineTrack>& videoTracks() { return m_videoTracks; }

    const QVector<TimelineTrack>& audioTracks() const { return m_audioTracks; }
    QVector<TimelineTrack>& audioTracks() { return m_audioTracks; }

    TimelineTrack* findTrack(qint64 trackId);
    const TimelineTrack* findTrack(qint64 trackId) const;
    TimelineTrack* findTrackForClip(qint64 clipId);
    const TimelineTrack* findTrackForClip(qint64 clipId) const;

    qint64 addTrack(TrackType type, const QString &name = QString());
    bool removeTrack(qint64 trackId);
    bool renameTrack(qint64 trackId, const QString &newName);

    // Clip operations
    qint64 addClip(const TimelineClip &clip, qint64 targetTrackId = -1);
    qint64 addMediaClip(const QString &filePath, ClipType type, qint64 targetTrackId,
                        qint64 timelineInMs, qint64 durationMs, bool separateAudio = false);
    qint64 addTextClip(const QString &text = "Texto de ejemplo", qint64 timelineInMs = 0,
                       qint64 durationMs = 5000, qint64 targetTrackId = -1);

    // Essential Requirement: Separar Audio / Desvincular Audio
    bool separateAudio(qint64 clipId);
    bool linkClips(qint64 clipId1, qint64 clipId2);

    bool splitClip(qint64 clipId, qint64 positionMs);
    bool splitAllAtPlayhead(qint64 positionMs);
    bool splitClips(const QList<qint64> &clipIds, qint64 positionMs);
    bool joinClips(const QList<qint64> &clipIds);

    bool trimClip(qint64 clipId, bool isLeftEdge, qint64 newTimelineMs);
    bool moveClip(qint64 clipId, qint64 targetTrackId, qint64 newTimelineInMs);
    qint64 duplicateClip(qint64 clipId);
    bool deleteClip(qint64 clipId, bool ripple = false);
    bool deleteClips(const QList<qint64> &clipIds, bool ripple = false);

    bool setClipFilter(qint64 clipId, VisualFilter filter);
    bool setClipsFilter(const QList<qint64> &clipIds, VisualFilter filter);
    bool addClipFilter(qint64 clipId, VisualFilter filter);
    bool addClipsFilter(const QList<qint64> &clipIds, VisualFilter filter);
    bool removeClipFilterAt(qint64 clipId, int index);
    bool moveClipFilter(qint64 clipId, int fromIndex, int toIndex);
    bool setClipFilterStack(qint64 clipId, const QVector<VisualFilter> &stack);
    bool setClipsFilterStack(const QList<qint64> &clipIds, const QVector<VisualFilter> &stack);
    bool clearClipFilters(qint64 clipId);
    bool clearClipsFilters(const QList<qint64> &clipIds);

    bool setClipMotionPath(qint64 clipId, const MotionPath &path);
    bool setClipMotionPreset(qint64 clipId, MotionPreset preset);
    bool setClipMotionStartPoint(qint64 clipId, const QPointF &pos);
    bool setClipMotionEndPoint(qint64 clipId, const QPointF &pos);

    // Z-Index / Capas (Pista base = 1, pistas superiores = 2, 3...)
    int trackZIndex(qint64 trackId) const;
    int clipZIndex(qint64 clipId) const;
    int totalVideoZLevels() const;
    bool setClipZIndex(qint64 clipId, int targetZIndex);

    // Color & Luminosity Adjustments (Global / Master & Per-clip)
    const ColorAdjustments& globalColorAdjustments() const { return m_globalColorAdjustments; }
    void setGlobalColorAdjustments(const ColorAdjustments &adj, bool saveUndo = true);
    void setGlobalColorGradeMode(ColorGradeMode mode, bool saveUndo = true);
    void setGlobalBrightness(int b, bool saveUndo = true);
    void setGlobalLuminosity(int l, bool saveUndo = true);
    void setGlobalRedPresence(int r, bool saveUndo = true);
    void setGlobalGreenPresence(int g, bool saveUndo = true);
    void setGlobalBluePresence(int b, bool saveUndo = true);
    void setGlobalLumaCurve(const ColorCurve &curve, bool saveUndo = true);
    void setGlobalColorCurve(const ColorCurve &curve, bool saveUndo = true);
    void setGlobalRedCurve(const ColorCurve &curve, bool saveUndo = true);
    void setGlobalGreenCurve(const ColorCurve &curve, bool saveUndo = true);
    void setGlobalBlueCurve(const ColorCurve &curve, bool saveUndo = true);
    void setGlobalTimeOfDay(bool enabled, float val, bool saveUndo = true);
    void resetGlobalColorAdjustments(bool saveUndo = true);

    bool setClipColorAdjustments(qint64 clipId, const ColorAdjustments &adj, bool saveUndo = true);
    bool setClipsColorAdjustments(const QList<qint64> &clipIds, const ColorAdjustments &adj, bool saveUndo = true);
    bool setClipColorGradeMode(qint64 clipId, ColorGradeMode mode, bool saveUndo = true);
    bool setClipBrightness(qint64 clipId, int b, bool saveUndo = true);
    bool setClipLuminosity(qint64 clipId, int l, bool saveUndo = true);
    bool setClipRedPresence(qint64 clipId, int r, bool saveUndo = true);
    bool setClipGreenPresence(qint64 clipId, int g, bool saveUndo = true);
    bool setClipBluePresence(qint64 clipId, int b, bool saveUndo = true);
    bool setClipTimeOfDay(qint64 clipId, bool enabled, float val, bool saveUndo = true);
    bool setClipsTimeOfDay(const QList<qint64> &clipIds, bool enabled, float val, bool saveUndo = true);
    bool setClipLumaCurve(qint64 clipId, const ColorCurve &curve, bool saveUndo = true);
    bool setClipColorCurve(qint64 clipId, const ColorCurve &curve, bool saveUndo = true);
    bool setClipRedCurve(qint64 clipId, const ColorCurve &curve, bool saveUndo = true);
    bool setClipGreenCurve(qint64 clipId, const ColorCurve &curve, bool saveUndo = true);
    bool setClipBlueCurve(qint64 clipId, const ColorCurve &curve, bool saveUndo = true);
    bool resetClipColorAdjustments(qint64 clipId, bool saveUndo = true);

    TimelineClip* findClip(qint64 clipId);
    const TimelineClip* findClip(qint64 clipId) const;

    qint64 totalDurationMs() const;
    qint64 contentDurationMs() const;
    qint64 customDurationMs() const { return m_customDurationMs; }
    void setCustomDurationMs(qint64 durationMs);
    bool isCustomDuration() const { return m_customDurationMs > 0; }

    // Aspect Ratio & Resolution
    ProjectAspectRatio aspectRatio() const { return m_aspectRatio; }
    void setAspectRatio(ProjectAspectRatio ratio, bool saveUndo = true);
    QSize canvasSize() const { return projectAspectRatioDimensions(m_aspectRatio); }

    // Markers management
    const QVector<TimelineMarker>& markers() const { return m_markers; }
    qint64 addMarker(qint64 timeMs, const QString &name = QString(), const QColor &color = QColor("#58a6ff"), bool saveUndo = true);
    bool removeMarker(qint64 markerId, bool saveUndo = true);
    TimelineMarker* findMarker(qint64 markerId);
    const TimelineMarker* findMarker(qint64 markerId) const;
    void clearMarkers(bool saveUndo = true);
    void setMarkers(const QVector<TimelineMarker> &markers);

    // Track controls
    void setTrackMuted(qint64 trackId, bool muted);
    void setTrackSolo(qint64 trackId, bool solo);
    void setTrackVisible(qint64 trackId, bool visible);
    void setTrackLocked(qint64 trackId, bool locked);
    void setTrackVolume(qint64 trackId, double volume);
    void notifyChange();

    // Undo / Redo system
    void saveState(const QString &description = QString());
    bool canUndo() const { return !m_undoStack.isEmpty(); }
    bool canRedo() const { return !m_redoStack.isEmpty(); }
    void undo();
    void redo();
    void clearHistory();
    QString undoText() const;
    QString redoText() const;

signals:
    void timelineChanged();
    void clipAdded(qint64 clipId);
    void clipRemoved(qint64 clipId);
    void clipModified(qint64 clipId);
    void durationChanged(qint64 durationMs);
    void undoRedoStateChanged();
    void globalColorAdjustmentsChanged();
    void markersChanged();
    void aspectRatioChanged(ProjectAspectRatio ratio, const QSize &canvasSize);

private:
    qint64 m_nextClipId = 1;
    qint64 m_nextTrackId = 1;
    qint64 m_nextMarkerId = 1;
    QVector<TimelineTrack> m_videoTracks;
    QVector<TimelineTrack> m_audioTracks;
    QVector<TimelineMarker> m_markers;
    ProjectAspectRatio m_aspectRatio = ProjectAspectRatio::Landscape_16_9;
    qint64 m_customDurationMs = 0;
    ColorAdjustments m_globalColorAdjustments;

    QVector<TimelineSnapshot> m_undoStack;
    QVector<TimelineSnapshot> m_redoStack;
    static constexpr int kMaxUndoSteps = 50;
};
