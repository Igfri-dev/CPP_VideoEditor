#include "videoframedecoder.h"
#include "gpuaccelerationmanager.h"
#include "../medialibrary/mediaitem.h"
#include <QFileInfo>
#include <QImageReader>
#include <QMutexLocker>
#include <QPainter>
#include <QThread>

VideoFrameDecoder& VideoFrameDecoder::instance()
{
    static VideoFrameDecoder s_instance;
    return s_instance;
}

VideoFrameDecoder::VideoFrameDecoder()
{
    m_maxCacheMb = 1024; // Default to 1024 MB configurable RAM limit
    m_frameCache.setMaxCost(m_maxCacheMb);
}

VideoFrameDecoder::~VideoFrameDecoder()
{
    clearCache();
}

void VideoFrameDecoder::setMaxCacheSizeMb(int mb)
{
    QMutexLocker locker(&m_mutex);
    m_maxCacheMb = qMax(64, mb);
    m_frameCache.setMaxCost(m_maxCacheMb);
}

int VideoFrameDecoder::maxCacheSizeMb() const
{
    QMutexLocker locker(&m_mutex);
    return m_maxCacheMb;
}

int VideoFrameDecoder::currentCacheSizeMb() const
{
    QMutexLocker locker(&m_mutex);
    return m_frameCache.totalCost();
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
    for (auto it = m_thumbContexts.begin(); it != m_thumbContexts.end(); ++it) {
        closeContext(it.value());
    }
    m_thumbContexts.clear();
}

void VideoFrameDecoder::closeContext(DecoderContext *ctx)
{
    if (!ctx) return;
    if (ctx->swsCtx) {
        sws_freeContext(ctx->swsCtx);
        ctx->swsCtx = nullptr;
    }
    if (ctx->hwDeviceCtx) {
        av_buffer_unref(&ctx->hwDeviceCtx);
        ctx->hwDeviceCtx = nullptr;
    }
    if (ctx->codecCtx) {
        avcodec_free_context(&ctx->codecCtx);
    }
    if (ctx->fmtCtx) {
        avformat_close_input(&ctx->fmtCtx);
    }
    delete ctx;
}

