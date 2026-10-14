#include "pluginregistry.h"
#include <QtMath>
#include <algorithm>
#include <vector>

// -----------------------------------------------------------------------------
// Reference Plugin 1: Glitch FX (Digital Glitch Artifacts)
// -----------------------------------------------------------------------------
class GlitchEffectPlugin : public IVideoEffectPlugin {
public:
    QString id() const override { return "plugin.fx.glitch"; }
    QString name() const override { return "Glitch Digital (RGB Split)"; }
    QString category() const override { return "Estilizado / Retro"; }
    QString description() const override { return "Simula artefactos digitales y separación cromática de bloques."; }

    QList<PluginParameter> parameters() const override {
        QList<PluginParameter> list;
        PluginParameter p1;
        p1.id = "amount";
        p1.name = "Intensidad";
        p1.type = PluginParamType::Float;
        p1.defaultValue = 0.5;
        p1.minValue = 0.0;
        p1.maxValue = 1.0;
        list.append(p1);

        PluginParameter p2;
        p2.id = "block_shift";
        p2.name = "Desplazamiento de Bloques";
        p2.type = PluginParamType::Float;
        p2.defaultValue = 12.0;
        p2.minValue = 0.0;
        p2.maxValue = 50.0;
        list.append(p2);
        return list;
    }

    void processFrame(QImage &frame, qint64 timeMs, const QMap<QString, QVariant> &params) override {
        if (frame.isNull()) return;
        float amount = params.value("amount", 0.5f).toFloat();
        int shift = static_cast<int>(params.value("block_shift", 12.0f).toFloat() * amount);
        if (shift <= 0) return;

        QImage orig = frame.copy();
        const int w = frame.width();
        const int h = frame.height();

        // Horizontal RGB split offset
        for (int y = 0; y < h; ++y) {
            QRgb *dst = reinterpret_cast<QRgb*>(frame.scanLine(y));
            const QRgb *src = reinterpret_cast<const QRgb*>(orig.constScanLine(y));

            int dynamicShift = shift;
            if ((y + (timeMs / 50)) % 16 < 4) {
                dynamicShift = static_cast<int>(shift * 2.5);
            }

            for (int x = 0; x < w; ++x) {
                int rX = std::clamp(x + dynamicShift, 0, w - 1);
                int bX = std::clamp(x - dynamicShift, 0, w - 1);

                int r = qRed(src[rX]);
                int g = qGreen(src[x]);
                int b = qBlue(src[bX]);
                int a = qAlpha(src[x]);

                dst[x] = qRgba(r, g, b, a);
            }
        }
    }
};

// -----------------------------------------------------------------------------
// Reference Plugin 2: CRT Scanlines (Retro Display)
// -----------------------------------------------------------------------------
class CrtScanlinesPlugin : public IVideoEffectPlugin {
public:
    QString id() const override { return "plugin.fx.crt"; }
    QString name() const override { return "Líneas CRT Retro"; }
    QString category() const override { return "Estilizado / Retro"; }
    QString description() const override { return "Añade líneas de escaneo analógicas y viñeteado de fósforo."; }

    QList<PluginParameter> parameters() const override {
        QList<PluginParameter> list;
        PluginParameter p1;
        p1.id = "density";
        p1.name = "Densidad de Líneas";
        p1.type = PluginParamType::Int;
        p1.defaultValue = 4;
        p1.minValue = 2.0;
        p1.maxValue = 12.0;
        list.append(p1);

        PluginParameter p2;
        p2.id = "darkness";
        p2.name = "Oscuridad de Línea";
        p2.type = PluginParamType::Float;
        p2.defaultValue = 0.35;
        p2.minValue = 0.0;
        p2.maxValue = 1.0;
        list.append(p2);
        return list;
    }

    void processFrame(QImage &frame, qint64, const QMap<QString, QVariant> &params) override {
        if (frame.isNull()) return;
        int density = std::max(2, params.value("density", 4).toInt());
        float darkness = params.value("darkness", 0.35f).toFloat();
        float mul = 1.0f - darkness;

        const int w = frame.width();
        const int h = frame.height();

        for (int y = 0; y < h; ++y) {
            if (y % density == 0) {
                QRgb *dst = reinterpret_cast<QRgb*>(frame.scanLine(y));
                for (int x = 0; x < w; ++x) {
                    QRgb p = dst[x];
                    int r = static_cast<int>(qRed(p) * mul);
                    int g = static_cast<int>(qGreen(p) * mul);
                    int b = static_cast<int>(qBlue(p) * mul);
                    dst[x] = qRgba(r, g, b, qAlpha(p));
                }
            }
        }
    }
};

