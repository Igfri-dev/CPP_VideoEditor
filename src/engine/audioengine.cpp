#include "audioengine.h"
#include <QMediaDevices>
#include <QAudioDevice>
#include <QtMath>
#include <cmath>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
}

AudioEngine::AudioEngine(TimelineModel *model, QObject *parent)
    : QObject(parent)
    , m_model(model)
{
    m_format.setSampleRate(44100);
    m_format.setChannelCount(2);
    m_format.setSampleFormat(QAudioFormat::Int16);

    QAudioDevice defaultDevice = QMediaDevices::defaultAudioOutput();
    m_audioSink = new QAudioSink(defaultDevice, m_format, this);
    m_audioSink->setBufferSize(44100 * 4 / 10); // ~100ms buffer for low latency

    connect(&m_feedTimer, &QTimer::timeout, this, &AudioEngine::onFeedAudio);
}

AudioEngine::~AudioEngine()
{
    stopPlayback();
}

void AudioEngine::setMasterVolume(double volume)
{
    m_masterVolume = qBound(0.0, volume, 2.0);
    if (m_audioSink) {
        m_audioSink->setVolume(qBound(0.0, m_masterVolume, 1.0));
    }
}

bool AudioEngine::isAudioOutputActive() const
{
    return m_isPlaying && m_audioSink && m_audioIo &&
           (m_audioSink->state() == QAudio::ActiveState || m_audioSink->state() == QAudio::IdleState);
}

qint64 AudioEngine::currentAudiblePositionMs() const
{
    if (!m_isPlaying || !m_audioSink) {
        return m_lastAudiblePositionMs;
    }
    qint64 playedUSecs = m_audioSink->processedUSecs();
    qint64 calculatedMs = m_startTimelineMs + (playedUSecs / 1000LL);
    return qMax(m_lastAudiblePositionMs, calculatedMs);
}

void AudioEngine::setPosition(qint64 timelineMs)
{
    m_startTimelineMs = qMax<qint64>(0, timelineMs);
    m_writeTimelineMs = m_startTimelineMs;
    m_lastAudiblePositionMs = m_startTimelineMs;
    if (m_audioSink) {
        m_audioSink->reset();
        if (m_isPlaying) {
            m_audioIo = m_audioSink->start();
            onFeedAudio();
        }
    }
}

void AudioEngine::startPlayback(qint64 startTimelineMs)
{
    m_startTimelineMs = qMax<qint64>(0, startTimelineMs);
    m_writeTimelineMs = m_startTimelineMs;
    m_lastAudiblePositionMs = m_startTimelineMs;
    m_isPlaying = true;
    if (m_audioSink) {
        m_audioSink->reset();
        m_audioIo = m_audioSink->start();
    }
    onFeedAudio();
    m_feedTimer.start(16); // Run at ~60 Hz for smooth playhead updates
}

void AudioEngine::pausePlayback()
{
    m_isPlaying = false;
    m_feedTimer.stop();
    if (m_audioSink) {
        m_audioSink->reset(); // Instantly clears audio buffer so no trailing audio leaks!
    }
    m_audioIo = nullptr;
    emit audioLevelsChanged(0.0, 0.0);
}

void AudioEngine::stopPlayback()
{
    m_isPlaying = false;
    m_feedTimer.stop();
    if (m_audioSink) {
        m_audioSink->stop();
    }
    m_audioIo = nullptr;
    emit audioLevelsChanged(0.0, 0.0);
}

const AudioEngine::DecodedAudio* AudioEngine::getOrCreateDecodedAudio(const QString &filePath)
{
    QMutexLocker locker(&m_cacheMutex);
    if (m_audioCache.contains(filePath)) {
        return &m_audioCache[filePath];
    }

    DecodedAudio da = decodeFileToPcm(filePath);
    m_audioCache.insert(filePath, da);
    return &m_audioCache[filePath];
}

