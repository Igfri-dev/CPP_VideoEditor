#pragma once

#include <QObject>
#include <QAudioSink>
#include <QAudioFormat>
#include <QIODevice>
#include <QByteArray>
#include <QMap>
#include <QMutex>
#include <QTimer>
#include "../core/timelinemodel.h"

class AudioEngine : public QObject {
    Q_OBJECT

public:
    explicit AudioEngine(TimelineModel *model, QObject *parent = nullptr);
    ~AudioEngine() override;

    void startPlayback(qint64 startTimelineMs);
    void stopPlayback();
    void pausePlayback();
    void setPosition(qint64 timelineMs);
    void setMasterVolume(double volume);
    double masterVolume() const { return m_masterVolume; }

    bool isPlaying() const { return m_isPlaying; }
    bool isAudioOutputActive() const;
    qint64 currentAudiblePositionMs() const;
    void feedAudio();

signals:
    void positionAdvanced(qint64 timelineMs);
    void audioLevelsChanged(double leftLevel, double rightLevel);

private slots:
    void onFeedAudio();

private:
    struct DecodedAudio {
        int sampleRate = 44100;
        int channels = 2;
        QByteArray pcmData; // 16-bit signed stereo little endian
        qint64 durationMs = 0;
    };

    const DecodedAudio* getOrCreateDecodedAudio(const QString &filePath);
    DecodedAudio decodeFileToPcm(const QString &filePath);

    TimelineModel *m_model = nullptr;
    QAudioSink *m_audioSink = nullptr;
    QIODevice *m_audioIo = nullptr;
    QAudioFormat m_format;
    QTimer m_feedTimer;

    qint64 m_startTimelineMs = 0;
    qint64 m_writeTimelineMs = 0;
    qint64 m_writeSampleIndex = 0;
    qint64 m_lastAudiblePositionMs = 0;
    double m_masterVolume = 1.0;
    bool m_isPlaying = false;

    QMutex m_cacheMutex;
    QMap<QString, DecodedAudio> m_audioCache;
};
