#include "audioeffects.h"
#include <QtMath>

// Cookbook RBJ Audio EQ biquad filter calculations
static void calcLowShelf(BiquadFilter &f, float f0, float gainDb, float Fs)
{
    if (std::abs(gainDb) < 0.01f) {
        f.b0 = 1.0f; f.b1 = 0.0f; f.b2 = 0.0f;
        f.a1 = 0.0f; f.a2 = 0.0f;
        return;
    }
    float A = std::pow(10.0f, gainDb / 40.0f);
    float w0 = 2.0f * static_cast<float>(M_PI) * f0 / Fs;
    float cosw0 = std::cos(w0);
    float sinw0 = std::sin(w0);
    float alpha = sinw0 / 2.0f * std::sqrt((A + 1.0f / A) * (1.0f / 0.707f - 1.0f) + 2.0f);
    float sqrtA2 = 2.0f * std::sqrt(A) * alpha;

    float b0 =    A * ( (A+1.0f) - (A-1.0f)*cosw0 + sqrtA2 );
    float b1 = 2.0f*A * ( (A-1.0f) - (A+1.0f)*cosw0 );
    float b2 =    A * ( (A+1.0f) - (A-1.0f)*cosw0 - sqrtA2 );
    float a0 =          (A+1.0f) + (A-1.0f)*cosw0 + sqrtA2;
    float a1 =   -2.0f * ( (A-1.0f) + (A+1.0f)*cosw0 );
    float a2 =          (A+1.0f) + (A-1.0f)*cosw0 - sqrtA2;

    f.b0 = b0 / a0; f.b1 = b1 / a0; f.b2 = b2 / a0;
    f.a1 = a1 / a0; f.a2 = a2 / a0;
}

static void calcPeaking(BiquadFilter &f, float f0, float gainDb, float Q, float Fs)
{
    if (std::abs(gainDb) < 0.01f) {
        f.b0 = 1.0f; f.b1 = 0.0f; f.b2 = 0.0f;
        f.a1 = 0.0f; f.a2 = 0.0f;
        return;
    }
    float A = std::pow(10.0f, gainDb / 40.0f);
    float w0 = 2.0f * static_cast<float>(M_PI) * f0 / Fs;
    float alpha = std::sin(w0) / (2.0f * Q);
    float cosw0 = std::cos(w0);

    float b0 =  1.0f + alpha * A;
    float b1 = -2.0f * cosw0;
    float b2 =  1.0f - alpha * A;
    float a0 =  1.0f + alpha / A;
    float a1 = -2.0f * cosw0;
    float a2 =  1.0f - alpha / A;

    f.b0 = b0 / a0; f.b1 = b1 / a0; f.b2 = b2 / a0;
    f.a1 = a1 / a0; f.a2 = a2 / a0;
}

static void calcHighShelf(BiquadFilter &f, float f0, float gainDb, float Fs)
{
    if (std::abs(gainDb) < 0.01f) {
        f.b0 = 1.0f; f.b1 = 0.0f; f.b2 = 0.0f;
        f.a1 = 0.0f; f.a2 = 0.0f;
        return;
    }
    float A = std::pow(10.0f, gainDb / 40.0f);
    float w0 = 2.0f * static_cast<float>(M_PI) * f0 / Fs;
    float cosw0 = std::cos(w0);
    float sinw0 = std::sin(w0);
    float alpha = sinw0 / 2.0f * std::sqrt((A + 1.0f / A) * (1.0f / 0.707f - 1.0f) + 2.0f);
    float sqrtA2 = 2.0f * std::sqrt(A) * alpha;

    float b0 =    A * ( (A+1.0f) + (A-1.0f)*cosw0 + sqrtA2 );
    float b1 = -2.0f*A * ( (A-1.0f) + (A+1.0f)*cosw0 );
    float b2 =    A * ( (A+1.0f) + (A-1.0f)*cosw0 - sqrtA2 );
    float a0 =          (A+1.0f) - (A-1.0f)*cosw0 + sqrtA2;
    float a1 =    2.0f * ( (A-1.0f) - (A+1.0f)*cosw0 );
    float a2 =          (A+1.0f) - (A-1.0f)*cosw0 - sqrtA2;

    f.b0 = b0 / a0; f.b1 = b1 / a0; f.b2 = b2 / a0;
    f.a1 = a1 / a0; f.a2 = a2 / a0;
}

