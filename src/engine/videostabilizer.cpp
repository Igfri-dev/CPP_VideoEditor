#include "videostabilizer.h"
#include <QPainter>
#include <QtMath>
#include <algorithm>
#include <cmath>

QJsonObject StabilizationSettings::toJson() const
{
    QJsonObject obj;
    obj["enabled"] = enabled;
    obj["smoothness"] = smoothness;
    obj["cropFactor"] = cropFactor;
    obj["smoothRotation"] = smoothRotation;
    return obj;
}

StabilizationSettings StabilizationSettings::fromJson(const QJsonObject &obj)
{
    StabilizationSettings s;
    if (obj.contains("enabled")) s.enabled = obj["enabled"].toBool();
    if (obj.contains("smoothness")) s.smoothness = obj["smoothness"].toDouble(0.60);
    if (obj.contains("cropFactor")) s.cropFactor = obj["cropFactor"].toDouble(1.08);
    if (obj.contains("smoothRotation")) s.smoothRotation = obj["smoothRotation"].toBool(true);
    return s;
}

VideoStabilizer::VideoStabilizer()
{
}

StabilizationTransform VideoStabilizer::estimateMotionDelta(const QImage &prevFrame, const QImage &curFrame)
{
    StabilizationTransform delta;
    if (prevFrame.isNull() || curFrame.isNull()) return delta;

    int w = prevFrame.width();
    int h = prevFrame.height();
    if (w <= 16 || h <= 16) return delta;

    // Multi-point optical block matching across center and 4 quadrants
    const int searchRange = 16;
    const int blockSize = 32;

    int samplePointsX[5] = { w / 2, w / 4, (3 * w) / 4, w / 4, (3 * w) / 4 };
    int samplePointsY[5] = { h / 2, h / 4, h / 4, (3 * h) / 4, (3 * h) / 4 };

    double totalDx = 0.0;
    double totalDy = 0.0;
    int validBlocks = 0;

    for (int pt = 0; pt < 5; ++pt) {
        int cx = samplePointsX[pt];
        int cy = samplePointsY[pt];

        // Check if sample block has sufficient texture / contrast
        int minG = 255, maxG = 0;
        for (int by = -blockSize / 2; by <= blockSize / 2; by += 4) {
            int y1 = cy + by;
            if (y1 < 0 || y1 >= h) continue;
            const QRgb *line1 = reinterpret_cast<const QRgb*>(prevFrame.constScanLine(y1));
            for (int bx = -blockSize / 2; bx <= blockSize / 2; bx += 4) {
                int x1 = cx + bx;
                if (x1 < 0 || x1 >= w) continue;
                int g = qGray(line1[x1]);
                minG = std::min(minG, g);
                maxG = std::max(maxG, g);
            }
        }
        if (maxG - minG < 15) {
            continue; // Ignore flat / untextured blocks
        }

        int bestShiftX = 0;
        int bestShiftY = 0;
        double bestSsd = 1e15;

        for (int dy = -searchRange; dy <= searchRange; dy += 2) {
            for (int dx = -searchRange; dx <= searchRange; dx += 2) {
                double ssd = 0.0;
                int count = 0;

                for (int by = -blockSize / 2; by <= blockSize / 2; by += 4) {
                    int y1 = cy + by;
                    int y2 = cy + by + dy;
                    if (y1 < 0 || y1 >= h || y2 < 0 || y2 >= h) continue;

                    const QRgb *line1 = reinterpret_cast<const QRgb*>(prevFrame.constScanLine(y1));
                    const QRgb *line2 = reinterpret_cast<const QRgb*>(curFrame.constScanLine(y2));

                    for (int bx = -blockSize / 2; bx <= blockSize / 2; bx += 4) {
                        int x1 = cx + bx;
                        int x2 = cx + bx + dx;
                        if (x1 < 0 || x1 >= w || x2 < 0 || x2 >= w) continue;

                        int diff = qGray(line1[x1]) - qGray(line2[x2]);
                        ssd += (diff * diff);
                        count++;
                    }
                }

                if (count > 0 && ssd < bestSsd) {
                    bestSsd = ssd;
                    bestShiftX = dx;
                    bestShiftY = dy;
                }
            }
        }

        totalDx += bestShiftX;
        totalDy += bestShiftY;
        validBlocks++;
    }

    if (validBlocks > 0) {
        delta.dx = totalDx / validBlocks;
        delta.dy = totalDy / validBlocks;
    }

    return delta;
}

QVector<StabilizationTransform> VideoStabilizer::computeStabilizationTransforms(
    const QVector<QImage> &frames,
    const StabilizationSettings &settings)
{
    QVector<StabilizationTransform> transforms;
    if (frames.isEmpty()) return transforms;

    int n = frames.size();
    transforms.resize(n);

    // 1. Compute frame-to-frame displacement deltas
    QVector<double> cumX(n, 0.0);
    QVector<double> cumY(n, 0.0);

    for (int i = 1; i < n; ++i) {
        StabilizationTransform delta = estimateMotionDelta(frames[i - 1], frames[i]);
        cumX[i] = cumX[i - 1] + delta.dx;
        cumY[i] = cumY[i - 1] + delta.dy;
    }

    // 2. Temporal smoothing window (Gaussian / Moving Average)
    int radius = std::clamp(static_cast<int>(settings.smoothness * 15.0), 1, 20);

    for (int i = 0; i < n; ++i) {
        double sumX = 0.0;
        double sumY = 0.0;
        double weightSum = 0.0;

        for (int k = -radius; k <= radius; ++k) {
            int idx = std::clamp(i + k, 0, n - 1);
            double w = 1.0 - (std::abs(k) / static_cast<double>(radius + 1));
            sumX += cumX[idx] * w;
            sumY += cumY[idx] * w;
            weightSum += w;
        }

        double smoothX = sumX / weightSum;
        double smoothY = sumY / weightSum;

        // Corrective compensation vector
        transforms[i].dx = smoothX - cumX[i];
        transforms[i].dy = smoothY - cumY[i];
        transforms[i].scale = settings.cropFactor;
    }

    return transforms;
}

QImage VideoStabilizer::applyStabilization(const QImage &source,
                                          const StabilizationTransform &transform)
{
    if (source.isNull()) return source;

    QImage result(source.size(), source.format());
    result.fill(Qt::black);

    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // Center transform
    double cx = source.width() / 2.0;
    double cy = source.height() / 2.0;

    painter.translate(cx, cy);
    if (transform.scale > 1.0) {
        painter.scale(transform.scale, transform.scale);
    }
    if (std::abs(transform.dAngle) > 0.001) {
        painter.rotate(transform.dAngle);
    }
    painter.translate(-cx + transform.dx, -cy + transform.dy);

    painter.drawImage(0, 0, source);
    return result;
}
