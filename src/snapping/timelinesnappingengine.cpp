#include "timelinesnappingengine.h"
#include "core/timelinemodel.h"
#include <cmath>
#include <algorithm>
#include <vector>

namespace Snapping {

qint64 TimelineSnappingEngine::snapToFrame(qint64 timeMs, double fps)
{
    if (fps <= 0.0) return timeMs;
    double frameDurationMs = 1000.0 / fps;
    double frame = std::round(static_cast<double>(timeMs) / frameDurationMs);
    return static_cast<qint64>(std::round(frame * frameDurationMs));
}

TimelineSnapResult TimelineSnappingEngine::snapTime(
    qint64 desiredTimeMs,
    const TimelineModel *model,
    const TimelineSnapSettings &settings,
    double pixelsPerSecond,
    qint64 ignoreClipId,
    qint64 playheadPositionMs)
{
    TimelineSnapResult result;
    result.timeMs = desiredTimeMs;
    result.snapped = false;
    result.type = TimelineSnapType::None;

    if (!settings.enabled) {
        if (settings.frameSnap) {
            result.timeMs = snapToFrame(desiredTimeMs, settings.fps);
        }
        return result;
    }

    double pps = (pixelsPerSecond > 0.1) ? pixelsPerSecond : 50.0;
    qint64 snapThresholdMs = static_cast<qint64>((settings.thresholdPixels / pps) * 1000.0);
    if (snapThresholdMs < 1) snapThresholdMs = 1;

    std::vector<TimelineSnapCandidate> candidates;

    // 1. Playhead
    if (settings.playhead) {
        qint64 dist = std::abs(desiredTimeMs - playheadPositionMs);
        if (dist <= snapThresholdMs) {
            candidates.push_back({playheadPositionMs, dist, 1, TimelineSnapType::Playhead});
        }
    }

    // 2. Timeline Start (0 ms)
    {
        qint64 dist = std::abs(desiredTimeMs - 0);
        if (dist <= snapThresholdMs) {
            candidates.push_back({0, dist, 2, TimelineSnapType::ClipStart});
        }
    }

    // 3. Clip Edges & Cuts on all tracks
    if (model && (settings.clipEdges || settings.otherTrackCuts)) {
        const TimelineClip *ignClip = model->findClip(ignoreClipId);
        qint64 linkedIgnoreId = (ignClip && ignClip->isLinked()) ? ignClip->linkedClipId() : -1;

        auto checkTracks = [&](const QVector<TimelineTrack> &tracks) {
            for (const TimelineTrack &track : tracks) {
                for (const TimelineClip &clip : track.clips()) {
                    if (clip.id() == ignoreClipId || clip.id() == linkedIgnoreId) continue;

                    // In point
                    qint64 inMs = clip.timelineInMs();
                    qint64 distIn = std::abs(desiredTimeMs - inMs);
                    if (distIn <= snapThresholdMs) {
                        candidates.push_back({inMs, distIn, 3, TimelineSnapType::ClipStart});
                    }

                    // Out point
                    qint64 outMs = clip.timelineOutMs();
                    qint64 distOut = std::abs(desiredTimeMs - outMs);
                    if (distOut <= snapThresholdMs) {
                        candidates.push_back({outMs, distOut, 3, TimelineSnapType::ClipEnd});
                    }
                }
            }
        };

        checkTracks(model->videoTracks());
        checkTracks(model->audioTracks());
    }

    if (!candidates.empty()) {
        auto best = candidates[0];
        for (size_t i = 1; i < candidates.size(); ++i) {
            if (candidates[i].distanceMs < best.distanceMs) {
                best = candidates[i];
            } else if (candidates[i].distanceMs == best.distanceMs && candidates[i].priority < best.priority) {
                best = candidates[i];
            }
        }

        result.timeMs = best.timeMs;
        result.snapped = true;
        result.type = best.type;
        return result;
    }

    if (settings.frameSnap) {
        result.timeMs = snapToFrame(desiredTimeMs, settings.fps);
        result.snapped = false;
        result.type = TimelineSnapType::Frame;
    }

    return result;
}

} // namespace Snapping
