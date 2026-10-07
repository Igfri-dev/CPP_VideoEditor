#pragma once

#include <QString>
#include <QColor>
#include <QImage>
#include <QtGlobal>
#include "motionpath.h"

enum class ClipType {
    Video,
    Audio,
    Image,
    Text
};

enum class VisualFilter {
    None,
    Grayscale,
    Sepia,
    Invert,
    HighContrast,
    Brightness,
    Warm,
    Cool,
    Vibrant,
    Desaturate,
    Vignette,
    VintageFilm,
    Cyberpunk,
    NightVision,
    Noir,
    Blur,
    HeavyBlur,
    Pixelate,
    EdgeDetect,
    ChromaticAberration,
    Posterize,
    Solarize,
    MirrorH,
    MirrorV
};

struct VisualFilterInfo {
    VisualFilter filter;
    QString name;
    QString category;
};

QString clipTypeToString(ClipType type);
QString visualFilterToString(VisualFilter filter);
VisualFilter stringToVisualFilter(const QString &str);
QList<VisualFilterInfo> allVisualFilters();

enum class TransitionType {
    None,
    DipToBlack,
    DipToWhite,
    CrossDissolve,
    WipeLeft,
    WipeRight,
    WipeUp,
    WipeDown,
    SlideLeft,
    SlideRight,
    ZoomIn
};

QString transitionTypeToString(TransitionType type);
TransitionType stringToTransitionType(const QString &str);
QString transitionTypeDisplayName(TransitionType type);
QList<TransitionType> allTransitionTypes();

enum class ClipScaleMode {
    FillCrop = 0,     // Llenar lienzo cortando el desborde (KeepAspectRatioByExpanding)
    FitLetterbox = 1, // Ajustar al lienzo manteniendo bandas negras (KeepAspectRatio)
    Stretch = 2       // Estirar al lienzo sin mantener proporción
};

QString clipScaleModeToString(ClipScaleMode mode);
ClipScaleMode stringToClipScaleMode(const QString &str);
QString clipScaleModeDisplayName(ClipScaleMode mode);

#include "colorcurve.h"

enum class ColorGradeMode {
    Sliders = 0,
    Curves = 1
};

enum class TimeOfDaySourceMode {
    Auto = 0,
    Manual = 1
};

struct ColorAdjustments {
    ColorGradeMode mode = ColorGradeMode::Sliders;

    // Mode 0: Sliders
    int brightness = 0;   // [-100, 100], 0 = neutro / original
    int luminosity = 0;   // [-100, 100], 0 = neutro / contraste original
    int red = 0;          // [-100, 100], 0 = presencia estándar canal rojo
    int green = 0;        // [-100, 100], 0 = presencia estándar canal verde
    int blue = 0;         // [-100, 100], 0 = presencia estándar canal azul

    // Mode 1: Curves / Graphs
    ColorCurve lumaCurve = ColorCurve::defaultLuma();
    ColorCurve colorCurve = ColorCurve::defaultColorSpectrum();
    ColorCurve redCurve = ColorCurve::defaultChannel(CurveType::Red);
    ColorCurve greenCurve = ColorCurve::defaultChannel(CurveType::Green);
    ColorCurve blueCurve = ColorCurve::defaultChannel(CurveType::Blue);

    // Time of Day (Natural Diurnal Relighting & Relative Lighting)
    bool timeOfDayEnabled = false;
    float timeOfDay = 0.60f; // Target Time [0.0f, 1.0f]: 0.00 = Noche, 0.60 = Mediodía (Neutro), 1.00 = Atardecer
    TimeOfDaySourceMode timeOfDaySourceMode = TimeOfDaySourceMode::Auto;
    float timeOfDaySourceTime = 0.60f; // Source / Reference Time when in Manual mode [0.0f, 1.0f]
    float timeOfDayIntensity = 1.0f;  // Relighting Strength [0.0f, 1.0f] (0% = original, 100% = full relighted)

