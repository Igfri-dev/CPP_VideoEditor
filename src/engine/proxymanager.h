#pragma once

#include <QString>
#include <QObject>
#include <QSize>
#include <QImage>
#include <QMap>
#include <QMutex>
#include <functional>

enum class ProxyResolution {
    Half = 0,    // 50% scale
    Quarter = 1, // 25% scale
    Low = 2      // 360p draft
};

QString proxyResolutionToString(ProxyResolution res);

class ProxyManager : public QObject {
    Q_OBJECT
public:
    static ProxyManager& instance();

    bool isProxyModeEnabled() const { return m_proxyModeEnabled; }
    void setProxyModeEnabled(bool enabled);

    ProxyResolution defaultProxyResolution() const { return m_defaultResolution; }
    void setDefaultProxyResolution(ProxyResolution res) { m_defaultResolution = res; }

    QString cacheDirectory() const { return m_cacheDir; }
    void setCacheDirectory(const QString &dir);

    bool hasProxy(const QString &originalPath) const;
    QString getProxyPath(const QString &originalPath) const;
    void registerProxy(const QString &originalPath, const QString &proxyPath);

    // Determines effective playback path (returns proxy if active and available, else original)
    QString effectiveMediaFilePath(const QString &originalPath) const;

    // Background or on-demand proxy generation helper
    bool generateProxy(const QString &originalPath,
                       ProxyResolution res = ProxyResolution::Half,
                       std::function<void(double progress)> progressCb = nullptr);

    // -------------------------------------------------------------------------
    // Render Cache for Complex Timeline Effects
    // -------------------------------------------------------------------------
    bool hasRenderCache(const QString &renderKey) const;
    QImage getRenderCache(const QString &renderKey) const;
    void storeRenderCache(const QString &renderKey, const QImage &frame);
    void clearRenderCache();
    qint64 renderCacheSizeMb() const;

signals:
    void proxyModeToggled(bool enabled);
    void proxyGenerationProgress(const QString &path, double progress);
    void proxyGenerationCompleted(const QString &path, const QString &proxyPath);

private:
    ProxyManager();
    mutable QMutex m_mutex;
    bool m_proxyModeEnabled = false;
    ProxyResolution m_defaultResolution = ProxyResolution::Half;
    QString m_cacheDir;

    QMap<QString, QString> m_proxyMap; // originalPath -> proxyPath
    QMap<QString, QImage> m_renderCache; // renderKey -> composite frame
};
