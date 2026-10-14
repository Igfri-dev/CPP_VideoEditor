#include "gpuaccelerationmanager.h"
#include "videoframedecoder.h"
#include <QMutexLocker>
#include <QSysInfo>
#include <QDebug>
#include <algorithm>

GpuAccelerationManager& GpuAccelerationManager::instance()
{
    static GpuAccelerationManager s_instance;
    return s_instance;
}

GpuAccelerationManager::GpuAccelerationManager()
{
    detectHardware();
}

void GpuAccelerationManager::detectHardware()
{
    QMutexLocker locker(&m_mutex);

    // 1. Detect Host CPU Architecture and Operating System
#if defined(Q_OS_MACOS)
    #if defined(__aarch64__) || defined(__arm64__)
        m_architectureStr = "Apple Silicon ARM64 (macOS)";
        m_vendor = GpuVendor::Apple;
        m_vendorStr = "Apple";
        m_rendererStr = "Apple M-Series GPU (Unified Memory)";
    #else
        m_architectureStr = "Intel x86_64 (macOS)";
        m_vendor = GpuVendor::Intel;
        m_vendorStr = "Intel / AMD";
        m_rendererStr = "macOS Graphics Engine";
    #endif
#elif defined(Q_OS_WIN)
    #if defined(_M_ARM64)
        m_architectureStr = "ARM64 (Windows)";
    #else
        m_architectureStr = "x86_64 (Windows)";
    #endif
    m_vendor = GpuVendor::Unknown;
    m_vendorStr = "Auto-detecting GPU...";
    m_rendererStr = "Direct3D / OpenGL Hardware";
#elif defined(Q_OS_LINUX)
    #if defined(__aarch64__)
        m_architectureStr = "ARM64 (Linux)";
    #else
        m_architectureStr = "x86_64 (Linux)";
    #endif
    m_vendor = GpuVendor::Unknown;
    m_vendorStr = "Auto-detecting GPU...";
    m_rendererStr = "Mesa / Proprietary Driver";
#else
    m_architectureStr = QSysInfo::buildCpuArchitecture() + " (" + QSysInfo::productType() + ")";
    m_vendor = GpuVendor::Unknown;
    m_vendorStr = "Generic GPU";
    m_rendererStr = "Default Renderer";
#endif

    // 2. Query FFmpeg for compiled & supported hardware accelerators
    m_availableBackends.clear();
    m_availableBackends.append(HwAccelBackend::Auto);

    enum AVHWDeviceType type = AV_HWDEVICE_TYPE_NONE;
    bool hasVTB = false;
    bool hasCuda = false;
    bool hasQsv = false;
    bool hasD3D11 = false;
    bool hasVaapi = false;

    while ((type = av_hwdevice_iterate_types(type)) != AV_HWDEVICE_TYPE_NONE) {
        if (type == AV_HWDEVICE_TYPE_VIDEOTOOLBOX) {
            hasVTB = true;
        } else if (type == AV_HWDEVICE_TYPE_CUDA) {
            hasCuda = true;
        } else if (type == AV_HWDEVICE_TYPE_QSV) {
            hasQsv = true;
        } else if (type == AV_HWDEVICE_TYPE_D3D11VA) {
            hasD3D11 = true;
        } else if (type == AV_HWDEVICE_TYPE_VAAPI) {
            hasVaapi = true;
        }
    }

    if (hasVTB) {
        m_availableBackends.append(HwAccelBackend::AppleVideoToolbox);
    }
    if (hasCuda) {
        m_availableBackends.append(HwAccelBackend::NvidiaCuda);
    }
    if (hasQsv) {
        m_availableBackends.append(HwAccelBackend::IntelQuickSync);
    }
    if (hasD3D11) {
        m_availableBackends.append(HwAccelBackend::AmdD3D11VA);
        m_availableBackends.append(HwAccelBackend::Direct3D11);
    }
    if (hasVaapi) {
        m_availableBackends.append(HwAccelBackend::VaapiLinux);
    }
    m_availableBackends.append(HwAccelBackend::Software);
}

void GpuAccelerationManager::updateFromGlInfo(const QString &glVendor, const QString &glRenderer)
{
    QMutexLocker locker(&m_mutex);
    if (!glRenderer.isEmpty()) {
        m_rendererStr = glRenderer;
    }

    QString lowerV = glVendor.toLower();
    QString lowerR = glRenderer.toLower();

    if (lowerV.contains("nvidia") || lowerR.contains("nvidia") || lowerR.contains("geforce") || lowerR.contains("rtx") || lowerR.contains("gtx")) {
        m_vendor = GpuVendor::Nvidia;
        m_vendorStr = "NVIDIA";
    } else if (lowerV.contains("amd") || lowerV.contains("ati") || lowerR.contains("radeon") || lowerR.contains("amd")) {
        m_vendor = GpuVendor::Amd;
        m_vendorStr = "AMD";
    } else if (lowerV.contains("intel") || lowerR.contains("intel") || lowerR.contains("arc") || lowerR.contains("iris") || lowerR.contains("uhd")) {
        m_vendor = GpuVendor::Intel;
        m_vendorStr = "Intel";
    } else if (lowerV.contains("apple") || lowerR.contains("apple")) {
        m_vendor = GpuVendor::Apple;
        m_vendorStr = "Apple";
    }
}

