#include "videoframedecoder.h"
#include "../medialibrary/mediaitem.h"
#include <QFileInfo>
#include <QImageReader>
#include <QMutexLocker>
#include <QPainter>

VideoFrameDecoder& VideoFrameDecoder::instance()
{
    static VideoFrameDecoder s_instance;
    return s_instance;
}

VideoFrameDecoder::VideoFrameDecoder()
{
    m_frameCache.setMaxCost(300); // Store up to ~300 frames in memory for smooth scrubbing & playback
}

VideoFrameDecoder::~VideoFrameDecoder()
{
    clearCache();
}

void VideoFrameDecoder::clearCache()
{
    QMutexLocker locker(&m_mutex);
    m_frameCache.clear();
    m_imageCache.clear();
    m_dimensionsCache.clear();
    for (auto it = m_contexts.begin(); it != m_contexts.end(); ++it) {
        closeContext(it.value());
    }
    m_contexts.clear();
}

void VideoFrameDecoder::closeContext(DecoderContext *ctx)
{
    if (!ctx) return;
    if (ctx->codecCtx) {
        avcodec_free_context(&ctx->codecCtx);
    }
    if (ctx->fmtCtx) {
        avformat_close_input(&ctx->fmtCtx);
    }
    delete ctx;
}

VideoFrameDecoder::DecoderContext* VideoFrameDecoder::getOrCreateContext(const QString &filePath)
{
    if (m_contexts.contains(filePath)) {
        return m_contexts.value(filePath);
    }

    AVFormatContext *fmtCtx = nullptr;
    if (avformat_open_input(&fmtCtx, filePath.toUtf8().constData(), nullptr, nullptr) != 0) {
        return nullptr;
    }

    if (avformat_find_stream_info(fmtCtx, nullptr) < 0) {
        avformat_close_input(&fmtCtx);
        return nullptr;
    }

    int videoStreamIdx = -1;
    for (unsigned int i = 0; i < fmtCtx->nb_streams; ++i) {
        if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIdx = i;
            break;
        }
    }

    if (videoStreamIdx < 0) {
        avformat_close_input(&fmtCtx);
        return nullptr;
    }

    AVCodecParameters *codecPar = fmtCtx->streams[videoStreamIdx]->codecpar;
    const AVCodec *codec = avcodec_find_decoder(codecPar->codec_id);
    if (!codec) {
        avformat_close_input(&fmtCtx);
        return nullptr;
    }

    AVCodecContext *codecCtx = avcodec_alloc_context3(codec);
    if (!codecCtx || avcodec_parameters_to_context(codecCtx, codecPar) < 0 ||
        avcodec_open2(codecCtx, codec, nullptr) != 0) {
        if (codecCtx) avcodec_free_context(&codecCtx);
        avformat_close_input(&fmtCtx);
        return nullptr;
    }

    DecoderContext *ctx = new DecoderContext();
    ctx->fmtCtx = fmtCtx;
    ctx->codecCtx = codecCtx;
    ctx->videoStreamIdx = videoStreamIdx;
    ctx->timeBase = fmtCtx->streams[videoStreamIdx]->time_base;
    ctx->width = codecPar->width;
    ctx->height = codecPar->height;
    if (fmtCtx->duration != AV_NOPTS_VALUE) {
        ctx->durationMs = (fmtCtx->duration * 1000) / AV_TIME_BASE;
    }

    m_contexts.insert(filePath, ctx);
    return ctx;
}

QSize VideoFrameDecoder::getVideoDimensions(const QString &filePath)
{
    QMutexLocker locker(&m_mutex);
    if (m_dimensionsCache.contains(filePath)) {
        return m_dimensionsCache.value(filePath);
    }

    if (MediaItem::detectType(filePath) == ClipType::Image) {
        QImageReader reader(filePath);
        QSize s = reader.size();
        if (s.isValid() && s.width() > 0) {
            m_dimensionsCache.insert(filePath, s);
            return s;
        }
        QImage img(filePath);
        if (!img.isNull()) {
            m_dimensionsCache.insert(filePath, img.size());
            return img.size();
        }
    }

    DecoderContext *ctx = getOrCreateContext(filePath);
    if (ctx && ctx->width > 0 && ctx->height > 0) {
        QSize s(ctx->width, ctx->height);
        m_dimensionsCache.insert(filePath, s);
        return s;
    }

    return QSize(1920, 1080);
}

