#include "videoexporter.h"
#include "videocompositor.h"
#include <QProcess>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryFile>
#include <QCoreApplication>
#include <QDataStream>
#include <QStandardPaths>
#include <QMap>
#include <cmath>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
}

VideoExporter::VideoExporter(TimelineModel *model, QObject *parent)
    : QObject(parent)
    , m_model(model)
{
}

void VideoExporter::cancelExport()
{
    m_cancelRequested = true;
}

QString VideoExporter::defaultExtension(ExportContainer container)
{
    switch (container) {
    case ExportContainer::MP4: return "mp4";
    case ExportContainer::MOV: return "mov";
    case ExportContainer::MKV: return "mkv";
    case ExportContainer::WebM: return "webm";
    case ExportContainer::AVI: return "avi";
    case ExportContainer::GIF: return "gif";
    case ExportContainer::MP3: return "mp3";
    case ExportContainer::WAV: return "wav";
    case ExportContainer::AAC: return "m4a";
    case ExportContainer::FLAC: return "flac";
    case ExportContainer::OGG: return "ogg";
    }
    return "mp4";
}

ExportContainer VideoExporter::containerFromExtension(const QString &ext)
{
    QString e = ext.toLower();
    if (e.startsWith(".")) e = e.mid(1);

    if (e == "mov") return ExportContainer::MOV;
    if (e == "mkv") return ExportContainer::MKV;
    if (e == "webm") return ExportContainer::WebM;
    if (e == "avi") return ExportContainer::AVI;
    if (e == "gif") return ExportContainer::GIF;
    if (e == "mp3") return ExportContainer::MP3;
    if (e == "wav") return ExportContainer::WAV;
    if (e == "aac" || e == "m4a") return ExportContainer::AAC;
    if (e == "flac") return ExportContainer::FLAC;
    if (e == "ogg" || e == "oga") return ExportContainer::OGG;
    return ExportContainer::MP4;
}

VideoCodec VideoExporter::resolveVideoCodec(ExportContainer container, VideoCodec requested)
{
    if (requested != VideoCodec::Auto) return requested;
    switch (container) {
    case ExportContainer::WebM: return VideoCodec::VP9;
    case ExportContainer::MOV: return VideoCodec::ProRes;
    case ExportContainer::AVI: return VideoCodec::MPEG4;
    case ExportContainer::GIF: return VideoCodec::GIF;
    default: return VideoCodec::H264;
    }
}

AudioCodec VideoExporter::resolveAudioCodec(ExportContainer container, AudioCodec requested)
{
    if (requested != AudioCodec::Auto) return requested;
    switch (container) {
    case ExportContainer::WebM: return AudioCodec::Opus;
    case ExportContainer::MOV: return AudioCodec::PCM_16;
    case ExportContainer::AVI: return AudioCodec::MP3;
    case ExportContainer::GIF: return AudioCodec::None;
    case ExportContainer::MP3: return AudioCodec::MP3;
    case ExportContainer::WAV: return AudioCodec::PCM_16;
    case ExportContainer::AAC: return AudioCodec::AAC;
    case ExportContainer::FLAC: return AudioCodec::FLAC;
    case ExportContainer::OGG: return AudioCodec::Opus;
    default: return AudioCodec::AAC;
    }
}

