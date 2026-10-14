#pragma once

#include <QString>
#include <QColor>
#include <QImage>
#include <QtGlobal>
#include "motionpath.h"
#include "keyframe.h"

enum class ClipType {
    Video,
    Audio,
    Image,
    Text,
    Adjustment,
    Compound,
    SolidColor
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

enum class MaskShape {
    Rectangle = 0,
    Ellipse = 1,
    Freehand = 2
};

inline QString maskShapeToString(MaskShape shape) {
    switch (shape) {
    case MaskShape::Rectangle: return "Rectangle";
    case MaskShape::Ellipse: return "Ellipse";
    case MaskShape::Freehand: return "Freehand";
    }
    return "Rectangle";
}

inline MaskShape stringToMaskShape(const QString &str) {
    if (str.compare("Ellipse", Qt::CaseInsensitive) == 0) return MaskShape::Ellipse;
    if (str.compare("Freehand", Qt::CaseInsensitive) == 0) return MaskShape::Freehand;
    return MaskShape::Rectangle;
}

struct ClipMask {
    bool enabled = false;
    MaskShape shape = MaskShape::Rectangle;
    QRectF normalizedRect{0.2, 0.2, 0.6, 0.6}; // Normalized coords [0..1]
    QVector<QPointF> polygonPoints;            // Normalized coords [0..1]
    int feather = 0;                           // Feather radius in pixels [0..200]
    double opacity = 1.0;
    bool inverted = false;

    AnimatedProperty<QPointF> animCenter{QPointF(0.5, 0.5)};
    AnimatedProperty<int> animFeather{0};
    AnimatedProperty<double> animOpacity{1.0};

    bool isIdentity() const { return !enabled; }

    bool operator==(const ClipMask &other) const {
        return enabled == other.enabled &&
               shape == other.shape &&
               normalizedRect == other.normalizedRect &&
               polygonPoints == other.polygonPoints &&
               feather == other.feather &&
               std::abs(opacity - other.opacity) < 1e-4 &&
               inverted == other.inverted &&
               animCenter == other.animCenter &&
               animFeather == other.animFeather &&
               animOpacity == other.animOpacity;
    }
    bool operator!=(const ClipMask &other) const { return !(*this == other); }
};

struct SpeedRamp {
    bool enabled = false;
    AnimatedProperty<double> animSpeed{1.0};

    bool operator==(const SpeedRamp &other) const {
        return enabled == other.enabled && animSpeed == other.animSpeed;
    }
    bool operator!=(const SpeedRamp &other) const { return !(*this == other); }

    qint64 calculateSourceOffset(qint64 timelineOffsetMs, double baseSpeed = 1.0) const {
        if (!enabled) {
            return qRound64(timelineOffsetMs * baseSpeed);
        }
        if (!animSpeed.isAnimated()) {
            return qRound64(timelineOffsetMs * animSpeed.baseValue());
        }
        const qint64 step = 20;
        double accumulatedSourceMs = 0.0;
        qint64 t = 0;
        while (t + step <= timelineOffsetMs) {
            double s0 = animSpeed.valueAt(t);
            double s1 = animSpeed.valueAt(t + step);
            accumulatedSourceMs += (s0 + s1) * 0.5 * step;
            t += step;
        }
        if (t < timelineOffsetMs) {
            qint64 rem = timelineOffsetMs - t;
            double s0 = animSpeed.valueAt(t);
            double s1 = animSpeed.valueAt(timelineOffsetMs);
            accumulatedSourceMs += (s0 + s1) * 0.5 * rem;
        }
        return qRound64(accumulatedSourceMs);
    }
};

struct ClipPluginEffect {
    QString pluginId;
    bool enabled = true;
    QMap<QString, QVariant> parameters;
};

#include <memory>
#include <QMap>
#include <QVariant>
#include "filters/chromakey.h"
#include "engine/videostabilizer.h"

class TimelineModel;

QString clipTypeToString(ClipType type);
ClipType stringToClipType(const QString &str);
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
    void setVolume(double volume) {
        m_volume = qBound(0.0, volume, 2.0);
        m_animVolume.setBaseValue(m_volume);
    }

    double opacity() const { return m_opacity; }
    void setOpacity(double opacity) {
        m_opacity = qBound(0.0, opacity, 1.0);
        m_animOpacity.setBaseValue(m_opacity);
    }

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

