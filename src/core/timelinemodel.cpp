#include "timelinemodel.h"
#include "medialibrary/mediaitem.h"
#include <QFileInfo>
#include <QtMath>
#include <algorithm>

QString projectAspectRatioToString(ProjectAspectRatio ratio)
{
    switch (ratio) {
    case ProjectAspectRatio::Landscape_16_9: return "16:9";
    case ProjectAspectRatio::Vertical_9_16: return "9:16";
    case ProjectAspectRatio::Square_1_1: return "1:1";
    case ProjectAspectRatio::Classic_4_3: return "4:3";
    case ProjectAspectRatio::Cinema_21_9: return "21:9";
    }
    return "16:9";
}

ProjectAspectRatio stringToProjectAspectRatio(const QString &str)
{
    QString s = str.trimmed();
    if (s == "9:16" || s.contains("Vertical", Qt::CaseInsensitive)) return ProjectAspectRatio::Vertical_9_16;
    if (s == "1:1" || s.contains("Square", Qt::CaseInsensitive) || s.contains("Cuadrado", Qt::CaseInsensitive)) return ProjectAspectRatio::Square_1_1;
    if (s == "4:3") return ProjectAspectRatio::Classic_4_3;
    if (s == "21:9" || s.contains("Cinema", Qt::CaseInsensitive)) return ProjectAspectRatio::Cinema_21_9;
    return ProjectAspectRatio::Landscape_16_9;
}

QString projectAspectRatioDisplayName(ProjectAspectRatio ratio)
{
    switch (ratio) {
    case ProjectAspectRatio::Landscape_16_9: return "16:9 Horizontal (YouTube / TV)";
    case ProjectAspectRatio::Vertical_9_16: return "9:16 Vertical (TikTok / Reels / Shorts)";
    case ProjectAspectRatio::Square_1_1: return "1:1 Cuadrado (Instagram Feed)";
    case ProjectAspectRatio::Classic_4_3: return "4:3 Clásico / TV Antigua";
    case ProjectAspectRatio::Cinema_21_9: return "21:9 Panorámico / Cinemascope";
    }
    return "16:9 Horizontal";
}

QSize projectAspectRatioDimensions(ProjectAspectRatio ratio)
{
    switch (ratio) {
    case ProjectAspectRatio::Landscape_16_9: return QSize(1920, 1080);
    case ProjectAspectRatio::Vertical_9_16: return QSize(1080, 1920);
    case ProjectAspectRatio::Square_1_1: return QSize(1080, 1080);
    case ProjectAspectRatio::Classic_4_3: return QSize(1440, 1080);
    case ProjectAspectRatio::Cinema_21_9: return QSize(2560, 1080);
    }
    return QSize(1920, 1080);
}

TimelineModel::TimelineModel(QObject *parent)
    : QObject(parent)
{
    resetProject();
}

void TimelineModel::resetProject()
{
    m_videoTracks.clear();
    m_audioTracks.clear();
    m_markers.clear();
    m_nextClipId = 1;
    m_nextTrackId = 1;
    m_nextMarkerId = 1;
    m_aspectRatio = ProjectAspectRatio::Landscape_16_9;

    // Default tracks: V2 (Overlay / Images), V1 (Main Video), A1, A2
    m_videoTracks.append(TimelineTrack(m_nextTrackId++, "V2 (Superposición / Imágenes)", TrackType::Video));
    m_videoTracks.append(TimelineTrack(m_nextTrackId++, "V1 (Video Principal)", TrackType::Video));
    m_audioTracks.append(TimelineTrack(m_nextTrackId++, "A1", TrackType::Audio));
    m_audioTracks.append(TimelineTrack(m_nextTrackId++, "A2", TrackType::Audio));
    m_customDurationMs = 0;
    m_globalColorAdjustments = ColorAdjustments();

    clearHistory();
    emit markersChanged();
    emit aspectRatioChanged(m_aspectRatio, canvasSize());
    notifyChange();
}

TimelineTrack* TimelineModel::findTrack(qint64 trackId)
{
    for (TimelineTrack &track : m_videoTracks) {
        if (track.id() == trackId) return &track;
    }
    for (TimelineTrack &track : m_audioTracks) {
        if (track.id() == trackId) return &track;
    }
    return nullptr;
}

const TimelineTrack* TimelineModel::findTrack(qint64 trackId) const
{
    for (const TimelineTrack &track : m_videoTracks) {
        if (track.id() == trackId) return &track;
    }
    for (const TimelineTrack &track : m_audioTracks) {
        if (track.id() == trackId) return &track;
    }
    return nullptr;
}

TimelineTrack* TimelineModel::findTrackForClip(qint64 clipId)
{
    for (TimelineTrack &track : m_videoTracks) {
        if (track.findClip(clipId)) return &track;
    }
    for (TimelineTrack &track : m_audioTracks) {
        if (track.findClip(clipId)) return &track;
    }
    return nullptr;
}

const TimelineTrack* TimelineModel::findTrackForClip(qint64 clipId) const
{
    for (const TimelineTrack &track : m_videoTracks) {
        if (track.findClip(clipId)) return &track;
    }
    for (const TimelineTrack &track : m_audioTracks) {
        if (track.findClip(clipId)) return &track;
    }
    return nullptr;
}

qint64 TimelineModel::addTrack(TrackType type, const QString &name)
{
    qint64 id = m_nextTrackId++;
    QString trackName = name;
    if (trackName.isEmpty()) {
        if (type == TrackType::Video) {
            trackName = QString("V%1").arg(m_videoTracks.size() + 1);
        } else {
            trackName = QString("A%1").arg(m_audioTracks.size() + 1);
        }
    }

    if (type == TrackType::Video) {
        // Insert new video tracks at the top
        m_videoTracks.prepend(TimelineTrack(id, trackName, type));
    } else {
        // Append new audio tracks at the bottom
        m_audioTracks.append(TimelineTrack(id, trackName, type));
    }

    notifyChange();
    return id;
}

bool TimelineModel::removeTrack(qint64 trackId)
{
    for (int i = 0; i < m_videoTracks.size(); ++i) {
        if (m_videoTracks[i].id() == trackId) {
            m_videoTracks.removeAt(i);
            notifyChange();
            return true;
        }
    }
    for (int i = 0; i < m_audioTracks.size(); ++i) {
        if (m_audioTracks[i].id() == trackId) {
            m_audioTracks.removeAt(i);
            notifyChange();
            return true;
        }
    }
    return false;
}

TimelineClip* TimelineModel::findClip(qint64 clipId)
{
    for (TimelineTrack &track : m_videoTracks) {
        if (TimelineClip *clip = track.findClip(clipId)) return clip;
    }
    for (TimelineTrack &track : m_audioTracks) {
        if (TimelineClip *clip = track.findClip(clipId)) return clip;
    }
    return nullptr;
}

const TimelineClip* TimelineModel::findClip(qint64 clipId) const
{
    for (const TimelineTrack &track : m_videoTracks) {
        if (const TimelineClip *clip = track.findClip(clipId)) return clip;
    }
    for (const TimelineTrack &track : m_audioTracks) {
        if (const TimelineClip *clip = track.findClip(clipId)) return clip;
    }
    return nullptr;
}

qint64 TimelineModel::addClip(const TimelineClip &clip, qint64 targetTrackId)
{
    saveState("Añadir clip");
    TimelineTrack *track = nullptr;
    if (targetTrackId > 0) {
        track = findTrack(targetTrackId);
    }
    if (!track || !track->canAcceptClipType(clip.type())) {
        if (clip.type() == ClipType::Audio) {
            track = m_audioTracks.isEmpty() ? nullptr : &m_audioTracks.first();
        } else {
            track = m_videoTracks.isEmpty() ? nullptr : &m_videoTracks.last();
        }
    }
    if (!track) {
        qint64 tid = addTrack(clip.type() == ClipType::Audio ? TrackType::Audio : TrackType::Video);
        track = findTrack(tid);
    }
    if (!track) return -1;

    TimelineClip newClip = clip;
    if (newClip.id() <= 0) {
        newClip.setId(m_nextClipId++);
    } else {
        m_nextClipId = qMax(m_nextClipId, newClip.id() + 1);
    }
    newClip.setTrackId(track->id());
    track->addClip(newClip);

    emit clipAdded(newClip.id());
    notifyChange();
    return newClip.id();
}

