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
    m_audioSink->setBufferSize(44100 * 4 * 40 / 100); // ~400ms buffer to prevent underruns

    connect(&m_feedTimer, &QTimer::timeout, this, &AudioEngine::onFeedAudio);
}

AudioEngine::~AudioEngine()
{
    stopPlayback();
}

void AudioEngine::feedAudio()
{
    onFeedAudio();
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
    m_writeSampleIndex = (m_startTimelineMs * 44100LL) / 1000LL;
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
    m_writeSampleIndex = (m_startTimelineMs * 44100LL) / 1000LL;
    m_writeTimelineMs = m_startTimelineMs;
    m_lastAudiblePositionMs = m_startTimelineMs;
    m_isPlaying = true;
    if (m_audioSink) {
        m_audioSink->reset();
        m_audioIo = m_audioSink->start();
    }
    onFeedAudio();
    onFeedAudio(); // Prime initial buffer with ~200-300ms headroom
    m_feedTimer.start(15); // Run at ~66 Hz for smooth updates
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

    if (codecCtx->ch_layout.nb_channels <= 0) {
        av_channel_layout_default(&codecCtx->ch_layout, 2);
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
        av_channel_layout_uninit(&outLayout);
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

    // Flush remaining frames from decoder
    avcodec_send_packet(codecCtx, nullptr);
    while (avcodec_receive_frame(codecCtx, frame) == 0) {
        uint8_t *outData[1] = { reinterpret_cast<uint8_t*>(outBuffer) };
        int converted = swr_convert(swr, outData, maxOutSamples,
                                    (const uint8_t**)frame->data, frame->nb_samples);
        if (converted > 0) {
            result.pcmData.append(reinterpret_cast<const char*>(outBuffer), converted * 4);
        }
    }

    // Flush delayed samples in resampler
    uint8_t *outData[1] = { reinterpret_cast<uint8_t*>(outBuffer) };
    int converted = swr_convert(swr, outData, maxOutSamples, nullptr, 0);
    if (converted > 0) {
        result.pcmData.append(reinterpret_cast<const char*>(outBuffer), converted * 4);
    }

    av_packet_free(&pkt);
    av_frame_free(&frame);
    av_channel_layout_uninit(&outLayout);
    swr_free(&swr);
    avcodec_free_context(&codecCtx);
    avformat_close_input(&fmtCtx);

    result.durationMs = (result.pcmData.size() * 1000LL) / (44100LL * 4LL);
    return result;
}

void AudioEngine::onFeedAudio()
{
    static bool inFeed = false;
    if (inFeed || !m_isPlaying || !m_audioIo || !m_audioSink) {
        return;
    }
    struct Guard {
        bool &flag;
        Guard(bool &f) : flag(f) { flag = true; }
        ~Guard() { flag = false; }
    } guard(inFeed);

    // 1. Calculate actual audible position based on DAC processed time
    qint64 currentAudibleMs = currentAudiblePositionMs();
    m_lastAudiblePositionMs = currentAudibleMs;

    if (m_model && currentAudibleMs >= m_model->totalDurationMs()) {
        pausePlayback();
        emit positionAdvanced(m_model->totalDurationMs());
        return;
    }

    int bytesFree = m_audioSink->bytesFree();
    if (bytesFree < 1024) {
        emit positionAdvanced(currentAudibleMs);
        return;
    }

    // 2. Buffer management: keep buffer filled ~350-400ms ahead of currentAudibleMs
    const qint64 currentAudibleSample = (currentAudibleMs * 44100LL) / 1000LL;
    if (m_writeSampleIndex < currentAudibleSample) {
        m_writeSampleIndex = currentAudibleSample;
    }
    const qint64 leadSamples = m_writeSampleIndex - currentAudibleSample;
    const qint64 maxLeadSamples = (44100LL * 350LL) / 1000LL; // ~350ms lead buffer

    if (leadSamples >= maxLeadSamples) {
        emit positionAdvanced(currentAudibleMs);
        return;
    }

    int samplesNeeded = static_cast<int>(maxLeadSamples - leadSamples);
    // Mix in batches of up to ~100ms (4410 samples) per tick to quickly replenish buffer and prevent starvation
    int samplesToMix = qMin(samplesNeeded, 44100 * 100 / 1000);
    samplesToMix = qMin(samplesToMix, bytesFree / 4);
    if (samplesToMix <= 0) {
        emit positionAdvanced(currentAudibleMs);
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

    const qint64 chunkStartSample = m_writeSampleIndex;
    const qint64 chunkEndSample = m_writeSampleIndex + samplesToMix;

    for (const TimelineTrack &track : m_model->audioTracks()) {
        if (track.isMuted()) continue;
        if (hasSoloTrack && !track.isSolo()) continue;

        double trackVol = track.volume();
        if (trackVol <= 0.001) continue;

        for (const TimelineClip &clip : track.clips()) {
            if (clip.isAudioMuted()) continue;

            const qint64 clipTlInSample = (clip.timelineInMs() * 44100LL) / 1000LL;
            const qint64 clipTlOutSample = (clip.timelineOutMs() * 44100LL) / 1000LL;

            // Check if clip overlaps with current mixing chunk
            if (chunkEndSample <= clipTlInSample || chunkStartSample >= clipTlOutSample) {
                continue;
            }

            const DecodedAudio *da = getOrCreateDecodedAudio(clip.filePath());
            if (!da || da->pcmData.isEmpty()) continue;

            const int16_t *pcm16 = reinterpret_cast<const int16_t*>(da->pcmData.constData());
            const qint64 totalPcmStereoSamples = da->pcmData.size() / 4;

            const qint64 clipSrcInSample = (clip.sourceInMs() * 44100LL) / 1000LL;
            const double speed = (clip.speed() > 0.001) ? clip.speed() : 1.0;

            const int iStart = static_cast<int>(qMax<qint64>(0, clipTlInSample - chunkStartSample));
            const int iEnd = static_cast<int>(qMin<qint64>(samplesToMix, clipTlOutSample - chunkStartSample));

            for (int i = iStart; i < iEnd; ++i) {
                const qint64 tlSample = chunkStartSample + i;
                const qint64 curMs = (tlSample * 1000LL) / 44100LL;

                const double clipVol = clip.volumeAt(curMs) * trackVol * m_masterVolume;
                if (clipVol <= 0.001) continue;

                const qint64 offset = tlSample - clipTlInSample;

                int16_t leftSample = 0;
                int16_t rightSample = 0;

                if (std::abs(speed - 1.0) < 0.0001) {
                    const qint64 srcIdx = clipSrcInSample + offset;
                    if (srcIdx >= 0 && srcIdx < totalPcmStereoSamples) {
                        leftSample = pcm16[srcIdx * 2];
                        rightSample = pcm16[srcIdx * 2 + 1];
                    }
                } else {
                    const double srcExact = clipSrcInSample + (offset * speed);
                    const qint64 s0 = static_cast<qint64>(std::floor(srcExact));
                    const double frac = srcExact - s0;

                    if (s0 >= 0 && s0 < totalPcmStereoSamples) {
                        if (frac > 0.001 && s0 + 1 < totalPcmStereoSamples) {
                            leftSample = static_cast<int16_t>(pcm16[s0 * 2] * (1.0 - frac) + pcm16[(s0 + 1) * 2] * frac);
                            rightSample = static_cast<int16_t>(pcm16[s0 * 2 + 1] * (1.0 - frac) + pcm16[(s0 + 1) * 2 + 1] * frac);
                        } else {
                            leftSample = pcm16[s0 * 2];
                            rightSample = pcm16[s0 * 2 + 1];
                        }
                    }
                }

                mixBuffer[i * 2] += static_cast<int32_t>(leftSample * clipVol);
                mixBuffer[i * 2 + 1] += static_cast<int32_t>(rightSample * clipVol);
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
        m_writeSampleIndex += (written / 4);
        m_writeTimelineMs = (m_writeSampleIndex * 1000LL) / 44100LL;
    }

    emit positionAdvanced(currentAudibleMs);
}