VideoFrameDecoder::DecoderContext* VideoFrameDecoder::getOrCreateContext(const QString &filePath, const QSize &targetSize)
{
    bool isThumbnail = (targetSize.isValid() && targetSize.width() > 0 && targetSize.width() <= 320 && targetSize.height() <= 320);
    auto &map = isThumbnail ? m_thumbContexts : m_contexts;
    if (map.contains(filePath)) {
        return map.value(filePath);
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
    if (!codecCtx || avcodec_parameters_to_context(codecCtx, codecPar) < 0) {
        if (codecCtx) avcodec_free_context(&codecCtx);
        avformat_close_input(&fmtCtx);
        return nullptr;
    }

    // Enable multithreaded slice & frame decoding for high performance
    codecCtx->thread_count = qBound(1, QThread::idealThreadCount(), 8);
    codecCtx->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;

    DecoderContext *ctx = new DecoderContext();
    ctx->fmtCtx = fmtCtx;
    ctx->codecCtx = codecCtx;
    ctx->videoStreamIdx = videoStreamIdx;
    ctx->timeBase = fmtCtx->streams[videoStreamIdx]->time_base;
    ctx->width = codecPar->width;
    ctx->height = codecPar->height;

    // Hardware acceleration initialization (NVIDIA CUDA, Apple VideoToolbox, Intel QSV, AMD D3D11VA)
    AVHWDeviceType hwType = GpuAccelerationManager::instance().activeFfmpegHwType();
    if (hwType != AV_HWDEVICE_TYPE_NONE && GpuAccelerationManager::instance().isHwDecodingEnabled()) {
        for (int i = 0;; i++) {
            const AVCodecHWConfig *config = avcodec_get_hw_config(codec, i);
            if (!config) break;
            if ((config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) &&
                config->device_type == hwType) {
                ctx->hwPixelFormat = config->pix_fmt;
                break;
            }
        }

        if (ctx->hwPixelFormat != AV_PIX_FMT_NONE) {
            AVBufferRef *hwCtx = nullptr;
            if (av_hwdevice_ctx_create(&hwCtx, hwType, nullptr, nullptr, 0) == 0) {
                ctx->hwDeviceCtx = hwCtx;
                codecCtx->hw_device_ctx = av_buffer_ref(hwCtx);
                codecCtx->opaque = ctx;
                codecCtx->get_format = [](AVCodecContext *c, const enum AVPixelFormat *pix_fmts) -> enum AVPixelFormat {
                    DecoderContext *d = static_cast<DecoderContext*>(c->opaque);
                    if (d && d->hwPixelFormat != AV_PIX_FMT_NONE) {
                        for (const enum AVPixelFormat *p = pix_fmts; *p != AV_PIX_FMT_NONE; ++p) {
                            if (*p == d->hwPixelFormat) {
                                return *p;
                            }
                        }
                    }
                    return pix_fmts[0];
                };
            }
        }
    }

    if (avcodec_open2(codecCtx, codec, nullptr) != 0) {
        closeContext(ctx);
        return nullptr;
    }

    if (fmtCtx->duration != AV_NOPTS_VALUE) {
        ctx->durationMs = (fmtCtx->duration * 1000) / AV_TIME_BASE;
    }

    AVStream *st = fmtCtx->streams[videoStreamIdx];
    double fps = 30.0;
    if (st->avg_frame_rate.num > 0 && st->avg_frame_rate.den > 0) {
        fps = av_q2d(st->avg_frame_rate);
    } else if (st->r_frame_rate.num > 0 && st->r_frame_rate.den > 0) {
        fps = av_q2d(st->r_frame_rate);
    }
    if (fps <= 0.0 || fps > 240.0 || std::isnan(fps)) {
        fps = 30.0;
    }
    ctx->fps = fps;

    double tb = av_q2d(ctx->timeBase);
    if (tb > 0.0) {
        ctx->oneFramePts = qMax<int64_t>(1, static_cast<int64_t>((1.0 / fps) / tb + 0.5));
    } else {
        ctx->oneFramePts = 1;
    }

    map.insert(filePath, ctx);
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

    bool isScaled = targetSize.isValid() && targetSize.width() > 0 && targetSize.height() > 0;

    // 1. If image file, return cached static image (scaled if targetSize specified)
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
        QImage cachedImg = m_imageCache.value(filePath);
        if (isScaled && !cachedImg.isNull() && (targetSize.width() < cachedImg.width() || targetSize.height() < cachedImg.height())) {
            return cachedImg.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        return cachedImg;
    }

    DecoderContext *ctx = getOrCreateContext(filePath, targetSize);
    if (!ctx) {
        return QImage();
    }

    // Determine target dimensions for scaled decoding
    int tw = 0;
    int th = 0;
    if (isScaled && ctx->width > 0 && ctx->height > 0) {
        if (targetSize.width() < ctx->width || targetSize.height() < ctx->height) {
            QSize s = QSize(ctx->width, ctx->height).scaled(targetSize, Qt::KeepAspectRatio);
            tw = qMax(2, (s.width() / 2) * 2);
            th = qMax(2, (s.height() / 2) * 2);
        }
    }

    // 2. Deterministic frame index based on stream FPS
    double tb = av_q2d(ctx->timeBase);
    if (tb <= 0.0) tb = 0.001;
    int64_t targetFrameIdx = qRound64((static_cast<double>(timestampMs) * ctx->fps) / 1000.0);
    if (targetFrameIdx < 0) targetFrameIdx = 0;

    QString cacheKey = QString("%1_f%2_%3x%4").arg(filePath).arg(targetFrameIdx).arg(tw).arg(th);
    if (QImage *cached = m_frameCache.object(cacheKey)) {
        return *cached;
    }

    int64_t targetPts = static_cast<int64_t>((static_cast<double>(timestampMs) / 1000.0) / tb + 0.5);
    int64_t halfFramePts = qMax<int64_t>(1, static_cast<int64_t>((0.5 / ctx->fps) / tb + 0.5));
    int64_t seekMarginPts = qMax<int64_t>(ctx->oneFramePts * 4, static_cast<int64_t>(0.100 / tb));
    int64_t maxForwardSkipPts = static_cast<int64_t>(1.5 / tb);

    // If target is slightly behind last decoded frame (within seek margin), return lastGoodFrame
    // to prevent costly backward seeking and keyframe re-decode on 60 FPS forward jitter
    if (ctx->lastDecodedPts >= 0 && targetPts < ctx->lastDecodedPts && (ctx->lastDecodedPts - targetPts) <= seekMarginPts) {
        if (!ctx->lastGoodFrame.isNull()) {
            return ctx->lastGoodFrame;
        }
    }

    // Precise seeking decision
    bool needSeek = (ctx->lastDecodedPts < 0 ||
                     targetPts < (ctx->lastDecodedPts - seekMarginPts) ||
                     (targetPts - ctx->lastDecodedPts) > maxForwardSkipPts);

    if (needSeek) {
        avcodec_flush_buffers(ctx->codecCtx);
        if (av_seek_frame(ctx->fmtCtx, ctx->videoStreamIdx, targetPts, AVSEEK_FLAG_BACKWARD) < 0) {
            av_seek_frame(ctx->fmtCtx, ctx->videoStreamIdx, 0, AVSEEK_FLAG_BACKWARD);
        }
        ctx->lastDecodedPts = -1;
    }

    AVPacket *pkt = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    QImage result;
    bool found = false;

    auto convertFrame = [&](AVFrame *f) -> QImage {
        int outW = f->width;
        int outH = f->height;
        if (isScaled) {
            if (targetSize.width() < f->width || targetSize.height() < f->height) {
                QSize s = QSize(f->width, f->height).scaled(targetSize, Qt::KeepAspectRatio);
                outW = qMax(2, (s.width() / 2) * 2);
                outH = qMax(2, (s.height() / 2) * 2);
            }
        }
        if (outW <= 0) outW = 2;
        if (outH <= 0) outH = 2;

        QImage img(outW, outH, QImage::Format_RGB32);
        ctx->swsCtx = sws_getCachedContext(
            ctx->swsCtx,
            f->width, f->height, (AVPixelFormat)f->format,
            outW, outH, AV_PIX_FMT_BGRA,
            SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
        if (ctx->swsCtx) {
            uint8_t *destData[4] = { img.bits(), nullptr, nullptr, nullptr };
            int destLinesize[4] = { (int)img.bytesPerLine(), 0, 0, 0 };
            sws_scale(ctx->swsCtx, f->data, f->linesize, 0, f->height, destData, destLinesize);
        }
        return img;
    };

    int loopCount = 0;
    const int maxLoops = 300; // Guard against corrupt streams

    while (!found && loopCount++ < maxLoops) {
        int ret = avcodec_receive_frame(ctx->codecCtx, frame);
        if (ret == 0) {
            AVFrame *actualFrame = frame;
            AVFrame *swFrame = nullptr;

            // If decoded into hardware surface (VideoToolbox / CUDA / D3D11VA / QSV), transfer to system memory
            if (ctx->hwPixelFormat != AV_PIX_FMT_NONE && frame->format == ctx->hwPixelFormat) {
                swFrame = av_frame_alloc();
                if (av_hwframe_transfer_data(swFrame, frame, 0) == 0) {
                    swFrame->pts = frame->pts;
                    swFrame->pkt_dts = frame->pkt_dts;
                    actualFrame = swFrame;
                } else {
                    av_frame_free(&swFrame);
                    swFrame = nullptr;
                }
            }

            int64_t pts = actualFrame->pts != AV_NOPTS_VALUE ? actualFrame->pts : actualFrame->pkt_dts;
            if (pts == AV_NOPTS_VALUE) {
                pts = ctx->lastDecodedPts >= 0 ? ctx->lastDecodedPts + ctx->oneFramePts : targetPts;
            }
            ctx->lastDecodedPts = pts;

            double ptsSec = pts * tb;
            int64_t fIdx = qRound64(ptsSec * ctx->fps);

            // Acceptance check: reached target frame index or target PTS
            if (fIdx >= targetFrameIdx || pts >= targetPts - halfFramePts) {
                result = convertFrame(actualFrame);
                ctx->lastGoodFrame = result;
                int costMb = qMax(1, static_cast<int>(result.sizeInBytes() / (1024 * 1024)));
                QString fKey = QString("%1_f%2_%3x%4").arg(filePath).arg(fIdx).arg(tw).arg(th);
                m_frameCache.insert(fKey, new QImage(result), costMb);
                if (fIdx != targetFrameIdx) {
                    m_frameCache.insert(cacheKey, new QImage(result), costMb);
                }
                found = true;
            } else {
                // If within 2 frames of target during forward seek catchup, cache it for fast scrubbing
                if (targetFrameIdx - fIdx <= 2) {
                    QImage intermediate = convertFrame(actualFrame);
                    ctx->lastGoodFrame = intermediate;
                    int costMb = qMax(1, static_cast<int>(intermediate.sizeInBytes() / (1024 * 1024)));
                    QString fKey = QString("%1_f%2_%3x%4").arg(filePath).arg(fIdx).arg(tw).arg(th);
                    m_frameCache.insert(fKey, new QImage(intermediate), costMb);
                }
            }

            if (swFrame) {
                av_frame_free(&swFrame);
            }
            if (found) break;
        } else if (ret == AVERROR(EAGAIN)) {
            // Need more packets from container
            bool packetFed = false;
            while (av_read_frame(ctx->fmtCtx, pkt) >= 0) {
                if (pkt->stream_index == ctx->videoStreamIdx) {
                    avcodec_send_packet(ctx->codecCtx, pkt);
                    av_packet_unref(pkt);
                    packetFed = true;
                    break;
                }
                av_packet_unref(pkt);
            }
            if (!packetFed) {
                // EOF reached - flush decoder
                avcodec_send_packet(ctx->codecCtx, nullptr);
                if (avcodec_receive_frame(ctx->codecCtx, frame) == 0) {
                    AVFrame *actualFrame = frame;
                    AVFrame *swFrame = nullptr;
                    if (ctx->hwPixelFormat != AV_PIX_FMT_NONE && frame->format == ctx->hwPixelFormat) {
                        swFrame = av_frame_alloc();
                        if (av_hwframe_transfer_data(swFrame, frame, 0) == 0) {
                            actualFrame = swFrame;
                        } else {
                            av_frame_free(&swFrame);
                            swFrame = nullptr;
                        }
                    }
                    result = convertFrame(actualFrame);
                    ctx->lastGoodFrame = result;
                    if (swFrame) av_frame_free(&swFrame);
                    found = true;
                }
                break;
            }
        } else {
            // AVERROR_EOF or other error
            break;
        }
    }

    av_packet_free(&pkt);
    av_frame_free(&frame);

    // Fallback to lastGoodFrame to prevent black flicker
    if (result.isNull() && !ctx->lastGoodFrame.isNull()) {
        result = ctx->lastGoodFrame;
    }

    return result;
}
