#include "TimeOfDayFilter.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <QFile>

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
    float T = std::clamp(kelvin, 1800.0f, 16000.0f);

    if (std::abs(T - 6500.0f) < 0.5f && std::abs(tint) < 0.0005f) {
        return { 1.0f, 1.0f, 1.0f };
    }

    // 1. CCT -> CIE 1931 chromaticity (x, y)
    // For T < 4000K, follow Planckian Blackbody locus (Kang et al.)
    // For T >= 4000K, follow standard CIE Daylight locus (CIE 15:2004)
    float x = 0.0f;
    float y = 0.0f;
    float T2 = T * T;
    float T3 = T2 * T;

    if (T < 4000.0f) {
        x = (-0.2661239e9f / T3) - (0.2343580e6f / T2) + (0.8776956e3f / T) + 0.179910f;
        float x2 = x * x;
        float x3 = x2 * x;
        y = -1.1063814f * x3 - 1.34811020f * x2 + 2.18555832f * x - 0.20219683f;
    } else if (T <= 7000.0f) {
        x = (-4.6070e9f / T3) + (2.9678e6f / T2) + (0.09911e3f / T) + 0.244063f;
        y = -3.000f * x * x + 2.870f * x - 0.275f;
    } else {
        x = (-2.0064e9f / T3) + (1.9018e6f / T2) + (0.24748e3f / T) + 0.237040f;
        y = -3.000f * x * x + 2.870f * x - 0.275f;
    }

    // 2. Green-Magenta tint axis adjustment along iso-temperature lines
    y += tint * 0.05f;
    x -= tint * 0.01f;
    y = std::max(0.01f, y);

    // 3. CIE xy -> CIE XYZ (normalized to Y = 1.0)
    float X = x / y;
    float Y = 1.0f;
    float Z = (1.0f - x - y) / y;

    // 4. Bradford Chromatic Adaptation Transform (CAT)
    // Cone signals in Bradford LMS space: M_BFD * XYZ
    float L =  0.8951f * X + 0.2664f * Y - 0.1614f * Z;
    float M = -0.7502f * X + 1.7135f * Y + 0.0367f * Z;
    float S =  0.0389f * X - 0.0685f * Y + 1.0296f * Z;

    // D65 reference in Bradford LMS space (xD65=0.3127, yD65=0.3290)
    const float X_ref = 0.3127f / 0.3290f;
    const float Y_ref = 1.0f;
    const float Z_ref = (1.0f - 0.3127f - 0.3290f) / 0.3290f;

    const float L_D65 =  0.8951f * X_ref + 0.2664f * Y_ref - 0.1614f * Z_ref;
    const float M_D65 = -0.7502f * X_ref + 1.7135f * Y_ref + 0.0367f * Z_ref;
    const float S_D65 =  0.0389f * X_ref - 0.0685f * Y_ref + 1.0296f * Z_ref;

    // Relighting von Kries gains in cone space relative to D65
    float gL = L / L_D65;
    float gM = M / M_D65;
    float gS = S / S_D65;

    // Map cone signals back to XYZ using inverse Bradford matrix
    float X_adapt =  0.9869929f * (gL * L_D65) - 0.1470543f * (gM * M_D65) + 0.1599627f * (gS * S_D65);
    float Y_adapt =  0.4323053f * (gL * L_D65) + 0.5183603f * (gM * M_D65) + 0.0492912f * (gS * S_D65);
    float Z_adapt = -0.0085287f * (gL * L_D65) + 0.0400428f * (gM * M_D65) + 0.9684867f * (gS * S_D65);

    // 5. Convert adapted XYZ to Linear sRGB / Rec.709
    float r =  3.2404542f * X_adapt - 1.5371385f * Y_adapt - 0.4985314f * Z_adapt;
    float g = -0.9692660f * X_adapt + 1.8760108f * Y_adapt + 0.0415560f * Z_adapt;
    float b =  0.0556434f * X_adapt - 0.2040259f * Y_adapt + 1.0572252f * Z_adapt;

    // Normalized against D65 sRGB baseline
    const float r_ref =  3.2404542f * X_ref - 1.5371385f * Y_ref - 0.4985314f * Z_ref;
    const float g_ref = -0.9692660f * X_ref + 1.8760108f * Y_ref + 0.0415560f * Z_ref;
    const float b_ref =  0.0556434f * X_ref - 0.2040259f * Y_ref + 1.0572252f * Z_ref;

    r /= r_ref;
    g /= g_ref;
    b /= b_ref;

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
    auto midtoneGains = KelvinToRGB(profile.midtoneTemperature, 0.0f);
    auto highlightGains = KelvinToRGB(profile.highlightTemperature, 0.0f);
    float expMul = std::pow(2.0f, profile.exposureEV);

    float invS = 1.0f / (lut.size - 1);
    size_t idx = 0;

    const float midR = midtoneGains[0] * profile.midtoneGain;
    const float midG = midtoneGains[1] * profile.midtoneGain;
    const float midB = midtoneGains[2] * profile.midtoneGain;

    const float hiR = highlightGains[0] * profile.highlightGain;
    const float hiG = highlightGains[1] * profile.highlightGain;
    const float hiB = highlightGains[2] * profile.highlightGain;

    const float shLift = 1.0f + profile.shadowLift;
    const float shR = shadowGains[0] * shLift;
    const float shG = shadowGains[1] * shLift;
    const float shB = shadowGains[2] * shLift;

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

                r *= (shR * shadowW + midR * midtoneW + hiR * highlightW);
                g *= (shG * shadowW + midG * midtoneW + hiG * highlightW);
                b *= (shB * shadowW + midB * midtoneW + hiB * highlightW);

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