qint64 TimelineModel::addMediaClip(const QString &filePath, ClipType type, qint64 targetTrackId,
                                   qint64 timelineInMs, qint64 durationMs, bool separateAudio)
{
    saveState("Añadir medio");
    QFileInfo info(filePath);
    qint64 effectiveDuration = durationMs;
    if (effectiveDuration <= 0) {
        MediaItem item(filePath);
        effectiveDuration = item.durationMs() > 0 ? item.durationMs() : 5000;
    }

    if (type == ClipType::Video) {
        TimelineTrack *videoTrack = findTrack(targetTrackId);
        if (!videoTrack || videoTrack->type() != TrackType::Video) {
            videoTrack = m_videoTracks.isEmpty() ? nullptr : &m_videoTracks.last(); // typically V1
        }
        if (!videoTrack) {
            qint64 vId = addTrack(TrackType::Video, "V1");
            videoTrack = findTrack(vId);
        }

        qint64 startMs = videoTrack->nextAvailableTime(timelineInMs, effectiveDuration);
        qint64 vClipId = m_nextClipId++;

        TimelineClip vClip(vClipId, videoTrack->id(), info.fileName(), filePath, ClipType::Video);
        vClip.setTimelineInMs(startMs);
        vClip.setTimelineOutMs(startMs + effectiveDuration);
        vClip.setSourceInMs(0);
        vClip.setSourceOutMs(effectiveDuration);
        vClip.setSourceDurationMs(effectiveDuration);

        // Also add audio counterpart for video
        TimelineTrack *audioTrack = m_audioTracks.isEmpty() ? nullptr : &m_audioTracks.first(); // typically A1
        if (!audioTrack) {
            qint64 aId = addTrack(TrackType::Audio, "A1");
            audioTrack = findTrack(aId);
        }

        qint64 aClipId = m_nextClipId++;
        TimelineClip aClip(aClipId, audioTrack->id(), QString("%1 (Audio)").arg(info.fileName()), filePath, ClipType::Audio);
        aClip.setTimelineInMs(startMs);
        aClip.setTimelineOutMs(startMs + effectiveDuration);
        aClip.setSourceInMs(0);
        aClip.setSourceOutMs(effectiveDuration);
        aClip.setSourceDurationMs(effectiveDuration);

        if (!separateAudio) {
            // Linked by default
            vClip.setLinkedClipId(aClipId);
            aClip.setLinkedClipId(vClipId);
        } else {
            // Separated into distinct elements from the start
            vClip.setLinkedClipId(-1);
            aClip.setLinkedClipId(-1);
        }

        // Mute the video clip's internal audio track so sound only plays from the audio clip
        vClip.setAudioMuted(true);

        videoTrack->addClip(vClip);
        audioTrack->addClip(aClip);

        emit clipAdded(vClipId);
        emit clipAdded(aClipId);
        notifyChange();
        return vClipId;
    }

    // Audio or Image
    TimelineTrack *track = findTrack(targetTrackId);
    if (!track || !track->canAcceptClipType(type)) {
        if (type == ClipType::Audio) {
            track = m_audioTracks.isEmpty() ? nullptr : &m_audioTracks.first();
        } else if (type == ClipType::Image) {
            // Overlay track is first in videoTracks (V2)
            track = m_videoTracks.isEmpty() ? nullptr : &m_videoTracks.first();
        } else {
            track = m_videoTracks.isEmpty() ? nullptr : &m_videoTracks.last();
        }
    }
    if (!track) {
        qint64 tid = addTrack(type == ClipType::Audio ? TrackType::Audio : TrackType::Video,
                              type == ClipType::Image ? "V2 (Superposición / Imágenes)" : "V1 (Video Principal)");
        track = findTrack(tid);
    }
    if (!track) return -1;

    qint64 startMs = track->nextAvailableTime(timelineInMs, effectiveDuration);
    qint64 clipId = m_nextClipId++;
    TimelineClip clip(clipId, track->id(), info.fileName(), filePath, type);
    clip.setTimelineInMs(startMs);
    clip.setTimelineOutMs(startMs + effectiveDuration);
    clip.setSourceInMs(0);
    clip.setSourceOutMs(effectiveDuration);
    clip.setSourceDurationMs(effectiveDuration);

    track->addClip(clip);
    emit clipAdded(clipId);
    notifyChange();
    return clipId;
}

qint64 TimelineModel::addTextClip(const QString &text, qint64 timelineInMs, qint64 durationMs, qint64 targetTrackId)
{
    saveState("Añadir texto");

    TimelineTrack *track = findTrack(targetTrackId);
    if (!track || !track->canAcceptClipType(ClipType::Text)) {
        track = m_videoTracks.isEmpty() ? nullptr : &m_videoTracks.first();
    }
    if (!track) {
        qint64 tid = addTrack(TrackType::Video, "V2 (Superposición / Texto)");
        track = findTrack(tid);
    }
    if (!track) return -1;

    qint64 effectiveDur = durationMs > 0 ? durationMs : 5000;
    qint64 startMs = track->nextAvailableTime(timelineInMs, effectiveDur);
    qint64 clipId = m_nextClipId++;

    QString content = text.isEmpty() ? "Texto de ejemplo" : text;
    QString clipName = content.length() > 20 ? (content.left(18) + "...") : content;

    TimelineClip clip(clipId, track->id(), clipName, "", ClipType::Text);
    clip.setTimelineInMs(startMs);
    clip.setTimelineOutMs(startMs + effectiveDur);
    clip.setSourceInMs(0);
    clip.setSourceOutMs(effectiveDur);
    clip.setSourceDurationMs(effectiveDur);
    clip.setTextContent(content);

    track->addClip(clip);
    emit clipAdded(clipId);
    notifyChange();
    return clipId;
}

bool TimelineModel::separateAudio(qint64 clipId)
{
    if (!findClip(clipId)) return false;

    saveState("Separar audio");

    TimelineClip *clip = findClip(clipId);
    if (!clip) return false;

    // If it's already linked to an audio clip, unlinking them separates them completely
    if (clip->isLinked()) {
        qint64 linkedId = clip->linkedClipId();
        clip->setLinkedClipId(-1);
        TimelineClip *linkedClip = findClip(linkedId);
        if (linkedClip) {
            linkedClip->setLinkedClipId(-1);
            emit clipModified(linkedId);
        }
        emit clipModified(clipId);
        notifyChange();
        return true;
    }

    // If it's a video clip that doesn't have an independent audio clip yet
    if (clip->type() == ClipType::Video) {
        TimelineTrack *audioTrack = m_audioTracks.isEmpty() ? nullptr : &m_audioTracks.first();
        if (!audioTrack) {
            qint64 aId = addTrack(TrackType::Audio, "A1");
            audioTrack = findTrack(aId);
        }
        if (!audioTrack) return false;

        qint64 aClipId = m_nextClipId++;
        TimelineClip aClip(aClipId, audioTrack->id(), QString("%1 (Audio)").arg(clip->name()),
                           clip->filePath(), ClipType::Audio);
        aClip.setTimelineInMs(clip->timelineInMs());
        aClip.setTimelineOutMs(clip->timelineOutMs());
        aClip.setSourceInMs(clip->sourceInMs());
        aClip.setSourceOutMs(clip->sourceOutMs());
        aClip.setSourceDurationMs(clip->sourceDurationMs());
        aClip.setSpeed(clip->speed());
        aClip.setVolume(clip->volume());
        aClip.setFadeInMs(clip->fadeInMs());
        aClip.setFadeOutMs(clip->fadeOutMs());
        aClip.setLinkedClipId(-1); // Completely independent!

        clip->setAudioMuted(true);
        clip->setLinkedClipId(-1);

        audioTrack->addClip(aClip);
        emit clipAdded(aClipId);
        emit clipModified(clipId);
        notifyChange();
        return true;
    }

    return false;
}