    // Advanced Relative Relighting parameters
    float timeOfDaySkinProtection = 1.0f;    // Multiplier for skin protection [0.0f, 1.0f] (default 1.0f)
    float timeOfDaySkyInfluence = 1.0f;       // Multiplier for sky exposure drop [0.0f, 1.0f] (default 1.0f)
    float timeOfDayHighlightWarmth = 0.0f;   // Highlight warmth bias [-1.0f, 1.0f] (default 0.0f)
    float timeOfDayShadowCoolness = 0.0f;    // Shadow coolness bias [-1.0f, 1.0f] (default 0.0f)
    float timeOfDayExposureBias = 0.0f;      // Exposure offset in EV [-2.0f, 2.0f] (default 0.0f)
    float timeOfDayLutStrength = 1.0f;       // Multiplier for 3D LUT look [0.0f, 1.0f] (default 1.0f)

    bool isIdentity() const {
        if (timeOfDayEnabled && timeOfDayIntensity > 0.001f) {
            if (timeOfDaySourceMode == TimeOfDaySourceMode::Manual) {
                if (std::abs(timeOfDay - timeOfDaySourceTime) > 0.005f ||
                    std::abs(timeOfDayExposureBias) > 0.01f ||
                    std::abs(timeOfDayHighlightWarmth) > 0.01f ||
                    std::abs(timeOfDayShadowCoolness) > 0.01f) {
                    return false;
                }
            } else {
                if (std::abs(timeOfDay - 0.60f) > 0.005f ||
                    std::abs(timeOfDayExposureBias) > 0.01f ||
                    std::abs(timeOfDayHighlightWarmth) > 0.01f ||
                    std::abs(timeOfDayShadowCoolness) > 0.01f) {
                    return false;
                }
            }
        }
        if (mode == ColorGradeMode::Sliders) {
            return brightness == 0 && luminosity == 0 && red == 0 && green == 0 && blue == 0;
        } else {
            return lumaCurve.isIdentity() && colorCurve.isIdentity() &&
                   redCurve.isIdentity() && greenCurve.isIdentity() && blueCurve.isIdentity();
        }
    }

    bool operator==(const ColorAdjustments &o) const {
        return mode == o.mode &&
               brightness == o.brightness && luminosity == o.luminosity &&
               red == o.red && green == o.green && blue == o.blue &&
               timeOfDayEnabled == o.timeOfDayEnabled &&
               std::abs(timeOfDay - o.timeOfDay) < 0.001f &&
               timeOfDaySourceMode == o.timeOfDaySourceMode &&
               std::abs(timeOfDaySourceTime - o.timeOfDaySourceTime) < 0.001f &&
               std::abs(timeOfDayIntensity - o.timeOfDayIntensity) < 0.001f &&
               std::abs(timeOfDaySkinProtection - o.timeOfDaySkinProtection) < 0.001f &&
               std::abs(timeOfDaySkyInfluence - o.timeOfDaySkyInfluence) < 0.001f &&
               std::abs(timeOfDayHighlightWarmth - o.timeOfDayHighlightWarmth) < 0.001f &&
               std::abs(timeOfDayShadowCoolness - o.timeOfDayShadowCoolness) < 0.001f &&
               std::abs(timeOfDayExposureBias - o.timeOfDayExposureBias) < 0.001f &&
               std::abs(timeOfDayLutStrength - o.timeOfDayLutStrength) < 0.001f &&
               lumaCurve == o.lumaCurve && colorCurve == o.colorCurve &&
               redCurve == o.redCurve && greenCurve == o.greenCurve && blueCurve == o.blueCurve;
    }
    bool operator!=(const ColorAdjustments &o) const {
        return !(*this == o);
    }
};

class TimelineClip {
public:
    TimelineClip();
    TimelineClip(qint64 id, qint64 trackId, const QString &name, const QString &filePath, ClipType type);

    qint64 id() const { return m_id; }
    void setId(qint64 id) { m_id = id; }

