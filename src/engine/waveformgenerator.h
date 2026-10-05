#pragma once

#include <QString>
#include <QVector>
#include <QMap>
#include <QMutex>

class WaveformGenerator {
public:
    static WaveformGenerator& instance();

    // Returns a list of normalized peak amplitudes [0.0, 1.0] sampled at pointsPerSecond (default 20 Hz)
    QVector<float> getWaveform(const QString &filePath, int pointsPerSecond = 20);

private:
    WaveformGenerator();
    ~WaveformGenerator();

    QVector<float> computeWaveform(const QString &filePath, int pointsPerSecond);

    QMutex m_mutex;
    QMap<QString, QVector<float>> m_cache;
};
