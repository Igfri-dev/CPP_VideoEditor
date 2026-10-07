#pragma once

#include <cstdint>
#include <array>
#include <vector>
#include <string>
#include <functional>
#include <algorithm>
#include <cmath>

/**
 * @struct TimeProfile
 * @brief Natural Time-of-Day Relighting Profile.
 *
 * Implements physically and perceptually sound lighting parameters:
 *  - Photometric Exposure (EV stops in linear light)
 *  - Correlated Color Temperature & Tint (Kelvin / Bradford adaptation)
 *  - Luminance-based Split Toning (Shadows, Midtones, Highlights)
 *  - Selective Purkinje scotopic vision shift
 *  - Skin tone protection & soft sky enhancement
 *  - 3D LUT photographic filmic look
 */
struct TimeProfile {
    // Photometric Exposure in linear space
    float exposureEV = 0.0f;             ///< Stops in EV (shifts linear light by pow(2.0, exposureEV))

    // Global White Balance & Chromatic Adaptation
    float temperature = 6500.0f;         ///< Correlated color temperature in Kelvin (D65 = 6500 K)
    float tint = 0.0f;                   ///< Green (-) to Magenta (+) tint offset [-1.0, 1.0]

    // Contrast & Saturation in linear perceptual luminance space
    float contrast = 1.0f;               ///< Contrast curve slope (1.0 = neutral)
    float saturation = 1.0f;             ///< Color saturation factor (1.0 = neutral)

    // Luminance Split Toning (Separate treatment for Shadows, Midtones, Highlights)
    float shadowTemperature = 6500.0f;   ///< Shadow color temperature in Kelvin
    float shadowTint = 0.0f;             ///< Shadow tint offset
    float shadowLift = 0.0f;             ///< Shadow black level lift [0.0, 0.1]

    float midtoneTemperature = 6500.0f;  ///< Midtone color temperature in Kelvin
    float midtoneGain = 1.0f;            ///< Midtone luminance multiplier

    float highlightTemperature = 6500.0f;///< Highlight color temperature in Kelvin
    float highlightGain = 1.0f;          ///< Highlight luminance multiplier
    float highlightRolloff = 0.20f;      ///< Filmic shoulder rolloff factor to prevent harsh clipping

    // Biological / Optical Perceptual Effects
    float purkinjeStrength = 0.0f;       ///< Scotopic rod response shift in dark tones [0.0, 1.0]
    float skyExposureDrop = 0.0f;        ///< Soft exposure compression on detected sky [0.0, 1.0]
    float skinProtection = 0.0f;         ///< Human skin tone preservation strength [0.0, 1.0]

    // 3D LUT Film Look
    float lutStrength = 0.0f;            ///< 3D LUT influence factor [0.0, 1.0]

    // Convenience legacy accessor
    float exposure() const { return exposureEV; }
};

/**
 * @struct Lut3D
 * @brief Discrete 3D Color Look-Up Table (typically 32x32x32 or 64x64x64).
 */
struct Lut3D {
    int size = 32;
    std::vector<float> data; // size * size * size * 3 floats (RGB)

    bool isValid() const {
        return size > 1 && data.size() == static_cast<size_t>(size * size * size * 3);
    }
};

class QImage;

/**
 * @struct RelativeSettings
 * @brief User-controllable modifiers and advanced biases for Relative Relighting.
 */
struct RelativeSettings {
    float intensity = 1.0f;              ///< Relighting Strength [0.0, 1.0] (0 = original, 1 = full relighted)
    float skinProtectionFactor = 1.0f;   ///< Multiplier for skin protection [0.0, 1.0]
    float skyInfluenceFactor = 1.0f;     ///< Multiplier for sky exposure drop [0.0, 1.0]
    float highlightWarmthBias = 0.0f;    ///< Highlight warmth bias [-1.0, 1.0]
    float shadowCoolnessBias = 0.0f;     ///< Shadow coolness bias [-1.0, 1.0]
    float exposureBias = 0.0f;           ///< Photometric exposure bias in EV [-2.0, 2.0]
    float lutStrengthFactor = 1.0f;      ///< 3D LUT influence multiplier [0.0, 1.0]
};

