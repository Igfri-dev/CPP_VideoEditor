#pragma once

#include <QImage>
#include <QSize>
#include "../core/timelinemodel.h"

class VideoCompositor {
public:
    static QImage renderFrame(TimelineModel *model, qint64 timelineMs, const QSize &canvasSize = QSize(1920, 1080), bool forPreview = false);
    static QImage applyFilter(const QImage &source, VisualFilter filter);
    static QImage applyFilters(const QImage &source, const QVector<VisualFilter> &filters);
    static QImage applyColorAdjustments(const QImage &source, const ColorAdjustments &adj);
    static QImage applyTimeOfDay(const QImage &source, const ColorAdjustments &adj);
    static QImage applyTimeOfDay(const QImage &source, float sliderValue);
    static QImage applyMask(const QImage &source, const ClipMask &mask, qint64 timelineOffsetMs = 0);
};