bool TimelineModel::linkClips(qint64 clipId1, qint64 clipId2)
{
    if (clipId1 == clipId2) return false;
    if (!findClip(clipId1) || !findClip(clipId2)) return false;

    saveState("Vincular clips");

    TimelineClip *clip1 = findClip(clipId1);
    TimelineClip *clip2 = findClip(clipId2);
    if (!clip1 || !clip2) return false;

    clip1->setLinkedClipId(clip2->id());
    clip2->setLinkedClipId(clip1->id());

    emit clipModified(clipId1);
    emit clipModified(clipId2);
    notifyChange();
    return true;
}

bool TimelineModel::splitClip(qint64 clipId, qint64 positionMs)
{
    TimelineClip *cCheck = findClip(clipId);
    if (!cCheck) return false;
    if (positionMs <= cCheck->timelineInMs() || positionMs >= cCheck->timelineOutMs()) {
        return false;
    }

    saveState("Cortar clip");

    TimelineTrack *track = findTrackForClip(clipId);
    if (!track) return false;
    TimelineClip *clip = track->findClip(clipId);
    if (!clip) return false;

    qint64 splitOffsetTimeline = positionMs - clip->timelineInMs();
    qint64 splitOffsetSource = qRound64(splitOffsetTimeline * clip->speed());

    // Create right slice
    TimelineClip rightSlice = *clip;
    rightSlice.setId(m_nextClipId++);
    rightSlice.setName(QString("%1 (Cut)").arg(clip->name()));
    rightSlice.setTimelineInMs(positionMs);
    rightSlice.setTimelineOutMs(clip->timelineOutMs());
    rightSlice.setSourceInMs(clip->sourceInMs() + splitOffsetSource);
    rightSlice.setSourceOutMs(clip->sourceOutMs());

    // Adjust left slice
    clip->setTimelineOutMs(positionMs);
    clip->setSourceOutMs(clip->sourceInMs() + splitOffsetSource);

    // If clip was linked, split linked clip too
    qint64 linkedId = clip->linkedClipId();
    if (linkedId > 0) {
        TimelineTrack *linkedTrack = findTrackForClip(linkedId);
        if (linkedTrack) {
            TimelineClip *linkedClip = linkedTrack->findClip(linkedId);
            if (linkedClip && positionMs > linkedClip->timelineInMs() && positionMs < linkedClip->timelineOutMs()) {
                qint64 linkedSplitOffset = positionMs - linkedClip->timelineInMs();
                qint64 linkedSplitSource = qRound64(linkedSplitOffset * linkedClip->speed());

                TimelineClip rightLinked = *linkedClip;
                rightLinked.setId(m_nextClipId++);
                rightLinked.setName(QString("%1 (Cut)").arg(linkedClip->name()));
                rightLinked.setTimelineInMs(positionMs);
                rightLinked.setTimelineOutMs(linkedClip->timelineOutMs());
                rightLinked.setSourceInMs(linkedClip->sourceInMs() + linkedSplitSource);
                rightLinked.setSourceOutMs(linkedClip->sourceOutMs());

                linkedClip->setTimelineOutMs(positionMs);
                linkedClip->setSourceOutMs(linkedClip->sourceInMs() + linkedSplitSource);

                rightSlice.setLinkedClipId(rightLinked.id());
                rightLinked.setLinkedClipId(rightSlice.id());

                linkedTrack->addClip(rightLinked);
                emit clipAdded(rightLinked.id());
                emit clipModified(linkedId);
            }
        }
    } else {
        rightSlice.setLinkedClipId(-1);
    }

    track->addClip(rightSlice);
    emit clipAdded(rightSlice.id());
    emit clipModified(clipId);
    notifyChange();
    return true;
}

bool TimelineModel::splitAllAtPlayhead(qint64 positionMs)
{
    QList<qint64> clipsToSplit;
    for (const TimelineTrack &track : m_videoTracks) {
        if (const TimelineClip *c = track.clipAtTime(positionMs)) {
            clipsToSplit.append(c->id());
        }
    }
    for (const TimelineTrack &track : m_audioTracks) {
        if (const TimelineClip *c = track.clipAtTime(positionMs)) {
            clipsToSplit.append(c->id());
        }
    }

    if (clipsToSplit.isEmpty()) return false;
    return splitClips(clipsToSplit, positionMs);
}

bool TimelineModel::trimClip(qint64 clipId, bool isLeftEdge, qint64 newTimelineMs)
{
    TimelineTrack *track = findTrackForClip(clipId);
    if (!track || track->isLocked()) return false;
    TimelineClip *clip = track->findClip(clipId);
    if (!clip) return false;

    const qint64 minDurationMs = 100; // minimum 100ms clip duration

    if (isLeftEdge) {
        qint64 clampedIn = qBound<qint64>(0, newTimelineMs, clip->timelineOutMs() - minDurationMs);
        qint64 diffTimeline = clampedIn - clip->timelineInMs();
        qint64 diffSource = qRound64(diffTimeline * clip->speed());
        qint64 newSourceIn = clip->sourceInMs() + diffSource;

        if (newSourceIn < 0 || newSourceIn >= clip->sourceOutMs()) {
            return false;
        }
        if (track->hasOverlap(clampedIn, clip->timelineOutMs(), clipId)) {
            return false;
        }

        clip->setTimelineInMs(clampedIn);
        clip->setSourceInMs(newSourceIn);
    } else {
        qint64 clampedOut = qMax<qint64>(clip->timelineInMs() + minDurationMs, newTimelineMs);
        qint64 diffTimeline = clampedOut - clip->timelineOutMs();
        qint64 diffSource = qRound64(diffTimeline * clip->speed());
        qint64 newSourceOut = clip->sourceOutMs() + diffSource;

        if (clip->sourceDurationMs() > 0 && newSourceOut > clip->sourceDurationMs()) {
            newSourceOut = clip->sourceDurationMs();
            clampedOut = clip->timelineInMs() + qRound64((newSourceOut - clip->sourceInMs()) / clip->speed());
        }
        if (track->hasOverlap(clip->timelineInMs(), clampedOut, clipId)) {
            return false;
        }

        clip->setTimelineOutMs(clampedOut);
        clip->setSourceOutMs(newSourceOut);
    }

    track->sortClips();
    emit clipModified(clipId);
    notifyChange();
    return true;
}

bool TimelineModel::moveClip(qint64 clipId, qint64 targetTrackId, qint64 newTimelineInMs)
{
    TimelineTrack *sourceTrack = findTrackForClip(clipId);
    if (!sourceTrack || sourceTrack->isLocked()) return false;
    TimelineClip *clip = sourceTrack->findClip(clipId);
    if (!clip) return false;

    TimelineTrack *targetTrack = findTrack(targetTrackId);
    if (!targetTrack || targetTrack->isLocked()) targetTrack = sourceTrack;
    if (!targetTrack->canAcceptClipType(clip->type())) targetTrack = sourceTrack;

    qint64 duration = clip->durationMs();
    qint64 targetIn = qMax<qint64>(0, newTimelineInMs);
    qint64 targetOut = targetIn + duration;

    // Check collision on target track
    if (targetTrack->hasOverlap(targetIn, targetOut, clipId)) {
        // Find next open spot nearby
        targetIn = targetTrack->nextAvailableTime(targetIn, duration);
        targetOut = targetIn + duration;
    }

    qint64 timeDelta = targetIn - clip->timelineInMs();

    // If changing track
    if (targetTrack->id() != sourceTrack->id()) {
        TimelineClip movedClip = *clip;
        movedClip.setTimelineInMs(targetIn);
        movedClip.setTimelineOutMs(targetOut);
        movedClip.setTrackId(targetTrack->id());

        sourceTrack->removeClip(clipId);
        targetTrack->addClip(movedClip);
    } else {
        clip->setTimelineInMs(targetIn);
        clip->setTimelineOutMs(targetOut);
        sourceTrack->sortClips();
    }

    // If linked clip exists, move it by timeDelta if desired
    if (clip->isLinked() && timeDelta != 0) {
        TimelineClip *linked = findClip(clip->linkedClipId());
        if (linked) {
            TimelineTrack *lTrack = findTrackForClip(linked->id());
            if (lTrack && !lTrack->isLocked()) {
                qint64 lIn = qMax<qint64>(0, linked->timelineInMs() + timeDelta);
                qint64 lOut = lIn + linked->durationMs();
                if (!lTrack->hasOverlap(lIn, lOut, linked->id())) {
                    linked->setTimelineInMs(lIn);
                    linked->setTimelineOutMs(lOut);
                    lTrack->sortClips();
                    emit clipModified(linked->id());
                }
            }
        }
    }

    emit clipModified(clipId);
    notifyChange();
    return true;
}

