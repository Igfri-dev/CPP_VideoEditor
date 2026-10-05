#pragma once

#include "clip.h"
#include <QString>
#include <QVector>

enum class TrackType {
    Video,
    Audio
};

class TimelineTrack {
public:
    TimelineTrack();
    TimelineTrack(qint64 id, const QString &name, TrackType type);

    qint64 id() const { return m_id; }
    void setId(qint64 id) { m_id = id; }

    QString name() const { return m_name; }
    void setName(const QString &name) { m_name = name; }

    TrackType type() const { return m_type; }
    void setType(TrackType type) { m_type = type; }

    bool isMuted() const { return m_isMuted; }
    void setMuted(bool muted) { m_isMuted = muted; }

    bool isSolo() const { return m_isSolo; }
    void setSolo(bool solo) { m_isSolo = solo; }

    bool isVisible() const { return m_isVisible; }
    void setVisible(bool visible) { m_isVisible = visible; }

    bool isLocked() const { return m_isLocked; }
    void setLocked(bool locked) { m_isLocked = locked; }

    double volume() const { return m_volume; }
    void setVolume(double volume) { m_volume = qBound(0.0, volume, 2.0); }

    const QVector<TimelineClip>& clips() const { return m_clips; }
    QVector<TimelineClip>& clips() { return m_clips; }

    bool canAcceptClipType(ClipType clipType) const;
    void addClip(const TimelineClip &clip);
    bool removeClip(qint64 clipId);
    TimelineClip* findClip(qint64 clipId);
    const TimelineClip* findClip(qint64 clipId) const;
    const TimelineClip* clipAtTime(qint64 timelineMs) const;

    bool hasOverlap(qint64 inMs, qint64 outMs, qint64 ignoreClipId = -1) const;
    qint64 nextAvailableTime(qint64 desiredInMs, qint64 durationMs) const;
    void sortClips();

private:
    qint64 m_id = 0;
    QString m_name;
    TrackType m_type = TrackType::Video;
    bool m_isMuted = false;
    bool m_isSolo = false;
    bool m_isVisible = true;
    bool m_isLocked = false;
    double m_volume = 1.0;
    QVector<TimelineClip> m_clips;
};