GpuVendor GpuAccelerationManager::detectedVendor() const
{
    QMutexLocker locker(&m_mutex);
    return m_vendor;
}

QString GpuAccelerationManager::vendorName() const
{
    QMutexLocker locker(&m_mutex);
    return m_vendorStr;
}

QString GpuAccelerationManager::rendererName() const
{
    QMutexLocker locker(&m_mutex);
    return m_rendererStr;
}

QString GpuAccelerationManager::architectureName() const
{
    QMutexLocker locker(&m_mutex);
    return m_architectureStr;
}

bool GpuAccelerationManager::isAppleSilicon() const
{
    QMutexLocker locker(&m_mutex);
    return m_vendor == GpuVendor::Apple;
}

bool GpuAccelerationManager::isNvidiaGpu() const
{
    QMutexLocker locker(&m_mutex);
    return m_vendor == GpuVendor::Nvidia;
}

bool GpuAccelerationManager::isAmdGpu() const
{
    QMutexLocker locker(&m_mutex);
    return m_vendor == GpuVendor::Amd;
}

bool GpuAccelerationManager::isIntelGpu() const
{
    QMutexLocker locker(&m_mutex);
    return m_vendor == GpuVendor::Intel;
}

QList<HwAccelBackend> GpuAccelerationManager::availableBackends() const
{
    QMutexLocker locker(&m_mutex);
    return m_availableBackends;
}

QString GpuAccelerationManager::backendDisplayName(HwAccelBackend backend) const
{
    switch (backend) {
    case HwAccelBackend::Auto:
        return "Auto (Detección Óptima de Hardware)";
    case HwAccelBackend::AppleVideoToolbox:
        return "Apple VideoToolbox (Apple Silicon M-Series)";
    case HwAccelBackend::NvidiaCuda:
        return "NVIDIA CUDA / NVDEC (GeForce / RTX / Quadro)";
    case HwAccelBackend::IntelQuickSync:
        return "Intel QuickSync (Arc / Iris Xe / UHD)";
    case HwAccelBackend::AmdD3D11VA:
        return "AMD Radeon D3D11VA / AMF (Radeon RX / Vega)";
    case HwAccelBackend::Direct3D11:
        return "DirectX 11 (D3D11VA Hardware Acceleration)";
    case HwAccelBackend::VaapiLinux:
        return "Linux VA-API Hardware Acceleration";
    case HwAccelBackend::Software:
        return "CPU Software (Sin aceleración por GPU)";
    }
    return "Desconocido";
}

HwAccelBackend GpuAccelerationManager::selectedBackend() const
{
    QMutexLocker locker(&m_mutex);
    return m_selectedBackend;
}

void GpuAccelerationManager::setSelectedBackend(HwAccelBackend backend)
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_selectedBackend == backend) return;
        m_selectedBackend = backend;
    }
    // Flush decoder cache so newly opened contexts utilize the selected backend
    VideoFrameDecoder::instance().clearCache();
    emit backendChanged(backend);
}

AVHWDeviceType GpuAccelerationManager::activeFfmpegHwType() const
{
    QMutexLocker locker(&m_mutex);
    if (!m_hwDecodingEnabled) {
        return AV_HWDEVICE_TYPE_NONE;
    }

    HwAccelBackend backend = m_selectedBackend;
    if (backend == HwAccelBackend::Auto) {
        // Choose automatically according to detected vendor and available backends
        if (m_vendor == GpuVendor::Apple && m_availableBackends.contains(HwAccelBackend::AppleVideoToolbox)) {
            return AV_HWDEVICE_TYPE_VIDEOTOOLBOX;
        }
        if (m_vendor == GpuVendor::Nvidia && m_availableBackends.contains(HwAccelBackend::NvidiaCuda)) {
            return AV_HWDEVICE_TYPE_CUDA;
        }
        if (m_vendor == GpuVendor::Intel && m_availableBackends.contains(HwAccelBackend::IntelQuickSync)) {
            return AV_HWDEVICE_TYPE_QSV;
        }
        if (m_availableBackends.contains(HwAccelBackend::Direct3D11)) {
            return AV_HWDEVICE_TYPE_D3D11VA;
        }
        if (m_availableBackends.contains(HwAccelBackend::AppleVideoToolbox)) {
            return AV_HWDEVICE_TYPE_VIDEOTOOLBOX;
        }
        if (m_availableBackends.contains(HwAccelBackend::NvidiaCuda)) {
            return AV_HWDEVICE_TYPE_CUDA;
        }
        if (m_availableBackends.contains(HwAccelBackend::VaapiLinux)) {
            return AV_HWDEVICE_TYPE_VAAPI;
        }
        return AV_HWDEVICE_TYPE_NONE;
    }

    switch (backend) {
    case HwAccelBackend::AppleVideoToolbox:
        return AV_HWDEVICE_TYPE_VIDEOTOOLBOX;
    case HwAccelBackend::NvidiaCuda:
        return AV_HWDEVICE_TYPE_CUDA;
    case HwAccelBackend::IntelQuickSync:
        return AV_HWDEVICE_TYPE_QSV;
    case HwAccelBackend::AmdD3D11VA:
    case HwAccelBackend::Direct3D11:
        return AV_HWDEVICE_TYPE_D3D11VA;
    case HwAccelBackend::VaapiLinux:
        return AV_HWDEVICE_TYPE_VAAPI;
    case HwAccelBackend::Software:
    case HwAccelBackend::Auto:
    default:
        return AV_HWDEVICE_TYPE_NONE;
    }
}