AudioEngine::DecodedAudio AudioEngine::decodeFileToPcm(const QString &filePath)
{
    DecodedAudio result;
    result.sampleRate = 44100;
    result.channels = 2;

    AVFormatContext *fmtCtx = nullptr;
    if (avformat_open_input(&fmtCtx, filePath.toUtf8().constData(), nullptr, nullptr) != 0) {
        return result;
    }
    if (avformat_find_stream_info(fmtCtx, nullptr) < 0) {
        avformat_close_input(&fmtCtx);
        return result;
    }

    int audioStreamIdx = -1;
    for (unsigned int i = 0; i < fmtCtx->nb_streams; ++i) {
        if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioStreamIdx = i;
            break;
        }
    }

    if (audioStreamIdx < 0) {
        avformat_close_input(&fmtCtx);
        return result;
    }

    AVCodecParameters *codecPar = fmtCtx->streams[audioStreamIdx]->codecpar;
    const AVCodec *codec = avcodec_find_decoder(codecPar->codec_id);
    if (!codec) {
        avformat_close_input(&fmtCtx);
        return result;
    }

    AVCodecContext *codecCtx = avcodec_alloc_context3(codec);
    if (!codecCtx || avcodec_parameters_to_context(codecCtx, codecPar) < 0 ||
        avcodec_open2(codecCtx, codec, nullptr) != 0) {
        if (codecCtx) avcodec_free_context(&codecCtx);
        avformat_close_input(&fmtCtx);
        return result;
    }

    SwrContext *swr = swr_alloc();
    av_opt_set_chlayout(swr, "in_chlayout", &codecCtx->ch_layout, 0);
    av_opt_set_int(swr, "in_sample_rate", codecCtx->sample_rate, 0);
    av_opt_set_sample_fmt(swr, "in_sample_fmt", codecCtx->sample_fmt, 0);

    AVChannelLayout outLayout;
    av_channel_layout_default(&outLayout, 2); // Stereo
    av_opt_set_chlayout(swr, "out_chlayout", &outLayout, 0);
    av_opt_set_int(swr, "out_sample_rate", 44100, 0);
    av_opt_set_sample_fmt(swr, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);

    if (swr_init(swr) < 0) {
        swr_free(&swr);
        avcodec_free_context(&codecCtx);
        avformat_close_input(&fmtCtx);
        return result;
    }

    AVPacket *pkt = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();

    const int maxOutSamples = 4096;
    int16_t outBuffer[maxOutSamples * 2]; // stereo

    while (av_read_frame(fmtCtx, pkt) >= 0) {
        if (pkt->stream_index == audioStreamIdx) {
            if (avcodec_send_packet(codecCtx, pkt) == 0) {
                while (avcodec_receive_frame(codecCtx, frame) == 0) {
                    uint8_t *outData[1] = { reinterpret_cast<uint8_t*>(outBuffer) };
                    int converted = swr_convert(swr, outData, maxOutSamples,
                                                (const uint8_t**)frame->data, frame->nb_samples);
                    if (converted > 0) {
                        result.pcmData.append(reinterpret_cast<const char*>(outBuffer), converted * 4);
                    }
                }
            }
        }
        av_packet_unref(pkt);
    }

    av_packet_free(&pkt);
    av_frame_free(&frame);
    swr_free(&swr);
    avcodec_free_context(&codecCtx);
    avformat_close_input(&fmtCtx);

    result.durationMs = (result.pcmData.size() * 1000LL) / (44100LL * 4LL);
    return result;
}

