#version 330 core

// Interactive Time of Day Fragment Shader
// Diurnal Color Grading: Night (0.0) -> Morning (0.33) -> Day (0.66) -> Sunset (1.0)

in vec2 TexCoord;
out vec4 FragColor;

// Agnostic shader inputs
uniform sampler2D u_texture;
uniform float     u_exposure;        // EV stops (e.g. -2.5 to 0.0)
uniform vec3      u_colorTint;       // Diurnal tint multiplier
uniform float     u_skyBlend;        // Sky blending influence [0.0, 1.0]
uniform float     u_timeOfDay;       // Slider [0.0 = Night, 0.33 = Morning, 0.66 = Day, 1.0 = Sunset]

void main()
{
    vec4 texColor = texture(u_texture, TexCoord);
    vec3 rgb = texColor.rgb;

    // -------------------------------------------------------------------------
    // 1. Procedural Sky Masking (Chrominance / Luminance Heuristics)
    // -------------------------------------------------------------------------
    // ITU-R BT.709 perceived luminance
    float luma = dot(rgb, vec3(0.2126, 0.7152, 0.0722));

    // Sky chrominance signature: Blue channel dominates over red and green
    // High blue delta indicates clear blue skies; high luminance indicates overcast/bright clouds
    float blueDominance = clamp((rgb.b - max(rgb.r, rgb.g * 0.85)) * 3.0, 0.0, 1.0);
    float brightThreshold = smoothstep(0.40, 0.85, luma);

    // Vertical height prior: Celestial sky is concentrated in the upper region
    // TexCoord.y runs from 0.0 (bottom) to 1.0 (top)
    float verticalPrior = smoothstep(0.20, 0.75, TexCoord.y);

    // Combined procedural sky mask
    float skyConfidence = max(blueDominance * 0.75, brightThreshold * 0.40);
    float skyMask = clamp(skyConfidence * verticalPrior, 0.0, 1.0);

    // -------------------------------------------------------------------------
    // 2. Exposure Shifting & Linear RGB Tinting
    // -------------------------------------------------------------------------
    float exposureMultiplier = pow(2.0, u_exposure);
    vec3 gradedScenery = rgb * exposureMultiplier * u_colorTint;

    // Purkinje scotopic vision shift at night (low light rod photoreception)
    // Desaturates warmer colors and gently enhances cool blue moonlit tones
    if (u_timeOfDay < 0.33) {
        float nightWeight = (0.33 - u_timeOfDay) / 0.33;
        float sceneryLuma = dot(gradedScenery, vec3(0.2126, 0.7152, 0.0722));
        vec3 purkinjeRgb = vec3(sceneryLuma) * vec3(0.60, 0.82, 1.25);
        gradedScenery = mix(gradedScenery, purkinjeRgb, nightWeight * 0.50);
    }

    // -------------------------------------------------------------------------
    // 3. Procedural Celestial Sky Gradient
    // -------------------------------------------------------------------------
    // Pre-defined celestial sky color keys (Zenith / Top -> Horizon / Bottom)
    vec3 nightSkyZenith    = vec3(0.010, 0.018, 0.055);
    vec3 nightSkyHorizon   = vec3(0.030, 0.055, 0.130);

    vec3 morningSkyZenith  = vec3(0.220, 0.380, 0.680);
    vec3 morningSkyHorizon = vec3(0.960, 0.620, 0.420);

    vec3 daySkyZenith      = vec3(0.240, 0.540, 0.920);
    vec3 daySkyHorizon     = vec3(0.720, 0.860, 0.990);

    vec3 sunsetSkyZenith   = vec3(0.160, 0.100, 0.380);
    vec3 sunsetSkyHorizon  = vec3(0.960, 0.360, 0.120);

    vec3 skyZenith;
    vec3 skyHorizon;

    if (u_timeOfDay <= 0.33) {
        float t = clamp(u_timeOfDay / 0.33, 0.0, 1.0);
        skyZenith  = mix(nightSkyZenith, morningSkyZenith, t);
        skyHorizon = mix(nightSkyHorizon, morningSkyHorizon, t);
    } else if (u_timeOfDay <= 0.66) {
        float t = clamp((u_timeOfDay - 0.33) / 0.33, 0.0, 1.0);
        skyZenith  = mix(morningSkyZenith, daySkyZenith, t);
        skyHorizon = mix(morningSkyHorizon, daySkyHorizon, t);
    } else {
        float t = clamp((u_timeOfDay - 0.66) / 0.34, 0.0, 1.0);
        skyZenith  = mix(daySkyZenith, sunsetSkyZenith, t);
        skyHorizon = mix(daySkyHorizon, sunsetSkyHorizon, t);
    }

    // Celestial gradient interpolating vertically across frame
    float skyHeight = clamp(TexCoord.y, 0.0, 1.0);
    vec3 proceduralSky = mix(skyHorizon, skyZenith, skyHeight);

    // -------------------------------------------------------------------------
    // 4. Composite Scenery & Sky
    // -------------------------------------------------------------------------
    // Blend procedural sky into detected sky regions according to profile's skyBlend factor
    vec3 blendedSky = mix(gradedScenery, proceduralSky, u_skyBlend);
    vec3 finalColor = mix(gradedScenery, blendedSky, skyMask);

    FragColor = vec4(clamp(finalColor, 0.0, 1.0), texColor.a);
}
