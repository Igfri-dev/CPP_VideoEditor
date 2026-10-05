#include "mediaitem.h"
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QUuid>
#include <cmath>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

QStringList MediaItem::supportedVideoExtensions()
{
    return {
        "mp4", "mov", "mkv", "webm", "avi", "wmv", "flv", "m4v",
        "ts", "mts", "m2ts", "3gp", "ogv", "vob", "mpg", "mpeg", "mxf", "asf", "f4v", "rmvb", "divx"
    };
}

QStringList MediaItem::supportedAudioExtensions()
{
    return {
        "mp3", "wav", "aac", "m4a", "flac", "ogg", "oga", "opus",
        "wma", "aiff", "aif", "ac3", "eac3", "mka", "amr", "alac", "ape", "mid", "midi"
    };
}

QStringList MediaItem::supportedImageExtensions()
{
    return {
        "png", "jpg", "jpeg", "bmp", "gif", "webp", "tiff", "tif",
        "ico", "svg", "ppm", "pgm", "pbm", "tga", "avif", "heic", "heif", "jfif", "hdr"
    };
}

QStringList MediaItem::allSupportedExtensions()
{
    static QStringList allExts;
    if (allExts.isEmpty()) {
        allExts.append(supportedVideoExtensions());
        allExts.append(supportedAudioExtensions());
        allExts.append(supportedImageExtensions());
    }
    return allExts;
}

ClipType MediaItem::detectType(const QString &filePath)
{
    const QString ext = QFileInfo(filePath).suffix().toLower();
    if (supportedAudioExtensions().contains(ext)) return ClipType::Audio;
    if (supportedImageExtensions().contains(ext)) return ClipType::Image;
    return ClipType::Video;
}

MediaItem::MediaItem()
    : m_id(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
}

MediaItem::MediaItem(const QString &filePath)
    : m_id(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
    setFilePath(filePath);
}

