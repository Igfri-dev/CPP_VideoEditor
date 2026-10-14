#pragma once

#include <QObject>
#include <QString>
#include <QSize>
#include <QList>
#include <QMutex>

extern "C" {
#include <libavutil/hwcontext.h>
}

enum class GpuVendor {
    Unknown,
    Apple,
    Nvidia,
    Amd,
    Intel,
    Software
};

enum class HwAccelBackend {
    Auto = 0,
    AppleVideoToolbox,
    NvidiaCuda,
    IntelQuickSync,
    AmdD3D11VA,
    Direct3D11,
    VaapiLinux,
    Software
};

enum class PreviewResolution {
    Full = 0,     // 100%
    Half = 1,     // 50%
    Quarter = 2,  // 25%
    Draft = 3     // 12.5%
};

class GpuAccelerationManager : public QObject {
    Q_OBJECT

public:
    static GpuAccelerationManager& instance();

    // Hardware and Vendor Detection
    GpuVendor detectedVendor() const;
    QString vendorName() const;
    QString rendererName() const;
    QString architectureName() const;
    bool isAppleSilicon() const;
    bool isNvidiaGpu() const;
    bool isAmdGpu() const;
    bool isIntelGpu() const;

    // Available Backends on this System
    QList<HwAccelBackend> availableBackends() const;
    QString backendDisplayName(HwAccelBackend backend) const;

    // Active Backend Configuration
    HwAccelBackend selectedBackend() const;
    void setSelectedBackend(HwAccelBackend backend);
    AVHWDeviceType activeFfmpegHwType() const;
    bool isHwDecodingEnabled() const;
    void setHwDecodingEnabled(bool enabled);

    // RAM Cache Management
    int maxRamCacheMb() const;
    void setMaxRamCacheMb(int mb);
    int currentRamCacheMb() const;
    void clearRamCache();

    // Preview Resolution Management
    PreviewResolution previewResolution() const;
    void setPreviewResolution(PreviewResolution res);
    QSize calculatePreviewSize(const QSize &canvasSize) const;
    QString previewResolutionDisplayName(PreviewResolution res) const;

    // GPU OpenGL Shader Rendering
    bool isGpuRenderingEnabled() const;
    void setGpuRenderingEnabled(bool enabled);

    // Dynamic update when OpenGL context is initialized
    void updateFromGlInfo(const QString &glVendor, const QString &glRenderer);

signals:
    void backendChanged(HwAccelBackend backend);
    void previewResolutionChanged(PreviewResolution res);
    void ramCacheSettingsChanged(int maxMb);
    void gpuRenderingChanged(bool enabled);

private:
    GpuAccelerationManager();
    ~GpuAccelerationManager() override = default;

    void detectHardware();

    mutable QMutex m_mutex;
    GpuVendor m_vendor = GpuVendor::Unknown;
    QString m_vendorStr;
    QString m_rendererStr;
    QString m_architectureStr;
    QList<HwAccelBackend> m_availableBackends;
    HwAccelBackend m_selectedBackend = HwAccelBackend::Auto;
    bool m_hwDecodingEnabled = true;

    int m_maxRamCacheMb = 1024;
    PreviewResolution m_previewResolution = PreviewResolution::Half; // Default to Half (50%) for optimal interactive performance
    bool m_gpuRenderingEnabled = true;
};