// -----------------------------------------------------------------------------
// Reference Audio Plugin 3: Echo / Delay
// -----------------------------------------------------------------------------
class EchoDelayPlugin : public IAudioEffectPlugin {
public:
    QString id() const override { return "plugin.audio.echo"; }
    QString name() const override { return "Eco / Delay Digital"; }
    QString description() const override { return "Línea de retraso con retroalimentación y control dry/wet."; }

    QList<PluginParameter> parameters() const override {
        QList<PluginParameter> list;
        PluginParameter p1;
        p1.id = "delay_ms";
        p1.name = "Tiempo de Retraso (ms)";
        p1.type = PluginParamType::Float;
        p1.defaultValue = 250.0;
        p1.minValue = 20.0;
        p1.maxValue = 1000.0;
        list.append(p1);

        PluginParameter p2;
        p2.id = "feedback";
        p2.name = "Retroalimentación (Feedback)";
        p2.type = PluginParamType::Float;
        p2.defaultValue = 0.40;
        p2.minValue = 0.0;
        p2.maxValue = 0.95;
        list.append(p2);
        return list;
    }

    void processAudio(float *samples, int count, int, const QMap<QString, QVariant> &params) override {
        if (!samples || count <= 0) return;
        float delayMs = params.value("delay_ms", 250.0f).toFloat();
        float fb = std::clamp(params.value("feedback", 0.40f).toFloat(), 0.0f, 0.95f);

        int delaySamples = static_cast<int>((delayMs / 1000.0f) * 44100.0f);
        if (delaySamples <= 10) return;

        if (m_buffer.size() != delaySamples) {
            m_buffer.assign(delaySamples, 0.0f);
            m_writeIdx = 0;
        }

        for (int i = 0; i < count; ++i) {
            float in = samples[i];
            float delayed = m_buffer[m_writeIdx];
            m_buffer[m_writeIdx] = in + delayed * fb;
            m_writeIdx = (m_writeIdx + 1) % delaySamples;
            samples[i] = in + delayed * 0.5f;
        }
    }

private:
    std::vector<float> m_buffer;
    int m_writeIdx = 0;
};

// -----------------------------------------------------------------------------
// PluginRegistry implementation
// -----------------------------------------------------------------------------

PluginRegistry& PluginRegistry::instance()
{
    static PluginRegistry s_instance;
    return s_instance;
}

PluginRegistry::PluginRegistry()
{
    registerDefaultPlugins();
}

void PluginRegistry::registerDefaultPlugins()
{
    registerVideoPlugin(std::make_shared<GlitchEffectPlugin>());
    registerVideoPlugin(std::make_shared<CrtScanlinesPlugin>());
    registerAudioPlugin(std::make_shared<EchoDelayPlugin>());
}

void PluginRegistry::registerVideoPlugin(std::shared_ptr<IVideoEffectPlugin> plugin)
{
    if (plugin) {
        m_videoPlugins.insert(plugin->id(), plugin);
    }
}

void PluginRegistry::registerAudioPlugin(std::shared_ptr<IAudioEffectPlugin> plugin)
{
    if (plugin) {
        m_audioPlugins.insert(plugin->id(), plugin);
    }
}

std::shared_ptr<IVideoEffectPlugin> PluginRegistry::findVideoPlugin(const QString &id) const
{
    if (m_videoPlugins.contains(id)) return m_videoPlugins.value(id);
    for (auto it = m_videoPlugins.begin(); it != m_videoPlugins.end(); ++it) {
        if (it.key().endsWith(id) || it.key().contains(id) || it.value()->name().contains(id, Qt::CaseInsensitive)) {
            return it.value();
        }
    }
    return nullptr;
}

std::shared_ptr<IAudioEffectPlugin> PluginRegistry::findAudioPlugin(const QString &id) const
{
    if (m_audioPlugins.contains(id)) return m_audioPlugins.value(id);
    for (auto it = m_audioPlugins.begin(); it != m_audioPlugins.end(); ++it) {
        if (it.key().endsWith(id) || it.key().contains(id) || it.value()->name().contains(id, Qt::CaseInsensitive)) {
            return it.value();
        }
    }
    return nullptr;
}

QList<std::shared_ptr<IVideoEffectPlugin>> PluginRegistry::videoPlugins() const
{
    return m_videoPlugins.values();
}

QList<std::shared_ptr<IAudioEffectPlugin>> PluginRegistry::audioPlugins() const
{
    return m_audioPlugins.values();
}