qint64 TimelineModel::duplicateClip(qint64 clipId)
{
    if (!findClip(clipId)) return -1;

    saveState("Duplicar clip");

    TimelineTrack *track = findTrackForClip(clipId);
    if (!track) return -1;
    TimelineClip *clip = track->findClip(clipId);
    if (!clip) return -1;

    TimelineClip copy = *clip;
    copy.setId(m_nextClipId++);
    copy.setName(QString("%1 (Copia)").arg(clip->name()));
    copy.setLinkedClipId(-1); // duplicate is independent

    qint64 duration = clip->durationMs();
    qint64 nextIn = track->nextAvailableTime(clip->timelineOutMs(), duration);
    copy.setTimelineInMs(nextIn);
    copy.setTimelineOutMs(nextIn + duration);

    track->addClip(copy);
    emit clipAdded(copy.id());
    notifyChange();
    return copy.id();
}

bool TimelineModel::deleteClip(qint64 clipId, bool ripple)
{
    TimelineTrack *trackCheck = findTrackForClip(clipId);
    if (!trackCheck || trackCheck->isLocked() || !trackCheck->findClip(clipId)) return false;

    saveState(ripple ? "Eliminar clip con rizado" : "Eliminar clip");

    TimelineTrack *track = findTrackForClip(clipId);
    if (!track || track->isLocked()) return false;
    TimelineClip *clip = track->findClip(clipId);
    if (!clip) return false;

    qint64 delIn = clip->timelineInMs();
    qint64 delDuration = clip->durationMs();
    qint64 linkedId = clip->linkedClipId();

    track->removeClip(clipId);
    emit clipRemoved(clipId);

    // If linked clip exists, unlink it (or keep it as an independent clip)
    if (linkedId > 0) {
        TimelineClip *linked = findClip(linkedId);
        if (linked) {
            linked->setLinkedClipId(-1);
            emit clipModified(linkedId);
        }
    }

    if (ripple) {
        for (TimelineClip &c : track->clips()) {
            if (c.timelineInMs() >= delIn) {
                c.setTimelineInMs(c.timelineInMs() - delDuration);
                c.setTimelineOutMs(c.timelineOutMs() - delDuration);
                emit clipModified(c.id());
            }
        }
        track->sortClips();
    }

    notifyChange();
    return true;
}

qint64 TimelineModel::contentDurationMs() const
{
    qint64 maxMs = 0;
    for (const TimelineTrack &track : m_videoTracks) {
        for (const TimelineClip &c : track.clips()) {
            maxMs = qMax(maxMs, c.timelineOutMs());
        }
    }
    for (const TimelineTrack &track : m_audioTracks) {
        for (const TimelineClip &c : track.clips()) {
            maxMs = qMax(maxMs, c.timelineOutMs());
        }
    }
    return maxMs;
}

qint64 TimelineModel::totalDurationMs() const
{
    if (m_customDurationMs > 0) {
        return m_customDurationMs;
    }
    return qMax<qint64>(10000, contentDurationMs());
}

void TimelineModel::setCustomDurationMs(qint64 durationMs)
{
    qint64 clamped = qMax<qint64>(0, durationMs);
    if (m_customDurationMs != clamped) {
        m_customDurationMs = clamped;
        notifyChange();
    }
}

void TimelineModel::setTrackMuted(qint64 trackId, bool muted)
{
    if (TimelineTrack *t = findTrack(trackId)) {
        t->setMuted(muted);
        notifyChange();
    }
}

void TimelineModel::setTrackSolo(qint64 trackId, bool solo)
{
    if (TimelineTrack *t = findTrack(trackId)) {
        t->setSolo(solo);
        notifyChange();
    }
}

void TimelineModel::setTrackVisible(qint64 trackId, bool visible)
{
    if (TimelineTrack *t = findTrack(trackId)) {
        t->setVisible(visible);
        notifyChange();
    }
}

void TimelineModel::setTrackLocked(qint64 trackId, bool locked)
{
    if (TimelineTrack *t = findTrack(trackId)) {
        t->setLocked(locked);
        notifyChange();
    }
}

void TimelineModel::setTrackVolume(qint64 trackId, double volume)
{
    if (TimelineTrack *t = findTrack(trackId)) {
        t->setVolume(volume);
        notifyChange();
    }
}

void TimelineModel::notifyChange()
{
    emit durationChanged(totalDurationMs());
    emit timelineChanged();
}

bool TimelineModel::renameTrack(qint64 trackId, const QString &newName)
{
    if (TimelineTrack *t = findTrack(trackId)) {
        saveState("Renombrar pista");
        t->setName(newName.trimmed().isEmpty() ? t->name() : newName.trimmed());
        notifyChange();
        return true;
    }
    return false;
}

void TimelineModel::setAspectRatio(ProjectAspectRatio ratio, bool saveUndo)
{
    if (m_aspectRatio == ratio) return;
    if (saveUndo) saveState("Cambiar relación de aspecto");
    m_aspectRatio = ratio;
    emit aspectRatioChanged(m_aspectRatio, canvasSize());
    notifyChange();
}

qint64 TimelineModel::addMarker(qint64 timeMs, const QString &name, const QColor &color, bool saveUndo)
{
    if (saveUndo) saveState("Añadir marcador");
    TimelineMarker marker;
    marker.id = m_nextMarkerId++;
    marker.timeMs = qMax<qint64>(0, timeMs);
    marker.name = name.isEmpty() ? QString("M%1").arg(marker.id) : name;
    marker.color = color;
    m_markers.append(marker);
    std::sort(m_markers.begin(), m_markers.end(), [](const TimelineMarker &a, const TimelineMarker &b) {
        return a.timeMs < b.timeMs;
    });
    emit markersChanged();
    notifyChange();
    return marker.id;
}

bool TimelineModel::removeMarker(qint64 markerId, bool saveUndo)
{
    for (int i = 0; i < m_markers.size(); ++i) {
        if (m_markers[i].id == markerId) {
            if (saveUndo) saveState("Eliminar marcador");
            m_markers.removeAt(i);
            emit markersChanged();
            notifyChange();
            return true;
        }
    }
    return false;
}

TimelineMarker* TimelineModel::findMarker(qint64 markerId)
{
    for (TimelineMarker &m : m_markers) {
        if (m.id == markerId) return &m;
    }
    return nullptr;
}

const TimelineMarker* TimelineModel::findMarker(qint64 markerId) const
{
    for (const TimelineMarker &m : m_markers) {
        if (m.id == markerId) return &m;
    }
    return nullptr;
}

void TimelineModel::clearMarkers(bool saveUndo)
{
    if (m_markers.isEmpty()) return;
    if (saveUndo) saveState("Limpiar marcadores");
    m_markers.clear();
    emit markersChanged();
    notifyChange();
}

