#pragma once

#include "snappingtypes.h"
#include <QVector>
#include <QtGlobal>

class TimelineModel;

namespace Snapping {

class TimelineSnappingEngine {
public:
    TimelineSnappingEngine() = default;

    TimelineSnapResult snapTime(
        qint64 desiredTimeMs,
        const TimelineModel *model,
        const TimelineSnapSettings &settings,
        double pixelsPerSecond,
        qint64 ignoreClipId = -1,
        qint64 playheadPositionMs = 0
    );

    static qint64 snapToFrame(qint64 timeMs, double fps);
};

} // namespace Snapping