    qint64 trackId() const { return m_trackId; }
    void setTrackId(qint64 trackId) { m_trackId = trackId; }

    QString name() const { return m_name; }
    void setName(const QString &name) { m_name = name; }

    QString filePath() const { return m_filePath; }
    void setFilePath(const QString &path) { m_filePath = path; }

    ClipType type() const { return m_type; }
    void setType(ClipType type) { m_type = type; }

    qint64 timelineInMs() const { return m_timelineInMs; }
    void setTimelineInMs(qint64 inMs) { m_timelineInMs = inMs; }

    qint64 timelineOutMs() const { return m_timelineOutMs; }
    void setTimelineOutMs(qint64 outMs) { m_timelineOutMs = outMs; }

    qint64 durationMs() const { return qMax<qint64>(0, m_timelineOutMs - m_timelineInMs); }

    qint64 sourceInMs() const { return m_sourceInMs; }
    void setSourceInMs(qint64 inMs) { m_sourceInMs = inMs; }

    qint64 sourceOutMs() const { return m_sourceOutMs; }
    void setSourceOutMs(qint64 outMs) { m_sourceOutMs = outMs; }

    qint64 sourceDurationMs() const { return m_sourceDurationMs; }
    void setSourceDurationMs(qint64 durationMs) { m_sourceDurationMs = durationMs; }

    double speed() const { return m_speed; }
    void setSpeed(double speed) { m_speed = qBound(0.1, speed, 8.0); }

    double volume() const { return m_volume; }
    void setVolume(double volume) { m_volume = qBound(0.0, volume, 2.0); }

    double opacity() const { return m_opacity; }
    void setOpacity(double opacity) { m_opacity = qBound(0.0, opacity, 1.0); }

    qint64 fadeInMs() const { return m_fadeInMs; }
    void setFadeInMs(qint64 ms) { m_fadeInMs = qMax<qint64>(0, ms); }

    qint64 fadeOutMs() const { return m_fadeOutMs; }
    void setFadeOutMs(qint64 ms) { m_fadeOutMs = qMax<qint64>(0, ms); }

    TransitionType transitionIn() const { return m_transitionIn; }
    void setTransitionIn(TransitionType t, qint64 durationMs = -1) {
        m_transitionIn = t;
        if (durationMs > 0) m_transitionInDurationMs = durationMs;
    }
    qint64 transitionInDurationMs() const { return m_transitionInDurationMs > 0 ? m_transitionInDurationMs : (m_fadeInMs > 0 ? m_fadeInMs : 500); }
    void setTransitionInDurationMs(qint64 ms) { m_transitionInDurationMs = qMax<qint64>(0, ms); }

    TransitionType transitionOut() const { return m_transitionOut; }
    void setTransitionOut(TransitionType t, qint64 durationMs = -1) {
        m_transitionOut = t;
        if (durationMs > 0) m_transitionOutDurationMs = durationMs;
    }
    qint64 transitionOutDurationMs() const { return m_transitionOutDurationMs > 0 ? m_transitionOutDurationMs : (m_fadeOutMs > 0 ? m_fadeOutMs : 500); }
    void setTransitionOutDurationMs(qint64 ms) { m_transitionOutDurationMs = qMax<qint64>(0, ms); }

    VisualFilter filter() const {
        return m_filterStack.isEmpty() ? VisualFilter::None : m_filterStack.first();
    }
    void setFilter(VisualFilter filter) {
        m_filterStack.clear();
        if (filter != VisualFilter::None) {
            m_filterStack.append(filter);
        }
    }

