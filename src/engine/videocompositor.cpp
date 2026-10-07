#include "videocompositor.h"
#include "videoframedecoder.h"
#include "TimeOfDayFilter.h"
#include <QPainter>
#include <QtMath>
#include <cmath>
#include <algorithm>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>

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

namespace {

struct SrgbLuts {
    float srgbToLinear[256];
    uint8_t linearToSrgb[4096];

    SrgbLuts() {
        for (int i = 0; i < 256; ++i) {
            float c = i * (1.0f / 255.0f);
            srgbToLinear[i] = (c <= 0.04045f) ? (c / 12.92f) : std::pow((c + 0.055f) / 1.055f, 2.4f);
        }
        for (int i = 0; i < 4096; ++i) {
            float lin = i * (1.0f / 4095.0f);
            float s = (lin <= 0.0031308f) ? (lin * 12.92f) : (1.055f * std::pow(lin, 1.0f / 2.4f) - 0.055f);
            linearToSrgb[i] = static_cast<uint8_t>(std::clamp(static_cast<int>(s * 255.0f + 0.5f), 0, 255));
        }
    }
};

static const SrgbLuts s_srgbLuts;

class PersistentWorkerPool {
public:
    static PersistentWorkerPool& instance() {
        static PersistentWorkerPool s_instance;
        return s_instance;
    }

    void parallelFor(int totalCount, const std::function<void(int start, int end)> &work) {
        if (totalCount <= 0) return;
        std::unique_lock<std::mutex> outerLock(m_entryMutex);
        const int numWorkers = m_threadCount;
        if (numWorkers <= 1) {
            work(0, totalCount);
            return;
        }

        const int chunkSize = (totalCount + numWorkers - 1) / numWorkers;

        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_work = &work;
            m_totalCount = totalCount;
            m_chunkSize = chunkSize;
            m_remainingWorkers = numWorkers - 1;
            m_generation++;
            m_cvStart.notify_all();
        }

        // Master thread executes chunk 0
        int start0 = 0;
        int end0 = std::min(totalCount, chunkSize);
        if (start0 < end0) {
            work(start0, end0);
        }

        // Wait for background workers to finish
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cvDone.wait(lock, [this]() {
            return m_remainingWorkers == 0;
        });
        m_work = nullptr;
    }

private:
    PersistentWorkerPool() : m_stop(false), m_generation(0), m_remainingWorkers(0), m_work(nullptr), m_totalCount(0), m_chunkSize(0) {
        int hw = static_cast<int>(std::thread::hardware_concurrency());
        m_threadCount = std::clamp(hw > 0 ? hw : 4, 1, 16);
        for (int i = 1; i < m_threadCount; ++i) {
            m_threads.emplace_back(&PersistentWorkerPool::workerLoop, this, i);
        }
    }

    ~PersistentWorkerPool() {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_stop = true;
            m_generation++;
            m_cvStart.notify_all();
        }
        for (auto &t : m_threads) {
            if (t.joinable()) {
                t.join();
            }
        }
    }

    void workerLoop(int threadIndex) {
        uint64_t myGen = 0;
        while (true) {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cvStart.wait(lock, [this, &myGen]() {
                return m_stop || m_generation > myGen;
            });
            if (m_stop) break;
            myGen = m_generation;

            int start = threadIndex * m_chunkSize;
            int end = std::min(m_totalCount, start + m_chunkSize);
            const auto *work = m_work;
            lock.unlock();

            if (work && start < end) {
                (*work)(start, end);
            }

            lock.lock();
            m_remainingWorkers--;
            if (m_remainingWorkers == 0) {
                m_cvDone.notify_one();
            }
        }
    }

    int m_threadCount = 4;
    std::vector<std::thread> m_threads;
    std::mutex m_entryMutex;
    std::mutex m_mutex;
    std::condition_variable m_cvStart;
    std::condition_variable m_cvDone;
    bool m_stop = false;
    uint64_t m_generation = 0;
    int m_remainingWorkers = 0;
    const std::function<void(int start, int end)> *m_work = nullptr;
    int m_totalCount = 0;
    int m_chunkSize = 0;
};

} // namespace

