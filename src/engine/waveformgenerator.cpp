#include "waveformgenerator.h"
#include <QMutexLocker>
#include <QtMath>
#include <cmath>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
}

WaveformGenerator& WaveformGenerator::instance()
{
    static WaveformGenerator s_instance;
    return s_instance;
}

WaveformGenerator::WaveformGenerator()
{
}

WaveformGenerator::~WaveformGenerator()
{
}

QVector<float> WaveformGenerator::getWaveform(const QString &filePath, int pointsPerSecond)
{
    QMutexLocker locker(&m_mutex);
    QString key = QString("%1_%2").arg(filePath).arg(pointsPerSecond);
    if (m_cache.contains(key)) {
        return m_cache.value(key);
    }

    QVector<float> waveform = computeWaveform(filePath, pointsPerSecond);
    m_cache.insert(key, waveform);
    return waveform;
}

QVector<float> WaveformGenerator::computeWaveform(const QString &filePath, int pointsPerSecond)
{
    QVector<float> peaks;

    AVFormatContext *fmtCtx = nullptr;
    if (avformat_open_input(&fmtCtx, filePath.toUtf8().constData(), nullptr, nullptr) != 0) {
        return peaks;
    }

    if (avformat_find_stream_info(fmtCtx, nullptr) < 0) {
        avformat_close_input(&fmtCtx);
        return peaks;
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
        return peaks;
    }

    AVCodecParameters *codecPar = fmtCtx->streams[audioStreamIdx]->codecpar;
    const AVCodec *codec = avcodec_find_decoder(codecPar->codec_id);
    if (!codec) {
        avformat_close_input(&fmtCtx);
        return peaks;
    }

    AVCodecContext *codecCtx = avcodec_alloc_context3(codec);
    if (!codecCtx || avcodec_parameters_to_context(codecCtx, codecPar) < 0 ||
        avcodec_open2(codecCtx, codec, nullptr) != 0) {
        if (codecCtx) avcodec_free_context(&codecCtx);
        avformat_close_input(&fmtCtx);
        return peaks;
    }

    // Set up resampler to 8000 Hz Mono 16-bit PCM for ultra-fast waveform extraction
    const int targetSampleRate = 8000;
    SwrContext *swr = swr_alloc();
    av_opt_set_chlayout(swr, "in_chlayout", &codecCtx->ch_layout, 0);
    av_opt_set_int(swr, "in_sample_rate", codecCtx->sample_rate, 0);
    av_opt_set_sample_fmt(swr, "in_sample_fmt", codecCtx->sample_fmt, 0);

    AVChannelLayout outLayout;
    av_channel_layout_default(&outLayout, 1); // Mono
    av_opt_set_chlayout(swr, "out_chlayout", &outLayout, 0);
    av_opt_set_int(swr, "out_sample_rate", targetSampleRate, 0);
    av_opt_set_sample_fmt(swr, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);

    if (swr_init(swr) < 0) {
        swr_free(&swr);
        avcodec_free_context(&codecCtx);
        avformat_close_input(&fmtCtx);
        return peaks;
    }

    const int samplesPerPoint = targetSampleRate / pointsPerSecond;
    AVPacket *pkt = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();

    int16_t outBuffer[4096];
    float currentPeak = 0.0f;
    int samplesInCurrentPoint = 0;

    while (av_read_frame(fmtCtx, pkt) >= 0) {
        if (pkt->stream_index == audioStreamIdx) {
            if (avcodec_send_packet(codecCtx, pkt) == 0) {
                while (avcodec_receive_frame(codecCtx, frame) == 0) {
                    uint8_t *outData[1] = { reinterpret_cast<uint8_t*>(outBuffer) };
                    int converted = swr_convert(swr, outData, 4096,
                                                (const uint8_t**)frame->data, frame->nb_samples);
                    for (int i = 0; i < converted; ++i) {
                        float absVal = std::abs(outBuffer[i]) / 32768.0f;
                        if (absVal > currentPeak) {
                            currentPeak = absVal;
                        }
                        samplesInCurrentPoint++;
                        if (samplesInCurrentPoint >= samplesPerPoint) {
                            peaks.append(qBound(0.05f, currentPeak, 1.0f));
                            currentPeak = 0.0f;
                            samplesInCurrentPoint = 0;
                        }
                    }
                }
            }
        }
        av_packet_unref(pkt);
    }

    if (samplesInCurrentPoint > 0) {
        peaks.append(qBound(0.05f, currentPeak, 1.0f));
    }

    av_packet_free(&pkt);
    av_frame_free(&frame);
    swr_free(&swr);
    avcodec_free_context(&codecCtx);
    avformat_close_input(&fmtCtx);

    return peaks;
}
