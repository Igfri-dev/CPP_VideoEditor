#include "TimeOfDayFilter.h"
#include <fstream>
#include <sstream>
#include <iostream>

// ============================================================================
// 7 Calibrated Diurnal Profile States per Colorimetric Specification
// ============================================================================

// 0.00: Night (Moonlit scotopic vision, -1.8 EV, cool shadows, neutral highlights)
const TimeProfile TimeOfDayFilter::ProfileNight = {
    -1.80f,      // exposureEV
    7500.0f,     // temperature
    0.02f,       // tint
    0.95f,       // contrast
    0.65f,       // saturation (mesopic vision desaturates)
    8500.0f,     // shadowTemperature (cool moonlit shadows)
    0.03f,       // shadowTint
    0.015f,      // shadowLift
    7200.0f,     // midtoneTemperature
    0.90f,       // midtoneGain
    6200.0f,     // highlightTemperature (artificial light/lamps kept warm & neutral!)
    1.05f,       // highlightGain
    0.25f,       // highlightRolloff
    0.40f,       // purkinjeStrength (rod photoreception shift in shadows)
    0.45f,       // skyExposureDrop
    0.35f,       // skinProtection
    0.25f        // lutStrength
};

// 0.15: Blue Hour (Atmospheric twilight blue, -1.1 EV, 9200K, saturated sky)
const TimeProfile TimeOfDayFilter::ProfileBlueHour = {
    -1.10f,      // exposureEV
    9200.0f,     // temperature
    0.05f,       // tint (delicate twilight magenta)
    1.05f,       // contrast
    1.10f,       // saturation
    9600.0f,     // shadowTemperature
    0.04f,       // shadowTint
    0.005f,      // shadowLift
    8800.0f,     // midtoneTemperature
    0.95f,       // midtoneGain
    6500.0f,     // highlightTemperature
    1.00f,       // highlightGain
    0.20f,       // highlightRolloff
    0.15f,       // purkinjeStrength
    0.25f,       // skyExposureDrop
    0.40f,       // skinProtection
    0.30f        // lutStrength
};

// 0.28: Dawn / Amanecer (First light, -0.6 EV, pastel pink/amber glow, 5200K)
const TimeProfile TimeOfDayFilter::ProfileDawn = {
    -0.60f,      // exposureEV
    5200.0f,     // temperature
    0.08f,       // tint (rosy pastel twilight)
    0.98f,       // contrast
    0.95f,       // saturation
    7200.0f,     // shadowTemperature (lingering cool night shadows)
    0.02f,       // shadowTint
    0.000f,      // shadowLift
    5600.0f,     // midtoneTemperature
    1.00f,       // midtoneGain
    4800.0f,     // highlightTemperature (warm pastel sunrise glow)
    1.05f,       // highlightGain
    0.20f,       // highlightRolloff
    0.00f,       // purkinjeStrength
    0.10f,       // skyExposureDrop
    0.45f,       // skinProtection
    0.25f        // lutStrength
};

// 0.42: Morning / Mañana (Crisp morning sunlight, -0.2 EV, 5800K)
const TimeProfile TimeOfDayFilter::ProfileMorning = {
    -0.20f,      // exposureEV
    5800.0f,     // temperature
    0.00f,       // tint
    1.02f,       // contrast
    1.02f,       // saturation
    6800.0f,     // shadowTemperature
    0.00f,       // shadowTint
    0.000f,      // shadowLift
    6000.0f,     // midtoneTemperature
    1.00f,       // midtoneGain
    5400.0f,     // highlightTemperature
    1.02f,       // highlightGain
    0.15f,       // highlightRolloff
    0.00f,       // purkinjeStrength
    0.05f,       // skyExposureDrop
    0.45f,       // skinProtection
    0.15f        // lutStrength
};

// 0.60: Noon / Mediodía (EXACT NEUTRAL REFERENCE: 0.0 EV, 6500K D65 neutral)
const TimeProfile TimeOfDayFilter::ProfileNoon = {
    0.00f,       // exposureEV
    6500.0f,     // temperature (Standard D65 daylight)
    0.00f,       // tint
    1.00f,       // contrast
    1.00f,       // saturation
    6500.0f,     // shadowTemperature
    0.00f,       // shadowTint
    0.000f,      // shadowLift
    6500.0f,     // midtoneTemperature
    1.00f,       // midtoneGain
    6500.0f,     // highlightTemperature
    1.00f,       // highlightGain
    0.00f,       // highlightRolloff (exact linear identity)
    0.00f,       // purkinjeStrength
    0.00f,       // skyExposureDrop
    0.00f,       // skinProtection
    0.00f        // lutStrength
};

