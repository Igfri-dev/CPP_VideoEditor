#pragma once

#include <QString>
#include <QImage>
#include <QMap>
#include <QCache>
#include <QRecursiveMutex>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/hwcontext.h>
}

class VideoFrameDecoder {
public:
    static VideoFrameDecoder& instance();

    QImage getFrame(const QString &filePath, qint64 timestampMs, const QSize &targetSize = QSize(1920, 1080));
    QSize getVideoDimensions(const QString &filePath);
    void clearCache();

    // Configurable RAM Cache bounds
    void setMaxCacheSizeMb(int mb);
    int maxCacheSizeMb() const;
    int currentCacheSizeMb() const;

private:
    VideoFrameDecoder();
    ~VideoFrameDecoder();

    struct DecoderContext {
        AVFormatContext *fmtCtx = nullptr;
        AVCodecContext *codecCtx = nullptr;
        AVBufferRef *hwDeviceCtx = nullptr;
        AVPixelFormat hwPixelFormat = AV_PIX_FMT_NONE;
        int videoStreamIdx = -1;
        AVRational timeBase = {1, 1000};
        double fps = 30.0;
        int64_t oneFramePts = 1;
        qint64 durationMs = 0;
        int width = 0;
        int height = 0;
        int64_t lastDecodedPts = -1;
        QImage lastGoodFrame;
        SwsContext *swsCtx = nullptr;
    };

    DecoderContext* getOrCreateContext(const QString &filePath, const QSize &targetSize = QSize());
    void closeContext(DecoderContext *ctx);

    mutable QRecursiveMutex m_mutex;
    QMap<QString, DecoderContext*> m_contexts;
    QMap<QString, DecoderContext*> m_thumbContexts;
    QCache<QString, QImage> m_frameCache;
    QMap<QString, QImage> m_imageCache;
    QMap<QString, QSize> m_dimensionsCache;
    int m_maxCacheMb = 1024;
};