QString VideoExporter::findFfmpegExecutable()
{
    // 1. Check local portable directory alongside application binary (useful for Windows/Linux portable bundles)
    QString appDir = QCoreApplication::applicationDirPath();
#if defined(Q_OS_WIN)
    QString localExe = QDir(appDir).filePath("ffmpeg.exe");
    if (QFile::exists(localExe)) return localExe;
#else
    QString localExe = QDir(appDir).filePath("ffmpeg");
    if (QFile::exists(localExe)) return localExe;
#endif

    // 2. Search in system PATH via QStandardPaths (cross-platform, finds ffmpeg.exe on Windows, ffmpeg on macOS/Linux)
    QString sysExe = QStandardPaths::findExecutable("ffmpeg");
    if (!sysExe.isEmpty()) {
        return sysExe;
    }

    // 3. Platform-specific known directories
#if defined(Q_OS_MACOS)
    if (QFile::exists("/opt/homebrew/bin/ffmpeg")) {
        return "/opt/homebrew/bin/ffmpeg";
    }
    if (QFile::exists("/usr/local/bin/ffmpeg")) {
        return "/usr/local/bin/ffmpeg";
    }
#elif defined(Q_OS_LINUX)
    if (QFile::exists("/usr/bin/ffmpeg")) {
        return "/usr/bin/ffmpeg";
    }
    if (QFile::exists("/usr/local/bin/ffmpeg")) {
        return "/usr/local/bin/ffmpeg";
    }
#elif defined(Q_OS_WIN)
    if (QFile::exists("C:/ffmpeg/bin/ffmpeg.exe")) {
        return "C:/ffmpeg/bin/ffmpeg.exe";
    }
#endif

    return "ffmpeg";
}

QString VideoExporter::findFfmpegPath() const
{
    return findFfmpegExecutable();
}

bool VideoExporter::isEncoderAvailable(const QString &encoderName)
{
    static QMap<QString, bool> s_cache;
    static QString s_cachedEncodersOutput;
    static bool s_hasQueried = false;

    if (!s_hasQueried) {
        s_hasQueried = true;
        QString ffmpegBin = findFfmpegExecutable();
        QProcess proc;
        proc.start(ffmpegBin, QStringList() << "-encoders");
        if (proc.waitForFinished(1500)) {
            s_cachedEncodersOutput = QString::fromUtf8(proc.readAllStandardOutput());
        }
    }

    if (s_cache.contains(encoderName)) {
        return s_cache.value(encoderName);
    }

    bool available = s_cachedEncodersOutput.contains(encoderName);
    s_cache.insert(encoderName, available);
    return available;
}

bool VideoExporter::isHardwareAccelerationAvailable()
{
#if defined(Q_OS_MACOS)
    return isEncoderAvailable("h264_videotoolbox");
#elif defined(Q_OS_WIN)
    return isEncoderAvailable("h264_nvenc") || isEncoderAvailable("h264_qsv") || isEncoderAvailable("h264_amf");
#elif defined(Q_OS_LINUX)
    return isEncoderAvailable("h264_nvenc") || isEncoderAvailable("h264_vaapi");
#else
    return false;
#endif
}

QString VideoExporter::platformHardwareAccelerationName()
{
#if defined(Q_OS_MACOS)
    return "Apple Silicon (VideoToolbox)";
#elif defined(Q_OS_WIN)
    if (isEncoderAvailable("h264_nvenc")) return "NVIDIA NVENC (GPU)";
    if (isEncoderAvailable("h264_qsv")) return "Intel Quick Sync (QSV)";
    if (isEncoderAvailable("h264_amf")) return "AMD AMF (GPU)";
    return "GPU (NVIDIA NVENC / Intel QSV / AMD)";
#elif defined(Q_OS_LINUX)
    if (isEncoderAvailable("h264_nvenc")) return "NVIDIA NVENC (GPU)";
    if (isEncoderAvailable("h264_vaapi")) return "VA-API (Intel/AMD)";
    return "VA-API / NVIDIA NVENC (GPU)";
#else
    return "GPU";
#endif
}

QString VideoExporter::resolveH264Encoder(bool useHardware)
{
    if (!useHardware) return "libx264";

#if defined(Q_OS_MACOS)
    if (isEncoderAvailable("h264_videotoolbox")) return "h264_videotoolbox";
#elif defined(Q_OS_WIN)
    if (isEncoderAvailable("h264_nvenc")) return "h264_nvenc";
    if (isEncoderAvailable("h264_qsv")) return "h264_qsv";
    if (isEncoderAvailable("h264_amf")) return "h264_amf";
#elif defined(Q_OS_LINUX)
    if (isEncoderAvailable("h264_nvenc")) return "h264_nvenc";
    if (isEncoderAvailable("h264_vaapi")) return "h264_vaapi";
#endif

    return "libx264";
}