        m_animTimeOfDay.setBaseValue(m_colorAdjustments.timeOfDay);
        m_animTimeOfDayIntensity.setBaseValue(m_colorAdjustments.timeOfDayIntensity);
        m_animBrightness.setBaseValue(m_colorAdjustments.brightness);
        m_animLuminosity.setBaseValue(m_colorAdjustments.luminosity);
    }
    bool isTimeOfDayEnabled() const { return m_colorAdjustments.timeOfDayEnabled; }
    void setTimeOfDayEnabled(bool enabled) { m_colorAdjustments.timeOfDayEnabled = enabled; }
    float timeOfDay() const { return m_colorAdjustments.timeOfDay; }
    void setTimeOfDay(float val) {
        m_colorAdjustments.timeOfDay = qBound(0.0f, val, 1.0f);
        m_animTimeOfDay.setBaseValue(m_colorAdjustments.timeOfDay);
    }
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
    void setBrightness(int b) {
        m_colorAdjustments.brightness = qBound(-100, b, 100);
        m_animBrightness.setBaseValue(m_colorAdjustments.brightness);
    }
    int luminosity() const { return m_colorAdjustments.luminosity; }
    void setLuminosity(int l) {
        m_colorAdjustments.luminosity = qBound(-100, l, 100);
        m_animLuminosity.setBaseValue(m_colorAdjustments.luminosity);
    }
    int redPresence() const { return m_colorAdjustments.red; }
    void setRedPresence(int r) { m_colorAdjustments.red = qBound(-100, r, 100); }
    int greenPresence() const { return m_colorAdjustments.green; }
    void setGreenPresence(int g) { m_colorAdjustments.green = qBound(-100, g, 100); }
    int bluePresence() const { return m_colorAdjustments.blue; }
    void setBluePresence(int b) { m_colorAdjustments.blue = qBound(-100, b, 100); }
    void resetColorAdjustments() {
        m_colorAdjustments = ColorAdjustments();
        m_animTimeOfDay.setBaseValue(0.60f);
        m_animTimeOfDay.clearKeyframes();
        m_animTimeOfDayIntensity.setBaseValue(1.0f);
        m_animTimeOfDayIntensity.clearKeyframes();
        m_animBrightness.setBaseValue(0);
        m_animBrightness.clearKeyframes();
        m_animLuminosity.setBaseValue(0);
        m_animLuminosity.clearKeyframes();
    }

    qint64 linkedClipId() const { return m_linkedClipId; }
    void setLinkedClipId(qint64 linkedId) { m_linkedClipId = linkedId; }
    bool isLinked() const { return m_linkedClipId > 0; }

    QColor color() const { return m_color; }
    void setColor(const QColor &color) { m_color = color; }

    bool isAudioMuted() const { return m_isAudioMuted; }
    void setAudioMuted(bool muted) { m_isAudioMuted = muted; }

    // Transformation properties (Position, Scale, Rotation)
    double posX() const { return m_posX; }
    void setPosX(double x) {
        m_posX = x;
        m_animPosition.setBaseValue(QPointF(m_posX, m_posY));
    }

    double posY() const { return m_posY; }
    void setPosY(double y) {
        m_posY = y;
        m_animPosition.setBaseValue(QPointF(m_posX, m_posY));
    }

    double scaleX() const { return m_scaleX; }
    void setScaleX(double sx) {
        m_scaleX = qBound(0.05, sx, 10.0);
        m_animScaleX.setBaseValue(m_scaleX);
    }

    double scaleY() const { return m_scaleY; }
    void setScaleY(double sy) {
        m_scaleY = qBound(0.05, sy, 10.0);
        m_animScaleY.setBaseValue(m_scaleY);
    }

    double scale() const { return m_scaleX; }
    void setScale(double s) {
        double bounded = qBound(0.05, s, 10.0);
        m_scaleX = bounded;
        m_scaleY = bounded;
        m_animScaleX.setBaseValue(bounded);
        m_animScaleY.setBaseValue(bounded);
    }

    double rotation() const { return m_rotation; }
    void setRotation(double deg) {
        // Keep within -360 to +360 range
        while (deg > 360.0) deg -= 360.0;
        while (deg < -360.0) deg += 360.0;
        m_rotation = deg;
        m_animRotation.setBaseValue(deg);
    }

    void resetTransform() {
        m_posX = 0.0;
        m_posY = 0.0;
        m_scaleX = 1.0;
        m_scaleY = 1.0;
        m_rotation = 0.0;
        m_scaleMode = ClipScaleMode::FillCrop;
        m_motionPath.clearWaypoints();
        m_animPosition.setBaseValue(QPointF(0, 0));
        m_animPosition.clearKeyframes();
        m_animScaleX.setBaseValue(1.0);
        m_animScaleX.clearKeyframes();
        m_animScaleY.setBaseValue(1.0);
        m_animScaleY.clearKeyframes();
        m_animRotation.setBaseValue(0.0);
        m_animRotation.clearKeyframes();
    }

    ClipScaleMode scaleMode() const { return m_scaleMode; }
    void setScaleMode(ClipScaleMode mode) { m_scaleMode = mode; }

    MotionPath& motionPath() { return m_motionPath; }
    const MotionPath& motionPath() const { return m_motionPath; }
    void setMotionPath(const MotionPath &path) { m_motionPath = path; }
    bool hasMotionPath() const { return m_motionPath.isEnabled(); }

    // Keyframe Animation System (AnimatedProperty<T>)
    const AnimatedProperty<float>& animTimeOfDay() const { return m_animTimeOfDay; }
    AnimatedProperty<float>& animTimeOfDay() { return m_animTimeOfDay; }
    void setAnimTimeOfDay(const AnimatedProperty<float> &anim) { m_animTimeOfDay = anim; }

    const AnimatedProperty<float>& animTimeOfDayIntensity() const { return m_animTimeOfDayIntensity; }
    AnimatedProperty<float>& animTimeOfDayIntensity() { return m_animTimeOfDayIntensity; }
    void setAnimTimeOfDayIntensity(const AnimatedProperty<float> &anim) { m_animTimeOfDayIntensity = anim; }

    const AnimatedProperty<int>& animBrightness() const { return m_animBrightness; }
    AnimatedProperty<int>& animBrightness() { return m_animBrightness; }
    void setAnimBrightness(const AnimatedProperty<int> &anim) { m_animBrightness = anim; }

    const AnimatedProperty<int>& animLuminosity() const { return m_animLuminosity; }
    AnimatedProperty<int>& animLuminosity() { return m_animLuminosity; }
    void setAnimLuminosity(const AnimatedProperty<int> &anim) { m_animLuminosity = anim; }

    const AnimatedProperty<double>& animOpacity() const { return m_animOpacity; }
    AnimatedProperty<double>& animOpacity() { return m_animOpacity; }
    void setAnimOpacity(const AnimatedProperty<double> &anim) { m_animOpacity = anim; }

    const AnimatedProperty<double>& animVolume() const { return m_animVolume; }
    AnimatedProperty<double>& animVolume() { return m_animVolume; }
    void setAnimVolume(const AnimatedProperty<double> &anim) { m_animVolume = anim; }

    const AnimatedProperty<double>& animScaleX() const { return m_animScaleX; }
    AnimatedProperty<double>& animScaleX() { return m_animScaleX; }
    void setAnimScaleX(const AnimatedProperty<double> &anim) { m_animScaleX = anim; }

    const AnimatedProperty<double>& animScaleY() const { return m_animScaleY; }
    AnimatedProperty<double>& animScaleY() { return m_animScaleY; }
    void setAnimScaleY(const AnimatedProperty<double> &anim) { m_animScaleY = anim; }

    const AnimatedProperty<double>& animScale() const { return m_animScaleX; }
    AnimatedProperty<double>& animScale() { return m_animScaleX; }
    void setAnimScale(const AnimatedProperty<double> &anim) { m_animScaleX = anim; m_animScaleY = anim; }

    const AnimatedProperty<double>& animRotation() const { return m_animRotation; }
    AnimatedProperty<double>& animRotation() { return m_animRotation; }
    void setAnimRotation(const AnimatedProperty<double> &anim) { m_animRotation = anim; }

    const AnimatedProperty<QPointF>& animPosition() const { return m_animPosition; }
    AnimatedProperty<QPointF>& animPosition() { return m_animPosition; }
    void setAnimPosition(const AnimatedProperty<QPointF> &anim) { m_animPosition = anim; }

    bool hasKeyframes() const {
        return m_animTimeOfDay.isAnimated() || m_animTimeOfDayIntensity.isAnimated() ||
               m_animBrightness.isAnimated() || m_animLuminosity.isAnimated() ||
               m_animOpacity.isAnimated() || m_animVolume.isAnimated() ||
               m_animScaleX.isAnimated() || m_animScaleY.isAnimated() ||
               m_animRotation.isAnimated() || m_animPosition.isAnimated();
    }

    void clearAllKeyframes() {
        m_animTimeOfDay.clearKeyframes();
        m_animTimeOfDayIntensity.clearKeyframes();
        m_animBrightness.clearKeyframes();
        m_animLuminosity.clearKeyframes();
        m_animOpacity.clearKeyframes();
        m_animVolume.clearKeyframes();
        m_animScaleX.clearKeyframes();
        m_animScaleY.clearKeyframes();
        m_animRotation.clearKeyframes();
        m_animPosition.clearKeyframes();
    }

    QPointF positionAt(qint64 timelineMs) const;
    double posXAt(qint64 timelineMs) const;
    double posYAt(qint64 timelineMs) const;
    double scaleXAt(qint64 timelineMs) const;
    double scaleYAt(qint64 timelineMs) const;
    double scaleAt(qint64 timelineMs) const;
    double rotationAt(qint64 timelineMs) const;
    float timeOfDayAt(qint64 timelineMs) const;
    ColorAdjustments colorAdjustmentsAt(qint64 timelineMs) const;

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

    // Mask System
    const ClipMask& mask() const { return m_mask; }
    ClipMask& mask() { return m_mask; }
    void setMask(const ClipMask &m) { m_mask = m; }
    bool hasMask() const { return m_mask.enabled; }

    // Speed Ramp / Time Remapping System
    const SpeedRamp& speedRamp() const { return m_speedRamp; }
    SpeedRamp& speedRamp() { return m_speedRamp; }
    void setSpeedRamp(const SpeedRamp &sr) { m_speedRamp = sr; }
    bool hasSpeedRamp() const { return m_speedRamp.enabled; }

    // Professional Chroma Key System
    const ChromaKeySettings& chromaKey() const { return m_chromaKey; }
    ChromaKeySettings& chromaKey() { return m_chromaKey; }
    void setChromaKey(const ChromaKeySettings &ck) { m_chromaKey = ck; }
    bool hasChromaKey() const { return m_chromaKey.enabled; }

    // Video Stabilization System
    const StabilizationSettings& stabilization() const { return m_stabilization; }
    StabilizationSettings& stabilization() { return m_stabilization; }
    void setStabilization(const StabilizationSettings &s) { m_stabilization = s; }
    bool hasStabilization() const { return m_stabilization.enabled; }

    // Compound / Nested Timeline Clip
    std::shared_ptr<TimelineModel> subTimeline() const { return m_subTimeline; }
    void setSubTimeline(std::shared_ptr<TimelineModel> subTl) { m_subTimeline = subTl; }

    // Plugin Effects Architecture
    const QVector<ClipPluginEffect>& pluginEffects() const { return m_pluginEffects; }
    QVector<ClipPluginEffect>& pluginEffects() { return m_pluginEffects; }
    void addPluginEffect(const ClipPluginEffect &effect) { m_pluginEffects.append(effect); }
    void removePluginEffectAt(int index) {
        if (index >= 0 && index < m_pluginEffects.size()) {
            m_pluginEffects.removeAt(index);
        }
    }
    void clearPluginEffects() { m_pluginEffects.clear(); }
    bool hasPluginEffects() const { return !m_pluginEffects.isEmpty(); }

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

    // Feature extensions
    ClipMask m_mask;
    SpeedRamp m_speedRamp;
    ChromaKeySettings m_chromaKey;
    StabilizationSettings m_stabilization;
    std::shared_ptr<TimelineModel> m_subTimeline;
    QVector<ClipPluginEffect> m_pluginEffects;

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

    // Animated Properties
    AnimatedProperty<float> m_animTimeOfDay{0.60f};
    AnimatedProperty<float> m_animTimeOfDayIntensity{1.0f};
    AnimatedProperty<int> m_animBrightness{0};
    AnimatedProperty<int> m_animLuminosity{0};
    AnimatedProperty<double> m_animOpacity{1.0};
    AnimatedProperty<double> m_animVolume{1.0};
    AnimatedProperty<double> m_animScaleX{1.0};
    AnimatedProperty<double> m_animScaleY{1.0};
    AnimatedProperty<double> m_animRotation{0.0};
    AnimatedProperty<QPointF> m_animPosition{QPointF(0, 0)};
};