Lut3D TimeOfDayFilter::GetCachedProfileLut(const TimeProfile &profile, int size)
{
    static Lut3D s_cachedLut;
    static float s_cachedExp = -999.0f;
    static float s_cachedTemp = -1.0f;
    static float s_cachedTint = -1.0f;

    if (!s_cachedLut.isValid() ||
        std::abs(profile.exposureEV - s_cachedExp) > 0.002f ||
        std::abs(profile.temperature - s_cachedTemp) > 1.0f ||
        std::abs(profile.tint - s_cachedTint) > 0.002f)
    {
        s_cachedLut = GenerateProfileLut(profile, size);
        s_cachedExp = profile.exposureEV;
        s_cachedTemp = profile.temperature;
        s_cachedTint = profile.tint;
    }
    return s_cachedLut;
}

std::array<float, 3> TimeOfDayFilter::SampleLutTrilinear(const Lut3D &lut, float r, float g, float b)
{
    if (!lut.isValid()) {
        return { r, g, b };
    }

    const int sz = lut.size;
    const float s = static_cast<float>(sz - 1);
    const float cr = std::clamp(r, 0.0f, 1.0f) * s;
    const float cg = std::clamp(g, 0.0f, 1.0f) * s;
    const float cb = std::clamp(b, 0.0f, 1.0f) * s;

    const int r0 = static_cast<int>(cr);
    const int g0 = static_cast<int>(cg);
    const int b0 = static_cast<int>(cb);

    const int r1 = std::min(r0 + 1, sz - 1);
    const int g1 = std::min(g0 + 1, sz - 1);
    const int b1 = std::min(b0 + 1, sz - 1);

    const float fr = cr - r0;
    const float fg = cg - g0;
    const float fb = cb - b0;

    const float *data = lut.data.data();
    const int sz2 = sz * sz;
    const int stride_b0 = b0 * sz2 * 3;
    const int stride_b1 = b1 * sz2 * 3;
    const int stride_g0 = g0 * sz * 3;
    const int stride_g1 = g1 * sz * 3;
    const int r0_3 = r0 * 3;
    const int r1_3 = r1 * 3;

    const float *c000 = data + stride_b0 + stride_g0 + r0_3;
    const float *c100 = data + stride_b0 + stride_g0 + r1_3;
    const float *c010 = data + stride_b0 + stride_g1 + r0_3;
    const float *c110 = data + stride_b0 + stride_g1 + r1_3;
    const float *c001 = data + stride_b1 + stride_g0 + r0_3;
    const float *c101 = data + stride_b1 + stride_g0 + r1_3;
    const float *c011 = data + stride_b1 + stride_g1 + r0_3;
    const float *c111 = data + stride_b1 + stride_g1 + r1_3;

    std::array<float, 3> outRGB;
    for (int i = 0; i < 3; ++i) {
        float c00 = c000[i] + fr * (c100[i] - c000[i]);
        float c10 = c010[i] + fr * (c110[i] - c010[i]);
        float c01 = c001[i] + fr * (c101[i] - c001[i]);
        float c11 = c011[i] + fr * (c111[i] - c011[i]);

        float c0 = c00 + fg * (c10 - c00);
        float c1 = c01 + fg * (c11 - c01);

        outRGB[i] = c0 + fb * (c1 - c0);
    }

    return outRGB;
}

std::string TimeOfDayFilter::GetFragmentShaderSource()
{
    // 1. Try to load from Qt embedded resource
    QFile qrcFile(":/shaders/TimeOfDay.frag");
    if (qrcFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return qrcFile.readAll().toStdString();
    }

    // 2. Try to load from local filesystem
    std::ifstream file("src/filters/TimeOfDay.frag");
    if (file.is_open()) {
        std::stringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }
    return std::string();
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
        m_lastUniforms.midtoneGains[i] = midtoneGains[i] * profile.midtoneGain;
        m_lastUniforms.highlightGains[i] = highlightGains[i] * profile.highlightGain;
    }
    m_lastUniforms.contrast = profile.contrast;
    m_lastUniforms.saturation = profile.saturation;
    m_lastUniforms.shadowLift = profile.shadowLift;
    m_lastUniforms.midtoneGain = profile.midtoneGain;
    m_lastUniforms.highlightGain = profile.highlightGain;
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
        m_floatSetter("u_midtoneGain", profile.midtoneGain);
        m_floatSetter("u_highlightGain", profile.highlightGain);
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
        m_vec3Setter("u_midtoneGains", m_lastUniforms.midtoneGains[0], m_lastUniforms.midtoneGains[1], m_lastUniforms.midtoneGains[2]);
        m_vec3Setter("u_highlightGains", m_lastUniforms.highlightGains[0], m_lastUniforms.highlightGains[1], m_lastUniforms.highlightGains[2]);
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
