#version 330 core

// Natural Time-of-Day Relighting Fragment Shader
// Physically & perceptually based diurnal color grading across 7 diurnal phases:
// Night (0.00) -> Blue Hour (0.15) -> Dawn (0.28) -> Morning (0.42) -> Noon (0.60) -> Golden Hour (0.82) -> Sunset (1.00)

in vec2 TexCoord;
out vec4 FragColor;

// Agnostic shader inputs
uniform sampler2D u_texture;

// Relighting uniforms
uniform float u_exposureEV;
uniform vec3  u_whiteBalanceGains;
uniform vec3  u_shadowGains;
uniform vec3  u_midtoneGains;
uniform vec3  u_highlightGains;
uniform float u_contrast;
uniform float u_saturation;
uniform float u_shadowLift;
uniform float u_highlightRolloff;
uniform float u_purkinjeStrength;
uniform float u_skyExposureDrop;
uniform float u_skinProtection;
uniform float u_lutStrength;
uniform float u_timeOfDay;

// -----------------------------------------------------------------------------
// Accurate Color Space Conversions (Rec.709 / sRGB <-> Linear RGB)
// -----------------------------------------------------------------------------
vec3 srgbToLinear(vec3 c)
{
    c = clamp(c, 0.0, 1.0);
    return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(0.04045, c));
}

vec3 linearToSrgb(vec3 c)
{
    c = clamp(c, 0.0, 1.0);
    return mix(c * 12.92, 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055, step(0.0031308, c));
}