QString VideoExporter::resolveHevcEncoder(bool useHardware)
{
    if (!useHardware) return "libx265";

#if defined(Q_OS_MACOS)
    if (isEncoderAvailable("hevc_videotoolbox")) return "hevc_videotoolbox";
#elif defined(Q_OS_WIN)
    if (isEncoderAvailable("hevc_nvenc")) return "hevc_nvenc";
    if (isEncoderAvailable("hevc_qsv")) return "hevc_qsv";
    if (isEncoderAvailable("hevc_amf")) return "hevc_amf";
#elif defined(Q_OS_LINUX)
    if (isEncoderAvailable("hevc_nvenc")) return "hevc_nvenc";
    if (isEncoderAvailable("hevc_vaapi")) return "hevc_vaapi";
#endif

    return "libx265";
}

QString VideoExporter::resolveProResEncoder(bool useHardware)
{
#if defined(Q_OS_MACOS)
    if (useHardware && isEncoderAvailable("prores_videotoolbox")) {
        return "prores_videotoolbox";
    }
#endif
    Q_UNUSED(useHardware);
    return "prores_ks";
}

struct TempAudioData {
    int sampleRate = 44100;
    int channels = 2;
    QByteArray pcmData;
};

static TempAudioData decodeAudioFileToPcm(const QString &filePath)
{
    TempAudioData result;
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
    swr_init(swr);

    AVPacket *pkt = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();

    while (av_read_frame(fmtCtx, pkt) >= 0) {
        if (pkt->stream_index == audioStreamIdx) {
            if (avcodec_send_packet(codecCtx, pkt) == 0) {
                while (avcodec_receive_frame(codecCtx, frame) == 0) {
                    int outSamples = swr_get_out_samples(swr, frame->nb_samples);
                    int bufSize = outSamples * 2 * sizeof(int16_t);
                    QByteArray convertedChunk(bufSize, 0);
                    uint8_t *outData[1] = { reinterpret_cast<uint8_t*>(convertedChunk.data()) };

                    int converted = swr_convert(swr, outData, outSamples,
                                                (const uint8_t**)frame->data, frame->nb_samples);
                    if (converted > 0) {
                        result.pcmData.append(convertedChunk.constData(), converted * 2 * sizeof(int16_t));
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

    return result;
}

bool VideoExporter::mixTimelineAudioToWav(TimelineModel *model, qint64 startMs, qint64 durationMs, const QString &outputWavPath)
{
    QFile file(outputWavPath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    const int sampleRate = 44100;
    const int channels = 2;
    const int bitsPerSample = 16;
    const qint64 totalDurationMs = durationMs;
    const qint64 totalSamples = (totalDurationMs * sampleRate) / 1000;
    const qint64 dataChunkSize = totalSamples * channels * (bitsPerSample / 8);

    // Write WAV header
    QByteArray header(44, 0);
    char *hdr = header.data();
    memcpy(hdr, "RIFF", 4);
    *reinterpret_cast<uint32_t*>(hdr + 4) = static_cast<uint32_t>(36 + dataChunkSize);
    memcpy(hdr + 8, "WAVE", 4);
    memcpy(hdr + 12, "fmt ", 4);
    *reinterpret_cast<uint32_t*>(hdr + 16) = 16; // PCM
    *reinterpret_cast<uint16_t*>(hdr + 20) = 1;  // Linear PCM
    *reinterpret_cast<uint16_t*>(hdr + 22) = channels;
    *reinterpret_cast<uint32_t*>(hdr + 24) = sampleRate;
    *reinterpret_cast<uint32_t*>(hdr + 28) = sampleRate * channels * (bitsPerSample / 8);
    *reinterpret_cast<uint16_t*>(hdr + 32) = channels * (bitsPerSample / 8);
    *reinterpret_cast<uint16_t*>(hdr + 34) = bitsPerSample;
    memcpy(hdr + 36, "data", 4);
    *reinterpret_cast<uint32_t*>(hdr + 40) = static_cast<uint32_t>(dataChunkSize);
    file.write(header);

    // Cache decoded audio of active clips
    QMap<QString, TempAudioData> audioCache;
    if (model) {
        for (const TimelineTrack &track : model->audioTracks()) {
            if (track.isMuted()) continue;
            for (const TimelineClip &clip : track.clips()) {
                if (!clip.isAudioMuted() && !audioCache.contains(clip.filePath())) {
                    audioCache.insert(clip.filePath(), decodeAudioFileToPcm(clip.filePath()));
                }
            }
        }
    }

    const int chunkSize = 4096;
    qint64 samplesWritten = 0;
    QVector<int32_t> mixBuffer(chunkSize * channels, 0);
    QByteArray outputBytes(chunkSize * channels * 2, 0);

    while (samplesWritten < totalSamples) {
        int samplesToProcess = qMin<qint64>(chunkSize, totalSamples - samplesWritten);
        mixBuffer.fill(0, samplesToProcess * channels);

        if (model) {
            for (const TimelineTrack &track : model->audioTracks()) {
                if (track.isMuted()) continue;
                double trackVol = track.volume();
                if (trackVol <= 0.001) continue;

                for (const TimelineClip &clip : track.clips()) {
                    if (clip.isAudioMuted()) continue;

                    qint64 chunkStartTimelineMs = startMs + (samplesWritten * 1000LL) / sampleRate;
                    qint64 chunkEndTimelineMs = startMs + ((samplesWritten + samplesToProcess) * 1000LL) / sampleRate;

                    if (chunkEndTimelineMs <= clip.timelineInMs() || chunkStartTimelineMs >= clip.timelineOutMs()) {
                        continue;
                    }

                    if (!audioCache.contains(clip.filePath())) continue;
                    const TempAudioData &da = audioCache[clip.filePath()];
                    if (da.pcmData.isEmpty()) continue;

                    const int16_t *pcm16 = reinterpret_cast<const int16_t*>(da.pcmData.constData());
                    const qint64 totalPcmStereoSamples = da.pcmData.size() / 4;

                    for (int i = 0; i < samplesToProcess; ++i) {
                        qint64 curMs = startMs + ((samplesWritten + i) * 1000LL) / sampleRate;
                        if (curMs < clip.timelineInMs() || curMs >= clip.timelineOutMs()) {
                            continue;
                        }

                        double vol = clip.volumeAt(curMs) * trackVol;
                        if (vol <= 0.001) continue;

                        qint64 srcMs = clip.mapTimelineToSourceMs(curMs);
                        qint64 srcSampleIdx = (srcMs * sampleRate) / 1000LL;

                        if (srcSampleIdx >= 0 && srcSampleIdx < totalPcmStereoSamples) {
                            int16_t left = pcm16[srcSampleIdx * 2];
                            int16_t right = pcm16[srcSampleIdx * 2 + 1];
                            mixBuffer[i * 2] += static_cast<int32_t>(left * vol);
                            mixBuffer[i * 2 + 1] += static_cast<int32_t>(right * vol);
                        }
                    }
                }
            }
        }

        // Clamp to 16-bit signed PCM
        int16_t *out16 = reinterpret_cast<int16_t*>(outputBytes.data());
        for (int i = 0; i < samplesToProcess * channels; ++i) {
            out16[i] = static_cast<int16_t>(qBound(-32767, mixBuffer[i], 32767));
        }

        file.write(outputBytes.constData(), samplesToProcess * channels * 2);
        samplesWritten += samplesToProcess;
    }

    file.close();
    return true;
}

QStringList VideoExporter::buildFfmpegArgs(const ExportConfig &config, const QString &tempWavPath) const
{
    QStringList args;

    if (config.isAudioOnly()) {
        args << "-y" << "-i" << tempWavPath;
        switch (config.container) {
        case ExportContainer::MP3:
            args << "-c:a" << "libmp3lame" << "-b:a" << QString("%1k").arg(config.audioBitrateKbps);
            break;
        case ExportContainer::WAV:
            args << "-c:a" << "pcm_s16le";
            break;
        case ExportContainer::AAC:
            args << "-c:a" << "aac" << "-b:a" << QString("%1k").arg(config.audioBitrateKbps);
            break;
        case ExportContainer::FLAC:
            args << "-c:a" << "flac";
            break;
        case ExportContainer::OGG:
            args << "-c:a" << "libopus" << "-b:a" << QString("%1k").arg(config.audioBitrateKbps);
            break;
        default:
            args << "-c:a" << "pcm_s16le";
            break;
        }
        args << config.outputPath;
        return args;
    }

    if (config.isGif()) {
        args << "-y"
             << "-f" << "rawvideo"
             << "-pixel_format" << "bgra"
             << "-video_size" << QString("%1x%2").arg(config.resolution.width()).arg(config.resolution.height())
             << "-framerate" << QString::number(config.fps)
             << "-i" << "-"
             << "-filter_complex" << "[0:v] split [a][b];[a] palettegen=stats_mode=full [p];[b][p] paletteuse=dither=bayer:bayer_scale=3"
             << config.outputPath;
        return args;
    }

    // Video containers: MP4, MOV, MKV, WebM, AVI
    args << "-y"
         << "-f" << "rawvideo"
         << "-pixel_format" << "bgra"
         << "-video_size" << QString("%1x%2").arg(config.resolution.width()).arg(config.resolution.height())
         << "-framerate" << QString::number(config.fps)
         << "-i" << "-";

    VideoCodec resolvedV = resolveVideoCodec(config.container, config.videoCodec);
    AudioCodec resolvedA = resolveAudioCodec(config.container, config.audioCodec);

    if (resolvedA != AudioCodec::None) {
        args << "-i" << tempWavPath;
    }

    int crf = config.crf;
    if (config.quality == QualityPreset::High) crf = 18;
    else if (config.quality == QualityPreset::Standard) crf = 23;
    else if (config.quality == QualityPreset::Low) crf = 28;

    // Estimate bitrate for hardware encoders
    int targetBitrateKbps = 8000;
    if (config.resolution.width() >= 3840) targetBitrateKbps = (crf <= 18) ? 35000 : ((crf <= 23) ? 25000 : 15000);
    else if (config.resolution.width() >= 1920) targetBitrateKbps = (crf <= 18) ? 14000 : ((crf <= 23) ? 8000 : 4500);
    else targetBitrateKbps = (crf <= 18) ? 6000 : ((crf <= 23) ? 4000 : 2500);

    // Video codec
    switch (resolvedV) {
    case VideoCodec::H264: {
        bool allowHw = config.useHardwareAcceleration && config.container != ExportContainer::WebM && config.container != ExportContainer::AVI;
        QString encoder = resolveH264Encoder(allowHw);
        if (encoder == "libx264") {
            args << "-c:v" << "libx264"
                 << "-pix_fmt" << "yuv420p"
                 << "-preset" << "fast"
                 << "-crf" << QString::number(crf);
        } else if (encoder.contains("nvenc")) {
            args << "-c:v" << encoder
                 << "-preset" << "p4"
                 << "-b:v" << QString("%1k").arg(targetBitrateKbps)
                 << "-pix_fmt" << "yuv420p";
        } else {
            args << "-c:v" << encoder
                 << "-b:v" << QString("%1k").arg(targetBitrateKbps)
                 << "-pix_fmt" << "yuv420p";
        }
        break;
    }
    case VideoCodec::H265_HEVC: {
        bool allowHw = config.useHardwareAcceleration && config.container != ExportContainer::WebM && config.container != ExportContainer::AVI;
        QString encoder = resolveHevcEncoder(allowHw);
        if (encoder == "libx265") {
            args << "-c:v" << "libx265"
                 << "-pix_fmt" << "yuv420p"
                 << "-preset" << "fast"
                 << "-crf" << QString::number(crf);
        } else if (encoder.contains("nvenc")) {
            args << "-c:v" << encoder
                 << "-preset" << "p4"
                 << "-b:v" << QString("%1k").arg(targetBitrateKbps * 3 / 4)
                 << "-pix_fmt" << "yuv420p";
        } else {
            args << "-c:v" << encoder
                 << "-b:v" << QString("%1k").arg(targetBitrateKbps * 3 / 4)
                 << "-pix_fmt" << "yuv420p";
        }
        break;
    }
    case VideoCodec::VP9:
        args << "-c:v" << "libvpx-vp9"
             << "-pix_fmt" << "yuv420p"
             << "-b:v" << "0"
             << "-crf" << QString::number(qBound(15, crf + 7, 45));
        break;
    case VideoCodec::VP8:
        args << "-c:v" << "libvpx"
             << "-pix_fmt" << "yuv420p"
             << "-b:v" << "2M"
             << "-crf" << QString::number(qBound(10, crf - 10, 42));
        break;
    case VideoCodec::ProRes: {
        QString encoder = resolveProResEncoder(config.useHardwareAcceleration);
        if (encoder == "prores_videotoolbox") {
            args << "-c:v" << "prores_videotoolbox";
        } else {
            args << "-c:v" << "prores_ks"
                 << "-profile:v" << "3"
                 << "-pix_fmt" << "yuv422p10le";
        }
        break;
    }
    case VideoCodec::MPEG4:
        args << "-c:v" << "mpeg4"
             << "-q:v" << "3";
        break;
    case VideoCodec::AV1:
        args << "-c:v" << "libsvtav1"
             << "-pix_fmt" << "yuv420p"
             << "-crf" << QString::number(crf);
        break;
    default:
        args << "-c:v" << "libx264"
             << "-pix_fmt" << "yuv420p"
             << "-preset" << "fast"
             << "-crf" << QString::number(crf);
        break;
    }

    // Audio codec
    if (resolvedA == AudioCodec::None) {
        args << "-an";
    } else {
        switch (resolvedA) {
        case AudioCodec::AAC:
            args << "-c:a" << "aac" << "-b:a" << QString("%1k").arg(config.audioBitrateKbps);
            break;
        case AudioCodec::MP3:
            args << "-c:a" << "libmp3lame" << "-b:a" << QString("%1k").arg(config.audioBitrateKbps);
            break;
        case AudioCodec::Opus:
            args << "-c:a" << "libopus" << "-b:a" << QString("%1k").arg(config.audioBitrateKbps);
            break;
        case AudioCodec::Vorbis:
            args << "-c:a" << "libvorbis" << "-b:a" << QString("%1k").arg(config.audioBitrateKbps);
            break;
        case AudioCodec::FLAC:
            args << "-c:a" << "flac";
            break;
        case AudioCodec::PCM_16:
            args << "-c:a" << "pcm_s16le";
            break;
        default:
            args << "-c:a" << "aac" << "-b:a" << QString("%1k").arg(config.audioBitrateKbps);
            break;
        }
        args << "-shortest";
    }

    args << config.outputPath;
    return args;
}

void VideoExporter::startExport(const QString &outputPath, const QSize &resolution, int fps, qint64 durationMs, qint64 startMs)
{
    ExportConfig config;
    config.outputPath = outputPath;
    config.resolution = resolution;
    config.fps = fps;
    config.durationMs = durationMs;
    config.startMs = startMs;
    config.container = containerFromExtension(QFileInfo(outputPath).suffix());
    config.useHardwareAcceleration = false; // Keep software default for pure programmatic API/tests

    startExport(config);
}

void VideoExporter::startExport(const ExportConfig &config)
{
    if (!m_model) {
        emit exportFinished(false, "", "No hay proyecto cargado.");
        return;
    }

    m_isExporting = true;
    m_cancelRequested = false;

    qint64 totalDurationMs = (config.durationMs > 0) ? config.durationMs : m_model->totalDurationMs();
    qint64 startOffsetMs = qMax<qint64>(0, config.startMs);
    if (totalDurationMs <= 0) {
        m_isExporting = false;
        emit exportFinished(false, "", "La duración del archivo a exportar es inválida.");
        return;
    }

    // Ensure output path has appropriate extension
    QString finalOutputPath = config.outputPath;
    QString requiredExt = "." + defaultExtension(config.container);
    if (!finalOutputPath.endsWith(requiredExt, Qt::CaseInsensitive)) {
        finalOutputPath += requiredExt;
    }

    ExportConfig effectiveConfig = config;
    effectiveConfig.outputPath = finalOutputPath;

    // Temporary WAV for audio processing
    QString tempWavPath = QDir::temp().filePath(QString("cpp_video_export_%1.wav").arg(QCoreApplication::applicationPid()));
    mixTimelineAudioToWav(m_model, startOffsetMs, totalDurationMs, tempWavPath);

    QString ffmpegPath = findFfmpegPath();
    QStringList args = buildFfmpegArgs(effectiveConfig, tempWavPath);

    // Audio-only fast path
    if (effectiveConfig.isAudioOnly()) {
        emit progressUpdated(20, "Codificando pista de audio...");
        QProcess audioProc;
        audioProc.start(ffmpegPath, args);
        if (!audioProc.waitForStarted(3000)) {
            QFile::remove(tempWavPath);
            m_isExporting = false;
            emit exportFinished(false, "", "No se pudo iniciar el proceso de codificación de audio.");
            return;
        }

        audioProc.waitForFinished(10000);
        QFile::remove(tempWavPath);
        m_isExporting = false;

        if (audioProc.exitStatus() == QProcess::NormalExit && audioProc.exitCode() == 0 && QFile::exists(finalOutputPath)) {
            emit progressUpdated(100, "¡Exportación de audio completada!");
            emit exportFinished(true, finalOutputPath, "");
        } else {
            QString err = QString::fromUtf8(audioProc.readAllStandardError());
            emit exportFinished(false, "", QString("Error al exportar audio:\n%1").arg(err.right(500)));
        }
        return;
    }

    // Video / GIF pipeline
    int totalFrames = qMax(1, static_cast<int>((totalDurationMs * effectiveConfig.fps) / 1000));

    QProcess ffmpegProc;
    ffmpegProc.start(ffmpegPath, args);
    if (!ffmpegProc.waitForStarted(3000)) {
        QFile::remove(tempWavPath);
        m_isExporting = false;
        emit exportFinished(false, "", "No se pudo iniciar el proceso de FFmpeg. Verifica que esté instalado.");
        return;
    }

    emit progressUpdated(0, "Renderizando fotogramas...");

    for (int frameIdx = 0; frameIdx < totalFrames; ++frameIdx) {
        if (m_cancelRequested) {
            ffmpegProc.kill();
            ffmpegProc.waitForFinished(1000);
            QFile::remove(tempWavPath);
            QFile::remove(finalOutputPath);
            m_isExporting = false;
            emit exportFinished(false, "", "Exportación cancelada por el usuario.");
            return;
        }

        qint64 tMs = startOffsetMs + (frameIdx * 1000LL) / effectiveConfig.fps;
        QImage frame = VideoCompositor::renderFrame(m_model, tMs, effectiveConfig.resolution);
        QImage bgra = frame.convertToFormat(QImage::Format_ARGB32_Premultiplied);

        ffmpegProc.write(reinterpret_cast<const char*>(bgra.constBits()), bgra.sizeInBytes());
        ffmpegProc.waitForBytesWritten(100);

        if (frameIdx % 5 == 0 || frameIdx == totalFrames - 1) {
            int percent = (frameIdx * 100) / totalFrames;
            emit progressUpdated(percent, QString("Renderizando fotograma %1 de %2 (%3%)...")
                                              .arg(frameIdx + 1).arg(totalFrames).arg(percent));
            QCoreApplication::processEvents();
        }
    }

    ffmpegProc.closeWriteChannel();
    ffmpegProc.waitForFinished(20000);

    QFile::remove(tempWavPath);
    m_isExporting = false;

    if (ffmpegProc.exitStatus() == QProcess::NormalExit && ffmpegProc.exitCode() == 0 && QFile::exists(finalOutputPath)) {
        emit progressUpdated(100, "¡Exportación completada con éxito!");
        emit exportFinished(true, finalOutputPath, "");
    } else {
        QString errOutput = QString::fromUtf8(ffmpegProc.readAllStandardError());
        emit exportFinished(false, "", QString("Error en FFmpeg al codificar el medio:\n%1").arg(errOutput.right(500)));
    }
}