void TimelineModel::setMarkers(const QVector<TimelineMarker> &markers)
{
    m_markers = markers;
    std::sort(m_markers.begin(), m_markers.end(), [](const TimelineMarker &a, const TimelineMarker &b) {
        return a.timeMs < b.timeMs;
    });
    for (const auto &m : m_markers) {
        if (m.id >= m_nextMarkerId) m_nextMarkerId = m.id + 1;
    }
    emit markersChanged();
    notifyChange();
}

void TimelineModel::saveState(const QString &description)
{
    TimelineSnapshot snap;
    snap.videoTracks = m_videoTracks;
    snap.audioTracks = m_audioTracks;
    snap.markers = m_markers;
    snap.aspectRatio = m_aspectRatio;
    snap.nextClipId = m_nextClipId;
    snap.nextTrackId = m_nextTrackId;
    snap.nextMarkerId = m_nextMarkerId;
    snap.customDurationMs = m_customDurationMs;
    snap.globalColorAdjustments = m_globalColorAdjustments;
    snap.description = description;

    m_undoStack.append(snap);
    if (m_undoStack.size() > kMaxUndoSteps) {
        m_undoStack.removeFirst();
    }
    m_redoStack.clear();
    emit undoRedoStateChanged();
}

void TimelineModel::undo()
{
    if (m_undoStack.isEmpty()) return;

    // Save current state to redoStack
    TimelineSnapshot current;
    current.videoTracks = m_videoTracks;
    current.audioTracks = m_audioTracks;
    current.markers = m_markers;
    current.aspectRatio = m_aspectRatio;
    current.nextClipId = m_nextClipId;
    current.nextTrackId = m_nextTrackId;
    current.nextMarkerId = m_nextMarkerId;
    current.customDurationMs = m_customDurationMs;
    current.globalColorAdjustments = m_globalColorAdjustments;
    m_redoStack.append(current);

    TimelineSnapshot prev = m_undoStack.takeLast();
    m_videoTracks = prev.videoTracks;
    m_audioTracks = prev.audioTracks;
    m_markers = prev.markers;
    m_aspectRatio = prev.aspectRatio;
    m_nextClipId = prev.nextClipId;
    m_nextTrackId = prev.nextTrackId;
    m_nextMarkerId = prev.nextMarkerId;
    m_customDurationMs = prev.customDurationMs;
    m_globalColorAdjustments = prev.globalColorAdjustments;

    notifyChange();
    emit markersChanged();
    emit aspectRatioChanged(m_aspectRatio, canvasSize());
    emit undoRedoStateChanged();
    emit globalColorAdjustmentsChanged();
}

void TimelineModel::redo()
{
    if (m_redoStack.isEmpty()) return;

    // Save current state to undoStack
    TimelineSnapshot current;
    current.videoTracks = m_videoTracks;
    current.audioTracks = m_audioTracks;
    current.markers = m_markers;
    current.aspectRatio = m_aspectRatio;
    current.nextClipId = m_nextClipId;
    current.nextTrackId = m_nextTrackId;
    current.nextMarkerId = m_nextMarkerId;
    current.customDurationMs = m_customDurationMs;
    current.globalColorAdjustments = m_globalColorAdjustments;
    m_undoStack.append(current);

    TimelineSnapshot next = m_redoStack.takeLast();
    m_videoTracks = next.videoTracks;
    m_audioTracks = next.audioTracks;
    m_markers = next.markers;
    m_aspectRatio = next.aspectRatio;
    m_nextClipId = next.nextClipId;
    m_nextTrackId = next.nextTrackId;
    m_nextMarkerId = next.nextMarkerId;
    m_customDurationMs = next.customDurationMs;
    m_globalColorAdjustments = next.globalColorAdjustments;

    notifyChange();
    emit markersChanged();
    emit aspectRatioChanged(m_aspectRatio, canvasSize());
    emit undoRedoStateChanged();
    emit globalColorAdjustmentsChanged();
}

void TimelineModel::clearHistory()
{
    m_undoStack.clear();
    m_redoStack.clear();
    emit undoRedoStateChanged();
}

QString TimelineModel::undoText() const
{
    if (m_undoStack.isEmpty()) return QString();
    return m_undoStack.last().description;
}

QString TimelineModel::redoText() const
{
    if (m_redoStack.isEmpty()) return QString();
    return m_redoStack.last().description;
}

bool TimelineModel::splitClips(const QList<qint64> &clipIds, qint64 positionMs)
{
    if (clipIds.isEmpty()) return false;
    saveState("Cortar clips seleccionados");

    bool splitAny = false;
    for (qint64 cid : clipIds) {
        TimelineClip *clip = findClip(cid);
        if (!clip) continue;
        if (positionMs > clip->timelineInMs() && positionMs < clip->timelineOutMs()) {
            TimelineTrack *track = findTrackForClip(cid);
            if (!track || track->isLocked()) continue;

            qint64 splitOffsetTimeline = positionMs - clip->timelineInMs();
            qint64 splitOffsetSource = qRound64(splitOffsetTimeline * clip->speed());

            TimelineClip rightSlice = *clip;
            rightSlice.setId(m_nextClipId++);
            rightSlice.setName(QString("%1 (Cut)").arg(clip->name()));
            rightSlice.setTimelineInMs(positionMs);
            rightSlice.setTimelineOutMs(clip->timelineOutMs());
            rightSlice.setSourceInMs(clip->sourceInMs() + splitOffsetSource);
            rightSlice.setSourceOutMs(clip->sourceOutMs());

            clip->setTimelineOutMs(positionMs);
            clip->setSourceOutMs(clip->sourceInMs() + splitOffsetSource);

            qint64 linkedId = clip->linkedClipId();
            if (linkedId > 0) {
                TimelineTrack *linkedTrack = findTrackForClip(linkedId);
                if (linkedTrack && !linkedTrack->isLocked()) {
                    TimelineClip *linkedClip = linkedTrack->findClip(linkedId);
                    if (linkedClip && positionMs > linkedClip->timelineInMs() && positionMs < linkedClip->timelineOutMs()) {
                        qint64 linkedSplitOffset = positionMs - linkedClip->timelineInMs();
                        qint64 linkedSplitSource = qRound64(linkedSplitOffset * linkedClip->speed());

                        TimelineClip rightLinked = *linkedClip;
                        rightLinked.setId(m_nextClipId++);
                        rightLinked.setName(QString("%1 (Cut)").arg(linkedClip->name()));
                        rightLinked.setTimelineInMs(positionMs);
                        rightLinked.setTimelineOutMs(linkedClip->timelineOutMs());
                        rightLinked.setSourceInMs(linkedClip->sourceInMs() + linkedSplitSource);
                        rightLinked.setSourceOutMs(linkedClip->sourceOutMs());

                        linkedClip->setTimelineOutMs(positionMs);
                        linkedClip->setSourceOutMs(linkedClip->sourceInMs() + linkedSplitSource);

                        rightSlice.setLinkedClipId(rightLinked.id());
                        rightLinked.setLinkedClipId(rightSlice.id());

                        linkedTrack->addClip(rightLinked);
                        emit clipAdded(rightLinked.id());
                        emit clipModified(linkedId);
                    }
                }
            } else {
                rightSlice.setLinkedClipId(-1);
            }

            track->addClip(rightSlice);
            emit clipAdded(rightSlice.id());
            emit clipModified(cid);
            splitAny = true;
        }
    }

    if (splitAny) {
        notifyChange();
        return true;
    }
    return false;
}

