#include "gpusettingsdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QMessageBox>

GpuSettingsDialog::GpuSettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Configuración de Rendimiento y Aceleración por GPU");
    setMinimumWidth(540);
    setStyleSheet(
        "QDialog { background-color: #1e1e2e; color: #cdd6f4; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; }"
        "QGroupBox { font-weight: bold; border: 1px solid #45475a; border-radius: 6px; margin-top: 12px; padding-top: 14px; color: #89b4fa; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }"
        "QLabel { color: #cdd6f4; font-size: 13px; }"
        "QCheckBox { color: #cdd6f4; font-size: 13px; spacing: 8px; }"
        "QCheckBox::indicator { width: 16px; height: 16px; }"
        "QComboBox { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 5px 8px; min-height: 24px; }"
        "QComboBox::drop-down { border: none; }"
        "QSpinBox { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 4px 8px; }"
        "QSlider::groove:horizontal { height: 6px; background: #313244; border-radius: 3px; }"
        "QSlider::sub-page:horizontal { background: #89b4fa; border-radius: 3px; }"
        "QSlider::handle:horizontal { background: #cdd6f4; width: 16px; margin-top: -5px; margin-bottom: -5px; border-radius: 8px; }"
        "QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 6px 14px; font-weight: 500; }"
        "QPushButton:hover { background-color: #45475a; border-color: #89b4fa; }"
        "QPushButton:pressed { background-color: #585b70; }"
        "QPushButton#primaryBtn { background-color: #89b4fa; color: #11111b; font-weight: bold; border: none; }"
        "QPushButton#primaryBtn:hover { background-color: #b4befe; }"
    );

    setupUi();
    loadCurrentSettings();
    updateCacheLabel();
}

void GpuSettingsDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(14);
    mainLayout->setContentsMargins(16, 16, 16, 16);

    auto &gpu = GpuAccelerationManager::instance();

    // 1. Hardware Info Group
    QGroupBox *infoGroup = new QGroupBox("Arquitectura y Dispositivo GPU", this);
    QFormLayout *infoLayout = new QFormLayout(infoGroup);
    infoLayout->setSpacing(8);

    m_archLabel = new QLabel(gpu.architectureName(), infoGroup);
    m_archLabel->setStyleSheet("font-weight: bold; color: #a6adc8;");
    infoLayout->addRow("Arquitectura CPU:", m_archLabel);

    m_gpuLabel = new QLabel(QString("%1 (%2)").arg(gpu.vendorName()).arg(gpu.rendererName()), infoGroup);
    m_gpuLabel->setStyleSheet("font-weight: bold; color: #a6e3a1;");
    infoLayout->addRow("GPU Detectada:", m_gpuLabel);

    mainLayout->addWidget(infoGroup);

    // 2. GPU Hardware Acceleration Group
    QGroupBox *accelGroup = new QGroupBox("Aceleración de Decodificación por GPU", this);
    QVBoxLayout *accelLayout = new QVBoxLayout(accelGroup);
    accelLayout->setSpacing(10);

    m_hwDecodingCheck = new QCheckBox("Habilitar decodificación acelerada por hardware", accelGroup);
    m_hwDecodingCheck->setChecked(gpu.isHwDecodingEnabled());
    accelLayout->addWidget(m_hwDecodingCheck);

    QHBoxLayout *backendLayout = new QHBoxLayout();
    QLabel *backendLbl = new QLabel("Motor / Backend:", accelGroup);
    m_backendCombo = new QComboBox(accelGroup);

    for (HwAccelBackend b : gpu.availableBackends()) {
        m_backendCombo->addItem(gpu.backendDisplayName(b), static_cast<int>(b));
    }
    backendLayout->addWidget(backendLbl);
    backendLayout->addWidget(m_backendCombo, 1);
    accelLayout->addLayout(backendLayout);

    mainLayout->addWidget(accelGroup);

    // 3. GPU Screen Rendering Group
    QGroupBox *renderGroup = new QGroupBox("Renderizado en Monitor", this);
    QVBoxLayout *renderLayout = new QVBoxLayout(renderGroup);
    renderLayout->setSpacing(10);

    m_gpuRenderingCheck = new QCheckBox("Renderizado en tiempo real por GPU (OpenGL Shaders)", renderGroup);
    m_gpuRenderingCheck->setChecked(gpu.isGpuRenderingEnabled());
    renderLayout->addWidget(m_gpuRenderingCheck);

    QHBoxLayout *resLayout = new QHBoxLayout();
    QLabel *resLbl = new QLabel("Resolución del previsualizador:", renderGroup);
    m_resolutionCombo = new QComboBox(renderGroup);
    m_resolutionCombo->addItem(gpu.previewResolutionDisplayName(PreviewResolution::Full), static_cast<int>(PreviewResolution::Full));
    m_resolutionCombo->addItem(gpu.previewResolutionDisplayName(PreviewResolution::Half), static_cast<int>(PreviewResolution::Half));
    m_resolutionCombo->addItem(gpu.previewResolutionDisplayName(PreviewResolution::Quarter), static_cast<int>(PreviewResolution::Quarter));
    m_resolutionCombo->addItem(gpu.previewResolutionDisplayName(PreviewResolution::Draft), static_cast<int>(PreviewResolution::Draft));
    resLayout->addWidget(resLbl);
    resLayout->addWidget(m_resolutionCombo, 1);
    renderLayout->addLayout(resLayout);

    mainLayout->addWidget(renderGroup);

    // 4. RAM Cache Bounds Group
    QGroupBox *cacheGroup = new QGroupBox("Gestión de Memoria RAM para Fotogramas", this);
    QVBoxLayout *cacheLayout = new QVBoxLayout(cacheGroup);
    cacheLayout->setSpacing(10);

    QHBoxLayout *sliderBox = new QHBoxLayout();
    QLabel *sliderLbl = new QLabel("Límite de memoria caché:", cacheGroup);
    m_cacheSlider = new QSlider(Qt::Horizontal, cacheGroup);
    m_cacheSlider->setRange(128, 8192);
    m_cacheSlider->setSingleStep(128);
    m_cacheSlider->setPageStep(512);

    m_cacheSpinBox = new QSpinBox(cacheGroup);
    m_cacheSpinBox->setRange(128, 8192);
    m_cacheSpinBox->setSingleStep(128);
    m_cacheSpinBox->setSuffix(" MB");

    connect(m_cacheSlider, &QSlider::valueChanged, m_cacheSpinBox, &QSpinBox::setValue);
    connect(m_cacheSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), m_cacheSlider, &QSlider::setValue);

    sliderBox->addWidget(sliderLbl);
    sliderBox->addWidget(m_cacheSlider, 1);
    sliderBox->addWidget(m_cacheSpinBox);
    cacheLayout->addLayout(sliderBox);

    QHBoxLayout *statusBox = new QHBoxLayout();
    m_currentCacheLabel = new QLabel(cacheGroup);
    m_clearCacheBtn = new QPushButton("Limpiar Caché de RAM", cacheGroup);
    connect(m_clearCacheBtn, &QPushButton::clicked, this, &GpuSettingsDialog::onClearCacheClicked);

    statusBox->addWidget(m_currentCacheLabel, 1);
    statusBox->addWidget(m_clearCacheBtn);
    cacheLayout->addLayout(statusBox);

    mainLayout->addWidget(cacheGroup);

    // 5. Action Buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch(1);

    m_cancelBtn = new QPushButton("Cancelar", this);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(m_cancelBtn);

    m_applyBtn = new QPushButton("Aplicar", this);
    connect(m_applyBtn, &QPushButton::clicked, this, &GpuSettingsDialog::onApply);
    btnLayout->addWidget(m_applyBtn);

    m_okBtn = new QPushButton("Aceptar", this);
    m_okBtn->setObjectName("primaryBtn");
    connect(m_okBtn, &QPushButton::clicked, [this]() {
        onApply();
        accept();
    });
    btnLayout->addWidget(m_okBtn);

    mainLayout->addLayout(btnLayout);
}