QImage VideoFrameDecoder::getFrame(const QString &filePath, qint64 timestampMs, const QSize &targetSize)
{
    QMutexLocker locker(&m_mutex);

    // 1. If image file, return cached static image
    if (MediaItem::detectType(filePath) == ClipType::Image) {
        if (!m_imageCache.contains(filePath)) {
            QImage img(filePath);
            if (img.isNull()) {
                DecoderContext *ctx = getOrCreateContext(filePath);
                if (ctx) {
                    AVPacket *pkt = av_packet_alloc();
                    AVFrame *frame = av_frame_alloc();
                    while (av_read_frame(ctx->fmtCtx, pkt) >= 0) {
                        if (pkt->stream_index == ctx->videoStreamIdx) {
                            if (avcodec_send_packet(ctx->codecCtx, pkt) == 0) {
                                if (avcodec_receive_frame(ctx->codecCtx, frame) == 0) {
                                    int outW = frame->width;
                                    int outH = frame->height;
                                    if (targetSize.isValid() && targetSize.width() > 0 && targetSize.height() > 0) {
                                        if (targetSize.width() <= 320 && targetSize.height() <= 320) {
                                            QSize s = QSize(frame->width, frame->height).scaled(targetSize, Qt::KeepAspectRatio);
                                            outW = s.width();
                                            outH = s.height();
                                        }
                                    }
                                    img = QImage(outW, outH, QImage::Format_RGB32);
                                    SwsContext *sws = sws_getContext(
                                        frame->width, frame->height, (AVPixelFormat)frame->format,
                                        outW, outH, AV_PIX_FMT_BGRA,
                                        SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
                                    if (sws) {
                                        uint8_t *destData[4] = { img.bits(), nullptr, nullptr, nullptr };
                                        int destLinesize[4] = { (int)img.bytesPerLine(), 0, 0, 0 };
                                        sws_scale(sws, frame->data, frame->linesize, 0, frame->height, destData, destLinesize);
                                        sws_freeContext(sws);
                                    }
                                    av_packet_unref(pkt);
                                    break;
                                }
                            }
                        }
                        av_packet_unref(pkt);
                    }
                    av_packet_free(&pkt);
                    av_frame_free(&frame);
                }
            }
            m_imageCache.insert(filePath, img);
        }
        return m_imageCache.value(filePath);
    }

    // 2. Quantize timestamp to ~30fps frame interval (33ms) for caching
    qint64 quantMs = (timestampMs / 33) * 33;
    bool isThumb = (targetSize.isValid() && targetSize.width() > 0 && targetSize.width() <= 320 && targetSize.height() <= 320);
    int tw = isThumb ? targetSize.width() : 0;
    int th = isThumb ? targetSize.height() : 0;
    QString cacheKey = QString("%1_%2_%3x%4").arg(filePath).arg(quantMs).arg(tw).arg(th);
    if (QImage *cached = m_frameCache.object(cacheKey)) {
        return *cached;
    }

    DecoderContext *ctx = getOrCreateContext(filePath);
    if (!ctx) {
        return QImage();
    }

    // Calculate target PTS
    double tb = av_q2d(ctx->timeBase);
    int64_t targetPts = (static_cast<double>(timestampMs) / 1000.0) / tb;
    int64_t oneSecPts = static_cast<int64_t>(1.0 / tb);
    int64_t backwardThresholdPts = static_cast<int64_t>(0.35 / tb); // 350ms backward threshold

    // Only seek if we have no last PTS, or if target is more than 350ms in the past,
    // or more than 1.5 seconds in the future
    bool needSeek = (ctx->lastDecodedPts < 0 ||
                     (ctx->lastDecodedPts - targetPts) > backwardThresholdPts ||
                     (targetPts - ctx->lastDecodedPts) > static_cast<int64_t>(1.5 * oneSecPts));

    if (needSeek) {
        avcodec_flush_buffers(ctx->codecCtx);
        av_seek_frame(ctx->fmtCtx, ctx->videoStreamIdx, targetPts, AVSEEK_FLAG_BACKWARD);
        ctx->lastDecodedPts = -1;
    }

    AVPacket *pkt = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    QImage result;
    bool found = false;

    auto convertFrame = [&](AVFrame *f) -> QImage {
        int outW = f->width;
        int outH = f->height;
        if (targetSize.isValid() && targetSize.width() > 0 && targetSize.height() > 0) {
            if (targetSize.width() <= 320 && targetSize.height() <= 320) {
                // Downscale for thumbnails preserving aspect ratio strictly
                QSize thumbSize = QSize(f->width, f->height).scaled(targetSize, Qt::KeepAspectRatio);
                outW = thumbSize.width();
                outH = thumbSize.height();
            }
        }
        QImage img(outW, outH, QImage::Format_RGB32);
        SwsContext *sws = sws_getContext(
            f->width, f->height, (AVPixelFormat)f->format,
            outW, outH, AV_PIX_FMT_BGRA,
            SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
        if (sws) {
            uint8_t *destData[4] = { img.bits(), nullptr, nullptr, nullptr };
            int destLinesize[4] = { (int)img.bytesPerLine(), 0, 0, 0 };
            sws_scale(sws, f->data, f->linesize, 0, f->height, destData, destLinesize);
            sws_freeContext(sws);
        }
        return img;
    };

    // First: drain any pending frames from the decoder buffer
    while (!needSeek && avcodec_receive_frame(ctx->codecCtx, frame) == 0) {
        int64_t pts = frame->pts != AV_NOPTS_VALUE ? frame->pts : frame->pkt_dts;
        ctx->lastDecodedPts = pts;
        qint64 frameMs = static_cast<qint64>(pts * tb * 1000.0);
        qint64 fQuant = (frameMs / 33) * 33;
        QString fKey = QString("%1_%2_%3x%4").arg(filePath).arg(fQuant).arg(tw).arg(th);

        if (pts >= targetPts - 2) {
            result = convertFrame(frame);
            ctx->lastGoodFrame = result;
            m_frameCache.insert(fKey, new QImage(result), 1);
            found = true;
            break;
        } else {
            QImage fImg = convertFrame(frame);
            ctx->lastGoodFrame = fImg;
            m_frameCache.insert(fKey, new QImage(fImg), 1);
        }
    }

    // Second: read packets from container until target frame is reached
    while (!found && av_read_frame(ctx->fmtCtx, pkt) >= 0) {
        if (pkt->stream_index == ctx->videoStreamIdx) {
            int ret = avcodec_send_packet(ctx->codecCtx, pkt);
            if (ret == 0) {
                while (avcodec_receive_frame(ctx->codecCtx, frame) == 0) {
                    int64_t pts = frame->pts != AV_NOPTS_VALUE ? frame->pts : frame->pkt_dts;
                    ctx->lastDecodedPts = pts;
                    qint64 frameMs = static_cast<qint64>(pts * tb * 1000.0);
                    qint64 fQuant = (frameMs / 33) * 33;
                    QString fKey = QString("%1_%2_%3x%4").arg(filePath).arg(fQuant).arg(tw).arg(th);

                    if (!needSeek || pts >= targetPts - 2) {
                        result = convertFrame(frame);
                        ctx->lastGoodFrame = result;
                        m_frameCache.insert(fKey, new QImage(result), 1);
                        found = true;
                        break;
                    } else {
                        QImage fImg = convertFrame(frame);
                        ctx->lastGoodFrame = fImg;
                        m_frameCache.insert(fKey, new QImage(fImg), 1);
                    }
                }
            }
        }
        av_packet_unref(pkt);
        if (found) break;
    }

    av_packet_free(&pkt);
    av_frame_free(&frame);

    // If decoding didn't find a new frame (e.g. at end of stream or read error),
    // fallback to lastGoodFrame to completely prevent black frame flickering!
    if (result.isNull() && !ctx->lastGoodFrame.isNull()) {
        result = ctx->lastGoodFrame;
        m_frameCache.insert(cacheKey, new QImage(result), 1);
    }

    return result;
}
