#pragma once

#include <QString>
#include <QImage>
#include <QVariant>
#include <QMap>
#include <QList>

enum class PluginParamType {
    Float = 0,
    Int = 1,
    Bool = 2,
    Color = 3,
    Choice = 4
};

struct PluginParameter {
    QString id;
    QString name;
    PluginParamType type = PluginParamType::Float;
    QVariant defaultValue;
    double minValue = 0.0;
    double maxValue = 1.0;
    QStringList choices;
};

class IVideoEffectPlugin {
public:
    virtual ~IVideoEffectPlugin() = default;

    virtual QString id() const = 0;
    virtual QString name() const = 0;
    virtual QString category() const = 0;
    virtual QString description() const = 0;

    virtual QList<PluginParameter> parameters() const = 0;

    virtual void processFrame(QImage &frame,
                              qint64 timeMs,
                              const QMap<QString, QVariant> &params) = 0;
};

class IAudioEffectPlugin {
public:
    virtual ~IAudioEffectPlugin() = default;

    virtual QString id() const = 0;
    virtual QString name() const = 0;
    virtual QString description() const = 0;

    virtual QList<PluginParameter> parameters() const = 0;

    virtual void processAudio(float *samples,
                              int count,
                              int channels,
                              const QMap<QString, QVariant> &params) = 0;
};
