#include "videocompositor.h"
#include "videoframedecoder.h"
#include <QPainter>
#include <QtMath>

QImage VideoCompositor::applyFilter(const QImage &source, VisualFilter filter)
{
    if (filter == VisualFilter::None || source.isNull()) {
        return source;
    }

    const int w = source.width();
    const int h = source.height();

    // 1. Spatial / 2D transformation filters
    if (filter == VisualFilter::Blur) {
        int sw = qMax(1, w / 4);
        int sh = qMax(1, h / 4);
        QImage down = source.scaled(sw, sh, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        return down.scaled(source.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    if (filter == VisualFilter::HeavyBlur) {
        int sw = qMax(1, w / 10);
        int sh = qMax(1, h / 10);
        QImage down = source.scaled(sw, sh, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        return down.scaled(source.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    if (filter == VisualFilter::Pixelate) {
        int blockSize = 16;
        int sw = qMax(1, w / blockSize);
        int sh = qMax(1, h / blockSize);
        QImage down = source.scaled(sw, sh, Qt::IgnoreAspectRatio, Qt::FastTransformation);
        return down.scaled(source.size(), Qt::IgnoreAspectRatio, Qt::FastTransformation);
    }
    if (filter == VisualFilter::MirrorH) {
        QImage result = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        int halfW = w / 2;
        for (int y = 0; y < h; ++y) {
            QRgb *line = reinterpret_cast<QRgb*>(result.scanLine(y));
            for (int x = 0; x < halfW; ++x) {
                line[w - 1 - x] = line[x];
            }
        }
        return result;
    }
    if (filter == VisualFilter::MirrorV) {
        QImage result = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        int halfH = h / 2;
        for (int y = 0; y < halfH; ++y) {
            const QRgb *srcLine = reinterpret_cast<const QRgb*>(result.constScanLine(y));
            QRgb *dstLine = reinterpret_cast<QRgb*>(result.scanLine(h - 1 - y));
            memcpy(dstLine, srcLine, w * sizeof(QRgb));
        }
        return result;
    }
    if (filter == VisualFilter::ChromaticAberration) {
        QImage srcFmt = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        QImage result(srcFmt.size(), QImage::Format_ARGB32_Premultiplied);
        const int offset = 8;
        for (int y = 0; y < h; ++y) {
            const QRgb *srcLine = reinterpret_cast<const QRgb*>(srcFmt.constScanLine(y));
            QRgb *dstLine = reinterpret_cast<QRgb*>(result.scanLine(y));
            for (int x = 0; x < w; ++x) {
                int rx = qBound(0, x - offset, w - 1);
                int bx = qBound(0, x + offset, w - 1);
                int r = qRed(srcLine[rx]);
                int g = qGreen(srcLine[x]);
                int b = qBlue(srcLine[bx]);
                int a = qAlpha(srcLine[x]);
                dstLine[x] = qRgba(r, g, b, a);
            }
        }
        return result;
    }
    if (filter == VisualFilter::EdgeDetect) {
        QImage srcFmt = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        QImage result(srcFmt.size(), QImage::Format_ARGB32_Premultiplied);
        for (int y = 0; y < h - 1; ++y) {
            const QRgb *curLine = reinterpret_cast<const QRgb*>(srcFmt.constScanLine(y));
            const QRgb *nextLine = reinterpret_cast<const QRgb*>(srcFmt.constScanLine(y + 1));
            QRgb *dstLine = reinterpret_cast<QRgb*>(result.scanLine(y));
            for (int x = 0; x < w - 1; ++x) {
                int g0 = qGray(curLine[x]);
                int gx = qGray(curLine[x + 1]);
                int gy = qGray(nextLine[x]);
                int delta = qAbs(g0 - gx) + qAbs(g0 - gy);
                int edge = qBound(0, delta * 3, 255);
                dstLine[x] = qRgba(edge, edge, edge, qAlpha(curLine[x]));
            }
            dstLine[w - 1] = qRgba(0, 0, 0, qAlpha(curLine[w - 1]));
        }
        memset(result.scanLine(h - 1), 0, w * sizeof(QRgb));
        return result;
    }

    // 2. Pixel-wise color grading and atmospheric filters
    QImage result = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const double cx = w / 2.0;
    const double cy = h / 2.0;

    for (int y = 0; y < h; ++y) {
        QRgb *line = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < w; ++x) {
            QRgb pixel = line[x];
            int a = qAlpha(pixel);
            int r = qRed(pixel);
            int g = qGreen(pixel);
            int b = qBlue(pixel);

            switch (filter) {
            case VisualFilter::Grayscale: {
                int gray = qGray(pixel);
                line[x] = qRgba(gray, gray, gray, a);
                break;
            }
            case VisualFilter::Sepia: {
                int sr = qBound(0, static_cast<int>(0.393 * r + 0.769 * g + 0.189 * b), 255);
                int sg = qBound(0, static_cast<int>(0.349 * r + 0.686 * g + 0.168 * b), 255);
                int sb = qBound(0, static_cast<int>(0.272 * r + 0.534 * g + 0.131 * b), 255);
                line[x] = qRgba(sr, sg, sb, a);
                break;
            }
            case VisualFilter::Invert: {
                line[x] = qRgba(255 - r, 255 - g, 255 - b, a);
                break;
            }
            case VisualFilter::HighContrast: {
                auto contrastAdj = [](int v) {
                    return qBound(0, static_cast<int>((v - 128) * 1.6 + 128), 255);
                };
                line[x] = qRgba(contrastAdj(r), contrastAdj(g), contrastAdj(b), a);
                break;
            }
            case VisualFilter::Brightness: {
                line[x] = qRgba(qMin(255, r + 45), qMin(255, g + 45), qMin(255, b + 45), a);
                break;
            }
            case VisualFilter::Warm: {
                int wr = qBound(0, r + 35, 255);
                int wg = qBound(0, g + 12, 255);
                int wb = qBound(0, b - 25, 255);
                line[x] = qRgba(wr, wg, wb, a);
                break;
            }
            case VisualFilter::Cool: {
                int cr = qBound(0, r - 25, 255);
                int cg = qBound(0, g + 12, 255);
                int cb = qBound(0, b + 40, 255);
                line[x] = qRgba(cr, cg, cb, a);
                break;
            }
            case VisualFilter::Vibrant: {
                int gray = qGray(pixel);
                int vr = qBound(0, static_cast<int>(gray + (r - gray) * 1.7), 255);
                int vg = qBound(0, static_cast<int>(gray + (g - gray) * 1.7), 255);
                int vb = qBound(0, static_cast<int>(gray + (b - gray) * 1.7), 255);
                line[x] = qRgba(vr, vg, vb, a);
                break;
            }
            case VisualFilter::Desaturate: {
                int gray = qGray(pixel);
                line[x] = qRgba((r + gray) / 2, (g + gray) / 2, (b + gray) / 2, a);
                break;
            }
            case VisualFilter::Vignette: {
                double dx = (x - cx) / cx;
                double dy = (y - cy) / cy;
                double distSq = dx * dx + dy * dy;
                if (distSq > 0.3) {
                    double f = qBound(0.15, 1.0 - (distSq - 0.3) * 0.9, 1.0);
                    line[x] = qRgba(static_cast<int>(r * f), static_cast<int>(g * f), static_cast<int>(b * f), a);
                }
                break;
            }
            case VisualFilter::VintageFilm: {
                int grain = ((x * 19 + y * 37) % 21) - 10;
                int vr = qBound(0, static_cast<int>(r * 1.08 + 16 + grain), 255);
                int vg = qBound(0, static_cast<int>(g * 0.96 + 10 + grain), 255);
                int vb = qBound(0, static_cast<int>(b * 0.82 + grain), 255);
                line[x] = qRgba(vr, vg, vb, a);
                break;
            }
            case VisualFilter::Cyberpunk: {
                double lum = qGray(pixel) / 255.0;
                int cpr = qBound(0, static_cast<int>((1.0 - lum) * 15 + lum * 255), 255);
                int cpg = qBound(0, static_cast<int>((1.0 - lum) * 170 + lum * 35), 255);
                int cpb = qBound(0, static_cast<int>((1.0 - lum) * 235 + lum * 200), 255);
                line[x] = qRgba(cpr, cpg, cpb, a);
                break;
            }
            case VisualFilter::NightVision: {
                int nvg = qBound(0, static_cast<int>((r * 0.3 + g * 0.6 + b * 0.1) * 1.35 + 25), 255);
                if (y % 4 == 0) nvg = static_cast<int>(nvg * 0.75);
                line[x] = qRgba(nvg / 5, nvg, nvg / 5, a);
                break;
            }
            case VisualFilter::Noir: {
                int gray = qGray(pixel);
                int noirVal = qBound(0, static_cast<int>((gray - 110) * 1.7 + 110), 255);
                double dx = (x - cx) / cx;
                double dy = (y - cy) / cy;
                double dSq = dx * dx + dy * dy;
                if (dSq > 0.4) {
                    double f = qBound(0.25, 1.0 - (dSq - 0.4) * 0.8, 1.0);
                    noirVal = static_cast<int>(noirVal * f);
                }
                line[x] = qRgba(noirVal, noirVal, noirVal, a);
                break;
            }
            case VisualFilter::Posterize: {
                int pr = (r / 64) * 85;
                int pg = (g / 64) * 85;
                int pb = (b / 64) * 85;
                line[x] = qRgba(pr, pg, pb, a);
                break;
            }
            case VisualFilter::Solarize: {
                int sr = (r > 128) ? (255 - r) : (r * 2);
                int sg = (g > 128) ? (255 - g) : (g * 2);
                int sb = (b > 128) ? (255 - b) : (b * 2);
                line[x] = qRgba(sr, sg, sb, a);
                break;
            }
            default:
                break;
            }
        }
    }

    return result;
}

QImage VideoCompositor::applyFilters(const QImage &source, const QVector<VisualFilter> &filters)
{
    if (source.isNull() || filters.isEmpty()) {
        return source;
    }
    QImage result = source;
    // Hierarchy rule: Top of the stack (index 0) is applied ON TOP OF the effects below it (index size()-1).
    // Therefore, process from bottom (size()-1) to top (0).
    for (int i = filters.size() - 1; i >= 0; --i) {
        if (filters[i] != VisualFilter::None) {
            result = applyFilter(result, filters[i]);
        }
    }
    return result;
}

QImage VideoCompositor::applyColorAdjustments(const QImage &source, const ColorAdjustments &adj)
{
    if (adj.isIdentity() || source.isNull()) {
        return source;
    }

    const int w = source.width();
    const int h = source.height();

    if (adj.mode == ColorGradeMode::Sliders) {
        // Mode 0: Sliders (Brightness, Luminosity, Red, Green, Blue)
        uint8_t lutR[256];
        uint8_t lutG[256];
        uint8_t lutB[256];

        double lumFactor = 1.0;
        if (adj.luminosity != 0) {
            lumFactor = (259.0 * (adj.luminosity + 255.0)) / (255.0 * (259.0 - adj.luminosity));
        }

        double bOffset = adj.brightness * 1.28;

        auto calcChannel = [&](int inputVal, int channelAdj) -> uint8_t {
            double v = 128.0 + lumFactor * (static_cast<double>(inputVal) - 128.0);
            v += bOffset;
            if (channelAdj >= 0) {
                v = v * (1.0 + (channelAdj / 100.0)) + (channelAdj * 0.5);
            } else {
                v = v * ((100.0 + channelAdj) / 100.0);
            }
            return static_cast<uint8_t>(qBound(0.0, v + 0.5, 255.0));
        };

        for (int i = 0; i < 256; ++i) {
            lutR[i] = calcChannel(i, adj.red);
            lutG[i] = calcChannel(i, adj.green);
            lutB[i] = calcChannel(i, adj.blue);
        }

        QImage result = source.convertToFormat(QImage::Format_ARGB32);
        for (int y = 0; y < h; ++y) {
            QRgb *line = reinterpret_cast<QRgb*>(result.scanLine(y));
            for (int x = 0; x < w; ++x) {
                QRgb p = line[x];
                int a = qAlpha(p);
                if (a == 0) continue;
                line[x] = qRgba(lutR[qRed(p)], lutG[qGreen(p)], lutB[qBlue(p)], a);
            }
        }
        return result;
    }

    // Mode 1: Curves / Graphs (Color Spectrum Gradient vs Intensity & Luma/RGB Curves)
    bool hasLuma = !adj.lumaCurve.isIdentity();
    bool hasColor = !adj.colorCurve.isIdentity();
    bool hasRed = !adj.redCurve.isIdentity();
    bool hasGreen = !adj.greenCurve.isIdentity();
    bool hasBlue = !adj.blueCurve.isIdentity();

    if (!hasLuma && !hasColor && !hasRed && !hasGreen && !hasBlue) {
        return source;
    }

    uint8_t lutR[256];
    uint8_t lutG[256];
    uint8_t lutB[256];

    for (int i = 0; i < 256; ++i) {
        double vR = i / 255.0;
        double vG = i / 255.0;
        double vB = i / 255.0;

        if (hasLuma) {
            vR = adj.lumaCurve.evaluate(vR);
            vG = adj.lumaCurve.evaluate(vG);
            vB = adj.lumaCurve.evaluate(vB);
        }
        if (hasRed) {
            vR = adj.redCurve.evaluate(vR);
        }
        if (hasGreen) {
            vG = adj.greenCurve.evaluate(vG);
        }
        if (hasBlue) {
            vB = adj.blueCurve.evaluate(vB);
        }

        lutR[i] = static_cast<uint8_t>(qBound(0.0, vR * 255.0 + 0.5, 255.0));
        lutG[i] = static_cast<uint8_t>(qBound(0.0, vG * 255.0 + 0.5, 255.0));
        lutB[i] = static_cast<uint8_t>(qBound(0.0, vB * 255.0 + 0.5, 255.0));
    }

    double hueFactors[360];
    if (hasColor) {
        for (int deg = 0; deg < 360; ++deg) {
            hueFactors[deg] = adj.colorCurve.evaluate(deg / 360.0) * 2.0;
        }
    }

    QImage result = source.convertToFormat(QImage::Format_ARGB32);

    for (int y = 0; y < h; ++y) {
        QRgb *line = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < w; ++x) {
            QRgb p = line[x];
            int a = qAlpha(p);
            if (a == 0) continue;

            int r = lutR[qRed(p)];
            int g = lutG[qGreen(p)];
            int b = lutB[qBlue(p)];

            if (hasColor) {
                int maxVal = qMax(r, qMax(g, b));
                int minVal = qMin(r, qMin(g, b));
                int delta = maxVal - minVal;

                if (delta > 0) {
                    double hDeg = 0.0;
                    if (maxVal == r) {
                        hDeg = 60.0 * (static_cast<double>(g - b) / delta);
                        if (hDeg < 0.0) hDeg += 360.0;
                    } else if (maxVal == g) {
                        hDeg = 60.0 * (2.0 + static_cast<double>(b - r) / delta);
                    } else {
                        hDeg = 60.0 * (4.0 + static_cast<double>(r - g) / delta);
                    }

                    int degIdx = qBound(0, static_cast<int>(hDeg + 0.5), 359);
                    double factor = hueFactors[degIdx];

                    if (std::abs(factor - 1.0) > 0.005) {
                        double L = 0.299 * r + 0.587 * g + 0.114 * b;
                        r = static_cast<int>(qBound(0.0, L + (r - L) * factor + 0.5, 255.0));
                        g = static_cast<int>(qBound(0.0, L + (g - L) * factor + 0.5, 255.0));
                        b = static_cast<int>(qBound(0.0, L + (b - L) * factor + 0.5, 255.0));
                    }
                }
            }

            line[x] = qRgba(r, g, b, a);
        }
    }

    return result;
}

QImage VideoCompositor::renderFrame(TimelineModel *model, qint64 timelineMs, const QSize &canvasSize)
{
    QImage canvas(canvasSize, QImage::Format_RGB32);
    canvas.fill(Qt::black);

    if (!model) {
        return canvas;
    }

    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // In model, videoTracks contains [V2, V1]. Lowest track (V1) is rendered first,
    // so iterate backwards: from size()-1 down to 0
    const QVector<TimelineTrack> &vTracks = model->videoTracks();
    for (int i = vTracks.size() - 1; i >= 0; --i) {
        const TimelineTrack &track = vTracks[i];
        if (!track.isVisible()) {
            continue;
        }

        const TimelineClip *clip = track.clipAtTime(timelineMs);
        if (!clip) {
            continue;
        }

        QImage frame;
        if (clip->type() == ClipType::Text) {
            frame = clip->renderTextImage(canvasSize);
        } else {
            qint64 sourceMs = clip->mapTimelineToSourceMs(timelineMs);
            frame = VideoFrameDecoder::instance().getFrame(clip->filePath(), sourceMs, canvasSize);
        }
        if (frame.isNull()) {
            continue;
        }

        double opacity = clip->opacityAt(timelineMs);
        if (opacity <= 0.0) {
            continue;
        }

        if (clip->hasFilters()) {
            frame = applyFilters(frame, clip->filterStack());
        } else if (clip->filter() != VisualFilter::None) {
            frame = applyFilter(frame, clip->filter());
        }

        // Apply clip-level color & luminosity adjustments
        if (!clip->colorAdjustments().isIdentity()) {
            frame = applyColorAdjustments(frame, clip->colorAdjustments());
        }

        // Calculate base size fitting within canvas maintaining aspect ratio
        QSize refSize = model ? model->canvasSize() : QSize(1920, 1080);
        QSize baseSize;
        if (clip->type() == ClipType::Text) {
            double sx = canvasSize.width() / static_cast<double>(refSize.width());
            double sy = canvasSize.height() / static_cast<double>(refSize.height());
            baseSize = QSize(qRound(frame.width() * sx), qRound(frame.height() * sy));
        } else {
            if (clip->scaleMode() == ClipScaleMode::FitLetterbox) {
                baseSize = frame.size().scaled(canvasSize, Qt::KeepAspectRatio);
            } else if (clip->scaleMode() == ClipScaleMode::Stretch) {
                baseSize = canvasSize;
            } else {
                // Default: FillCrop (KeepAspectRatioByExpanding)
                // Preserves original video aspect ratio and fills the canvas, cleanly cutting overflowing content
                baseSize = frame.size().scaled(canvasSize, Qt::KeepAspectRatioByExpanding);
            }
        }
        double baseW = baseSize.width();
        double baseH = baseSize.height();

        // Calculate center position: canvas center + offset relative to reference canvas
        double scaleFactorX = canvasSize.width() / static_cast<double>(refSize.width());
        double scaleFactorY = canvasSize.height() / static_cast<double>(refSize.height());
        double cx = (canvasSize.width() / 2.0) + (clip->posXAt(timelineMs) * scaleFactorX);
        double cy = (canvasSize.height() / 2.0) + (clip->posYAt(timelineMs) * scaleFactorY);

        // Transition calculations
        double inFactor = 1.0;
        if (clip->fadeInMs() > 0 && timelineMs < clip->timelineInMs() + clip->fadeInMs()) {
            inFactor = qBound(0.0, static_cast<double>(timelineMs - clip->timelineInMs()) / clip->fadeInMs(), 1.0);
        }
        double outFactor = 1.0;
        if (clip->fadeOutMs() > 0 && timelineMs > clip->timelineOutMs() - clip->fadeOutMs()) {
            outFactor = qBound(0.0, static_cast<double>(clip->timelineOutMs() - timelineMs) / clip->fadeOutMs(), 1.0);
        }

        painter.save();
        painter.translate(cx, cy);

        // Apply slide transitions
        if (inFactor < 1.0 && clip->transitionIn() == TransitionType::SlideLeft) {
            painter.translate((1.0 - inFactor) * canvasSize.width(), 0.0);
        } else if (inFactor < 1.0 && clip->transitionIn() == TransitionType::SlideRight) {
            painter.translate(-(1.0 - inFactor) * canvasSize.width(), 0.0);
        }
        if (outFactor < 1.0 && clip->transitionOut() == TransitionType::SlideLeft) {
            painter.translate(-(1.0 - outFactor) * canvasSize.width(), 0.0);
        } else if (outFactor < 1.0 && clip->transitionOut() == TransitionType::SlideRight) {
            painter.translate((1.0 - outFactor) * canvasSize.width(), 0.0);
        }

        if (qAbs(clip->rotation()) > 0.001) {
            painter.rotate(clip->rotation());
        }
        if (qAbs(clip->scaleX() - 1.0) > 0.001 || qAbs(clip->scaleY() - 1.0) > 0.001) {
            painter.scale(clip->scaleX(), clip->scaleY());
        }

        // Apply zoom transition
        if (inFactor < 1.0 && clip->transitionIn() == TransitionType::ZoomIn) {
            painter.scale(inFactor, inFactor);
        }
        if (outFactor < 1.0 && clip->transitionOut() == TransitionType::ZoomIn) {
            painter.scale(outFactor, outFactor);
        }

        // Apply wipe transitions via clipping
        if (inFactor < 1.0) {
            if (clip->transitionIn() == TransitionType::WipeLeft) {
                painter.setClipRect(QRectF(-baseW / 2.0, -baseH / 2.0, baseW * inFactor, baseH));
            } else if (clip->transitionIn() == TransitionType::WipeRight) {
                painter.setClipRect(QRectF(-baseW / 2.0 + baseW * (1.0 - inFactor), -baseH / 2.0, baseW * inFactor, baseH));
            } else if (clip->transitionIn() == TransitionType::WipeDown) {
                painter.setClipRect(QRectF(-baseW / 2.0, -baseH / 2.0, baseW, baseH * inFactor));
            } else if (clip->transitionIn() == TransitionType::WipeUp) {
                painter.setClipRect(QRectF(-baseW / 2.0, -baseH / 2.0 + baseH * (1.0 - inFactor), baseW, baseH * inFactor));
            }
        }
        if (outFactor < 1.0) {
            if (clip->transitionOut() == TransitionType::WipeLeft) {
                painter.setClipRect(QRectF(-baseW / 2.0, -baseH / 2.0, baseW * outFactor, baseH));
            } else if (clip->transitionOut() == TransitionType::WipeRight) {
                painter.setClipRect(QRectF(-baseW / 2.0 + baseW * (1.0 - outFactor), -baseH / 2.0, baseW * outFactor, baseH));
            } else if (clip->transitionOut() == TransitionType::WipeDown) {
                painter.setClipRect(QRectF(-baseW / 2.0, -baseH / 2.0, baseW, baseH * outFactor));
            } else if (clip->transitionOut() == TransitionType::WipeUp) {
                painter.setClipRect(QRectF(-baseW / 2.0, -baseH / 2.0 + baseH * (1.0 - outFactor), baseW, baseH * outFactor));
            }
        }

        painter.setOpacity(opacity);
        painter.drawImage(QRectF(-baseW / 2.0, -baseH / 2.0, baseW, baseH), frame);

        // Dip to white overlay
        if (inFactor < 1.0 && clip->transitionIn() == TransitionType::DipToWhite) {
            painter.fillRect(QRectF(-baseW / 2.0, -baseH / 2.0, baseW, baseH), QColor(255, 255, 255, static_cast<int>(255 * (1.0 - inFactor))));
        }
        if (outFactor < 1.0 && clip->transitionOut() == TransitionType::DipToWhite) {
            painter.fillRect(QRectF(-baseW / 2.0, -baseH / 2.0, baseW, baseH), QColor(255, 255, 255, static_cast<int>(255 * (1.0 - outFactor))));
        }

        painter.restore();
    }

    painter.end();

    // Apply global/master video color & luminosity adjustments to the final composite
    if (model && !model->globalColorAdjustments().isIdentity()) {
        canvas = applyColorAdjustments(canvas, model->globalColorAdjustments());
    }

    return canvas;
}
