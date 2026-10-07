#include "videoframedecoder.h"
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
    if (ctx->codecCtx) {
        avcodec_free_context(&ctx->codecCtx);
    }
    if (ctx->fmtCtx) {
        avformat_close_input(&ctx->fmtCtx);
    }
    delete ctx;
}

VideoFrameDecoder::DecoderContext* VideoFrameDecoder::getOrCreateContext(const QString &filePath, bool isThumbnail)
{
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

    // Enable multithreaded slice & frame decoding for high performance on 1080p / 4K
    codecCtx->thread_count = qBound(1, QThread::idealThreadCount(), 8);
    codecCtx->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;

    if (avcodec_open2(codecCtx, codec, nullptr) != 0) {
        avcodec_free_context(&codecCtx);
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

    bool isThumb = (targetSize.isValid() && targetSize.width() > 0 && targetSize.width() <= 320 && targetSize.height() <= 320);
    int tw = isThumb ? targetSize.width() : 0;
    int th = isThumb ? targetSize.height() : 0;

    DecoderContext *ctx = getOrCreateContext(filePath, isThumb);
    if (!ctx) {
        return QImage();
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
    int64_t maxForwardSkipPts = static_cast<int64_t>(1.5 / tb);

    // Precise seeking decision:
    // - Never decoded yet (lastDecodedPts < 0)
    // - Target is earlier than current decoder position (backward scrub/jump)
    // - Target is more than 1.5 seconds in the future (fast forward / long jump)
    bool needSeek = (ctx->lastDecodedPts < 0 ||
                     targetPts < (ctx->lastDecodedPts - halfFramePts) ||
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
        if (targetSize.isValid() && targetSize.width() > 0 && targetSize.height() > 0) {
            if (isThumb) {
                QSize thumbSize = QSize(f->width, f->height).scaled(targetSize, Qt::KeepAspectRatio);
                outW = thumbSize.width();
                outH = thumbSize.height();
            }
        }
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
            int64_t pts = frame->pts != AV_NOPTS_VALUE ? frame->pts : frame->pkt_dts;
            if (pts == AV_NOPTS_VALUE) {
                pts = ctx->lastDecodedPts >= 0 ? ctx->lastDecodedPts + ctx->oneFramePts : targetPts;
            }
            ctx->lastDecodedPts = pts;

            double ptsSec = pts * tb;
            int64_t fIdx = qRound64(ptsSec * ctx->fps);

            // Acceptance check: reached target frame index or target PTS
            if (fIdx >= targetFrameIdx || pts >= targetPts - halfFramePts) {
                result = convertFrame(frame);
                ctx->lastGoodFrame = result;
                QString fKey = QString("%1_f%2_%3x%4").arg(filePath).arg(fIdx).arg(tw).arg(th);
                m_frameCache.insert(fKey, new QImage(result), 1);
                if (fIdx != targetFrameIdx) {
                    m_frameCache.insert(cacheKey, new QImage(result), 1);
                }
                found = true;
                break;
            } else {
                // If within 2 frames of target during forward seek catchup, cache it for fast scrubbing
                if (targetFrameIdx - fIdx <= 2) {
                    QImage intermediate = convertFrame(frame);
                    ctx->lastGoodFrame = intermediate;
                    QString fKey = QString("%1_f%2_%3x%4").arg(filePath).arg(fIdx).arg(tw).arg(th);
                    m_frameCache.insert(fKey, new QImage(intermediate), 1);
                }
            }
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
                    result = convertFrame(frame);
                    ctx->lastGoodFrame = result;
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

    // Fallback to lastGoodFrame to prevent black flicker, but NEVER poison the cache!
    if (result.isNull() && !ctx->lastGoodFrame.isNull()) {
        result = ctx->lastGoodFrame;
    }

    return result;
}
