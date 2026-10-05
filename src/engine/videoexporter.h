#pragma once

#include <QObject>
#include <QSize>
#include <QString>
#include "../core/timelinemodel.h"

enum class ExportContainer {
    MP4,        // .mp4
    MOV,        // .mov
    MKV,        // .mkv
    WebM,       // .webm
    AVI,        // .avi
    GIF,        // .gif
    MP3,        // .mp3
    WAV,        // .wav
    AAC,        // .m4a / .aac
    FLAC,       // .flac
    OGG         // .ogg
};

enum class VideoCodec {
    Auto,           // Automatic based on container
    H264,           // libx264 or h264_videotoolbox
    H265_HEVC,      // libx265 or hevc_videotoolbox
    VP9,            // libvpx-vp9
    VP8,            // libvpx
    ProRes,         // prores / prores_ks
    MPEG4,          // mpeg4
    AV1,            // libsvtav1
    GIF             // Palette-optimized GIF
};

enum class AudioCodec {
    Auto,           // Automatic based on container
    AAC,            // aac
    MP3,            // libmp3lame
    Opus,           // libopus
    Vorbis,         // libvorbis
    FLAC,           // flac
    PCM_16,         // pcm_s16le
    None            // No audio / Mute
};

enum class QualityPreset {
    High,           // CRF 18 / Master
    Standard,       // CRF 23 / Balanced
    Low,            // CRF 28 / Compact
    Custom          // User-specified CRF
};

struct ExportConfig {
    QString outputPath;
    QSize resolution = QSize(1920, 1080);
    int fps = 30;
    qint64 durationMs = -1;
    qint64 startMs = 0;

    ExportContainer container = ExportContainer::MP4;
    VideoCodec videoCodec = VideoCodec::Auto;
    AudioCodec audioCodec = AudioCodec::Auto;
    QualityPreset quality = QualityPreset::Standard;
    int crf = 23;
    int audioBitrateKbps = 192;
    bool useHardwareAcceleration = true; // Platform-optimized hardware acceleration (Apple VideoToolbox / NVENC / QSV / VA-API)

    bool isAudioOnly() const {
        return container == ExportContainer::MP3 ||
               container == ExportContainer::WAV ||
               container == ExportContainer::AAC ||
               container == ExportContainer::FLAC ||
               container == ExportContainer::OGG;
    }

    bool isGif() const {
        return container == ExportContainer::GIF;
    }
};

class VideoExporter : public QObject {
    Q_OBJECT

public:
    explicit VideoExporter(TimelineModel *model, QObject *parent = nullptr);

    void startExport(const ExportConfig &config);
    // Overload for backward compatibility
    void startExport(const QString &outputPath, const QSize &resolution = QSize(1920, 1080), int fps = 30, qint64 durationMs = -1, qint64 startMs = 0);

    void cancelExport();
    bool isExporting() const { return m_isExporting; }

    static QString defaultExtension(ExportContainer container);
    static ExportContainer containerFromExtension(const QString &ext);
    static VideoCodec resolveVideoCodec(ExportContainer container, VideoCodec requested);
    static AudioCodec resolveAudioCodec(ExportContainer container, AudioCodec requested);

    static bool mixTimelineAudioToWav(TimelineModel *model, qint64 startMs, qint64 durationMs, const QString &outputWavPath);

    // Multiplatform hardware acceleration and executable discovery helpers
    static QString findFfmpegExecutable();
    static bool isHardwareAccelerationAvailable();
    static QString platformHardwareAccelerationName();
    static QString resolveH264Encoder(bool useHardware);
    static QString resolveHevcEncoder(bool useHardware);
    static QString resolveProResEncoder(bool useHardware);
    static bool isEncoderAvailable(const QString &encoderName);

signals:
    void progressUpdated(int percent, const QString &statusText);
    void exportFinished(bool success, const QString &outputPath, const QString &errorMessage);

private:
    TimelineModel *m_model = nullptr;
    bool m_isExporting = false;
    bool m_cancelRequested = false;

    QString findFfmpegPath() const;
    QStringList buildFfmpegArgs(const ExportConfig &config, const QString &tempWavPath) const;
};
