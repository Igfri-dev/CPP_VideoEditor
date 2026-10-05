#pragma once

#include <QImage>
#include <QSize>
#include "../core/timelinemodel.h"

class VideoCompositor {
public:
    static QImage renderFrame(TimelineModel *model, qint64 timelineMs, const QSize &canvasSize = QSize(1920, 1080));
    static QImage applyFilter(const QImage &source, VisualFilter filter);
    static QImage applyFilters(const QImage &source, const QVector<VisualFilter> &filters);
    static QImage applyColorAdjustments(const QImage &source, const ColorAdjustments &adj);
};