/**
 * @struct FrameAnalysisResult
 * @brief Optical and colorimetric estimation of input video lighting.
 */
struct FrameAnalysisResult {
    float estimatedTimeOfDay = 0.60f;    ///< Estimated source diurnal time [0.0, 1.0] (0.60 = neutral Noon)
    float estimatedCCT = 6500.0f;        ///< Estimated Correlated Color Temperature in Kelvin
    float meanLuminance = 0.25f;         ///< Estimated scene mean linear luminance
    float confidence = 1.0f;             ///< Estimation confidence score [0.0, 1.0]
};

/**
 * @struct TimeOfDayUniforms
 * @brief Interpolated uniform parameters ready to be uploaded to the GPU fragment shader.
 */
struct TimeOfDayUniforms {
    float exposureEV = 0.0f;
    float whiteBalanceGains[3] = {1.0f, 1.0f, 1.0f};
    float shadowGains[3] = {1.0f, 1.0f, 1.0f};
    float midtoneGains[3] = {1.0f, 1.0f, 1.0f};
    float highlightGains[3] = {1.0f, 1.0f, 1.0f};
    float contrast = 1.0f;
    float saturation = 1.0f;
    float shadowLift = 0.0f;
    float midtoneGain = 1.0f;
    float highlightGain = 1.0f;
    float highlightRolloff = 0.20f;
    float purkinjeStrength = 0.0f;
    float skyExposureDrop = 0.0f;
    float skinProtection = 0.0f;
    float lutStrength = 0.0f;
    float timeOfDay = 0.60f;
    float sourceTimeOfDay = 0.60f;
    float intensity = 1.0f;
    unsigned int textureId = 0;
};

/**
 * @class TimeOfDayFilter
 * @brief Natural Time-of-Day Relighting engine simulating diurnal illumination.
 *
 * Linearly & Hermite splines interpolate across 7 calibrated states:
 *  - 0.00: Night        (Moonlit scotopic vision, -1.8 EV, cool shadows, neutral highlights)
 *  - 0.15: Blue Hour    (Atmospheric twilight blue, -1.1 EV, 9200K, saturated sky)
 *  - 0.28: Dawn         (First light, -0.6 EV, pastel pink/amber glow, 5200K)
 *  - 0.42: Morning      (Crisp sunlight, -0.2 EV, 5800K)
 *  - 0.60: Noon / Day   (NEUTRAL REFERENCE: 0.0 EV, 6500K D65, exact 0ms identity)
 *  - 0.82: Golden Hour  (Warm low sun, 3800K, cool shadows, protected skin tones, 3200K highlights)
 *  - 1.00: Sunset       (Deep crimson horizon, 3000K, rich amber highlights, high rolloff)
 */
class TimeOfDayFilter {
public:
    using RelativeSettings = ::RelativeSettings;
    using FrameAnalysisResult = ::FrameAnalysisResult;

    // Agnostic uniform callbacks
    using FloatUniformSetter = std::function<void(const char* name, float value)>;
    using Vec3UniformSetter = std::function<void(const char* name, float x, float y, float z)>;
    using TextureUniformSetter = std::function<void(const char* name, unsigned int textureUnit, unsigned int textureId)>;

    TimeOfDayFilter();
    ~TimeOfDayFilter() = default;

    /**
     * @brief Computes smooth Hermite spline interpolation across the 7 diurnal profiles.
     * @param sliderValue Clamped value between 0.0f and 1.0f.
     * @return Interpolated TimeProfile with all relighting parameters.
     */
    static TimeProfile CalculateProfile(float sliderValue);