void AudioEngine::onFeedAudio()
{
    if (!m_isPlaying || !m_audioIo || !m_audioSink) {
        return;
    }

    // 1. Calculate actual audible position based on DAC processed time
    qint64 currentAudibleMs = currentAudiblePositionMs();
    m_lastAudiblePositionMs = currentAudibleMs;

    if (m_model && currentAudibleMs >= m_model->totalDurationMs()) {
        pausePlayback();
        emit positionAdvanced(m_model->totalDurationMs());
        return;
    }

    // Emit audible position so playhead and video match what is currently heard
    emit positionAdvanced(currentAudibleMs);

    // 2. Buffer management: keep buffer filled ~80-120ms ahead of currentAudibleMs
    int bytesFree = m_audioSink->bytesFree();
    if (bytesFree < 1024) {
        return;
    }

    qint64 leadTimeMs = m_writeTimelineMs - currentAudibleMs;
    if (leadTimeMs >= 100) {
        return;
    }

    int msToMix = qMin<int>(35, 100 - static_cast<int>(leadTimeMs));
    int bytesToMix = (msToMix * 44100 * 4) / 1000;
    bytesToMix = qMin(bytesToMix, bytesFree);
    int samplesToMix = bytesToMix / 4;
    if (samplesToMix <= 0) {
        return;
    }

    bool hasSoloTrack = false;
    for (const TimelineTrack &track : m_model->audioTracks()) {
        if (track.isSolo()) {
            hasSoloTrack = true;
            break;
        }
    }

    QVector<int32_t> mixBuffer(samplesToMix * 2, 0);

    for (const TimelineTrack &track : m_model->audioTracks()) {
        if (track.isMuted()) continue;
        if (hasSoloTrack && !track.isSolo()) continue;

        double trackVol = track.volume();
        if (trackVol <= 0.001) continue;

        for (const TimelineClip &clip : track.clips()) {
            if (clip.isAudioMuted()) continue;

            qint64 chunkStartMs = m_writeTimelineMs;
            qint64 chunkEndMs = m_writeTimelineMs + msToMix;
            if (chunkEndMs <= clip.timelineInMs() || chunkStartMs >= clip.timelineOutMs()) {
                continue;
            }

            const DecodedAudio *da = getOrCreateDecodedAudio(clip.filePath());
            if (!da || da->pcmData.isEmpty()) continue;

            const int16_t *pcm16 = reinterpret_cast<const int16_t*>(da->pcmData.constData());
            const qint64 totalPcmStereoSamples = da->pcmData.size() / 4;

            for (int i = 0; i < samplesToMix; ++i) {
                qint64 currentSampleTimeMs = chunkStartMs + (i * 1000LL) / 44100LL;
                if (currentSampleTimeMs < clip.timelineInMs() || currentSampleTimeMs >= clip.timelineOutMs()) {
                    continue;
                }

                double clipVol = clip.volumeAt(currentSampleTimeMs) * trackVol * m_masterVolume;
                if (clipVol <= 0.001) continue;

                qint64 sourceMs = clip.mapTimelineToSourceMs(currentSampleTimeMs);
                qint64 sourceSampleIdx = (sourceMs * 44100LL) / 1000LL;

                if (sourceSampleIdx >= 0 && sourceSampleIdx < totalPcmStereoSamples) {
                    int16_t leftSample = pcm16[sourceSampleIdx * 2];
                    int16_t rightSample = pcm16[sourceSampleIdx * 2 + 1];

                    mixBuffer[i * 2] += static_cast<int32_t>(leftSample * clipVol);
                    mixBuffer[i * 2 + 1] += static_cast<int32_t>(rightSample * clipVol);
                }
            }
        }
    }

    // Calculate peak levels for VU meter
    double maxL = 0.0;
    double maxR = 0.0;
    for (int i = 0; i < samplesToMix; ++i) {
        double l = std::abs(mixBuffer[i * 2]) / 32768.0;
        double r = std::abs(mixBuffer[i * 2 + 1]) / 32768.0;
        if (l > maxL) maxL = l;
        if (r > maxR) maxR = r;
    }
    emit audioLevelsChanged(qBound(0.0, maxL, 1.0), qBound(0.0, maxR, 1.0));

    // Convert mixed 32-bit to 16-bit clamped PCM
    QByteArray outputBytes(samplesToMix * 4, 0);
    int16_t *out16 = reinterpret_cast<int16_t*>(outputBytes.data());
    for (int i = 0; i < samplesToMix * 2; ++i) {
        out16[i] = static_cast<int16_t>(qBound(-32767, mixBuffer[i], 32767));
    }

    qint64 written = m_audioIo->write(outputBytes);
    if (written > 0) {
        m_writeTimelineMs += (written * 1000LL) / (44100LL * 4LL);
    }
}
