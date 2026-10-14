#include "colormanagement.h"
#include <QtMath>
#include <thread>
#include <vector>

QString colorSpaceStandardToString(ColorSpaceStandard cs)
{
    switch (cs) {
    case ColorSpaceStandard::sRGB: return "sRGB";
    case ColorSpaceStandard::Rec709: return "Rec.709 (HDTV)";
    case ColorSpaceStandard::DisplayP3: return "Display P3 (Apple Wide Color)";
    case ColorSpaceStandard::DciP3: return "DCI-P3 (Cinema)";
    case ColorSpaceStandard::Rec2020: return "Rec.2020 (UHDTV HDR)";
    }
    return "Rec.709 (HDTV)";
}

ColorSpaceStandard stringToColorSpaceStandard(const QString &str)
{
    QString s = str.trimmed().toLower();
    if (s.contains("srgb")) return ColorSpaceStandard::sRGB;
    if (s.contains("p3") && s.contains("dci")) return ColorSpaceStandard::DciP3;
    if (s.contains("p3") || s.contains("display")) return ColorSpaceStandard::DisplayP3;
    if (s.contains("2020") || s.contains("hdr")) return ColorSpaceStandard::Rec2020;
    return ColorSpaceStandard::Rec709;
}

QString transferFunctionToString(TransferFunction tf)
{
    switch (tf) {
    case TransferFunction::sRGB: return "sRGB EOTF";
    case TransferFunction::Gamma22: return "Gamma 2.2";
    case TransferFunction::Gamma24: return "BT.1886 (Gamma 2.4)";
    case TransferFunction::Linear: return "Linear Light";
    case TransferFunction::PQ: return "SMPTE ST 2084 (PQ HDR)";
    case TransferFunction::HLG: return "ARIB STD-B67 (HLG HDR)";
    }
    return "sRGB EOTF";
}

TransferFunction stringToTransferFunction(const QString &str)
{
    QString s = str.trimmed().toLower();
    if (s.contains("2.2") || s.contains("gamma22")) return TransferFunction::Gamma22;
    if (s.contains("2.4") || s.contains("1886") || s.contains("gamma24")) return TransferFunction::Gamma24;
    if (s.contains("linear")) return TransferFunction::Linear;
    if (s.contains("pq") || s.contains("2084")) return TransferFunction::PQ;
    if (s.contains("hlg") || s.contains("b67")) return TransferFunction::HLG;
    return TransferFunction::sRGB;
}

ColorSpaceManager& ColorSpaceManager::instance()
{
    static ColorSpaceManager s_instance;
    return s_instance;
}

ColorSpaceManager::ColorSpaceManager()
{
}

QVector3D ColorSpaceManager::decodeToLinear(const QVector3D &c, TransferFunction tf)
{
    float r = std::clamp(c.x(), 0.0f, 1.0f);
    float g = std::clamp(c.y(), 0.0f, 1.0f);
    float b = std::clamp(c.z(), 0.0f, 1.0f);

    auto decodeChannel = [tf](float v) -> float {
        switch (tf) {
        case TransferFunction::Linear:
            return v;
        case TransferFunction::Gamma22:
            return std::pow(v, 2.2f);
        case TransferFunction::Gamma24:
            return std::pow(v, 2.4f);
        case TransferFunction::sRGB:
            return (v <= 0.04045f) ? (v / 12.92f) : std::pow((v + 0.055f) / 1.055f, 2.4f);
        case TransferFunction::PQ: {
            // ST 2084 Inverse EOTF (Non-linear to Linear Optical Nits scaled to 0..1)
            float N = std::pow(std::clamp(v, 0.0f, 1.0f), 1.0f / 78.84375f); // 1 / m2
            float num = std::max(0.0f, N - 0.8359375f);                      // c1
            float den = 18.8515625f - 18.6875f * N;                          // c2 - c3*N
            return (den > 0.0f) ? std::pow(num / den, 1.0f / 0.1593017578125f) : 0.0f; // 1 / m1
        }
        case TransferFunction::HLG: {
            // ARIB STD-B67 OETF inverse
            const float a = 0.17883277f;
            const float bHlg = 0.28466892f;
            const float cHlg = 0.55991073f;
            if (v <= 0.5f) {
                return (v * v) / 3.0f;
            } else {
                return (std::exp((v - cHlg) / a) + bHlg) / 12.0f;
            }
        }
        }
        return v;
    };

    return QVector3D(decodeChannel(r), decodeChannel(g), decodeChannel(b));
}

