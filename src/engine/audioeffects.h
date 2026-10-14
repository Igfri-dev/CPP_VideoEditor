#pragma once

#include <QString>
#include <QVector>
#include <QJsonObject>
#include <cmath>
#include <algorithm>

struct BiquadFilter {
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
    float a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;

    void reset() { z1 = z2 = 0.0f; }

    inline float process(float in) {
        float out = b0 * in + z1;
        z1 = b1 * in - a1 * out + z2;
        z2 = b2 * in - a2 * out;
        return out;
    }
};

class ParametricEQ {
public:
    ParametricEQ();

    void setSampleRate(float sampleRate);

    // Band 1: Low Shelf (Hz, Gain dB)
    void setLowShelf(float freqHz, float gainDb);
    // Band 2: Low-Mid Bell (Hz, Gain dB, Q)
    void setLowMid(float freqHz, float gainDb, float q = 1.0f);
    // Band 3: High-Mid Bell (Hz, Gain dB, Q)
    void setHighMid(float freqHz, float gainDb, float q = 1.0f);
    // Band 4: High Shelf (Hz, Gain dB)
    void setHighShelf(float freqHz, float gainDb);

    float lowGainDb() const { return m_lowGainDb; }
    float lowMidGainDb() const { return m_lowMidGainDb; }
    float highMidGainDb() const { return m_highMidGainDb; }
    float highGainDb() const { return m_highGainDb; }

    void process(float *samples, int count);
    void reset();

    QJsonObject toJson() const;
    void fromJson(const QJsonObject &obj);

private:
    void updateFilters();

    float m_sampleRate = 44100.0f;
    float m_lowFreq = 80.0f;
    float m_lowGainDb = 0.0f;

    float m_lowMidFreq = 300.0f;
    float m_lowMidGainDb = 0.0f;
    float m_lowMidQ = 1.0f;

    float m_highMidFreq = 2500.0f;
    float m_highMidGainDb = 0.0f;
    float m_highMidQ = 1.0f;

    float m_highFreq = 10000.0f;
    float m_highGainDb = 0.0f;

    BiquadFilter m_f1, m_f2, m_f3, m_f4;
};

class AudioCompressor {
public:
    AudioCompressor();

    void setSampleRate(float sampleRate);
    void setThresholdDb(float db) { m_thresholdDb = db; }
    void setRatio(float ratio) { m_ratio = std::max(1.0f, ratio); }
    void setAttackMs(float ms) { m_attackMs = ms; updateCoefficients(); }
    void setReleaseMs(float ms) { m_releaseMs = ms; updateCoefficients(); }
    void setMakeupGainDb(float db) { m_makeupGainDb = db; }

    float thresholdDb() const { return m_thresholdDb; }
    float ratio() const { return m_ratio; }
    float attackMs() const { return m_attackMs; }
    float releaseMs() const { return m_releaseMs; }
    float makeupGainDb() const { return m_makeupGainDb; }

    void process(float *samples, int count);
    void reset();

    QJsonObject toJson() const;
    void fromJson(const QJsonObject &obj);

private:
    void updateCoefficients();

    float m_sampleRate = 44100.0f;
    float m_thresholdDb = -18.0f;
    float m_ratio = 4.0f;
    float m_attackMs = 15.0f;
    float m_releaseMs = 100.0f;
    float m_makeupGainDb = 0.0f;

    float m_envelope = 0.0f;
    float m_attackCoeff = 0.0f;
    float m_releaseCoeff = 0.0f;
};

class BrickwallLimiter {
public:
    BrickwallLimiter(float ceilingDb = -0.2f);

    void setCeilingDb(float db) { m_ceilingDb = db; m_ceilingLin = std::pow(10.0f, db / 20.0f); }
    float ceilingDb() const { return m_ceilingDb; }

    void process(float *samples, int count);
    void reset() { m_peakEnv = 0.0f; }

private:
    float m_ceilingDb = -0.2f;
    float m_ceilingLin = 0.9772f;
    float m_peakEnv = 0.0f;
};

struct AudioMixerChannel {
    qint64 trackId = -1;
    QString name;
    float volumeDb = 0.0f;
    float pan = 0.0f; // -1.0 (Left) to +1.0 (Right)
    bool mute = false;
    bool solo = false;
    ParametricEQ eq;
    AudioCompressor comp;
    float currentPeak = 0.0f;
};

class AudioMixerSuite {
public:
    static AudioMixerSuite& instance();

    AudioMixerChannel* getOrCreateChannel(qint64 trackId);
    const QList<AudioMixerChannel>& channels() const { return m_channels; }

    ParametricEQ& masterEq() { return m_masterEq; }
    AudioCompressor& masterCompressor() { return m_masterComp; }
    BrickwallLimiter& masterLimiter() { return m_masterLimiter; }

    void processMasterBus(float *samples, int count);
    void resetAll();

private:
    AudioMixerSuite();
    QList<AudioMixerChannel> m_channels;
    ParametricEQ m_masterEq;
    AudioCompressor m_masterComp;
    BrickwallLimiter m_masterLimiter;
};
