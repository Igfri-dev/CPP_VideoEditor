#pragma once

#include <QString>
#include <QStringList>
#include <QPixmap>
#include <QSize>
#include "../core/clip.h"

class MediaItem {
public:
    MediaItem();
    explicit MediaItem(const QString &filePath);

    QString id() const { return m_id; }
    QString filePath() const { return m_filePath; }
    QString fileName() const { return m_fileName; }
    ClipType type() const { return m_type; }
    qint64 durationMs() const { return m_durationMs; }
    QSize resolution() const { return m_resolution; }
    int sampleRate() const { return m_sampleRate; }
    int channels() const { return m_channels; }
    QPixmap thumbnail() const { return m_thumbnail; }
    bool exists() const { return m_exists; }
    qint64 fileSizeBytes() const { return m_fileSizeBytes; }

    QString videoCodec() const { return m_videoCodec; }
    QString audioCodec() const { return m_audioCodec; }
    QString containerFormat() const { return m_containerFormat; }
    double fps() const { return m_fps; }
    qint64 bitrate() const { return m_bitrate; }

    void setId(const QString &id) { m_id = id; }
    void setFilePath(const QString &path);
    void setType(ClipType type) { m_type = type; }
    void setDurationMs(qint64 durationMs) { m_durationMs = durationMs; }
    void setResolution(const QSize &res) { m_resolution = res; }
    void setSampleRate(int rate) { m_sampleRate = rate; }
    void setChannels(int ch) { m_channels = ch; }
    void setThumbnail(const QPixmap &pixmap) { m_thumbnail = pixmap; }
    void setVideoCodec(const QString &codec) { m_videoCodec = codec; }
    void setAudioCodec(const QString &codec) { m_audioCodec = codec; }
    void setContainerFormat(const QString &fmt) { m_containerFormat = fmt; }
    void setFps(double fps) { m_fps = fps; }
    void setBitrate(qint64 bitrate) { m_bitrate = bitrate; }

    QString formattedDuration() const;
    QString formattedFileSize() const;
    QString formattedFps() const;
    QString formattedBitrate() const;
    QString videoCodecDisplayName() const;
    QString audioCodecDisplayName() const;
    QString containerDisplayName() const;
    QString detailsString() const;
    QString technicalSummary() const;

    static ClipType detectType(const QString &filePath);
    static QStringList supportedVideoExtensions();
    static QStringList supportedAudioExtensions();
    static QStringList supportedImageExtensions();
    static QStringList allSupportedExtensions();

private:
    QString m_id;
    QString m_filePath;
    QString m_fileName;
    ClipType m_type = ClipType::Video;
    qint64 m_durationMs = 0;
    QSize m_resolution;
    int m_sampleRate = 44100;
    int m_channels = 2;
    QPixmap m_thumbnail;
    bool m_exists = false;
    qint64 m_fileSizeBytes = 0;

    QString m_videoCodec;
    QString m_audioCodec;
    QString m_containerFormat;
    double m_fps = 0.0;
    qint64 m_bitrate = 0;
};