QVector3D ColorSpaceManager::encodeFromLinear(const QVector3D &c, TransferFunction tf)
{
    float r = std::max(0.0f, c.x());
    float g = std::max(0.0f, c.y());
    float b = std::max(0.0f, c.z());

    auto encodeChannel = [tf](float v) -> float {
        switch (tf) {
        case TransferFunction::Linear:
            return v;
        case TransferFunction::Gamma22:
            return std::pow(std::clamp(v, 0.0f, 1.0f), 1.0f / 2.2f);
        case TransferFunction::Gamma24:
            return std::pow(std::clamp(v, 0.0f, 1.0f), 1.0f / 2.4f);
        case TransferFunction::sRGB: {
            float cv = std::clamp(v, 0.0f, 1.0f);
            return (cv <= 0.0031308f) ? (cv * 12.92f) : (1.055f * std::pow(cv, 1.0f / 2.4f) - 0.055f);
        }
        case TransferFunction::PQ: {
            // ST 2084 EOTF
            float Y = std::clamp(v, 0.0f, 1.0f);
            float Ym1 = std::pow(Y, 0.1593017578125f);
            float num = 0.8359375f + 18.8515625f * Ym1;
            float den = 1.0f + 18.6875f * Ym1;
            return std::pow(num / den, 78.84375f);
        }
        case TransferFunction::HLG: {
            const float a = 0.17883277f;
            const float bHlg = 0.28466892f;
            const float cHlg = 0.55991073f;
            if (v <= (1.0f / 12.0f)) {
                return std::sqrt(3.0f * v);
            } else {
                return a * std::log(12.0f * v - bHlg) + cHlg;
            }
        }
        }
        return v;
    };

    return QVector3D(encodeChannel(r), encodeChannel(g), encodeChannel(b));
}

// Color Space Primaries to XYZ D65 Matrices
static const float M_REC709_TO_XYZ[9] = {
    0.4124564f, 0.3575761f, 0.1804375f,
    0.2126729f, 0.7151522f, 0.0721750f,
    0.0193339f, 0.1191920f, 0.9503041f
};

static const float M_XYZ_TO_REC709[9] = {
     3.2404542f, -1.5371385f, -0.4985314f,
    -0.9692660f,  1.8760108f,  0.0415560f,
     0.0556434f, -0.2040259f,  1.0572252f
};

static const float M_P3_TO_XYZ[9] = {
    0.4865709f, 0.2656677f, 0.1982173f,
    0.2289746f, 0.6917393f, 0.0792861f,
    0.0000000f, 0.0451134f, 1.0439444f
};

static const float M_XYZ_TO_P3[9] = {
     2.4934969f, -0.9313836f, -0.4027108f,
    -0.8294890f,  1.7626641f,  0.0236247f,
     0.0358458f, -0.0761724f,  0.9568845f
};

static const float M_REC2020_TO_XYZ[9] = {
    0.6369580f, 0.1446169f, 0.1688810f,
    0.2627002f, 0.6779981f, 0.0593017f,
    0.0000000f, 0.0280727f, 1.0609851f
};

static const float M_XYZ_TO_REC2020[9] = {
     1.7166512f, -0.3556708f, -0.2533663f,
    -0.6666844f,  1.6164812f,  0.0157685f,
     0.0176399f, -0.0427706f,  0.9421031f
};

static inline QVector3D mulMatVec(const float m[9], const QVector3D &v) {
    return QVector3D(
        m[0] * v.x() + m[1] * v.y() + m[2] * v.z(),
        m[3] * v.x() + m[4] * v.y() + m[5] * v.z(),
        m[6] * v.x() + m[7] * v.y() + m[8] * v.z()
    );
}

QVector3D ColorSpaceManager::convertColorSpace(const QVector3D &linearRgb,
                                              ColorSpaceStandard fromSpace,
                                              ColorSpaceStandard toSpace)
{
    if (fromSpace == toSpace) {
        return linearRgb;
    }

    // Convert from source to CIE XYZ D65
    QVector3D xyz;
    switch (fromSpace) {
    case ColorSpaceStandard::sRGB:
    case ColorSpaceStandard::Rec709:
        xyz = mulMatVec(M_REC709_TO_XYZ, linearRgb);
        break;
    case ColorSpaceStandard::DisplayP3:
    case ColorSpaceStandard::DciP3:
        xyz = mulMatVec(M_P3_TO_XYZ, linearRgb);
        break;
    case ColorSpaceStandard::Rec2020:
        xyz = mulMatVec(M_REC2020_TO_XYZ, linearRgb);
        break;
    }

    // Convert from CIE XYZ D65 to target
    QVector3D target;
    switch (toSpace) {
    case ColorSpaceStandard::sRGB:
    case ColorSpaceStandard::Rec709:
        target = mulMatVec(M_XYZ_TO_REC709, xyz);
        break;
    case ColorSpaceStandard::DisplayP3:
    case ColorSpaceStandard::DciP3:
        target = mulMatVec(M_XYZ_TO_P3, xyz);
        break;
    case ColorSpaceStandard::Rec2020:
        target = mulMatVec(M_XYZ_TO_REC2020, xyz);
        break;
    }

    return target;
}

