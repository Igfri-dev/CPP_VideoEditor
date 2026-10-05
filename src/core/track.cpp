#include "track.h"
#include <algorithm>

TimelineTrack::TimelineTrack()
    : m_id(0)
    , m_type(TrackType::Video)
{
}

TimelineTrack::TimelineTrack(qint64 id, const QString &name, TrackType type)
    : m_id(id)
    , m_name(name)
    , m_type(type)
{
}

bool TimelineTrack::canAcceptClipType(ClipType clipType) const
{
    if (m_type == TrackType::Video) {
        return clipType == ClipType::Video || clipType == ClipType::Image || clipType == ClipType::Text;
    } else {
        return clipType == ClipType::Audio;
    }
}

void TimelineTrack::addClip(const TimelineClip &clip)
{
    TimelineClip c = clip;
    c.setTrackId(m_id);
    m_clips.append(c);
    sortClips();
}

bool TimelineTrack::removeClip(qint64 clipId)
{
    for (int i = 0; i < m_clips.size(); ++i) {
        if (m_clips[i].id() == clipId) {
            m_clips.removeAt(i);
            return true;
        }
    }
    return false;
}

TimelineClip* TimelineTrack::findClip(qint64 clipId)
{
    for (int i = 0; i < m_clips.size(); ++i) {
        if (m_clips[i].id() == clipId) {
            return &m_clips[i];
        }
    }
    return nullptr;
}

const TimelineClip* TimelineTrack::findClip(qint64 clipId) const
{
    for (int i = 0; i < m_clips.size(); ++i) {
        if (m_clips[i].id() == clipId) {
            return &m_clips[i];
        }
    }
    return nullptr;
}

const TimelineClip* TimelineTrack::clipAtTime(qint64 timelineMs) const
{
    for (const TimelineClip &clip : m_clips) {
        if (timelineMs >= clip.timelineInMs() && timelineMs < clip.timelineOutMs()) {
            return &clip;
        }
    }
    for (const TimelineClip &clip : m_clips) {
        if (timelineMs == clip.timelineOutMs() && clip.durationMs() > 0) {
            return &clip;
        }
    }
    return nullptr;
}

bool TimelineTrack::hasOverlap(qint64 inMs, qint64 outMs, qint64 ignoreClipId) const
{
    for (const TimelineClip &clip : m_clips) {
        if (clip.id() == ignoreClipId) {
            continue;
        }
        if (inMs < clip.timelineOutMs() && outMs > clip.timelineInMs()) {
            return true;
        }
    }
    return false;
}

qint64 TimelineTrack::nextAvailableTime(qint64 desiredInMs, qint64 durationMs) const
{
    qint64 currentCandidate = qMax<qint64>(0, desiredInMs);
    bool collision = true;
    while (collision) {
        collision = false;
        for (const TimelineClip &clip : m_clips) {
            if (currentCandidate < clip.timelineOutMs() && (currentCandidate + durationMs) > clip.timelineInMs()) {
                currentCandidate = clip.timelineOutMs();
                collision = true;
                break;
            }
        }
    }
    return currentCandidate;
}

void TimelineTrack::sortClips()
{
    std::sort(m_clips.begin(), m_clips.end(), [](const TimelineClip &a, const TimelineClip &b) {
        return a.timelineInMs() < b.timelineInMs();
    });
}