bool TimelineModel::joinClips(const QList<qint64> &clipIds)
{
    if (clipIds.size() < 2) return false;

    saveState("Unir clips");

    // Group selected clip IDs by track
    QMap<qint64, QVector<qint64>> trackClips;
    for (qint64 cid : clipIds) {
        TimelineTrack *tr = findTrackForClip(cid);
        if (tr && !tr->isLocked()) {
            trackClips[tr->id()].append(cid);
        }
    }

    bool anyJoined = false;

    for (auto it = trackClips.begin(); it != trackClips.end(); ++it) {
        qint64 trId = it.key();
        TimelineTrack *track = findTrack(trId);
        if (!track || it.value().size() < 2) continue;

        QVector<qint64> cids = it.value();
        std::sort(cids.begin(), cids.end(), [this](qint64 a, qint64 b) {
            TimelineClip *ca = findClip(a);
            TimelineClip *cb = findClip(b);
            if (!ca || !cb) return false;
            return ca->timelineInMs() < cb->timelineInMs();
        });

        for (int i = 0; i < cids.size() - 1; ++i) {
            TimelineClip *ca = track->findClip(cids[i]);
            TimelineClip *cb = track->findClip(cids[i + 1]);
            if (!ca || !cb) continue;

            // Scenario 1: Slices of the same media file (healing a split)
            if (ca->filePath() == cb->filePath()) {
                ca->setTimelineOutMs(cb->timelineOutMs());
                ca->setSourceOutMs(cb->sourceOutMs());

                // If both have linked clips, heal the linked clips too
                if (ca->isLinked() && cb->isLinked()) {
                    TimelineClip *linkA = findClip(ca->linkedClipId());
                    TimelineClip *linkB = findClip(cb->linkedClipId());
                    if (linkA && linkB && linkA->filePath() == linkB->filePath()) {
                        linkA->setTimelineOutMs(linkB->timelineOutMs());
                        linkA->setSourceOutMs(linkB->sourceOutMs());
                        TimelineTrack *linkTr = findTrackForClip(linkB->id());
                        if (linkTr) linkTr->removeClip(linkB->id());
                    }
                }

                qint64 removeId = cb->id();
                track->removeClip(removeId);
                emit clipRemoved(removeId);
                emit clipModified(ca->id());
                anyJoined = true;
            } else {
                // Scenario 2: Different files on the same track -> join by snapping adjacent
                if (cb->timelineInMs() > ca->timelineOutMs()) {
                    qint64 shift = cb->timelineInMs() - ca->timelineOutMs();
                    cb->setTimelineInMs(ca->timelineOutMs());
                    cb->setTimelineOutMs(cb->timelineOutMs() - shift);
                    emit clipModified(cb->id());
                    anyJoined = true;
                }
            }
        }
    }

    if (anyJoined) {
        notifyChange();
        return true;
    } else {
        if (!m_undoStack.isEmpty() && m_undoStack.last().description == "Unir clips") {
            m_undoStack.removeLast();
            emit undoRedoStateChanged();
        }
    }
    return false;
}

bool TimelineModel::deleteClips(const QList<qint64> &clipIds, bool ripple)
{
    if (clipIds.isEmpty()) return false;
    saveState("Eliminar clips seleccionados");

    QList<qint64> sorted = clipIds;
    std::sort(sorted.begin(), sorted.end(), [this](qint64 a, qint64 b) {
        TimelineClip *ca = findClip(a);
        TimelineClip *cb = findClip(b);
        if (!ca || !cb) return false;
        return ca->timelineInMs() > cb->timelineInMs();
    });

    for (qint64 cid : sorted) {
        TimelineTrack *track = findTrackForClip(cid);
        if (!track || track->isLocked()) continue;
        TimelineClip *clip = track->findClip(cid);
        if (!clip) continue;

        qint64 delIn = clip->timelineInMs();
        qint64 delDuration = clip->durationMs();
        qint64 linkedId = clip->linkedClipId();

        track->removeClip(cid);
        emit clipRemoved(cid);

        if (linkedId > 0) {
            TimelineClip *linked = findClip(linkedId);
            if (linked) {
                linked->setLinkedClipId(-1);
                emit clipModified(linkedId);
            }
        }

        if (ripple) {
            for (TimelineClip &c : track->clips()) {
                if (c.timelineInMs() >= delIn) {
                    c.setTimelineInMs(c.timelineInMs() - delDuration);
                    c.setTimelineOutMs(c.timelineOutMs() - delDuration);
                    emit clipModified(c.id());
                }
            }
            track->sortClips();
        }
    }

    notifyChange();
    return true;
}

bool TimelineModel::setClipFilter(qint64 clipId, VisualFilter filter)
{
    return setClipsFilter({clipId}, filter);
}

bool TimelineModel::setClipsFilter(const QList<qint64> &clipIds, VisualFilter filter)
{
    if (clipIds.isEmpty()) return false;

    saveState(clipIds.size() > 1 ? "Aplicar efecto a clips" : "Aplicar efecto a clip");

    bool anyModified = false;
    for (qint64 cid : clipIds) {
        TimelineClip *clip = findClip(cid);
        if (clip && clip->filter() != filter) {
            clip->setFilter(filter);
            emit clipModified(cid);
            anyModified = true;
        }
    }

    if (anyModified) {
        notifyChange();
        return true;
    }
    return false;
}

bool TimelineModel::addClipFilter(qint64 clipId, VisualFilter filter)
{
    return addClipsFilter({clipId}, filter);
}

bool TimelineModel::addClipsFilter(const QList<qint64> &clipIds, VisualFilter filter)
{
    if (clipIds.isEmpty() || filter == VisualFilter::None) return false;

    saveState(clipIds.size() > 1 ? "Agregar efecto a clips" : "Agregar efecto a la pila");

    bool anyModified = false;
    for (qint64 cid : clipIds) {
        TimelineClip *clip = findClip(cid);
        if (clip) {
            clip->addFilter(filter);
            emit clipModified(cid);
            anyModified = true;
        }
    }

    if (anyModified) {
        notifyChange();
        return true;
    }
    return false;
}

bool TimelineModel::removeClipFilterAt(qint64 clipId, int index)
{
    const TimelineClip *check = findClip(clipId);
    if (!check || index < 0 || index >= check->filterStack().size()) return false;

    saveState("Eliminar efecto de la pila");
    TimelineClip *clip = findClip(clipId);
    if (!clip) return false;
    clip->removeFilterAt(index);
    emit clipModified(clipId);
    notifyChange();
    return true;
}

bool TimelineModel::moveClipFilter(qint64 clipId, int fromIndex, int toIndex)
{
    const TimelineClip *check = findClip(clipId);
    if (!check || fromIndex < 0 || fromIndex >= check->filterStack().size() ||
        toIndex < 0 || toIndex >= check->filterStack().size() || fromIndex == toIndex) {
        return false;
    }

    saveState("Reordenar jerarquía de efectos");
    TimelineClip *clip = findClip(clipId);
    if (!clip) return false;
    clip->moveFilter(fromIndex, toIndex);
    emit clipModified(clipId);
    notifyChange();
    return true;
}

bool TimelineModel::setClipFilterStack(qint64 clipId, const QVector<VisualFilter> &stack)
{
    return setClipsFilterStack({clipId}, stack);
}

bool TimelineModel::setClipsFilterStack(const QList<qint64> &clipIds, const QVector<VisualFilter> &stack)
{
    if (clipIds.isEmpty()) return false;

    saveState(clipIds.size() > 1 ? "Modificar pila de efectos de clips" : "Modificar pila de efectos");

    bool anyModified = false;
    for (qint64 cid : clipIds) {
        TimelineClip *clip = findClip(cid);
        if (clip) {
            clip->setFilterStack(stack);
            emit clipModified(cid);
            anyModified = true;
        }
    }

    if (anyModified) {
        notifyChange();
        return true;
    }
    return false;
}

bool TimelineModel::clearClipFilters(qint64 clipId)
{
    return clearClipsFilters({clipId});
}

bool TimelineModel::clearClipsFilters(const QList<qint64> &clipIds)
{
    if (clipIds.isEmpty()) return false;

    saveState(clipIds.size() > 1 ? "Limpiar efectos de clips" : "Limpiar efectos de la pila");

    bool anyModified = false;
    for (qint64 cid : clipIds) {
        TimelineClip *clip = findClip(cid);
        if (clip && clip->hasFilters()) {
            clip->clearFilters();
            emit clipModified(cid);
            anyModified = true;
        }
    }

    if (anyModified) {
        notifyChange();
        return true;
    }
    return false;
}