// Backwards-compatible alias to ProfileNoon
const TimeProfile TimeOfDayFilter::ProfileDay = TimeOfDayFilter::ProfileNoon;

// 0.82: Golden Hour (Rich golden low-angle sun, -0.25 EV, 3800K, cool shadows, protected skin)
const TimeProfile TimeOfDayFilter::ProfileGoldenHour = {
    -0.25f,      // exposureEV
    3800.0f,     // temperature
    -0.04f,      // tint (golden-amber warmth)
    1.08f,       // contrast
    1.10f,       // saturation
    7600.0f,     // shadowTemperature (crucial: shadows remain cool/cyan, creating rich chromatic depth!)
    -0.02f,      // shadowTint
    0.000f,      // shadowLift
    4800.0f,     // midtoneTemperature
    1.05f,       // midtoneGain
    3200.0f,     // highlightTemperature (radiant golden highlights)
    1.10f,       // highlightGain
    0.25f,       // highlightRolloff
    0.00f,       // purkinjeStrength
    0.15f,       // skyExposureDrop
    0.65f,       // skinProtection (protects faces from turning into orange masks)
    0.35f        // lutStrength
};

// 1.00: Sunset / Atardecer (Deep crimson horizon, -0.7 EV, 3000K, rich amber highlights)
const TimeProfile TimeOfDayFilter::ProfileSunset = {
    -0.70f,      // exposureEV
    3000.0f,     // temperature
    0.08f,       // tint (crimson twilight accent)
    1.12f,       // contrast
    1.15f,       // saturation
    8200.0f,     // shadowTemperature (deep contrasting twilight shadows)
    0.04f,       // shadowTint
    0.010f,      // shadowLift
    4200.0f,     // midtoneTemperature
    1.08f,       // midtoneGain
    2600.0f,     // highlightTemperature (deep amber/red specular sunset glow)
    1.15f,       // highlightGain
    0.35f,       // highlightRolloff
    0.00f,       // purkinjeStrength
    0.25f,       // skyExposureDrop
    0.60f,       // skinProtection
    0.40f        // lutStrength
};

static inline float LerpFloat(float a, float b, float t) {
    return a + t * (b - a);
}