void MediaItem::setFilePath(const QString &path)
{
    m_filePath = path;
    QFileInfo fi(path);
    m_fileName = fi.fileName();
    m_exists = fi.exists();
    m_fileSizeBytes = fi.size();
    m_type = detectType(path);
    m_containerFormat = fi.suffix().toLower();

    if (!m_exists) {
        // Draw missing thumbnail
        QImage img(160, 90, QImage::Format_RGB32);
        img.fill(QColor("#2d1515"));
        QPainter p(&img);
        p.setPen(Qt::red);
        p.drawText(img.rect(), Qt::AlignCenter, "No encontrado");
        m_thumbnail = QPixmap::fromImage(img);
        return;
    }

    if (m_type == ClipType::Image) {
        m_durationMs = 5000;
        QImage img(m_filePath);
        if (!img.isNull()) {
            m_resolution = img.size();
            m_thumbnail = QPixmap::fromImage(img.scaled(160, 90, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
            m_containerFormat = fi.suffix().toUpper();
            return;
        }
        // If QImage failed to load (e.g. TGA or exotic format), continue to probe with FFmpeg below
    }

    // Use FFmpeg to probe duration, resolution, audio specs, codecs, and generate thumbnail
    AVFormatContext *fmtCtx = nullptr;
    if (avformat_open_input(&fmtCtx, m_filePath.toUtf8().constData(), nullptr, nullptr) == 0) {
        if (avformat_find_stream_info(fmtCtx, nullptr) >= 0) {
            if (fmtCtx->duration != AV_NOPTS_VALUE) {
                m_durationMs = (fmtCtx->duration * 1000) / AV_TIME_BASE;
            }
            if (fmtCtx->bit_rate > 0) {
                m_bitrate = fmtCtx->bit_rate;
            }
            if (fmtCtx->iformat && fmtCtx->iformat->name) {
                m_containerFormat = QString::fromUtf8(fmtCtx->iformat->name);
            }

            int videoStreamIdx = -1;
            int audioStreamIdx = -1;
            for (unsigned int i = 0; i < fmtCtx->nb_streams; ++i) {
                if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && videoStreamIdx < 0) {
                    videoStreamIdx = i;
                } else if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && audioStreamIdx < 0) {
                    audioStreamIdx = i;
                }
            }

            if (videoStreamIdx >= 0) {
                AVCodecParameters *vCodecPar = fmtCtx->streams[videoStreamIdx]->codecpar;
                m_resolution = QSize(vCodecPar->width, vCodecPar->height);

                const char *vName = avcodec_get_name(vCodecPar->codec_id);
                if (vName) {
                    m_videoCodec = QString::fromUtf8(vName);
                }

                AVRational fpsRat = fmtCtx->streams[videoStreamIdx]->r_frame_rate;
                if (fpsRat.den == 0 || fpsRat.num == 0) {
                    fpsRat = fmtCtx->streams[videoStreamIdx]->avg_frame_rate;
                }
                if (fpsRat.den > 0 && fpsRat.num > 0) {
                    m_fps = av_q2d(fpsRat);
                }

                if (m_bitrate <= 0 && vCodecPar->bit_rate > 0) {
                    m_bitrate = vCodecPar->bit_rate;
                }

                // Try to extract a preview frame for thumbnail
                const AVCodec *codec = avcodec_find_decoder(vCodecPar->codec_id);
                if (codec) {
                    AVCodecContext *codecCtx = avcodec_alloc_context3(codec);
                    if (codecCtx && avcodec_parameters_to_context(codecCtx, vCodecPar) >= 0 &&
                        avcodec_open2(codecCtx, codec, nullptr) == 0) {
                        
                        AVPacket *pkt = av_packet_alloc();
                        AVFrame *frame = av_frame_alloc();
                        bool gotFrame = false;

                        while (av_read_frame(fmtCtx, pkt) >= 0 && !gotFrame) {
                            if (pkt->stream_index == videoStreamIdx) {
                                if (avcodec_send_packet(codecCtx, pkt) == 0) {
                                    if (avcodec_receive_frame(codecCtx, frame) == 0) {
                                        // Convert frame to QImage
                                        QImage frameImg(160, 90, QImage::Format_RGB32);
                                        SwsContext *sws = sws_getContext(
                                            frame->width, frame->height, (AVPixelFormat)frame->format,
                                            160, 90, AV_PIX_FMT_BGRA,
                                            SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
                                        if (sws) {
                                            uint8_t *destData[4] = { frameImg.bits(), nullptr, nullptr, nullptr };
                                            int destLinesize[4] = { (int)frameImg.bytesPerLine(), 0, 0, 0 };
                                            sws_scale(sws, frame->data, frame->linesize, 0, frame->height, destData, destLinesize);
                                            sws_freeContext(sws);
                                            m_thumbnail = QPixmap::fromImage(frameImg);
                                            gotFrame = true;
                                        }
                                    }
                                }
                            }
                            av_packet_unref(pkt);
                        }
                        av_packet_free(&pkt);
                        av_frame_free(&frame);
                    }
                    if (codecCtx) avcodec_free_context(&codecCtx);
                }
            }

            if (audioStreamIdx >= 0) {
                AVCodecParameters *aCodecPar = fmtCtx->streams[audioStreamIdx]->codecpar;
                m_sampleRate = aCodecPar->sample_rate;
                m_channels = aCodecPar->ch_layout.nb_channels > 0 ? aCodecPar->ch_layout.nb_channels : 2;

                const char *aName = avcodec_get_name(aCodecPar->codec_id);
                if (aName) {
                    m_audioCodec = QString::fromUtf8(aName);
                }

                if (m_bitrate <= 0 && aCodecPar->bit_rate > 0) {
                    m_bitrate = aCodecPar->bit_rate;
                }
            }
        }
        avformat_close_input(&fmtCtx);
    }

    // Default duration fallback if stream had 0
    if (m_durationMs <= 0) {
        m_durationMs = (m_type == ClipType::Image) ? 5000 : 10000;
    }

    // Fallback thumbnail if frame couldn't be extracted
    if (m_thumbnail.isNull()) {
        QImage placeholder(160, 90, QImage::Format_RGB32);
        placeholder.fill(m_type == ClipType::Audio ? QColor("#33240e") : QColor("#0d1e38"));
        QPainter p(&placeholder);
        p.setPen(Qt::white);
        p.drawText(placeholder.rect(), Qt::AlignCenter, m_type == ClipType::Audio ? "AUDIO" : (m_type == ClipType::Image ? "IMAGEN" : "VIDEO"));
        m_thumbnail = QPixmap::fromImage(placeholder);
    }
}

QString MediaItem::formattedDuration() const
{
    qint64 totalSecs = m_durationMs / 1000;
    qint64 mins = totalSecs / 60;
    qint64 secs = totalSecs % 60;
    qint64 msRemainder = (m_durationMs % 1000) / 100;
    if (mins >= 60) {
        qint64 hours = mins / 60;
        mins = mins % 60;
        return QString("%1:%2:%3")
            .arg(hours, 2, 10, QChar('0'))
            .arg(mins, 2, 10, QChar('0'))
            .arg(secs, 2, 10, QChar('0'));
    }
    return QString("%1:%2.%3")
        .arg(mins, 2, 10, QChar('0'))
        .arg(secs, 2, 10, QChar('0'))
        .arg(msRemainder);
}

QString MediaItem::formattedFileSize() const
{
    double bytes = static_cast<double>(m_fileSizeBytes);
    if (bytes >= 1024 * 1024 * 1024) {
        return QString("%1 GB").arg(bytes / (1024 * 1024 * 1024), 0, 'f', 1);
    }
    if (bytes >= 1024 * 1024) {
        return QString("%1 MB").arg(bytes / (1024 * 1024), 0, 'f', 1);
    }
    if (bytes >= 1024) {
        return QString("%1 KB").arg(bytes / 1024, 0, 'f', 0);
    }
    return QString("%1 B").arg(m_fileSizeBytes);
}

QString MediaItem::formattedFps() const
{
    if (m_fps <= 0.0) return "";
    if (std::abs(m_fps - std::round(m_fps)) < 0.05) {
        return QString("%1 fps").arg(static_cast<int>(std::round(m_fps)));
    }
    return QString("%1 fps").arg(m_fps, 0, 'f', 2);
}

QString MediaItem::formattedBitrate() const
{
    if (m_bitrate <= 0) return "";
    if (m_bitrate >= 1000000) {
        return QString("%1 Mbps").arg(m_bitrate / 1000000.0, 0, 'f', 1);
    }
    return QString("%1 kbps").arg(m_bitrate / 1000.0, 0, 'f', 0);
}

QString MediaItem::videoCodecDisplayName() const
{
    if (m_videoCodec.isEmpty()) return "N/A";
    QString c = m_videoCodec.toLower();
    if (c == "h264") return "H.264 / AVC";
    if (c == "hevc" || c == "h265") return "H.265 / HEVC";
    if (c == "vp9") return "Google VP9";
    if (c == "vp8") return "Google VP8";
    if (c == "prores") return "Apple ProRes";
    if (c == "av1") return "AOMedia AV1";
    if (c == "mpeg4") return "MPEG-4 Part 2";
    if (c == "mpeg2video") return "MPEG-2 Video";
    if (c == "mpeg1video") return "MPEG-1 Video";
    if (c == "mjpeg") return "Motion JPEG";
    if (c == "theora") return "Theora";
    if (c == "dnxhd") return "Avid DNxHD";
    if (c == "wmv3" || c == "vc1") return "Windows Media Video";
    if (c == "flv1") return "Flash Video";
    return m_videoCodec.toUpper();
}

QString MediaItem::audioCodecDisplayName() const
{
    if (m_audioCodec.isEmpty()) return "N/A";
    QString c = m_audioCodec.toLower();
    if (c == "aac") return "AAC";
    if (c == "mp3") return "MP3";
    if (c == "opus") return "Opus";
    if (c == "flac") return "FLAC";
    if (c == "vorbis") return "Vorbis";
    if (c.startsWith("pcm")) return "PCM";
    if (c == "ac3") return "Dolby AC-3";
    if (c == "eac3") return "Dolby Digital Plus (E-AC-3)";
    if (c == "alac") return "Apple Lossless (ALAC)";
    if (c == "wmav2" || c == "wmapro") return "WMA";
    return m_audioCodec.toUpper();
}

QString MediaItem::containerDisplayName() const
{
    if (m_containerFormat.isEmpty()) {
        return QFileInfo(m_filePath).suffix().toUpper();
    }
    QString c = m_containerFormat.toLower();
    if (c.contains("mp4") || c.contains("mov") || c.contains("m4a")) {
        QString ext = QFileInfo(m_filePath).suffix().toLower();
        if (ext == "mov") return "QuickTime (MOV)";
        if (ext == "m4a") return "MPEG-4 Audio (M4A)";
        return "MPEG-4 (MP4)";
    }
    if (c.contains("matroska") || c.contains("webm")) {
        QString ext = QFileInfo(m_filePath).suffix().toLower();
        if (ext == "webm") return "WebM";
        return "Matroska (MKV)";
    }
    if (c.contains("avi")) return "AVI";
    if (c.contains("ogg")) return "Ogg";
    if (c.contains("wav")) return "WAV";
    if (c.contains("flac")) return "FLAC";
    if (c.contains("mp3")) return "MP3";
    if (c.contains("image2") || c.contains("png") || c.contains("jpeg") || c.contains("bmp")) {
        return QFileInfo(m_filePath).suffix().toUpper();
    }
    return m_containerFormat;
}

QString MediaItem::detailsString() const
{
    if (m_type == ClipType::Video) {
        QStringList parts;
        parts << QString("%1x%2").arg(m_resolution.width()).arg(m_resolution.height());
        if (!formattedFps().isEmpty()) parts << formattedFps();
        if (!m_videoCodec.isEmpty()) parts << videoCodecDisplayName();
        parts << formattedDuration();
        return parts.join(" • ");
    } else if (m_type == ClipType::Audio) {
        QStringList parts;
        if (!m_audioCodec.isEmpty()) parts << audioCodecDisplayName();
        parts << QString("%1 kHz").arg(m_sampleRate / 1000.0, 0, 'f', 1);
        parts << QString("%1 ch").arg(m_channels);
        parts << formattedDuration();
        return parts.join(" • ");
    } else {
        return QString("%1x%2 • %3 • Imagen").arg(m_resolution.width()).arg(m_resolution.height()).arg(containerDisplayName());
    }
}

QString MediaItem::technicalSummary() const
{
    QStringList lines;
    lines << QString("📁 Archivo: %1 (%2)").arg(m_fileName).arg(formattedFileSize());
    lines << QString("📦 Formato/Contenedor: %1").arg(containerDisplayName());
    if (m_type == ClipType::Video) {
        lines << QString("🎬 Video: %1x%2 @ %3 (%4)")
                     .arg(m_resolution.width()).arg(m_resolution.height())
                     .arg(!formattedFps().isEmpty() ? formattedFps() : "30 fps")
                     .arg(videoCodecDisplayName());
        if (!m_audioCodec.isEmpty()) {
            lines << QString("🔊 Audio: %1 (%2 kHz, %3 ch)")
                         .arg(audioCodecDisplayName())
                         .arg(m_sampleRate / 1000.0, 0, 'f', 1)
                         .arg(m_channels);
        }
    } else if (m_type == ClipType::Audio) {
        lines << QString("🔊 Audio: %1 (%2 kHz, %3 ch)")
                     .arg(audioCodecDisplayName())
                     .arg(m_sampleRate / 1000.0, 0, 'f', 1)
                     .arg(m_channels);
    } else {
        lines << QString("🖼️ Imagen: %1x%2").arg(m_resolution.width()).arg(m_resolution.height());
    }
    if (!formattedBitrate().isEmpty()) {
        lines << QString("⚡ Tasa de bits: %1").arg(formattedBitrate());
    }
    lines << QString("⏱️ Duración: %1").arg(formattedDuration());
    return lines.join("\n");
}