QImage VideoCompositor::applyTimeOfDay(const QImage &source, const ColorAdjustments &adj)
{
    if (source.isNull()) return source;
    if (!adj.timeOfDayEnabled || adj.timeOfDayIntensity <= 0.001f) return source;

    const int w = source.width();
    const int h = source.height();
    if (w <= 0 || h <= 0) return source;

    float targetTime = std::clamp(adj.timeOfDay, 0.0f, 1.0f);
    float sourceTime = 0.60f;
    if (adj.timeOfDaySourceMode == TimeOfDaySourceMode::Auto) {
        sourceTime = TimeOfDayFilter::EstimateSourceTime(source);
    } else {
        sourceTime = std::clamp(adj.timeOfDaySourceTime, 0.0f, 1.0f);
    }

    // Fast-path: When source and target match and all biases are neutral
    if (std::abs(sourceTime - targetTime) < 0.005f &&
        std::abs(adj.timeOfDayExposureBias) < 0.01f &&
        std::abs(adj.timeOfDayHighlightWarmth) < 0.01f &&
        std::abs(adj.timeOfDayShadowCoolness) < 0.01f) {
        return source;
    }

    RelativeSettings settings;
    settings.intensity = std::clamp(adj.timeOfDayIntensity, 0.0f, 1.0f);
    settings.skinProtectionFactor = std::clamp(adj.timeOfDaySkinProtection, 0.0f, 1.0f);
    settings.skyInfluenceFactor = std::clamp(adj.timeOfDaySkyInfluence, 0.0f, 1.0f);
    settings.highlightWarmthBias = std::clamp(adj.timeOfDayHighlightWarmth, -1.0f, 1.0f);
    settings.shadowCoolnessBias = std::clamp(adj.timeOfDayShadowCoolness, -1.0f, 1.0f);
    settings.exposureBias = std::clamp(adj.timeOfDayExposureBias, -2.0f, 2.0f);
    settings.lutStrengthFactor = std::clamp(adj.timeOfDayLutStrength, 0.0f, 1.0f);

    TimeProfile srcProfile = TimeOfDayFilter::CalculateProfile(sourceTime);
    TimeProfile tgtProfile = TimeOfDayFilter::CalculateProfile(targetTime);
    TimeProfile profile = TimeOfDayFilter::CalculateRelativeProfile(sourceTime, targetTime, settings);

    auto toSrgbByte = [](float lin) -> uint8_t {
        int idx = std::clamp(static_cast<int>(lin * 4095.0f + 0.5f), 0, 4095);
        return s_srgbLuts.linearToSrgb[idx];
    };

    auto smoothstep = [](float edge0, float edge1, float x) -> float {
        float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    };

    auto acesFilm = [](float x) -> float {
        float num = x * (2.51f * x + 0.03f);
        float den = x * (2.43f * x + 0.59f) + 0.14f;
        return std::clamp(num / den, 0.0f, 1.0f);
    };

    const float expMultiplier = std::pow(2.0f, profile.exposureEV);

    auto srcWB = TimeOfDayFilter::KelvinToRGB(srcProfile.temperature, srcProfile.tint);
    auto tgtWB = TimeOfDayFilter::KelvinToRGB(tgtProfile.temperature, tgtProfile.tint);

    auto srcShadow = TimeOfDayFilter::KelvinToRGB(srcProfile.shadowTemperature, srcProfile.shadowTint);
    auto tgtShadow = TimeOfDayFilter::KelvinToRGB(tgtProfile.shadowTemperature, tgtProfile.shadowTint);

    auto srcMid = TimeOfDayFilter::KelvinToRGB(srcProfile.midtoneTemperature, 0.0f);
    auto tgtMid = TimeOfDayFilter::KelvinToRGB(tgtProfile.midtoneTemperature, 0.0f);

    auto srcHi = TimeOfDayFilter::KelvinToRGB(srcProfile.highlightTemperature, 0.0f);
    auto tgtHi = TimeOfDayFilter::KelvinToRGB(tgtProfile.highlightTemperature, 0.0f);

    std::array<float, 3> wbGains;
    std::array<float, 3> shadowGains;
    std::array<float, 3> midtoneGains;
    std::array<float, 3> highlightGains;

    for (int i = 0; i < 3; ++i) {
        wbGains[i] = tgtWB[i] / std::max(0.01f, srcWB[i]);
        shadowGains[i] = tgtShadow[i] / std::max(0.01f, srcShadow[i]);
        midtoneGains[i] = (tgtMid[i] * tgtProfile.midtoneGain) / std::max(0.01f, srcMid[i] * srcProfile.midtoneGain);
        highlightGains[i] = (tgtHi[i] * tgtProfile.highlightGain) / std::max(0.01f, srcHi[i] * srcProfile.highlightGain);
    }

    highlightGains[0] *= (1.0f + settings.highlightWarmthBias * 0.35f);
    highlightGains[2] *= (1.0f - settings.highlightWarmthBias * 0.35f);

    shadowGains[0] *= (1.0f - settings.shadowCoolnessBias * 0.30f);
    shadowGains[2] *= (1.0f + settings.shadowCoolnessBias * 0.30f);

    const float expWbR = expMultiplier * wbGains[0];
    const float expWbG = expMultiplier * wbGains[1];
    const float expWbB = expMultiplier * wbGains[2];

    const float shLift = 1.0f + profile.shadowLift;
    const float shR = shadowGains[0] * shLift;
    const float shG = shadowGains[1] * shLift;
    const float shB = shadowGains[2] * shLift;

    const float midR = midtoneGains[0];
    const float midG = midtoneGains[1];
    const float midB = midtoneGains[2];

    const float hiR = highlightGains[0];
    const float hiG = highlightGains[1];
    const float hiB = highlightGains[2];

    // 3D LUT cached profile lookup
    const bool applyLut = (profile.lutStrength > 0.001f);
    Lut3D activeLut;
    if (applyLut) {
        activeLut = TimeOfDayFilter::GetCachedProfileLut(profile, 16);
    }

    // Precalculate vertical sky prior table
    std::vector<float> vPriorTable(h);
    for (int y = 0; y < h; ++y) {
        float v = (h > 1) ? static_cast<float>(h - 1 - y) / (h - 1) : 1.0f;
        vPriorTable[y] = smoothstep(0.15f, 0.75f, v);
    }

    const bool hasAlpha = (source.format() == QImage::Format_ARGB32 || source.format() == QImage::Format_ARGB32_Premultiplied);
    const QImage::Format targetFormat = hasAlpha ? QImage::Format_ARGB32 : QImage::Format_RGB32;
    QImage inputImg = (source.format() == targetFormat) ? source : source.convertToFormat(targetFormat);
    QImage result(w, h, targetFormat);

    const int srcBpl = inputImg.bytesPerLine();
    const int dstBpl = result.bytesPerLine();
    const uchar *srcBits = inputImg.constBits();
    uchar *dstBits = result.bits();

    const float intensityVal = settings.intensity;

    PersistentWorkerPool::instance().parallelFor(h, [&](int yStart, int yEnd) {
        for (int y = yStart; y < yEnd; ++y) {
            const uint32_t *srcLine = reinterpret_cast<const uint32_t*>(srcBits + y * srcBpl);
            uint32_t *dstLine = reinterpret_cast<uint32_t*>(dstBits + y * dstBpl);
            float vPrior = vPriorTable[y];

            for (int x = 0; x < w; ++x) {
                uint32_t p = srcLine[x];
                uint32_t a = hasAlpha ? (p & 0xFF000000) : 0xFF000000;
                if (hasAlpha && a == 0) {
                    dstLine[x] = 0;
                    continue;
                }

                int r8 = (p >> 16) & 0xFF;
                int g8 = (p >> 8) & 0xFF;
                int b8 = p & 0xFF;

                // 1. Decode sRGB to Linear via static LUT
                float rLin = s_srgbLuts.srgbToLinear[r8];
                float gLin = s_srgbLuts.srgbToLinear[g8];
                float bLin = s_srgbLuts.srgbToLinear[b8];

                float origRLin = rLin;
                float origGLin = gLin;
                float origBLin = bLin;

                // 2. Soft Skin Tone Protection Mask (fast rejection for non-skin pixels)
                float skinMask = 0.0f;
                if (profile.skinProtection > 0.001f && r8 > g8 && r8 > b8) {
                    float rNorm = r8 * (1.0f / 255.0f);
                    float gNorm = g8 * (1.0f / 255.0f);
                    float bNorm = b8 * (1.0f / 255.0f);
                    float ySkin = 0.299f * rNorm + 0.587f * gNorm + 0.114f * bNorm;
                    float cb = -0.168736f * rNorm - 0.331264f * gNorm + 0.500000f * bNorm;
                    float cr =  0.500000f * rNorm - 0.418688f * gNorm - 0.081312f * bNorm;
                    float cbDist = (cb - (-0.09f)) * (1.0f / 0.07f);
                    float crDist = (cr - 0.09f) * (1.0f / 0.07f);
                    float ellipseDist = cbDist * cbDist + crDist * crDist;
                    if (ellipseDist < 1.0f) {
                        skinMask = (1.0f - ellipseDist) *
                                   smoothstep(0.15f, 0.35f, ySkin) * profile.skinProtection;
                    }
                }

                // 3. Soft Sky Adjustment (Exposure compression without cloud destruction)
                if (profile.skyExposureDrop > 0.001f && vPrior > 0.001f && (b8 > r8 || (r8 + g8 + b8) > 360)) {
                    float lumaSrgb = (0.2126f * r8 + 0.7152f * g8 + 0.0722f * b8) * (1.0f / 255.0f);
                    float blueDom = std::clamp((b8 - std::max(r8, (g8 * 87) >> 7)) * (3.0f / 255.0f), 0.0f, 1.0f);
                    float brightThresh = smoothstep(0.45f, 0.90f, lumaSrgb);
                    float skyConfidence = std::max(blueDom * 0.75f, brightThresh * 0.35f);
                    float skyMask = std::clamp(skyConfidence * vPrior, 0.0f, 1.0f);
                    float skyFactor = 1.0f - skyMask * profile.skyExposureDrop * 0.45f;
                    rLin *= skyFactor;
                    gLin *= skyFactor;
                    bLin *= skyFactor;
                }

                // 4 & 5. Photometric Exposure & White Balance Adaptation
                if (skinMask > 0.001f) {
                    float wbR = wbGains[0] * (1.0f - skinMask * 0.50f) + skinMask * 0.50f;
                    float wbG = wbGains[1] * (1.0f - skinMask * 0.50f) + skinMask * 0.50f;
                    float wbB = wbGains[2] * (1.0f - skinMask * 0.50f) + skinMask * 0.50f;
                    rLin *= (expMultiplier * wbR);
                    gLin *= (expMultiplier * wbG);
                    bLin *= (expMultiplier * wbB);
                } else {
                    rLin *= expWbR;
                    gLin *= expWbG;
                    bLin *= expWbB;
                }

                // 6. Luminance Split Toning (ShadowLift, MidtoneGain, HighlightGain)
                float lum = 0.2126f * rLin + 0.7152f * gLin + 0.0722f * bLin;
                float mShadow = 1.0f - smoothstep(0.02f, 0.35f, lum);
                float mHighlight = smoothstep(0.45f, 0.95f, lum);
                float mMidtone = std::clamp(1.0f - mShadow - mHighlight, 0.0f, 1.0f);

                float splitR = shR * mShadow + midR * mMidtone + hiR * mHighlight;
                float splitG = shG * mShadow + midG * mMidtone + hiG * mHighlight;
                float splitB = shB * mShadow + midB * mMidtone + hiB * mHighlight;

                if (skinMask > 0.0f) {
                    float skinProtectFactor = skinMask * 0.75f;
                    splitR = splitR * (1.0f - skinProtectFactor) + skinProtectFactor;
                    splitG = splitG * (1.0f - skinProtectFactor) + skinProtectFactor;
                    splitB = splitB * (1.0f - skinProtectFactor) + skinProtectFactor;
                }

                rLin *= splitR;
                gLin *= splitG;
                bLin *= splitB;

                // 7. Selective Purkinje Scotopic Vision Shift (Night low-light rods)
                if (profile.purkinjeStrength > 0.001f) {
                    float purkWeight = profile.purkinjeStrength * (mShadow + 0.35f * mMidtone) * (1.0f - skinMask);
                    float curLum = 0.2126f * rLin + 0.7152f * gLin + 0.0722f * bLin;
                    float purkR = curLum * 0.55f;
                    float purkG = curLum * 0.80f;
                    float purkB = curLum * 1.25f;
                    rLin = rLin * (1.0f - purkWeight) + purkR * purkWeight;
                    gLin = gLin * (1.0f - purkWeight) + purkG * purkWeight;
                    bLin = bLin * (1.0f - purkWeight) + purkB * purkWeight;
                }

                // 8. Contrast & Saturation in Perceptual Space
                if (std::abs(profile.saturation - 1.0f) > 0.001f) {
                    float postLum = 0.2126f * rLin + 0.7152f * gLin + 0.0722f * bLin;
                    rLin = std::max(0.0f, postLum + (rLin - postLum) * profile.saturation);
                    gLin = std::max(0.0f, postLum + (gLin - postLum) * profile.saturation);
                    bLin = std::max(0.0f, postLum + (bLin - postLum) * profile.saturation);
                }
                if (std::abs(profile.contrast - 1.0f) > 0.001f) {
                    rLin = std::max(0.0f, 0.18f + (rLin - 0.18f) * profile.contrast);
                    gLin = std::max(0.0f, 0.18f + (gLin - 0.18f) * profile.contrast);
                    bLin = std::max(0.0f, 0.18f + (bLin - 0.18f) * profile.contrast);
                }

                // 9. 3D LUT Photographic Film Grading
                if (applyLut && activeLut.isValid()) {
                    auto lutRgb = TimeOfDayFilter::SampleLutTrilinear(activeLut, rLin, gLin, bLin);
                    rLin = rLin * (1.0f - profile.lutStrength) + lutRgb[0] * profile.lutStrength;
                    gLin = gLin * (1.0f - profile.lutStrength) + lutRgb[1] * profile.lutStrength;
                    bLin = bLin * (1.0f - profile.lutStrength) + lutRgb[2] * profile.lutStrength;
                }

                // 10. Filmic Tone Mapping (Highlight rolloff)
                if (profile.highlightRolloff > 0.001f) {
                    float filmicR = acesFilm(rLin);
                    float filmicG = acesFilm(gLin);
                    float filmicB = acesFilm(bLin);
                    rLin = rLin * (1.0f - profile.highlightRolloff) + filmicR * profile.highlightRolloff;
                    gLin = gLin * (1.0f - profile.highlightRolloff) + filmicG * profile.highlightRolloff;
                    bLin = bLin * (1.0f - profile.highlightRolloff) + filmicB * profile.highlightRolloff;
                }

                // 11. Relighting Strength / Intensity Blending
                if (intensityVal < 0.999f) {
                    rLin = origRLin * (1.0f - intensityVal) + rLin * intensityVal;
                    gLin = origGLin * (1.0f - intensityVal) + gLin * intensityVal;
                    bLin = origBLin * (1.0f - intensityVal) + bLin * intensityVal;
                }

                // 12. Encode Linear Light back to sRGB Output
                uint32_t outR = toSrgbByte(rLin);
                uint32_t outG = toSrgbByte(gLin);
                uint32_t outB = toSrgbByte(bLin);

                dstLine[x] = a | (outR << 16) | (outG << 8) | outB;
            }
        }
    });

    return result;
}

