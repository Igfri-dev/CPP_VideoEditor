#include "motiontracker.h"
#include <QtMath>
#include <algorithm>
#include <cmath>

MotionTracker::MotionTracker()
{
}

TrackedPoint MotionTracker::trackStep(const QImage &templateImage,
                                      const QRectF &templateRoi,
                                      const QImage &targetFrame,
                                      qint64 targetTimeMs)
{
    TrackedPoint result;
    result.timeMs = targetTimeMs;

    if (templateImage.isNull() || targetFrame.isNull() || templateRoi.width() <= 2 || templateRoi.height() <= 2) {
        result.position = templateRoi.center();
        result.boundingBox = templateRoi;
        result.confidence = 0.0f;
        return result;
    }

    int tX = qBound(0, static_cast<int>(templateRoi.x()), templateImage.width() - 2);
    int tY = qBound(0, static_cast<int>(templateRoi.y()), templateImage.height() - 2);
    int tW = qBound(2, static_cast<int>(templateRoi.width()), templateImage.width() - tX);
    int tH = qBound(2, static_cast<int>(templateRoi.height()), templateImage.height() - tY);

    // Search window centered around template ROI center
    int centerX = tX + tW / 2;
    int centerY = tY + tH / 2;

    int minSearchX = std::max(0, centerX - m_searchRadius);
    int maxSearchX = std::min(targetFrame.width() - tW, centerX + m_searchRadius);
    int minSearchY = std::max(0, centerY - m_searchRadius);
    int maxSearchY = std::min(targetFrame.height() - tH, centerY + m_searchRadius);

    int bestX = tX;
    int bestY = tY;
    double bestScore = 1e18; // Minimize Sum of Squared Differences (SSD)

    const int stepSampling = std::max(1, (tW * tH) / 600);

    for (int sy = minSearchY; sy <= maxSearchY; sy += 2) {
        for (int sx = minSearchX; sx <= maxSearchX; sx += 2) {
            double currentSsd = 0.0;
            int samples = 0;

            for (int dy = 0; dy < tH; dy += 2) {
                const QRgb *tmplLine = reinterpret_cast<const QRgb*>(templateImage.constScanLine(tY + dy));
                const QRgb *tgtLine = reinterpret_cast<const QRgb*>(targetFrame.constScanLine(sy + dy));

                for (int dx = 0; dx < tW; dx += 2) {
                    QRgb p1 = tmplLine[tX + dx];
                    QRgb p2 = tgtLine[sx + dx];

                    int dr = qRed(p1) - qRed(p2);
                    int dg = qGreen(p1) - qGreen(p2);
                    int db = qBlue(p1) - qBlue(p2);

                    currentSsd += (dr * dr + dg * dg + db * db);
                    samples++;
                }
            }

            if (samples > 0) {
                currentSsd /= samples;
            }

            if (currentSsd < bestScore) {
                bestScore = currentSsd;
                bestX = sx;
                bestY = sy;
            }
        }
    }

    result.boundingBox = QRectF(bestX, bestY, tW, tH);
    result.position = result.boundingBox.center();
    result.confidence = std::clamp(1.0f - static_cast<float>(bestScore / (255.0 * 255.0 * 3.0)), 0.0f, 1.0f);

    return result;
}

QVector<TrackedPoint> MotionTracker::trackSequence(const QVector<QPair<qint64, QImage>> &frames,
                                                   const QRectF &initialRoi)
{
    QVector<TrackedPoint> points;
    if (frames.isEmpty()) return points;

    TrackedPoint p0;
    p0.timeMs = frames[0].first;
    p0.boundingBox = initialRoi;
    p0.position = initialRoi.center();
    p0.confidence = 1.0f;
    points.append(p0);

    QRectF curRoi = initialRoi;
    QImage prevImage = frames[0].second;

    for (int i = 1; i < frames.size(); ++i) {
        TrackedPoint nextP = trackStep(prevImage, curRoi, frames[i].second, frames[i].first);
        points.append(nextP);
        curRoi = nextP.boundingBox;
        prevImage = frames[i].second;
    }

    return points;
}

QVector<Keyframe<QPointF>> MotionTracker::toPositionKeyframes(const QVector<TrackedPoint> &points,
                                                             const QSize &canvasSize) const
{
    QVector<Keyframe<QPointF>> kfs;
    double halfW = canvasSize.width() / 2.0;
    double halfH = canvasSize.height() / 2.0;

    for (const auto &tp : points) {
        // Convert screen pixel center to canvas offset (-halfW .. +halfW, -halfH .. +halfH)
        QPointF canvasOffset(tp.position.x() - halfW, tp.position.y() - halfH);
        kfs.append(Keyframe<QPointF>(tp.timeMs, canvasOffset, KeyframeInterpolation::Linear));
    }

    return kfs;
}