    const QVector<VisualFilter>& filterStack() const { return m_filterStack; }
    void setFilterStack(const QVector<VisualFilter> &stack) { m_filterStack = stack; }
    void addFilter(VisualFilter filter) {
        if (filter != VisualFilter::None) {
            m_filterStack.append(filter);
        }
    }
    void insertFilter(int index, VisualFilter filter) {
        if (filter != VisualFilter::None) {
            int idx = qBound(0, index, m_filterStack.size());
            m_filterStack.insert(idx, filter);
        }
    }
    void removeFilterAt(int index) {
        if (index >= 0 && index < m_filterStack.size()) {
            m_filterStack.removeAt(index);
        }
    }
    void moveFilter(int fromIndex, int toIndex) {
        if (fromIndex >= 0 && fromIndex < m_filterStack.size() &&
            toIndex >= 0 && toIndex < m_filterStack.size() &&
            fromIndex != toIndex) {
            m_filterStack.move(fromIndex, toIndex);
        }
    }
    void clearFilters() { m_filterStack.clear(); }
    bool hasFilters() const {
        for (VisualFilter f : m_filterStack) {
            if (f != VisualFilter::None) return true;
        }
        return false;
    }

    // Color & Luminosity Adjustments (Brillo, Luminosidad, Presencia RGB)
    const ColorAdjustments& colorAdjustments() const { return m_colorAdjustments; }
    ColorAdjustments& colorAdjustments() { return m_colorAdjustments; }
    void setColorAdjustments(const ColorAdjustments &adj) {
        m_colorAdjustments = adj;
        m_colorAdjustments.brightness = qBound(-100, adj.brightness, 100);
        m_colorAdjustments.luminosity = qBound(-100, adj.luminosity, 100);
        m_colorAdjustments.red = qBound(-100, adj.red, 100);
        m_colorAdjustments.green = qBound(-100, adj.green, 100);
        m_colorAdjustments.blue = qBound(-100, adj.blue, 100);
        m_colorAdjustments.timeOfDay = qBound(0.0f, adj.timeOfDay, 1.0f);
        m_colorAdjustments.timeOfDaySourceTime = qBound(0.0f, adj.timeOfDaySourceTime, 1.0f);
        m_colorAdjustments.timeOfDayIntensity = qBound(0.0f, adj.timeOfDayIntensity, 1.0f);
        m_colorAdjustments.timeOfDaySkinProtection = qBound(0.0f, adj.timeOfDaySkinProtection, 1.0f);
        m_colorAdjustments.timeOfDaySkyInfluence = qBound(0.0f, adj.timeOfDaySkyInfluence, 1.0f);
        m_colorAdjustments.timeOfDayHighlightWarmth = qBound(-1.0f, adj.timeOfDayHighlightWarmth, 1.0f);
        m_colorAdjustments.timeOfDayShadowCoolness = qBound(-1.0f, adj.timeOfDayShadowCoolness, 1.0f);
        m_colorAdjustments.timeOfDayExposureBias = qBound(-2.0f, adj.timeOfDayExposureBias, 2.0f);
        m_colorAdjustments.timeOfDayLutStrength = qBound(0.0f, adj.timeOfDayLutStrength, 1.0f);
    }
    bool isTimeOfDayEnabled() const { return m_colorAdjustments.timeOfDayEnabled; }
    void setTimeOfDayEnabled(bool enabled) { m_colorAdjustments.timeOfDayEnabled = enabled; }
    float timeOfDay() const { return m_colorAdjustments.timeOfDay; }
    void setTimeOfDay(float val) { m_colorAdjustments.timeOfDay = qBound(0.0f, val, 1.0f); }
    ColorGradeMode colorGradeMode() const { return m_colorAdjustments.mode; }
    void setColorGradeMode(ColorGradeMode mode) { m_colorAdjustments.mode = mode; }
    const ColorCurve& lumaCurve() const { return m_colorAdjustments.lumaCurve; }
    void setLumaCurve(const ColorCurve &c) { m_colorAdjustments.lumaCurve = c; }
    const ColorCurve& colorCurve() const { return m_colorAdjustments.colorCurve; }
    void setColorCurve(const ColorCurve &c) { m_colorAdjustments.colorCurve = c; }
    const ColorCurve& redCurve() const { return m_colorAdjustments.redCurve; }
    void setRedCurve(const ColorCurve &c) { m_colorAdjustments.redCurve = c; }
    const ColorCurve& greenCurve() const { return m_colorAdjustments.greenCurve; }
    void setGreenCurve(const ColorCurve &c) { m_colorAdjustments.greenCurve = c; }
    const ColorCurve& blueCurve() const { return m_colorAdjustments.blueCurve; }
    void setBlueCurve(const ColorCurve &c) { m_colorAdjustments.blueCurve = c; }