static inline float Smoothstep01(float x) {
    float t = std::clamp(x, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

TimeOfDayFilter::TimeOfDayFilter()
{
    m_lastUniforms.exposureEV = ProfileNoon.exposureEV;
    auto wb = KelvinToRGB(ProfileNoon.temperature, ProfileNoon.tint);
    m_lastUniforms.whiteBalanceGains[0] = wb[0];
    m_lastUniforms.whiteBalanceGains[1] = wb[1];
    m_lastUniforms.whiteBalanceGains[2] = wb[2];
    m_lastUniforms.contrast = ProfileNoon.contrast;
    m_lastUniforms.saturation = ProfileNoon.saturation;
    m_lastUniforms.timeOfDay = 0.60f;
    m_lastUniforms.textureId = 0;
}

std::array<float, 3> TimeOfDayFilter::KelvinToRGB(float kelvin, float tint)
{
    float k = std::clamp(kelvin, 1800.0f, 16000.0f);
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;

    // Physical polynomial approximation of Planckian blackbody locus normalized to D65 (6500 K)
    if (k <= 6500.0f) {
        float factor = (6500.0f - k) / (6500.0f - 1800.0f);
        r = 1.0f + factor * 0.95f;
        b = 1.0f - factor * 0.65f;
        g = 1.0f - factor * 0.08f;
    } else {
        float factor = (k - 6500.0f) / (16000.0f - 6500.0f);
        r = 1.0f - factor * 0.40f;
        b = 1.0f + factor * 0.65f;
        g = 1.0f + factor * 0.04f;
    }

    // Green-Magenta tint axis adjustment
    g -= tint * 0.35f;
    r += tint * 0.08f;
    b += tint * 0.08f;

    return { std::max(0.01f, r), std::max(0.01f, g), std::max(0.01f, b) };
}

TimeProfile TimeOfDayFilter::CalculateProfile(float sliderValue)
{
    float t = std::clamp(sliderValue, 0.0f, 1.0f);

    struct KeyNode {
        float pos;
        const TimeProfile* profile;
    };

    static const std::array<KeyNode, 7> keys = {{
        { 0.00f, &ProfileNight },
        { 0.15f, &ProfileBlueHour },
        { 0.28f, &ProfileDawn },
        { 0.42f, &ProfileMorning },
        { 0.60f, &ProfileNoon },
        { 0.82f, &ProfileGoldenHour },
        { 1.00f, &ProfileSunset }
    }};

    // Find bounding key segment
    size_t idx = 0;
    while (idx < keys.size() - 2 && t > keys[idx + 1].pos) {
        ++idx;
    }

    const KeyNode &k0 = keys[idx];
    const KeyNode &k1 = keys[idx + 1];

    float segLength = k1.pos - k0.pos;
    float u = (segLength > 0.0001f) ? (t - k0.pos) / segLength : 0.0f;
    // Cubic Hermite smoothstep for derivative-continuous transitions without parameter overshooting
    float s = Smoothstep01(u);

    const TimeProfile &p0 = *k0.profile;
    const TimeProfile &p1 = *k1.profile;

    TimeProfile res;
    res.exposureEV           = LerpFloat(p0.exposureEV, p1.exposureEV, s);
    res.temperature          = LerpFloat(p0.temperature, p1.temperature, s);
    res.tint                 = LerpFloat(p0.tint, p1.tint, s);
    res.contrast             = LerpFloat(p0.contrast, p1.contrast, s);
    res.saturation           = LerpFloat(p0.saturation, p1.saturation, s);
    res.shadowTemperature    = LerpFloat(p0.shadowTemperature, p1.shadowTemperature, s);
    res.shadowTint           = LerpFloat(p0.shadowTint, p1.shadowTint, s);
    res.shadowLift           = LerpFloat(p0.shadowLift, p1.shadowLift, s);
    res.midtoneTemperature   = LerpFloat(p0.midtoneTemperature, p1.midtoneTemperature, s);
    res.midtoneGain          = LerpFloat(p0.midtoneGain, p1.midtoneGain, s);
    res.highlightTemperature = LerpFloat(p0.highlightTemperature, p1.highlightTemperature, s);
    res.highlightGain        = LerpFloat(p0.highlightGain, p1.highlightGain, s);
    res.highlightRolloff     = LerpFloat(p0.highlightRolloff, p1.highlightRolloff, s);
    res.purkinjeStrength     = LerpFloat(p0.purkinjeStrength, p1.purkinjeStrength, s);
    res.skyExposureDrop      = LerpFloat(p0.skyExposureDrop, p1.skyExposureDrop, s);
    res.skinProtection       = LerpFloat(p0.skinProtection, p1.skinProtection, s);
    res.lutStrength          = LerpFloat(p0.lutStrength, p1.lutStrength, s);

    return res;
}

const char* TimeOfDayFilter::GetPhaseName(float sliderValue)
{
    float t = std::clamp(sliderValue, 0.0f, 1.0f);
    if (t <= 0.07f) return "Noche";
    if (t <= 0.21f) return "Blue Hour";
    if (t <= 0.35f) return "Amanecer";
    if (t <= 0.51f) return "Mañana";
    if (t <= 0.71f) return "Mediodía (Neutro)";
    if (t <= 0.91f) return "Golden Hour";
    return "Atardecer";
}

const char* TimeOfDayFilter::GetSimulatedTime(float sliderValue)
{
    float t = std::clamp(sliderValue, 0.0f, 1.0f);
    if (t <= 0.07f) return "00:00";
    if (t <= 0.21f) return "05:30";
    if (t <= 0.35f) return "06:45";
    if (t <= 0.51f) return "09:00";
    if (t <= 0.71f) return "12:00";
    if (t <= 0.91f) return "18:30";
    return "20:15";
}

Lut3D TimeOfDayFilter::GenerateProfileLut(const TimeProfile &profile, int size)
{
    Lut3D lut;
    lut.size = std::clamp(size, 8, 64);
    lut.data.resize(lut.size * lut.size * lut.size * 3);

    auto wbGains = KelvinToRGB(profile.temperature, profile.tint);
    auto shadowGains = KelvinToRGB(profile.shadowTemperature, profile.shadowTint);
    auto highlightGains = KelvinToRGB(profile.highlightTemperature, 0.0f);
    float expMul = std::pow(2.0f, profile.exposureEV);

    float invS = 1.0f / (lut.size - 1);
    size_t idx = 0;

    for (int bIdx = 0; bIdx < lut.size; ++bIdx) {
        float inB = bIdx * invS;
        for (int gIdx = 0; gIdx < lut.size; ++gIdx) {
            float inG = gIdx * invS;
            for (int rIdx = 0; rIdx < lut.size; ++rIdx) {
                float inR = rIdx * invS;

                // 1. Exposure
                float r = inR * expMul;
                float g = inG * expMul;
                float b = inB * expMul;

                // 2. White balance
                r *= wbGains[0];
                g *= wbGains[1];
                b *= wbGains[2];

                // 3. Luminance-based split toning
                float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                float shadowW = 1.0f - Smoothstep01(lum / 0.35f);
                float highlightW = Smoothstep01((lum - 0.45f) / 0.45f);
                float midtoneW = std::clamp(1.0f - shadowW - highlightW, 0.0f, 1.0f);

                r *= (shadowGains[0] * (1.0f + profile.shadowLift) * shadowW + midtoneW + highlightGains[0] * highlightW);
                g *= (shadowGains[1] * (1.0f + profile.shadowLift) * shadowW + midtoneW + highlightGains[1] * highlightW);
                b *= (shadowGains[2] * (1.0f + profile.shadowLift) * shadowW + midtoneW + highlightGains[2] * highlightW);

                // 4. Contrast
                if (std::abs(profile.contrast - 1.0f) > 0.001f) {
                    r = 0.18f + (r - 0.18f) * profile.contrast;
                    g = 0.18f + (g - 0.18f) * profile.contrast;
                    b = 0.18f + (b - 0.18f) * profile.contrast;
                }

                lut.data[idx++] = std::clamp(r, 0.0f, 4.0f);
                lut.data[idx++] = std::clamp(g, 0.0f, 4.0f);
                lut.data[idx++] = std::clamp(b, 0.0f, 4.0f);
            }
        }
    }

    return lut;
}

std::array<float, 3> TimeOfDayFilter::SampleLutTrilinear(const Lut3D &lut, float r, float g, float b)
{
    if (!lut.isValid()) {
        return { r, g, b };
    }

    float s = static_cast<float>(lut.size - 1);
    float cr = std::clamp(r, 0.0f, 1.0f) * s;
    float cg = std::clamp(g, 0.0f, 1.0f) * s;
    float cb = std::clamp(b, 0.0f, 1.0f) * s;

    int r0 = static_cast<int>(cr);
    int g0 = static_cast<int>(cg);
    int b0 = static_cast<int>(cb);

    int r1 = std::min(r0 + 1, lut.size - 1);
    int g1 = std::min(g0 + 1, lut.size - 1);
    int b1 = std::min(b0 + 1, lut.size - 1);

    float fr = cr - r0;
    float fg = cg - g0;
    float fb = cb - b0;

    auto getLutRGB = [&](int ri, int gi, int bi) -> std::array<float, 3> {
        size_t index = (static_cast<size_t>(bi) * lut.size * lut.size +
                        static_cast<size_t>(gi) * lut.size +
                        static_cast<size_t>(ri)) * 3;
        return { lut.data[index], lut.data[index + 1], lut.data[index + 2] };
    };

    auto c000 = getLutRGB(r0, g0, b0);
    auto c100 = getLutRGB(r1, g0, b0);
    auto c010 = getLutRGB(r0, g1, b0);
    auto c110 = getLutRGB(r1, g1, b0);
    auto c001 = getLutRGB(r0, g0, b1);
    auto c101 = getLutRGB(r1, g0, b1);
    auto c011 = getLutRGB(r0, g1, b1);
    auto c111 = getLutRGB(r1, g1, b1);

    std::array<float, 3> outRGB;
    for (int i = 0; i < 3; ++i) {
        float c00 = LerpFloat(c000[i], c100[i], fr);
        float c10 = LerpFloat(c010[i], c110[i], fr);
        float c01 = LerpFloat(c001[i], c101[i], fr);
        float c11 = LerpFloat(c011[i], c111[i], fr);

        float c0 = LerpFloat(c00, c10, fg);
        float c1 = LerpFloat(c01, c11, fg);

        outRGB[i] = LerpFloat(c0, c1, fb);
    }

    return outRGB;
}

bool TimeOfDayFilter::LoadCubeFile(const std::string &filePath, Lut3D &outLut)
{
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    int size = 0;
    std::vector<float> values;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        if (line.rfind("LUT_3D_SIZE", 0) == 0) {
            std::istringstream ss(line);
            std::string tag;
            ss >> tag >> size;
            continue;
        }

        std::istringstream ss(line);
        float r, g, b;
        if (ss >> r >> g >> b) {
            values.push_back(r);
            values.push_back(g);
            values.push_back(b);
        }
    }

    if (size > 1 && values.size() == static_cast<size_t>(size * size * size * 3)) {
        outLut.size = size;
        outLut.data = std::move(values);
        return true;
    }

    return false;
}

bool TimeOfDayFilter::RenderTimelineSlice(int64_t currentFrame,
                                          int64_t startFrame,
                                          int64_t endFrame,
                                          float sliderValue,
                                          unsigned int textureId)
{
    if (currentFrame < startFrame || currentFrame > endFrame) {
        return false;
    }

    float clampedSlider = std::clamp(sliderValue, 0.0f, 1.0f);
    TimeProfile profile = CalculateProfile(clampedSlider);

    auto wbGains = KelvinToRGB(profile.temperature, profile.tint);
    auto shadowGains = KelvinToRGB(profile.shadowTemperature, profile.shadowTint);
    auto midtoneGains = KelvinToRGB(profile.midtoneTemperature, 0.0f);
    auto highlightGains = KelvinToRGB(profile.highlightTemperature, 0.0f);

    m_lastUniforms.exposureEV = profile.exposureEV;
    for (int i = 0; i < 3; ++i) {
        m_lastUniforms.whiteBalanceGains[i] = wbGains[i];
        m_lastUniforms.shadowGains[i] = shadowGains[i];
        m_lastUniforms.midtoneGains[i] = midtoneGains[i];
        m_lastUniforms.highlightGains[i] = highlightGains[i];
    }
    m_lastUniforms.contrast = profile.contrast;
    m_lastUniforms.saturation = profile.saturation;
    m_lastUniforms.shadowLift = profile.shadowLift;
    m_lastUniforms.highlightRolloff = profile.highlightRolloff;
    m_lastUniforms.purkinjeStrength = profile.purkinjeStrength;
    m_lastUniforms.skyExposureDrop = profile.skyExposureDrop;
    m_lastUniforms.skinProtection = profile.skinProtection;
    m_lastUniforms.lutStrength = profile.lutStrength;
    m_lastUniforms.timeOfDay = clampedSlider;
    m_lastUniforms.textureId = textureId;

    if (m_floatSetter) {
        m_floatSetter("u_exposureEV", profile.exposureEV);
        m_floatSetter("u_contrast", profile.contrast);
        m_floatSetter("u_saturation", profile.saturation);
        m_floatSetter("u_shadowLift", profile.shadowLift);
        m_floatSetter("u_highlightRolloff", profile.highlightRolloff);
        m_floatSetter("u_purkinjeStrength", profile.purkinjeStrength);
        m_floatSetter("u_skyExposureDrop", profile.skyExposureDrop);
        m_floatSetter("u_skinProtection", profile.skinProtection);
        m_floatSetter("u_lutStrength", profile.lutStrength);
        m_floatSetter("u_timeOfDay", clampedSlider);
    }
    if (m_vec3Setter) {
        m_vec3Setter("u_whiteBalanceGains", wbGains[0], wbGains[1], wbGains[2]);
        m_vec3Setter("u_shadowGains", shadowGains[0], shadowGains[1], shadowGains[2]);
        m_vec3Setter("u_midtoneGains", midtoneGains[0], midtoneGains[1], midtoneGains[2]);
        m_vec3Setter("u_highlightGains", highlightGains[0], highlightGains[1], highlightGains[2]);
    }
    if (m_textureSetter && textureId > 0) {
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
