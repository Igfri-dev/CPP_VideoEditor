#pragma once

#include <QColor>
#include <QImage>
#include <QString>
#include <QJsonObject>

struct ChromaKeySettings {
    bool enabled = false;
    QColor keyColor = QColor(0, 255, 0); // Default Chroma Green
    float tolerance = 0.35f;            // Similarity threshold [0.0..1.0]
    float smoothness = 0.15f;           // Softness of edges [0.0..1.0]
    float spillSuppression = 0.60f;     // Desaturation of key color spill on subject [0.0..1.0]
    float clipBlack = 0.05f;            // Crushes transparent noise in background [0.0..1.0]
    float clipWhite = 0.95f;            // Restores solid opacity in foreground [0.0..1.0]
    int choke = 0;                      // Erode (-N) / Dilate (+N) matte border in pixels
    bool showMatte = false;             // Preview black & white matte

    bool isIdentity() const {
        return !enabled;
    }

    bool operator==(const ChromaKeySettings &other) const {
        return enabled == other.enabled &&
               keyColor == other.keyColor &&
               std::abs(tolerance - other.tolerance) < 1e-4f &&
               std::abs(smoothness - other.smoothness) < 1e-4f &&
               std::abs(spillSuppression - other.spillSuppression) < 1e-4f &&
               std::abs(clipBlack - other.clipBlack) < 1e-4f &&
               std::abs(clipWhite - other.clipWhite) < 1e-4f &&
               choke == other.choke;
    }
    bool operator!=(const ChromaKeySettings &other) const { return !(*this == other); }

    QJsonObject toJson() const;
    static ChromaKeySettings fromJson(const QJsonObject &obj);
};

class ChromaKeyProcessor {
public:
    static QImage apply(const QImage &source, const ChromaKeySettings &settings);
};