bool TimelineModel::setClipMotionPath(qint64 clipId, const MotionPath &path)
{
    if (!findClip(clipId)) return false;
    saveState("Modificar ruta de desplazamiento");
    TimelineClip *clip = findClip(clipId);
    if (!clip) return false;
    clip->setMotionPath(path);
    emit clipModified(clipId);
    notifyChange();
    return true;
}

bool TimelineModel::setClipMotionPreset(qint64 clipId, MotionPreset preset)
{
    const TimelineClip *existingClip = findClip(clipId);
    if (!existingClip) return false;
    QPointF basePos(existingClip->posX(), existingClip->posY());
    MotionPath path = existingClip->motionPath();
    path.setPreset(preset, basePos);

    saveState("Cambiar preset de desplazamiento");
    TimelineClip *clip = findClip(clipId);
    if (!clip) return false;
    clip->setMotionPath(path);
    emit clipModified(clipId);
    notifyChange();
    return true;
}

bool TimelineModel::setClipMotionStartPoint(qint64 clipId, const QPointF &pos)
{
    const TimelineClip *existingClip = findClip(clipId);
    if (!existingClip) return false;
    MotionPath path = existingClip->motionPath();
    path.setStartPoint(pos);

    saveState("Mover punto de inicio");
    TimelineClip *clip = findClip(clipId);
    if (!clip) return false;
    clip->setMotionPath(path);
    emit clipModified(clipId);
    notifyChange();
    return true;
}

bool TimelineModel::setClipMotionEndPoint(qint64 clipId, const QPointF &pos)
{
    const TimelineClip *existingClip = findClip(clipId);
    if (!existingClip) return false;
    MotionPath path = existingClip->motionPath();
    path.setEndPoint(pos);

    saveState("Mover punto de fin");
    TimelineClip *clip = findClip(clipId);
    if (!clip) return false;
    clip->setMotionPath(path);
    emit clipModified(clipId);
    notifyChange();
    return true;
}

int TimelineModel::trackZIndex(qint64 trackId) const
{
    for (int i = 0; i < m_videoTracks.size(); ++i) {
        if (m_videoTracks[i].id() == trackId) {
            return m_videoTracks.size() - i; // Pista base al fondo de la lista tiene Z-Index 1
        }
    }
    return 0; // No es una pista de video
}

int TimelineModel::clipZIndex(qint64 clipId) const
{
    const TimelineTrack *t = findTrackForClip(clipId);
    if (!t) return 0;
    return trackZIndex(t->id());
}

int TimelineModel::totalVideoZLevels() const
{
    return m_videoTracks.size();
}

bool TimelineModel::setClipZIndex(qint64 clipId, int targetZIndex)
{
    TimelineClip *clip = findClip(clipId);
    if (!clip || clip->type() == ClipType::Audio) return false;

    targetZIndex = qMax(1, targetZIndex);

    // Si targetZIndex supera el número de pistas de video existentes, añadir pistas hasta alcanzarlo
    while (targetZIndex > m_videoTracks.size()) {
        addTrack(TrackType::Video); // prepends new top track
    }

    int targetTrackIdx = m_videoTracks.size() - targetZIndex;
    if (targetTrackIdx < 0 || targetTrackIdx >= m_videoTracks.size()) {
        return false;
    }

    qint64 targetTrackId = m_videoTracks[targetTrackIdx].id();
    TimelineTrack *currTrack = findTrackForClip(clipId);
    if (currTrack && currTrack->id() == targetTrackId) {
        return true; // Ya se encuentra en dicho Z-Index
    }

    TimelineTrack *targetTrack = findTrack(targetTrackId);
    if (!targetTrack) return false;

    saveState("Cambiar Z-Index de elemento");

    return moveClip(clipId, targetTrackId, clip->timelineInMs());
}

void TimelineModel::setGlobalColorAdjustments(const ColorAdjustments &adj, bool saveUndo)
{
    if (m_globalColorAdjustments == adj) return;
    if (saveUndo) {
        saveState("Ajustes generales de color");
    }
    m_globalColorAdjustments = adj;
    emit globalColorAdjustmentsChanged();
    notifyChange();
}

void TimelineModel::setGlobalBrightness(int b, bool saveUndo)
{
    int clamped = qBound(-100, b, 100);
    if (m_globalColorAdjustments.brightness == clamped) return;
    if (saveUndo) {
        saveState("Cambiar brillo general del video");
    }
    m_globalColorAdjustments.brightness = clamped;
    emit globalColorAdjustmentsChanged();
    notifyChange();
}

void TimelineModel::setGlobalLuminosity(int l, bool saveUndo)
{
    int clamped = qBound(-100, l, 100);
    if (m_globalColorAdjustments.luminosity == clamped) return;
    if (saveUndo) {
        saveState("Cambiar luminosidad general del video");
    }
    m_globalColorAdjustments.luminosity = clamped;
    emit globalColorAdjustmentsChanged();
    notifyChange();
}

void TimelineModel::setGlobalRedPresence(int r, bool saveUndo)
{
    int clamped = qBound(-100, r, 100);
    if (m_globalColorAdjustments.red == clamped) return;
    if (saveUndo) {
        saveState("Cambiar presencia de color rojo general");
    }
    m_globalColorAdjustments.red = clamped;
    emit globalColorAdjustmentsChanged();
    notifyChange();
}

void TimelineModel::setGlobalGreenPresence(int g, bool saveUndo)
{
    int clamped = qBound(-100, g, 100);
    if (m_globalColorAdjustments.green == clamped) return;
    if (saveUndo) {
        saveState("Cambiar presencia de color verde general");
    }
    m_globalColorAdjustments.green = clamped;
    emit globalColorAdjustmentsChanged();
    notifyChange();
}

void TimelineModel::setGlobalBluePresence(int b, bool saveUndo)
{
    int clamped = qBound(-100, b, 100);
    if (m_globalColorAdjustments.blue == clamped) return;
    if (saveUndo) {
        saveState("Cambiar presencia de color azul general");
    }
    m_globalColorAdjustments.blue = clamped;
    emit globalColorAdjustmentsChanged();
    notifyChange();
}

void TimelineModel::resetGlobalColorAdjustments(bool saveUndo)
{
    if (m_globalColorAdjustments.isIdentity()) return;
    if (saveUndo) {
        saveState("Restablecer ajustes generales de color");
    }
    m_globalColorAdjustments = ColorAdjustments();
    emit globalColorAdjustmentsChanged();
    notifyChange();
}

bool TimelineModel::setClipColorAdjustments(qint64 clipId, const ColorAdjustments &adj, bool saveUndo)
{
    return setClipsColorAdjustments({clipId}, adj, saveUndo);
}

bool TimelineModel::setClipsColorAdjustments(const QList<qint64> &clipIds, const ColorAdjustments &adj, bool saveUndo)
{
    if (clipIds.isEmpty()) return false;
    if (saveUndo) {
        saveState(clipIds.size() > 1 ? "Ajustar color de clips seleccionados" : "Ajustar color del clip");
    }

    bool anyModified = false;
    for (qint64 cid : clipIds) {
        TimelineClip *clip = findClip(cid);
        if (clip && clip->colorAdjustments() != adj) {
            clip->setColorAdjustments(adj);
            emit clipModified(cid);
            anyModified = true;
        }
    }

    if (anyModified) {
        notifyChange();
        return true;
    }
    return false;
}

bool TimelineModel::setClipBrightness(qint64 clipId, int b, bool saveUndo)
{
    TimelineClip *clip = findClip(clipId);
    if (!clip) return false;
    ColorAdjustments adj = clip->colorAdjustments();
    adj.brightness = qBound(-100, b, 100);
    return setClipColorAdjustments(clipId, adj, saveUndo);
}