void GpuSettingsDialog::loadCurrentSettings()
{
    auto &gpu = GpuAccelerationManager::instance();

    m_hwDecodingCheck->setChecked(gpu.isHwDecodingEnabled());

    int bIdx = m_backendCombo->findData(static_cast<int>(gpu.selectedBackend()));
    if (bIdx >= 0) {
        m_backendCombo->setCurrentIndex(bIdx);
    }

    m_gpuRenderingCheck->setChecked(gpu.isGpuRenderingEnabled());

    int rIdx = m_resolutionCombo->findData(static_cast<int>(gpu.previewResolution()));
    if (rIdx >= 0) {
        m_resolutionCombo->setCurrentIndex(rIdx);
    }

    int curMb = gpu.maxRamCacheMb();
    m_cacheSlider->setValue(curMb);
    m_cacheSpinBox->setValue(curMb);
}

void GpuSettingsDialog::updateCacheLabel()
{
    int usedMb = GpuAccelerationManager::instance().currentRamCacheMb();
    m_currentCacheLabel->setText(QString("Uso actual de RAM en caché: %1 MB").arg(usedMb));
}

void GpuSettingsDialog::onClearCacheClicked()
{
    GpuAccelerationManager::instance().clearRamCache();
    updateCacheLabel();
    QMessageBox::information(this, "Caché de RAM", "Se ha liberado la memoria RAM de los fotogramas decodificados.");
}

void GpuSettingsDialog::onApply()
{
    auto &gpu = GpuAccelerationManager::instance();

    gpu.setHwDecodingEnabled(m_hwDecodingCheck->isChecked());

    int bData = m_backendCombo->currentData().toInt();
    gpu.setSelectedBackend(static_cast<HwAccelBackend>(bData));

    gpu.setGpuRenderingEnabled(m_gpuRenderingCheck->isChecked());

    int rData = m_resolutionCombo->currentData().toInt();
    gpu.setPreviewResolution(static_cast<PreviewResolution>(rData));

    gpu.setMaxRamCacheMb(m_cacheSpinBox->value());

    updateCacheLabel();
}