bool GpuAccelerationManager::isHwDecodingEnabled() const
{
    QMutexLocker locker(&m_mutex);
    return m_hwDecodingEnabled;
}

void GpuAccelerationManager::setHwDecodingEnabled(bool enabled)
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_hwDecodingEnabled == enabled) return;
        m_hwDecodingEnabled = enabled;
    }
    VideoFrameDecoder::instance().clearCache();
    emit backendChanged(m_selectedBackend);
}

int GpuAccelerationManager::maxRamCacheMb() const
{
    QMutexLocker locker(&m_mutex);
    return m_maxRamCacheMb;
}

void GpuAccelerationManager::setMaxRamCacheMb(int mb)
{
    int clamped = qBound(64, mb, 32768);
    {
        QMutexLocker locker(&m_mutex);
        m_maxRamCacheMb = clamped;
    }
    VideoFrameDecoder::instance().setMaxCacheSizeMb(clamped);
    emit ramCacheSettingsChanged(clamped);
}

int GpuAccelerationManager::currentRamCacheMb() const
{
    return VideoFrameDecoder::instance().currentCacheSizeMb();
}

void GpuAccelerationManager::clearRamCache()
{
    VideoFrameDecoder::instance().clearCache();
}

PreviewResolution GpuAccelerationManager::previewResolution() const
{
    QMutexLocker locker(&m_mutex);
    return m_previewResolution;
}

void GpuAccelerationManager::setPreviewResolution(PreviewResolution res)
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_previewResolution == res) return;
        m_previewResolution = res;
    }
    emit previewResolutionChanged(res);
}

QSize GpuAccelerationManager::calculatePreviewSize(const QSize &canvasSize) const
{
    QMutexLocker locker(&m_mutex);
    if (canvasSize.isEmpty() || !canvasSize.isValid()) {
        return QSize(960, 540);
    }

    int cw = canvasSize.width();
    int ch = canvasSize.height();
    int targetW = cw;
    int targetH = ch;

    switch (m_previewResolution) {
    case PreviewResolution::Full:
        targetW = cw;
        targetH = ch;
        break;
    case PreviewResolution::Half:
        targetW = cw / 2;
        targetH = ch / 2;
        break;
    case PreviewResolution::Quarter:
        targetW = cw / 4;
        targetH = ch / 4;
        break;
    case PreviewResolution::Draft:
        targetW = qMax(160, cw / 8);
        targetH = qMax(90, ch / 8);
        break;
    }

    // Ensure dimensions are positive even numbers for optimal hardware & sws_scale alignment
    targetW = qMax(2, (targetW / 2) * 2);
    targetH = qMax(2, (targetH / 2) * 2);
    return QSize(targetW, targetH);
}

QString GpuAccelerationManager::previewResolutionDisplayName(PreviewResolution res) const
{
    switch (res) {
    case PreviewResolution::Full:
        return "100% (Completa)";
    case PreviewResolution::Half:
        return "50% (Media - Rendimiento Óptimo)";
    case PreviewResolution::Quarter:
        return "25% (Cuarto - Edición Rápida)";
    case PreviewResolution::Draft:
        return "12.5% (Borrador - Mínimo Consumo de RAM)";
    }
    return "50% (Media)";
}

bool GpuAccelerationManager::isGpuRenderingEnabled() const
{
    QMutexLocker locker(&m_mutex);
    return m_gpuRenderingEnabled;
}

void GpuAccelerationManager::setGpuRenderingEnabled(bool enabled)
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_gpuRenderingEnabled == enabled) return;
        m_gpuRenderingEnabled = enabled;
    }
    emit gpuRenderingChanged(enabled);
}
