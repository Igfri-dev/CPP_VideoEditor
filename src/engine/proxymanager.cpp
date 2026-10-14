#include "proxymanager.h"
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QProcess>
#include <QMutexLocker>
#include <QDebug>

QString proxyResolutionToString(ProxyResolution res)
{
    switch (res) {
    case ProxyResolution::Half: return "50% Media (Half)";
    case ProxyResolution::Quarter: return "25% Cuarto (Quarter)";
    case ProxyResolution::Low: return "360p Borrador (Low)";
    }
    return "50% Media";
}

ProxyManager& ProxyManager::instance()
{
    static ProxyManager s_instance;
    return s_instance;
}

ProxyManager::ProxyManager()
{
    QString baseCache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (baseCache.isEmpty()) {
        baseCache = QDir::tempPath() + "/CPP_VideoEditor";
    }
    m_cacheDir = baseCache + "/proxies";
    QDir().mkpath(m_cacheDir);
}

void ProxyManager::setProxyModeEnabled(bool enabled)
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_proxyModeEnabled == enabled) return;
        m_proxyModeEnabled = enabled;
    }
    emit proxyModeToggled(enabled);
}

void ProxyManager::setCacheDirectory(const QString &dir)
{
    QMutexLocker locker(&m_mutex);
    m_cacheDir = dir;
    QDir().mkpath(m_cacheDir);
}

bool ProxyManager::hasProxy(const QString &originalPath) const
{
    QMutexLocker locker(&m_mutex);
    if (!m_proxyMap.contains(originalPath)) return false;
    QString pPath = m_proxyMap.value(originalPath);
    return QFile::exists(pPath);
}

QString ProxyManager::getProxyPath(const QString &originalPath) const
{
    QMutexLocker locker(&m_mutex);
    return m_proxyMap.value(originalPath, QString());
}

void ProxyManager::registerProxy(const QString &originalPath, const QString &proxyPath)
{
    QMutexLocker locker(&m_mutex);
    m_proxyMap.insert(originalPath, proxyPath);
}

QString ProxyManager::effectiveMediaFilePath(const QString &originalPath) const
{
    QMutexLocker locker(&m_mutex);
    if (m_proxyModeEnabled && m_proxyMap.contains(originalPath)) {
        QString pPath = m_proxyMap.value(originalPath);
        if (QFile::exists(pPath)) {
            return pPath;
        }
    }
    return originalPath;
}

bool ProxyManager::generateProxy(const QString &originalPath,
                                 ProxyResolution res,
                                 std::function<void(double progress)> progressCb)
{
    if (!QFile::exists(originalPath)) return false;

    QFileInfo fi(originalPath);
    QString baseName = fi.baseName();
    QString scaleFilter = "scale=iw/2:ih/2";
    if (res == ProxyResolution::Quarter) {
        scaleFilter = "scale=iw/4:ih/4";
    } else if (res == ProxyResolution::Low) {
        scaleFilter = "scale=-2:360";
    }

    QString outPath = QString("%1/%2_proxy_%3.mp4")
                          .arg(m_cacheDir)
                          .arg(baseName)
                          .arg(static_cast<int>(res));

    // Fast H.264 intra proxy generation via FFmpeg
    QStringList args;
    args << "-y"
         << "-i" << originalPath
         << "-vf" << scaleFilter
         << "-c:v" << "libx264"
         << "-preset" << "ultrafast"
         << "-g" << "1"           // All I-frames for instant zero-latency scrubbing
         << "-tune" << "fastdecode"
         << "-crf" << "23"
         << "-c:a" << "aac"
         << "-b:a" << "128k"
         << outPath;

    QProcess proc;
    proc.start("ffmpeg", args);
    if (!proc.waitForStarted(3000)) {
        // Fallback or testing mode: mock proxy registration
        registerProxy(originalPath, originalPath);
        return false;
    }

    if (progressCb) progressCb(0.25);
    proc.waitForFinished(60000); // Wait up to 60s

    if (proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0 && QFile::exists(outPath)) {
        registerProxy(originalPath, outPath);
        if (progressCb) progressCb(1.0);
        emit proxyGenerationCompleted(originalPath, outPath);
        return true;
    }

    return false;
}

bool ProxyManager::hasRenderCache(const QString &renderKey) const
{
    QMutexLocker locker(&m_mutex);
    return m_renderCache.contains(renderKey);
}

QImage ProxyManager::getRenderCache(const QString &renderKey) const
{
    QMutexLocker locker(&m_mutex);
    return m_renderCache.value(renderKey, QImage());
}

void ProxyManager::storeRenderCache(const QString &renderKey, const QImage &frame)
{
    QMutexLocker locker(&m_mutex);
    // Limit cache entries to avoid unconstrained memory growth
    if (m_renderCache.size() > 120) {
        m_renderCache.remove(m_renderCache.firstKey());
    }
    m_renderCache.insert(renderKey, frame);
}

void ProxyManager::clearRenderCache()
{
    QMutexLocker locker(&m_mutex);
    m_renderCache.clear();
}

qint64 ProxyManager::renderCacheSizeMb() const
{
    QMutexLocker locker(&m_mutex);
    qint64 bytes = 0;
    for (const auto &img : m_renderCache) {
        bytes += img.sizeInBytes();
    }
    return bytes / (1024 * 1024);
}