// -----------------------------------------------------------------------------
// ACES-inspired Filmic Tone Mapping Curve for Smooth Highlight Rolloff
// -----------------------------------------------------------------------------
vec3 acesFilm(vec3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main()
{
    vec4 texColor = texture(u_texture, TexCoord);
    vec3 origSrgb = texColor.rgb;

    // Fast pass-through when exactly at neutral Noon (0.60)
    if (abs(u_timeOfDay - 0.60) < 0.002 &&
        abs(u_exposureEV) < 0.001 &&
        abs(u_contrast - 1.0) < 0.001 &&
        abs(u_saturation - 1.0) < 0.001)
    {
        FragColor = texColor;
        return;
    }

    // 1. Decode sRGB / Rec.709 to Linear RGB
    vec3 linearColor = srgbToLinear(origSrgb);

    // -------------------------------------------------------------------------
    // 2. Soft Skin Tone Protection (YCbCr chrominance cluster)
    // -------------------------------------------------------------------------
    float skinMask = 0.0;
    if (u_skinProtection > 0.001) {
        float Y_skin = dot(origSrgb, vec3(0.299, 0.587, 0.114));
        float Cb = -0.168736 * origSrgb.r - 0.331264 * origSrgb.g + 0.500000 * origSrgb.b;
        float Cr =  0.500000 * origSrgb.r - 0.418688 * origSrgb.g - 0.081312 * origSrgb.b;

        // Human skin tone centroid: Cb in [-0.15, -0.04], Cr in [0.03, 0.15]
        float cbDist = (Cb - (-0.09)) / 0.07;
        float crDist = (Cr - 0.09) / 0.07;
        float ellipseDist = cbDist * cbDist + crDist * crDist;
        skinMask = clamp(1.0 - ellipseDist, 0.0, 1.0) * smoothstep(0.15, 0.35, Y_skin) * u_skinProtection;
    }

    // -------------------------------------------------------------------------
    // 3. Soft Sky Adjustment (No procedural replacement; cloud details preserved)
    // -------------------------------------------------------------------------
    if (u_skyExposureDrop > 0.001) {
        float lumaSrgb = dot(origSrgb, vec3(0.2126, 0.7152, 0.0722));
        float blueDom = clamp((origSrgb.b - max(origSrgb.r, origSrgb.g * 0.85)) * 3.0, 0.0, 1.0);
        float brightThresh = smoothstep(0.45, 0.90, lumaSrgb);
        float verticalPrior = smoothstep(0.15, 0.75, TexCoord.y);
        float skyConfidence = max(blueDom * 0.75, brightThresh * 0.35);
        float skyMask = clamp(skyConfidence * verticalPrior, 0.0, 1.0);

        // Softly compress overexposed sky while retaining cloud and horizon textures
        float skyFactor = 1.0 - skyMask * u_skyExposureDrop * 0.45;
        linearColor *= skyFactor;
    }

    // -------------------------------------------------------------------------
    // 4. Photometric Exposure Shifting in Linear Light
    // -------------------------------------------------------------------------
    linearColor *= exp2(u_exposureEV);

    // -------------------------------------------------------------------------
    // 5. Global White Balance & Chromatic Adaptation
    // -------------------------------------------------------------------------
    vec3 wbGains = mix(u_whiteBalanceGains, vec3(1.0), skinMask * 0.50);
    linearColor *= wbGains;

    // -------------------------------------------------------------------------
    // 6. Luminance-based Split Toning (Shadows, Midtones, Highlights)
    // -------------------------------------------------------------------------
    float lumLinear = dot(linearColor, vec3(0.2126, 0.7152, 0.0722));
    float mShadow    = 1.0 - smoothstep(0.02, 0.35, lumLinear);
    float mHighlight = smoothstep(0.45, 0.95, lumLinear);
    float mMidtone   = clamp(1.0 - mShadow - mHighlight, 0.0, 1.0);

    vec3 splitGains = u_shadowGains * (1.0 + u_shadowLift) * mShadow +
                      u_midtoneGains * mMidtone +
                      u_highlightGains * mHighlight;

    // Preserve natural skin tones against extreme split tinting
    splitGains = mix(splitGains, vec3(1.0), skinMask * 0.75);
    linearColor *= splitGains;

    // -------------------------------------------------------------------------
    // 7. Selective Purkinje Scotopic Vision Shift (Night / Scotopic vision)
    // -------------------------------------------------------------------------
    // Affects shadows and lower midtones; highlights (lamps, windows) remain warm & neutral!
    if (u_purkinjeStrength > 0.001) {
        float purkinjeWeight = u_purkinjeStrength * (mShadow + 0.35 * mMidtone) * (1.0 - skinMask);
        float currentLum = dot(linearColor, vec3(0.2126, 0.7152, 0.0722));
        vec3 scotopic = vec3(currentLum) * vec3(0.55, 0.80, 1.25);
        linearColor = mix(linearColor, scotopic, purkinjeWeight);
    }

    // -------------------------------------------------------------------------
    // 8. Contrast and Saturation in Linear Perceptual Space
    // -------------------------------------------------------------------------
    float postLum = dot(linearColor, vec3(0.2126, 0.7152, 0.0722));

    // Perceptual Saturation
    if (abs(u_saturation - 1.0) > 0.001) {
        linearColor = max(vec3(0.0), mix(vec3(postLum), linearColor, u_saturation));
    }

    // Linear Contrast around perceptual mid-gray (0.18)
    if (abs(u_contrast - 1.0) > 0.001) {
        linearColor = max(vec3(0.0), 0.18 + (linearColor - 0.18) * u_contrast);
    }

    // -------------------------------------------------------------------------
    // 9. Filmic Tone Mapping with Smooth Highlight Rolloff
    // -------------------------------------------------------------------------
    if (u_highlightRolloff > 0.001) {
        vec3 filmic = acesFilm(linearColor);
        linearColor = mix(linearColor, filmic, u_highlightRolloff);
    }

    // -------------------------------------------------------------------------
    // 10. Encode Linear Light back to sRGB / Rec.709 Output
    // -------------------------------------------------------------------------
    vec3 finalSrgb = linearToSrgb(linearColor);
    FragColor = vec4(finalSrgb, texColor.a);
}