    int brightness() const { return m_colorAdjustments.brightness; }
    void setBrightness(int b) { m_colorAdjustments.brightness = qBound(-100, b, 100); }
    int luminosity() const { return m_colorAdjustments.luminosity; }
    void setLuminosity(int l) { m_colorAdjustments.luminosity = qBound(-100, l, 100); }
    int redPresence() const { return m_colorAdjustments.red; }
    void setRedPresence(int r) { m_colorAdjustments.red = qBound(-100, r, 100); }
    int greenPresence() const { return m_colorAdjustments.green; }
    void setGreenPresence(int g) { m_colorAdjustments.green = qBound(-100, g, 100); }
    int bluePresence() const { return m_colorAdjustments.blue; }
    void setBluePresence(int b) { m_colorAdjustments.blue = qBound(-100, b, 100); }
    void resetColorAdjustments() { m_colorAdjustments = ColorAdjustments(); }

    qint64 linkedClipId() const { return m_linkedClipId; }
    void setLinkedClipId(qint64 linkedId) { m_linkedClipId = linkedId; }
    bool isLinked() const { return m_linkedClipId > 0; }

    QColor color() const { return m_color; }
    void setColor(const QColor &color) { m_color = color; }

    bool isAudioMuted() const { return m_isAudioMuted; }
    void setAudioMuted(bool muted) { m_isAudioMuted = muted; }

    // Transformation properties (Position, Scale, Rotation)
    double posX() const { return m_posX; }
    void setPosX(double x) { m_posX = x; }

    double posY() const { return m_posY; }
    void setPosY(double y) { m_posY = y; }

    double scaleX() const { return m_scaleX; }
    void setScaleX(double sx) { m_scaleX = qBound(0.05, sx, 10.0); }

    double scaleY() const { return m_scaleY; }
    void setScaleY(double sy) { m_scaleY = qBound(0.05, sy, 10.0); }

    double scale() const { return m_scaleX; }
    void setScale(double s) {
        double bounded = qBound(0.05, s, 10.0);
        m_scaleX = bounded;
        m_scaleY = bounded;
    }

    double rotation() const { return m_rotation; }
    void setRotation(double deg) {
        // Keep within -360 to +360 range
        while (deg > 360.0) deg -= 360.0;
        while (deg < -360.0) deg += 360.0;
        m_rotation = deg;
    }

    void resetTransform() {
        m_posX = 0.0;
        m_posY = 0.0;
        m_scaleX = 1.0;
        m_scaleY = 1.0;
        m_rotation = 0.0;
        m_scaleMode = ClipScaleMode::FillCrop;
        m_motionPath.clearWaypoints();
    }

    ClipScaleMode scaleMode() const { return m_scaleMode; }
    void setScaleMode(ClipScaleMode mode) { m_scaleMode = mode; }

    MotionPath& motionPath() { return m_motionPath; }
    const MotionPath& motionPath() const { return m_motionPath; }
    void setMotionPath(const MotionPath &path) { m_motionPath = path; }
    bool hasMotionPath() const { return m_motionPath.isEnabled(); }

    QPointF positionAt(qint64 timelineMs) const {
        if (!hasMotionPath() || durationMs() <= 0) {
            return QPointF(m_posX, m_posY);
        }
        return m_motionPath.evaluateAtTime(timelineMs, m_timelineInMs, durationMs());
    }

    double posXAt(qint64 timelineMs) const {
        return positionAt(timelineMs).x();
    }