QVector3D ColorSpaceManager::toneMapAces(const QVector3D &hdrLinear)
{
    // ACES filmic tone curve
    auto aces = [](float x) -> float {
        const float a = 2.51f;
        const float b = 0.03f;
        const float c = 2.43f;
        const float d = 0.59f;
        const float e = 0.14f;
        return std::clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0f, 1.0f);
    };
    return QVector3D(aces(hdrLinear.x()), aces(hdrLinear.y()), aces(hdrLinear.z()));
}

QImage ColorSpaceManager::processFrameColorSpace(const QImage &source,
                                                 ColorSpaceStandard inputSpace,
                                                 TransferFunction inputTf,
                                                 ColorSpaceStandard outputSpace,
                                                 TransferFunction outputTf)
{
    if (source.isNull()) return QImage();
    if (inputSpace == outputSpace && inputTf == outputTf) {
        return source;
    }

    QImage result = source.convertToFormat(QImage::Format_ARGB32);
    const int w = result.width();
    const int h = result.height();

    // Fast 8-bit lookup table optimization for standard 1D transfer functions
    if (inputSpace == outputSpace &&
        inputTf != TransferFunction::Linear && outputTf != TransferFunction::Linear)
    {
        uint8_t lut[256];
        for (int i = 0; i < 256; ++i) {
            float inNorm = i / 255.0f;
            QVector3D lin = decodeToLinear(QVector3D(inNorm, inNorm, inNorm), inputTf);
            QVector3D outNorm = encodeFromLinear(lin, outputTf);
            lut[i] = static_cast<uint8_t>(qBound(0.0f, outNorm.x() * 255.0f + 0.5f, 255.0f));
        }

        for (int y = 0; y < h; ++y) {
            QRgb *line = reinterpret_cast<QRgb*>(result.scanLine(y));
            for (int x = 0; x < w; ++x) {
                QRgb p = line[x];
                int a = qAlpha(p);
                line[x] = qRgba(lut[qRed(p)], lut[qGreen(p)], lut[qBlue(p)], a);
            }
        }
        return result;
    }

    // Full 3D matrix conversion multithreaded
    unsigned int numThreads = std::min<unsigned int>(std::max(1u, std::thread::hardware_concurrency()), 8u);
    int rowsPerThread = (h + numThreads - 1) / numThreads;

    std::vector<std::thread> workers;
    workers.reserve(numThreads);

    for (unsigned int t = 0; t < numThreads; ++t) {
        int startY = t * rowsPerThread;
        int endY = std::min(h, startY + rowsPerThread);
        if (startY >= endY) break;

        workers.emplace_back([&result, startY, endY, w, inputTf, inputSpace, outputSpace, outputTf]() {
            for (int y = startY; y < endY; ++y) {
                QRgb *line = reinterpret_cast<QRgb*>(result.scanLine(y));
                for (int x = 0; x < w; ++x) {
                    QRgb p = line[x];
                    int a = qAlpha(p);
                    QVector3D inRgb(qRed(p) / 255.0f, qGreen(p) / 255.0f, qBlue(p) / 255.0f);
                    QVector3D lin = decodeToLinear(inRgb, inputTf);
                    QVector3D conv = convertColorSpace(lin, inputSpace, outputSpace);
                    QVector3D outRgb = encodeFromLinear(conv, outputTf);

                    int r = qBound(0, static_cast<int>(outRgb.x() * 255.0f + 0.5f), 255);
                    int g = qBound(0, static_cast<int>(outRgb.y() * 255.0f + 0.5f), 255);
                    int b = qBound(0, static_cast<int>(outRgb.z() * 255.0f + 0.5f), 255);
                    line[x] = qRgba(r, g, b, a);
                }
            }
        });
    }

    for (auto &worker : workers) {
        if (worker.joinable()) worker.join();
    }

    return result;
}