bool TimelineModel::setClipLuminosity(qint64 clipId, int l, bool saveUndo)
{
    TimelineClip *clip = findClip(clipId);
    if (!clip) return false;
    ColorAdjustments adj = clip->colorAdjustments();
    adj.luminosity = qBound(-100, l, 100);
    return setClipColorAdjustments(clipId, adj, saveUndo);
}

bool TimelineModel::setClipRedPresence(qint64 clipId, int r, bool saveUndo)
{
    TimelineClip *clip = findClip(clipId);
    if (!clip) return false;
    ColorAdjustments adj = clip->colorAdjustments();
    adj.red = qBound(-100, r, 100);
    return setClipColorAdjustments(clipId, adj, saveUndo);
}

bool TimelineModel::setClipGreenPresence(qint64 clipId, int g, bool saveUndo)
{
    TimelineClip *clip = findClip(clipId);
    if (!clip) return false;
    ColorAdjustments adj = clip->colorAdjustments();
    adj.green = qBound(-100, g, 100);
    return setClipColorAdjustments(clipId, adj, saveUndo);
}

bool TimelineModel::setClipBluePresence(qint64 clipId, int b, bool saveUndo)
{
    TimelineClip *clip = findClip(clipId);
    if (!clip) return false;
    ColorAdjustments adj = clip->colorAdjustments();
    adj.blue = qBound(-100, b, 100);
    return setClipColorAdjustments(clipId, adj, saveUndo);
}

void TimelineModel::setGlobalColorGradeMode(ColorGradeMode mode, bool saveUndo)
{
    if (m_globalColorAdjustments.mode == mode) return;
    if (saveUndo) {
        saveState("Cambiar modo de ajuste de color general");
    }
    m_globalColorAdjustments.mode = mode;
    emit globalColorAdjustmentsChanged();
    notifyChange();
}

void TimelineModel::setGlobalLumaCurve(const ColorCurve &curve, bool saveUndo)
{
    if (m_globalColorAdjustments.lumaCurve == curve) return;
    if (saveUndo) {
        saveState("Cambiar curva de brillo/luminosidad general");
    }
    m_globalColorAdjustments.lumaCurve = curve;
    emit globalColorAdjustmentsChanged();
    notifyChange();
}

void TimelineModel::setGlobalColorCurve(const ColorCurve &curve, bool saveUndo)
{
    if (m_globalColorAdjustments.colorCurve == curve) return;
    if (saveUndo) {
        saveState("Cambiar curva de espectro de color general");
    }
    m_globalColorAdjustments.colorCurve = curve;
    emit globalColorAdjustmentsChanged();
    notifyChange();
}

void TimelineModel::setGlobalRedCurve(const ColorCurve &curve, bool saveUndo)
{
    if (m_globalColorAdjustments.redCurve == curve) return;
    if (saveUndo) {
        saveState("Cambiar curva roja general");
    }
    m_globalColorAdjustments.redCurve = curve;
    emit globalColorAdjustmentsChanged();
    notifyChange();
}

void TimelineModel::setGlobalGreenCurve(const ColorCurve &curve, bool saveUndo)
{
    if (m_globalColorAdjustments.greenCurve == curve) return;
    if (saveUndo) {
        saveState("Cambiar curva verde general");
    }
    m_globalColorAdjustments.greenCurve = curve;
    emit globalColorAdjustmentsChanged();
    notifyChange();
}

void TimelineModel::setGlobalBlueCurve(const ColorCurve &curve, bool saveUndo)
{
    if (m_globalColorAdjustments.blueCurve == curve) return;
    if (saveUndo) {
        saveState("Cambiar curva azul general");
    }
    m_globalColorAdjustments.blueCurve = curve;
    emit globalColorAdjustmentsChanged();
    notifyChange();
}

bool TimelineModel::setClipColorGradeMode(qint64 clipId, ColorGradeMode mode, bool saveUndo)
{
    TimelineClip *clip = findClip(clipId);
    if (!clip) return false;
    ColorAdjustments adj = clip->colorAdjustments();
    adj.mode = mode;
    return setClipColorAdjustments(clipId, adj, saveUndo);
}

bool TimelineModel::setClipLumaCurve(qint64 clipId, const ColorCurve &curve, bool saveUndo)
{
    TimelineClip *clip = findClip(clipId);
    if (!clip) return false;
    ColorAdjustments adj = clip->colorAdjustments();
    adj.lumaCurve = curve;
    return setClipColorAdjustments(clipId, adj, saveUndo);
}

bool TimelineModel::setClipColorCurve(qint64 clipId, const ColorCurve &curve, bool saveUndo)
{
    TimelineClip *clip = findClip(clipId);
    if (!clip) return false;
    ColorAdjustments adj = clip->colorAdjustments();
    adj.colorCurve = curve;
    return setClipColorAdjustments(clipId, adj, saveUndo);
}

bool TimelineModel::setClipRedCurve(qint64 clipId, const ColorCurve &curve, bool saveUndo)
{
    TimelineClip *clip = findClip(clipId);
    if (!clip) return false;
    ColorAdjustments adj = clip->colorAdjustments();
    adj.redCurve = curve;
    return setClipColorAdjustments(clipId, adj, saveUndo);
}

bool TimelineModel::setClipGreenCurve(qint64 clipId, const ColorCurve &curve, bool saveUndo)
{
    TimelineClip *clip = findClip(clipId);
    if (!clip) return false;
    ColorAdjustments adj = clip->colorAdjustments();
    adj.greenCurve = curve;
    return setClipColorAdjustments(clipId, adj, saveUndo);
}

bool TimelineModel::setClipBlueCurve(qint64 clipId, const ColorCurve &curve, bool saveUndo)
{
    TimelineClip *clip = findClip(clipId);
    if (!clip) return false;
    ColorAdjustments adj = clip->colorAdjustments();
    adj.blueCurve = curve;
    return setClipColorAdjustments(clipId, adj, saveUndo);
}

bool TimelineModel::resetClipColorAdjustments(qint64 clipId, bool saveUndo)
{
    TimelineClip *clip = findClip(clipId);
    if (!clip) return false;
    return setClipColorAdjustments(clipId, ColorAdjustments(), saveUndo);
}

void TimelineModel::setGlobalTimeOfDay(bool enabled, float val, bool saveUndo)
{
    float clamped = qBound(0.0f, val, 1.0f);
    if (m_globalColorAdjustments.timeOfDayEnabled == enabled &&
        qAbs(m_globalColorAdjustments.timeOfDay - clamped) < 0.001f) {
        return;
    }
    if (saveUndo) {
        saveState("Cambiar hora del día general");
    }
    m_globalColorAdjustments.timeOfDayEnabled = enabled;
    m_globalColorAdjustments.timeOfDay = clamped;
    emit globalColorAdjustmentsChanged();
    notifyChange();
}

bool TimelineModel::setClipTimeOfDay(qint64 clipId, bool enabled, float val, bool saveUndo)
{
    return setClipsTimeOfDay({clipId}, enabled, val, saveUndo);
}

bool TimelineModel::setClipsTimeOfDay(const QList<qint64> &clipIds, bool enabled, float val, bool saveUndo)
{
    if (clipIds.isEmpty()) return false;
    float clamped = qBound(0.0f, val, 1.0f);
    if (saveUndo) {
        saveState(clipIds.size() > 1 ? "Ajustar hora del día de clips seleccionados" : "Ajustar hora del día del clip");
    }

    bool anyModified = false;
    for (qint64 cid : clipIds) {
        TimelineClip *clip = findClip(cid);
        if (clip) {
            ColorAdjustments adj = clip->colorAdjustments();
            if (adj.timeOfDayEnabled != enabled || qAbs(adj.timeOfDay - clamped) >= 0.001f) {
                adj.timeOfDayEnabled = enabled;
                adj.timeOfDay = clamped;
                clip->setColorAdjustments(adj);
                emit clipModified(cid);
                anyModified = true;
            }
        }
    }

    if (anyModified) {
        notifyChange();
        return true;
    }
    return false;
}



