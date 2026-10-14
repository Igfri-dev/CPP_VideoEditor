#include "chromakey.h"
#include <QtMath>
#include <algorithm>
#include <vector>

QJsonObject ChromaKeySettings::toJson() const
{
    QJsonObject obj;
    obj["enabled"] = enabled;
    obj["keyColor"] = keyColor.name(QColor::HexRgb);
    obj["tolerance"] = static_cast<double>(tolerance);
    obj["smoothness"] = static_cast<double>(smoothness);
    obj["spillSuppression"] = static_cast<double>(spillSuppression);
    obj["clipBlack"] = static_cast<double>(clipBlack);
    obj["clipWhite"] = static_cast<double>(clipWhite);
    obj["choke"] = choke;
    obj["showMatte"] = showMatte;
    return obj;
}

ChromaKeySettings ChromaKeySettings::fromJson(const QJsonObject &obj)
{
    ChromaKeySettings s;
    if (obj.contains("enabled")) s.enabled = obj["enabled"].toBool();
    if (obj.contains("keyColor")) s.keyColor = QColor(obj["keyColor"].toString("#00ff00"));
    if (obj.contains("tolerance")) s.tolerance = static_cast<float>(obj["tolerance"].toDouble(0.35));
    if (obj.contains("smoothness")) s.smoothness = static_cast<float>(obj["smoothness"].toDouble(0.15));
    if (obj.contains("spillSuppression")) s.spillSuppression = static_cast<float>(obj["spillSuppression"].toDouble(0.60));
    if (obj.contains("clipBlack")) s.clipBlack = static_cast<float>(obj["clipBlack"].toDouble(0.05));
    if (obj.contains("clipWhite")) s.clipWhite = static_cast<float>(obj["clipWhite"].toDouble(0.95));
    if (obj.contains("choke")) s.choke = obj["choke"].toInt(0);
    if (obj.contains("showMatte")) s.showMatte = obj["showMatte"].toBool(false);
    return s;
}

static inline void rgbToYCbCr(float r, float g, float b, float &Y, float &Cb, float &Cr)
{
    Y  =  0.299000f * r + 0.587000f * g + 0.114000f * b;
    Cb = -0.168736f * r - 0.331264f * g + 0.500000f * b;
    Cr =  0.500000f * r - 0.418688f * g - 0.081312f * b;
}

QImage ChromaKeyProcessor::apply(const QImage &source, const ChromaKeySettings &settings)
{
    if (source.isNull() || !settings.enabled) {
        return source;
    }

    QImage result = source.convertToFormat(QImage::Format_ARGB32);
    const int w = result.width();
    const int h = result.height();

    // Key color in normalized chromatic space
    float kR = settings.keyColor.redF();
    float kG = settings.keyColor.greenF();
    float kB = settings.keyColor.blueF();
    float kY, kCb, kCr;
    rgbToYCbCr(kR, kG, kB, kY, kCb, kCr);

    const float tol = std::max(0.001f, settings.tolerance);
    const float smooth = std::max(0.001f, settings.smoothness);
    const float spill = settings.spillSuppression;

    // Detect if key color is predominantly green or blue
    bool isGreenKey = (kG > kR && kG > kB);
    bool isBlueKey = (kB > kR && kB > kG);

    for (int y = 0; y < h; ++y) {
        QRgb *line = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < w; ++x) {
            QRgb p = line[x];
            int origA = qAlpha(p);
            if (origA == 0) continue;

            float r = qRed(p) / 255.0f;
            float g = qGreen(p) / 255.0f;
            float b = qBlue(p) / 255.0f;

            float py, pCb, pCr;
            rgbToYCbCr(r, g, b, py, pCb, pCr);

            // Euclidean distance in chrominance plane (Cb, Cr)
            float dCb = pCb - kCb;
            float dCr = pCr - kCr;
            float dist = std::sqrt(dCb * dCb + dCr * dCr);

            // Compute matte alpha
            float alpha = 1.0f;
            if (dist < tol) {
                alpha = 0.0f;
            } else if (dist < tol + smooth) {
                float t = (dist - tol) / smooth;
                alpha = t * t * (3.0f - 2.0f * t); // Smoothstep
            }

            // Black & White clipping in matte
            if (alpha <= settings.clipBlack) {
                alpha = 0.0f;
            } else if (alpha >= settings.clipWhite) {
                alpha = 1.0f;
            } else {
                alpha = (alpha - settings.clipBlack) / std::max(0.001f, settings.clipWhite - settings.clipBlack);
            }

            // Spill suppression
            if (spill > 0.001f && alpha > 0.0f) {
                if (isGreenKey) {
                    float maxOther = std::max(r, b);
                    if (g > maxOther) {
                        g = g * (1.0f - spill) + maxOther * spill;
                    }
                } else if (isBlueKey) {
                    float maxOther = std::max(r, g);
                    if (b > maxOther) {
                        b = b * (1.0f - spill) + maxOther * spill;
                    }
                }
            }

            int finalA = static_cast<int>(origA * std::clamp(alpha, 0.0f, 1.0f) + 0.5f);
            if (settings.showMatte) {
                line[x] = qRgba(finalA, finalA, finalA, 255);
            } else {
                int finalR = static_cast<int>(r * 255.0f + 0.5f);
                int finalG = static_cast<int>(g * 255.0f + 0.5f);
                int finalB = static_cast<int>(b * 255.0f + 0.5f);
                line[x] = qRgba(finalR, finalG, finalB, finalA);
            }
        }
    }

    return result;
}