    /**
     * @brief Computes relative relighting delta profile between source lighting and target time.
     * @param sourceTime Reference source time [0.0f, 1.0f] (0.60f = neutral Noon).
     * @param targetTime Target time [0.0f, 1.0f].
     * @param settings Advanced modifiers (intensity, biases, skin protection, etc.).
     * @return Relative TimeProfile containing photometric and chromatic deltas.
     */
    static TimeProfile CalculateRelativeProfile(float sourceTime,
                                                float targetTime,
                                                const RelativeSettings &settings = RelativeSettings());

    /**
     * @brief Fast, deterministic analysis of input video frame to estimate source lighting.
     * @param image Input frame.
     * @return FrameAnalysisResult with estimated source time, CCT and luminance.
     */
    static FrameAnalysisResult AnalyzeFrame(const QImage &image);

    /**
     * @brief Convenience helper returning estimated source timeOfDay in [0.0f, 1.0f].
     */
    static float EstimateSourceTime(const QImage &image);

    /**
     * @brief Converts color temperature (Kelvin) and green-magenta tint into normalized RGB gains.
     * @param kelvin Correlated color temperature in Kelvin (D65 daylight = 6500K).
     * @param tint Green (-) to Magenta (+) tint offset in [-1.0, 1.0].
     * @return RGB linear multipliers normalized so that D65 (6500K, 0.0) returns {1.0, 1.0, 1.0}.
     */
    static std::array<float, 3> KelvinToRGB(float kelvin, float tint = 0.0f);

    /**
     * @brief Returns human-readable stage name for UI display.
     */
    static const char* GetPhaseName(float sliderValue);

    /**
     * @brief Returns a representative simulated timecode string (e.g. "18:30").
     */
    static const char* GetSimulatedTime(float sliderValue);

    /**
     * @brief Generates an algorithmic 3D LUT corresponding to a TimeProfile.
     */
    static Lut3D GenerateProfileLut(const TimeProfile &profile, int size = 32);

    /**
     * @brief Retrieves a thread-safe cached 3D LUT for a given profile.
     */
    static Lut3D GetCachedProfileLut(const TimeProfile &profile, int size = 16);

    /**
     * @brief Returns the GLSL fragment shader source for TimeOfDay.frag.
     */
    static std::string GetFragmentShaderSource();

    /**
     * @brief Trilinear sampling of a 3D LUT.
     */
    static std::array<float, 3> SampleLutTrilinear(const Lut3D &lut, float r, float g, float b);

    /**
     * @brief Loads a standard .cube format 3D LUT file into memory.
     */
    static bool LoadCubeFile(const std::string &filePath, Lut3D &outLut);

    /**
     * @brief Renders a slice of timeline frames with relative relighting and uniform dispatch.
     */
    bool RenderTimelineSlice(int64_t currentFrame,
                             int64_t startFrame,
                             int64_t endFrame,
                             float targetTime,
                             float sourceTime,
                             float intensity,
                             unsigned int textureId,
                             const RelativeSettings &settings = RelativeSettings());

    /**
     * @brief Backwards-compatible overload using neutral Noon as source and 100% intensity.
     */
    bool RenderTimelineSlice(int64_t currentFrame,
                             int64_t startFrame,
                             int64_t endFrame,
                             float sliderValue,
                             unsigned int textureId);

    const TimeOfDayUniforms& CurrentUniforms() const { return m_lastUniforms; }

    void SetUniformSetters(FloatUniformSetter floatSetter,
                           Vec3UniformSetter vec3Setter,
                           TextureUniformSetter textureSetter);

    // 7 Calibrated Diurnal Profile States
    static const TimeProfile ProfileNight;
    static const TimeProfile ProfileBlueHour;
    static const TimeProfile ProfileDawn;
    static const TimeProfile ProfileMorning;
    static const TimeProfile ProfileNoon;
    static const TimeProfile ProfileDay;       ///< Backwards-compatible alias to ProfileNoon
    static const TimeProfile ProfileGoldenHour;
    static const TimeProfile ProfileSunset;

private:
    FloatUniformSetter m_floatSetter;
    Vec3UniformSetter m_vec3Setter;
    TextureUniformSetter m_textureSetter;
    TimeOfDayUniforms m_lastUniforms;
};
