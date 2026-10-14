#pragma once

#include <QImage>
#include <QRectF>
#include <QPointF>
#include <QVector>
#include "core/keyframe.h"

struct TrackedPoint {
    qint64 timeMs = 0;
    QPointF position; // In canvas coordinates (center relative)
    QRectF boundingBox;
    float confidence = 1.0f;
};

class MotionTracker {
public:
    MotionTracker();

    void setInitialRegion(const QRectF &rect) { m_initialRegion = rect; }
    QRectF initialRegion() const { return m_initialRegion; }

    void setSearchWindowRadius(int radiusPixels) { m_searchRadius = std::max(4, radiusPixels); }
    int searchWindowRadius() const { return m_searchRadius; }

    // Tracks target from templateImage across targetFrame
    TrackedPoint trackStep(const QImage &templateImage,
                           const QRectF &templateRoi,
                           const QImage &targetFrame,
                           qint64 targetTimeMs);

    // Track a series of frames
    QVector<TrackedPoint> trackSequence(const QVector<QPair<qint64, QImage>> &frames,
                                        const QRectF &initialRoi);

    // Converts tracked points into standard position Keyframes
    QVector<Keyframe<QPointF>> toPositionKeyframes(const QVector<TrackedPoint> &points,
                                                  const QSize &canvasSize) const;

private:
    QRectF m_initialRegion;
    int m_searchRadius = 32;
};