ParametricEQ::ParametricEQ()
{
    updateFilters();
}

void ParametricEQ::setSampleRate(float sampleRate)
{
    if (sampleRate > 1000.0f) {
        m_sampleRate = sampleRate;
        updateFilters();
    }
}

void ParametricEQ::setLowShelf(float freqHz, float gainDb)
{
    m_lowFreq = freqHz;
    m_lowGainDb = gainDb;
    calcLowShelf(m_f1, m_lowFreq, m_lowGainDb, m_sampleRate);
}

void ParametricEQ::setLowMid(float freqHz, float gainDb, float q)
{
    m_lowMidFreq = freqHz;
    m_lowMidGainDb = gainDb;
    m_lowMidQ = q;
    calcPeaking(m_f2, m_lowMidFreq, m_lowMidGainDb, m_lowMidQ, m_sampleRate);
}

void ParametricEQ::setHighMid(float freqHz, float gainDb, float q)
{
    m_highMidFreq = freqHz;
    m_highMidGainDb = gainDb;
    m_highMidQ = q;
    calcPeaking(m_f3, m_highMidFreq, m_highMidGainDb, m_highMidQ, m_sampleRate);
}

void ParametricEQ::setHighShelf(float freqHz, float gainDb)
{
    m_highFreq = freqHz;
    m_highGainDb = gainDb;
    calcHighShelf(m_f4, m_highFreq, m_highGainDb, m_sampleRate);
}

void ParametricEQ::updateFilters()
{
    calcLowShelf(m_f1, m_lowFreq, m_lowGainDb, m_sampleRate);
    calcPeaking(m_f2, m_lowMidFreq, m_lowMidGainDb, m_lowMidQ, m_sampleRate);
    calcPeaking(m_f3, m_highMidFreq, m_highMidGainDb, m_highMidQ, m_sampleRate);
    calcHighShelf(m_f4, m_highFreq, m_highGainDb, m_sampleRate);
}

void ParametricEQ::process(float *samples, int count)
{
    if (!samples || count <= 0) return;
    for (int i = 0; i < count; ++i) {
        float s = samples[i];
        s = m_f1.process(s);
        s = m_f2.process(s);
        s = m_f3.process(s);
        s = m_f4.process(s);
        samples[i] = s;
    }
}

void ParametricEQ::reset()
{
    m_f1.reset();
    m_f2.reset();
    m_f3.reset();
    m_f4.reset();
}

QJsonObject ParametricEQ::toJson() const
{
    QJsonObject obj;
    obj["lowGain"] = static_cast<double>(m_lowGainDb);
    obj["lowMidGain"] = static_cast<double>(m_lowMidGainDb);
    obj["highMidGain"] = static_cast<double>(m_highMidGainDb);
    obj["highGain"] = static_cast<double>(m_highGainDb);
    return obj;
}

void ParametricEQ::fromJson(const QJsonObject &obj)
{
    if (obj.contains("lowGain")) setLowShelf(m_lowFreq, static_cast<float>(obj["lowGain"].toDouble()));
    if (obj.contains("lowMidGain")) setLowMid(m_lowMidFreq, static_cast<float>(obj["lowMidGain"].toDouble()), m_lowMidQ);
    if (obj.contains("highMidGain")) setHighMid(m_highMidFreq, static_cast<float>(obj["highMidGain"].toDouble()), m_highMidQ);
    if (obj.contains("highGain")) setHighShelf(m_highFreq, static_cast<float>(obj["highGain"].toDouble()));
}

// -----------------------------------------------------------------------------
// AudioCompressor
// -----------------------------------------------------------------------------

AudioCompressor::AudioCompressor()
{
    updateCoefficients();
}

void AudioCompressor::setSampleRate(float sampleRate)
{
    if (sampleRate > 1000.0f) {
        m_sampleRate = sampleRate;
        updateCoefficients();
    }
}

void AudioCompressor::updateCoefficients()
{
    m_attackCoeff = std::exp(-1.0f / (m_sampleRate * (m_attackMs / 1000.0f)));
    m_releaseCoeff = std::exp(-1.0f / (m_sampleRate * (m_releaseMs / 1000.0f)));
}