    double posYAt(qint64 timelineMs) const {
        return positionAt(timelineMs).y();
    }

    qint64 mapTimelineToSourceMs(qint64 timelineMs) const;

    double opacityAt(qint64 timelineMs) const;
    double volumeAt(qint64 timelineMs) const;

    // Text Box Properties (for ClipType::Text)
    QString textContent() const { return m_textContent; }
    void setTextContent(const QString &text) { m_textContent = text; }

    QString richTextHtml() const { return m_richTextHtml; }
    void setRichTextHtml(const QString &html) { m_richTextHtml = html; }

    QString fontFamily() const { return m_fontFamily; }
    void setFontFamily(const QString &family) { m_fontFamily = family; }

    int fontSize() const { return m_fontSize; }
    void setFontSize(int size) { m_fontSize = qBound(8, size, 288); }

    bool isBold() const { return m_isBold; }
    void setBold(bool bold) { m_isBold = bold; }

    bool isItalic() const { return m_isItalic; }
    void setItalic(bool italic) { m_isItalic = italic; }

    bool isUnderline() const { return m_isUnderline; }
    void setUnderline(bool underline) { m_isUnderline = underline; }

    QColor textColor() const { return m_textColor; }
    void setTextColor(const QColor &color) { m_textColor = color; }

    QColor backgroundColor() const { return m_backgroundColor; }
    void setBackgroundColor(const QColor &color) { m_backgroundColor = color; }

    int textAlignment() const { return m_textAlignment; }
    void setTextAlignment(int align) { m_textAlignment = align; }

    int textBoxWidth() const { return m_textBoxWidth; }
    void setTextBoxWidth(int w) { m_textBoxWidth = qMax(0, w); }

    int textBoxHeight() const { return m_textBoxHeight; }
    void setTextBoxHeight(int h) { m_textBoxHeight = qMax(0, h); }

    QImage renderTextImage(const QSize &canvasSize = QSize(1920, 1080)) const;

private:
    qint64 m_id = 0;
    qint64 m_trackId = 0;
    QString m_name;
    QString m_filePath;
    ClipType m_type = ClipType::Video;
    qint64 m_timelineInMs = 0;
    qint64 m_timelineOutMs = 5000;
    qint64 m_sourceInMs = 0;
    qint64 m_sourceOutMs = 5000;
    qint64 m_sourceDurationMs = 5000;
    double m_speed = 1.0;
    double m_volume = 1.0;
    double m_opacity = 1.0;
    qint64 m_fadeInMs = 0;
    qint64 m_fadeOutMs = 0;
    TransitionType m_transitionIn = TransitionType::None;
    qint64 m_transitionInDurationMs = 500;
    TransitionType m_transitionOut = TransitionType::None;
    qint64 m_transitionOutDurationMs = 500;
    QVector<VisualFilter> m_filterStack;
    qint64 m_linkedClipId = -1;
    QColor m_color = QColor("#2f81f7");
    bool m_isAudioMuted = false;
    ColorAdjustments m_colorAdjustments;

    // Transforms
    double m_posX = 0.0;
    double m_posY = 0.0;
    double m_scaleX = 1.0;
    double m_scaleY = 1.0;
    double m_rotation = 0.0;
    ClipScaleMode m_scaleMode = ClipScaleMode::FillCrop;
    MotionPath m_motionPath;

    // Text Box Data
    QString m_textContent = "Texto de ejemplo";
    QString m_richTextHtml;
    QString m_fontFamily = "Arial";
    int m_fontSize = 48;
    bool m_isBold = false;
    bool m_isItalic = false;
    bool m_isUnderline = false;
    QColor m_textColor = Qt::white;
    QColor m_backgroundColor = Qt::transparent;
    int m_textAlignment = 0x0004 | 0x0080; // Qt::AlignHCenter | Qt::AlignVCenter
    int m_textBoxWidth = 0;
    int m_textBoxHeight = 0;
};