QImage VideoCompositor::applyTimeOfDay(const QImage &source, float sliderValue)
{
    ColorAdjustments adj;
    adj.timeOfDayEnabled = true;
    adj.timeOfDay = sliderValue;
    adj.timeOfDaySourceMode = TimeOfDaySourceMode::Manual;
    adj.timeOfDaySourceTime = 0.60f;
    adj.timeOfDayIntensity = 1.0f;
    return applyTimeOfDay(source, adj);
}

QImage VideoCompositor::applyColorAdjustments(const QImage &source, const ColorAdjustments &adj)
{
    if (adj.isIdentity() || source.isNull()) {
        return source;
    }

    QImage working = source;
    if (adj.timeOfDayEnabled) {
        working = applyTimeOfDay(working, adj);
    }

    // Check if slider/curves adjustments are active
    bool slidersOrCurvesIdentity = false;
    if (adj.mode == ColorGradeMode::Sliders) {
        slidersOrCurvesIdentity = (adj.brightness == 0 && adj.luminosity == 0 &&
                                   adj.red == 0 && adj.green == 0 && adj.blue == 0);
    } else {
        slidersOrCurvesIdentity = (adj.lumaCurve.isIdentity() && adj.colorCurve.isIdentity() &&
                                   adj.redCurve.isIdentity() && adj.greenCurve.isIdentity() && adj.blueCurve.isIdentity());
    }

    if (slidersOrCurvesIdentity) {
        return working;
    }

    const int w = working.width();
    const int h = working.height();

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

        QImage result = working.convertToFormat(QImage::Format_ARGB32);
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
        return working;
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

    QImage result = working.convertToFormat(QImage::Format_ARGB32);

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