void AudioCompressor::process(float *samples, int count)
{
    if (!samples || count <= 0) return;
    float makeupLinear = std::pow(10.0f, m_makeupGainDb / 20.0f);
    float threshLin = std::pow(10.0f, m_thresholdDb / 20.0f);
    float slope = 1.0f - (1.0f / m_ratio);

    for (int i = 0; i < count; ++i) {
        float inAbs = std::abs(samples[i]);
        if (inAbs > m_envelope) {
            m_envelope = inAbs + m_attackCoeff * (m_envelope - inAbs);
        } else {
            m_envelope = inAbs + m_releaseCoeff * (m_envelope - inAbs);
        }

        float gainReduction = 1.0f;
        if (m_envelope > threshLin) {
            float envDb = 20.0f * std::log10(std::max(1e-5f, m_envelope));
            float overDb = envDb - m_thresholdDb;
            float grDb = -overDb * slope;
            gainReduction = std::pow(10.0f, grDb / 20.0f);
        }

        samples[i] = samples[i] * gainReduction * makeupLinear;
    }
}

void AudioCompressor::reset()
{
    m_envelope = 0.0f;
}

QJsonObject AudioCompressor::toJson() const
{
    QJsonObject obj;
    obj["threshold"] = static_cast<double>(m_thresholdDb);
    obj["ratio"] = static_cast<double>(m_ratio);
    obj["attack"] = static_cast<double>(m_attackMs);
    obj["release"] = static_cast<double>(m_releaseMs);
    obj["makeup"] = static_cast<double>(m_makeupGainDb);
    return obj;
}

void AudioCompressor::fromJson(const QJsonObject &obj)
{
    if (obj.contains("threshold")) setThresholdDb(static_cast<float>(obj["threshold"].toDouble()));
    if (obj.contains("ratio")) setRatio(static_cast<float>(obj["ratio"].toDouble()));
    if (obj.contains("attack")) setAttackMs(static_cast<float>(obj["attack"].toDouble()));
    if (obj.contains("release")) setReleaseMs(static_cast<float>(obj["release"].toDouble()));
    if (obj.contains("makeup")) setMakeupGainDb(static_cast<float>(obj["makeup"].toDouble()));
}

// -----------------------------------------------------------------------------
// BrickwallLimiter
// -----------------------------------------------------------------------------

BrickwallLimiter::BrickwallLimiter(float ceilingDb)
    : m_ceilingDb(ceilingDb)
{
    m_ceilingLin = std::pow(10.0f, ceilingDb / 20.0f);
}

void BrickwallLimiter::process(float *samples, int count)
{
    if (!samples || count <= 0) return;
    const float releaseCoeff = 0.999f;

    for (int i = 0; i < count; ++i) {
        float peak = std::abs(samples[i]);
        if (peak > m_peakEnv) {
            m_peakEnv = peak;
        } else {
            m_peakEnv = m_peakEnv * releaseCoeff;
        }

        if (m_peakEnv > m_ceilingLin) {
            float scale = m_ceilingLin / m_peakEnv;
            samples[i] *= scale;
        }
    }
}

// -----------------------------------------------------------------------------
// AudioMixerSuite
// -----------------------------------------------------------------------------

AudioMixerSuite& AudioMixerSuite::instance()
{
    static AudioMixerSuite s_instance;
    return s_instance;
}

AudioMixerSuite::AudioMixerSuite()
{
}

AudioMixerChannel* AudioMixerSuite::getOrCreateChannel(qint64 trackId)
{
    for (auto &ch : m_channels) {
        if (ch.trackId == trackId) return &ch;
    }
    AudioMixerChannel ch;
    ch.trackId = trackId;
    ch.name = QString("Pista Audio %1").arg(trackId);
    m_channels.append(ch);
    return &m_channels.last();
}

void AudioMixerSuite::processMasterBus(float *samples, int count)
{
    if (!samples || count <= 0) return;
    m_masterEq.process(samples, count);
    m_masterComp.process(samples, count);
    m_masterLimiter.process(samples, count);
}

void AudioMixerSuite::resetAll()
{
    m_masterEq.reset();
    m_masterComp.reset();
    m_masterLimiter.reset();
    for (auto &ch : m_channels) {
        ch.eq.reset();
        ch.comp.reset();
    }
}
