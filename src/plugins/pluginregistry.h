#pragma once

#include "plugininterface.h"
#include <QObject>
#include <QMap>
#include <memory>

class PluginRegistry : public QObject {
    Q_OBJECT
public:
    static PluginRegistry& instance();

    void registerVideoPlugin(std::shared_ptr<IVideoEffectPlugin> plugin);
    void registerAudioPlugin(std::shared_ptr<IAudioEffectPlugin> plugin);

    std::shared_ptr<IVideoEffectPlugin> findVideoPlugin(const QString &id) const;
    std::shared_ptr<IAudioEffectPlugin> findAudioPlugin(const QString &id) const;

    QList<std::shared_ptr<IVideoEffectPlugin>> videoPlugins() const;
    QList<std::shared_ptr<IAudioEffectPlugin>> audioPlugins() const;

    void registerDefaultPlugins();

private:
    PluginRegistry();
    QMap<QString, std::shared_ptr<IVideoEffectPlugin>> m_videoPlugins;
    QMap<QString, std::shared_ptr<IAudioEffectPlugin>> m_audioPlugins;
};
