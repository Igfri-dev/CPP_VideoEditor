#include "TimeOfDayFilter.h"

// Hardcoded state profiles per specification:
// 0.00f: Night   (Exposure: -2.5f, Tint: Deep Midnight Blue [0.04f, 0.07f, 0.17f], SkyBlend: 1.0f)
const TimeProfile TimeOfDayFilter::ProfileNight = {
    -2.5f,
    {0.04f, 0.07f, 0.17f},
    1.0f
};

// 0.33f: Morning (Exposure: -0.5f, Tint: Pastel Amber [1.00f, 0.71f, 0.65f], SkyBlend: 0.4f)
const TimeProfile TimeOfDayFilter::ProfileMorning = {
    -0.5f,
    {1.00f, 0.71f, 0.65f},
    0.4f
};

// 0.66f: Day     (Exposure: 0.0f, Tint: Neutral White [1.00f, 1.00f, 1.00f], SkyBlend: 0.0f)
const TimeProfile TimeOfDayFilter::ProfileDay = {
    0.0f,
    {1.00f, 1.00f, 1.00f},
    0.0f
};

// 1.00f: Sunset  (Exposure: -0.8f, Tint: Crimson Sunset [0.90f, 0.37f, 0.17f], SkyBlend: 0.8f)
const TimeProfile TimeOfDayFilter::ProfileSunset = {
    -0.8f,
    {0.90f, 0.37f, 0.17f},
    0.8f
};

static inline float LerpFloat(float a, float b, float t) {
    return a + t * (b - a);
}

TimeOfDayFilter::TimeOfDayFilter()
{
    m_lastUniforms.exposure = ProfileDay.exposure;
    m_lastUniforms.colorTint[0] = ProfileDay.tint[0];
    m_lastUniforms.colorTint[1] = ProfileDay.tint[1];
    m_lastUniforms.colorTint[2] = ProfileDay.tint[2];
    m_lastUniforms.skyBlend = ProfileDay.skyBlend;
    m_lastUniforms.timeOfDay = 0.66f;
    m_lastUniforms.textureId = 0;
}

TimeProfile TimeOfDayFilter::CalculateProfile(float sliderValue)
{
    float t = std::clamp(sliderValue, 0.0f, 1.0f);

    const TimeProfile *pStart = nullptr;
    const TimeProfile *pEnd = nullptr;
    float segmentFactor = 0.0f;

    if (t <= 0.33f) {
        // Night -> Morning
        pStart = &ProfileNight;
        pEnd = &ProfileMorning;
        segmentFactor = t / 0.33f;
    } else if (t <= 0.66f) {
        // Morning -> Day
        pStart = &ProfileMorning;
        pEnd = &ProfileDay;
        segmentFactor = (t - 0.33f) / (0.66f - 0.33f);
    } else {
        // Day -> Sunset
        pStart = &ProfileDay;
        pEnd = &ProfileSunset;
        segmentFactor = (t - 0.66f) / (1.00f - 0.66f);
    }

    segmentFactor = std::clamp(segmentFactor, 0.0f, 1.0f);

    TimeProfile result;
    result.exposure = LerpFloat(pStart->exposure, pEnd->exposure, segmentFactor);
    for (int i = 0; i < 3; ++i) {
        result.tint[i] = LerpFloat(pStart->tint[i], pEnd->tint[i], segmentFactor);
    }
    result.skyBlend = LerpFloat(pStart->skyBlend, pEnd->skyBlend, segmentFactor);

    return result;
}

bool TimeOfDayFilter::RenderTimelineSlice(int64_t currentFrame,
                                          int64_t startFrame,
                                          int64_t endFrame,
                                          float sliderValue,
                                          unsigned int textureId)
{
    // Verify timeline slice bounds
    if (currentFrame < startFrame || currentFrame > endFrame) {
        return false;
    }

    float clampedSlider = std::clamp(sliderValue, 0.0f, 1.0f);
    TimeProfile profile = CalculateProfile(clampedSlider);

    // Cache uniforms
    m_lastUniforms.exposure = profile.exposure;
    m_lastUniforms.colorTint[0] = profile.tint[0];
    m_lastUniforms.colorTint[1] = profile.tint[1];
    m_lastUniforms.colorTint[2] = profile.tint[2];
    m_lastUniforms.skyBlend = profile.skyBlend;
    m_lastUniforms.timeOfDay = clampedSlider;
    m_lastUniforms.textureId = textureId;

    // Dispatch uniforms to rendering shader via decoupled callbacks
    if (m_floatSetter) {
        m_floatSetter("u_exposure", profile.exposure);
        m_floatSetter("u_skyBlend", profile.skyBlend);
        m_floatSetter("u_timeOfDay", clampedSlider);
    }

    if (m_vec3Setter) {
        m_vec3Setter("u_colorTint", profile.tint[0], profile.tint[1], profile.tint[2]);
    }

    if (m_textureSetter) {
        m_textureSetter("u_texture", 0, textureId);
    }

    return true;
}

void TimeOfDayFilter::SetUniformSetters(FloatUniformSetter floatSetter,
                                       Vec3UniformSetter vec3Setter,
                                       TextureUniformSetter textureSetter)
{
    m_floatSetter = std::move(floatSetter);
    m_vec3Setter = std::move(vec3Setter);
    m_textureSetter = std::move(textureSetter);
}
