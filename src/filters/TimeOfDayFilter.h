#pragma once

#include <cstdint>
#include <array>
#include <functional>
#include <algorithm>
#include <cmath>

/**
 * @struct TimeProfile
 * @brief Represents the physical color grading and celestial parameters for a specific time of day.
 */
struct TimeProfile {
    float exposure = 0.0f;      ///< Exposure compensation in EV stops (shifts luminosity by pow(2.0, exposure))
    float tint[3] = {1.0f, 1.0f, 1.0f}; ///< Linear RGB color tint multiplier [R, G, B]
    float skyBlend = 0.0f;      ///< Influence / blend factor of procedural celestial sky gradient [0.0, 1.0]
};

/**
 * @struct TimeOfDayUniforms
 * @brief Uniform values calculated from linear interpolation ready to be bound to a GPU fragment shader.
 */
struct TimeOfDayUniforms {
    float exposure = 0.0f;
    float colorTint[3] = {1.0f, 1.0f, 1.0f};
    float skyBlend = 0.0f;
    float timeOfDay = 0.66f;
    unsigned int textureId = 0;
};

/**
 * @class TimeOfDayFilter
 * @brief Cross-platform color grading filter simulating the diurnal cycle (Night -> Morning -> Day -> Sunset).
 * 
 * Linearly interpolates across 4 calibrated profiles:
 *  - 0.00: Night   (Exposure: -2.5, Tint: Midnight Blue [0.04, 0.07, 0.17], SkyBlend: 1.0)
 *  - 0.33: Morning (Exposure: -0.5, Tint: Pastel Amber [1.00, 0.71, 0.65], SkyBlend: 0.4)
 *  - 0.66: Day     (Exposure:  0.0, Tint: Neutral White [1.00, 1.00, 1.00], SkyBlend: 0.0)
 *  - 1.00: Sunset  (Exposure: -0.8, Tint: Crimson Sunset [0.90, 0.37, 0.17], SkyBlend: 0.8)
 */
class TimeOfDayFilter {
public:
    // Uniform callback types for toolkit/windowing agnostic rendering
    using FloatUniformSetter = std::function<void(const char* name, float value)>;
    using Vec3UniformSetter = std::function<void(const char* name, float x, float y, float z)>;
    using TextureUniformSetter = std::function<void(const char* name, unsigned int textureUnit, unsigned int textureId)>;

    TimeOfDayFilter();
    ~TimeOfDayFilter() = default;

    /**
     * @brief Computes piecewise linear interpolation across diurnal profiles for a slider value [0.0f, 1.0f].
     * @param sliderValue Clamped value between 0.0 (Night) and 1.0 (Sunset).
     * @return Interpolated TimeProfile with calculated exposure, tint, and sky blend.
     */
    static TimeProfile CalculateProfile(float sliderValue);

    /**
     * @brief Processes a user-selected slice of the timeline.
     * 
     * Verifies if currentFrame falls within [startFrame, endFrame]. If valid:
     * 1. Interpolates profiles according to sliderValue.
     * 2. Sets uniform parameters via agnostic uniform callbacks or updates cached uniforms.
     * 
     * @param currentFrame The current frame being rendered in the timeline.
     * @param startFrame Beginning frame of the user-selected active slice.
     * @param endFrame Ending frame of the user-selected active slice.
     * @param sliderValue Diurnal slider value [0.0f, 1.0f].
     * @param textureId OpenGL / GPU texture ID of the input video frame.
     * @return true if currentFrame was within [startFrame, endFrame] and uniforms were processed; false otherwise.
     */
    bool RenderTimelineSlice(int64_t currentFrame,
                             int64_t startFrame,
                             int64_t endFrame,
                             float sliderValue,
                             unsigned int textureId);

    /**
     * @brief Retrieve last computed uniforms from RenderTimelineSlice.
     */
    const TimeOfDayUniforms& CurrentUniforms() const { return m_lastUniforms; }

    /**
     * @brief Configures decoupled uniform setters for direct integration with any OpenGL/Vulkan/Metal pipeline.
     */
    void SetUniformSetters(FloatUniformSetter floatSetter,
                           Vec3UniformSetter vec3Setter,
                           TextureUniformSetter textureSetter);

    // Hardcoded profile presets
    static const TimeProfile ProfileNight;
    static const TimeProfile ProfileMorning;
    static const TimeProfile ProfileDay;
    static const TimeProfile ProfileSunset;

private:
    FloatUniformSetter m_floatSetter;
    Vec3UniformSetter m_vec3Setter;
    TextureUniformSetter m_textureSetter;
    TimeOfDayUniforms m_lastUniforms;
};
