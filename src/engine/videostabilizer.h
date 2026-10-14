#pragma once

#include <QVector>
#include <QImage>
#include <QJsonObject>
#include <QPair>

struct StabilizationTransform {
    double dx = 0.0;     // Corrective X translation in pixels
    double dy = 0.0;     // Corrective Y translation in pixels
    double dAngle = 0.0; // Corrective rotation in degrees
    double scale = 1.0;  // Crop factor compensation scale
};

struct StabilizationSettings {
    bool enabled = false;
    double smoothness = 0.60;   // Filtering window strength [0.0..1.0]
    double cropFactor = 1.08;   // Auto-zoom to hide boundary shifts [1.0..1.3]
    bool smoothRotation = true;

    bool isIdentity() const {
        return !enabled;
    }

    bool operator==(const StabilizationSettings &other) const {
        return enabled == other.enabled &&
               std::abs(smoothness - other.smoothness) < 1e-4 &&
               std::abs(cropFactor - other.cropFactor) < 1e-4 &&
               smoothRotation == other.smoothRotation;
    }
    bool operator!=(const StabilizationSettings &other) const { return !(*this == other); }

    QJsonObject toJson() const;
    static StabilizationSettings fromJson(const QJsonObject &obj);
};

class VideoStabilizer {
public:
    VideoStabilizer();

    // Analyzes inter-frame optical motion vector between two successive frames
    static StabilizationTransform estimateMotionDelta(const QImage &prevFrame, const QImage &curFrame);

    // Analyzes a sequence of frames and computes smoothed corrective stabilization offsets
    static QVector<StabilizationTransform> computeStabilizationTransforms(
        const QVector<QImage> &frames,
        const StabilizationSettings &settings);

    // Applies stabilization transform and cropping to a single frame
    static QImage applyStabilization(const QImage &source,
                                     const StabilizationTransform &transform);
};
