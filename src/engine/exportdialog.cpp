#include "exportdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QStandardPaths>
#include <QFileInfo>
#include <cmath>

static QString formatExportTime(double totalSec)
{
    int mins = static_cast<int>(totalSec) / 60;
    double secs = totalSec - (mins * 60);
    return QString("%1:%2").arg(mins, 2, 10, QChar('0')).arg(secs, 4, 'f', 1, QChar('0'));
}

ExportDialog::ExportDialog(TimelineModel *model, qint64 playheadPositionMs, QWidget *parent)
    : QDialog(parent)
    , m_model(model)
    , m_playheadPositionMs(playheadPositionMs)
{
    setWindowTitle("Exportar Medios (Multi-Formato y Códecs)");
    resize(580, 560);
    setMinimumSize(540, 520);
    setStyleSheet("background-color: #161b22; color: #c9d1d9;");

    m_exporter = new VideoExporter(m_model, this);
    connect(m_exporter, &VideoExporter::progressUpdated, this, &ExportDialog::onProgressUpdated);
    connect(m_exporter, &VideoExporter::exportFinished, this, &ExportDialog::onExportFinished);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(8);

    QLabel *titleLabel = new QLabel("Configuración de Exportación de Medios", this);
    titleLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #58a6ff;");
    mainLayout->addWidget(titleLabel);

    QFormLayout *form = new QFormLayout();
    form->setSpacing(8);

    // 1. Output path
    QHBoxLayout *pathLayout = new QHBoxLayout();
    m_pathEdit = new QLineEdit(this);
    QString defaultPath = QDir(QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)).filePath("proyecto_editado.mp4");
    m_pathEdit->setText(defaultPath);
    m_pathEdit->setStyleSheet("background-color: #0d1117; color: #c9d1d9; border: 1px solid #30363d; border-radius: 4px; padding: 5px;");
    m_browseBtn = new QPushButton("Explorar...", this);
    m_browseBtn->setStyleSheet("background-color: #21262d; color: #c9d1d9; border: 1px solid #30363d; border-radius: 4px; padding: 5px 10px;");
    connect(m_browseBtn, &QPushButton::clicked, this, &ExportDialog::browseOutputFile);
    pathLayout->addWidget(m_pathEdit);
    pathLayout->addWidget(m_browseBtn);
    form->addRow("Archivo de salida:", pathLayout);

    // 2. Format / Container
    m_containerCombo = new QComboBox(this);
    m_containerCombo->addItem("🎬 MP4 Video (.mp4) - Universal Compatible", static_cast<int>(ExportContainer::MP4));
    m_containerCombo->addItem("🍎 QuickTime MOV (.mov) - Apple ProRes / Edición", static_cast<int>(ExportContainer::MOV));
    m_containerCombo->addItem("📦 Matroska MKV (.mkv) - Contenedor Abierto", static_cast<int>(ExportContainer::MKV));
    m_containerCombo->addItem("🌐 WebM Video (.webm) - HTML5 Web (VP9 / Opus)", static_cast<int>(ExportContainer::WebM));
    m_containerCombo->addItem("📼 AVI Video (.avi) - Tradicional (MPEG-4)", static_cast<int>(ExportContainer::AVI));
    m_containerCombo->addItem("🎞️ GIF Animado (.gif) - Animación Web de Alta Calidad", static_cast<int>(ExportContainer::GIF));
    m_containerCombo->addItem("🎵 Audio MP3 (.mp3) - Solo Audio Comprimido", static_cast<int>(ExportContainer::MP3));
    m_containerCombo->addItem("🔊 Audio WAV (.wav) - Solo Audio PCM sin compresión", static_cast<int>(ExportContainer::WAV));
    m_containerCombo->addItem("🎶 Audio AAC (.m4a) - Solo Audio Alta Fidelidad", static_cast<int>(ExportContainer::AAC));
    m_containerCombo->addItem("🎼 Audio FLAC (.flac) - Solo Audio Lossless", static_cast<int>(ExportContainer::FLAC));
    m_containerCombo->addItem("📻 Audio OGG (.ogg) - Solo Audio Libre (Opus)", static_cast<int>(ExportContainer::OGG));
    m_containerCombo->setStyleSheet("background-color: #0d1117; color: #c9d1d9; border: 1px solid #30363d; border-radius: 4px; padding: 5px;");
    connect(m_containerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ExportDialog::onContainerChanged);
    form->addRow("Formato / Contenedor:", m_containerCombo);

    // Video Section
    m_videoSectionWidget = new QWidget(this);
    QFormLayout *videoForm = new QFormLayout(m_videoSectionWidget);
    videoForm->setContentsMargins(0, 0, 0, 0);
    videoForm->setSpacing(8);

    m_videoCodecCombo = new QComboBox(m_videoSectionWidget);
    m_videoCodecCombo->setStyleSheet("background-color: #0d1117; color: #c9d1d9; border: 1px solid #30363d; border-radius: 4px; padding: 5px;");
    connect(m_videoCodecCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ExportDialog::onVideoCodecChanged);
    videoForm->addRow("Códec de video:", m_videoCodecCombo);

    QString hwAccelName = VideoExporter::platformHardwareAccelerationName();
    m_hwAccelCheck = new QCheckBox(QString("⚡ Usar aceleración por hardware %1").arg(hwAccelName), m_videoSectionWidget);
#if defined(Q_OS_MACOS)
    m_hwAccelCheck->setChecked(true);
#else
    m_hwAccelCheck->setChecked(VideoExporter::isHardwareAccelerationAvailable());
#endif
    m_hwAccelCheck->setStyleSheet("color: #58a6ff; font-weight: bold; margin-left: 2px;");
    videoForm->addRow("", m_hwAccelCheck);

    m_resCombo = new QComboBox(m_videoSectionWidget);
    m_resCombo->addItem("1080p Full HD (1920x1080)", QSize(1920, 1080));
    m_resCombo->addItem("720p HD (1280x720)", QSize(1280, 720));
    m_resCombo->addItem("480p SD (854x480)", QSize(854, 480));
    m_resCombo->addItem("4K Ultra HD (3840x2160)", QSize(3840, 2160));
    m_resCombo->addItem("360p Compacto / GIF (640x360)", QSize(640, 360));
    m_resCombo->setStyleSheet("background-color: #0d1117; color: #c9d1d9; border: 1px solid #30363d; border-radius: 4px; padding: 5px;");
    videoForm->addRow("Resolución:", m_resCombo);

    m_fpsCombo = new QComboBox(m_videoSectionWidget);
    m_fpsCombo->addItem("30 fps (Estándar)", 30);
    m_fpsCombo->addItem("60 fps (Fluido)", 60);
    m_fpsCombo->addItem("24 fps (Cinemático)", 24);
    m_fpsCombo->addItem("15 fps (Compacto / GIF)", 15);
    m_fpsCombo->setStyleSheet("background-color: #0d1117; color: #c9d1d9; border: 1px solid #30363d; border-radius: 4px; padding: 5px;");
    connect(m_fpsCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ExportDialog::updateDurationSummary);
    videoForm->addRow("Cuadros por segundo:", m_fpsCombo);

    // Quality row
    QHBoxLayout *qualityLayout = new QHBoxLayout();
    m_qualityCombo = new QComboBox(m_videoSectionWidget);
    m_qualityCombo->addItem("Equilibrada (CRF 23 - Recomendado)", static_cast<int>(QualityPreset::Standard));
    m_qualityCombo->addItem("Alta Calidad (CRF 18 / Master)", static_cast<int>(QualityPreset::High));
    m_qualityCombo->addItem("Tamaño Compacto (CRF 28)", static_cast<int>(QualityPreset::Low));
    m_qualityCombo->addItem("Personalizada (CRF)...", static_cast<int>(QualityPreset::Custom));
    m_qualityCombo->setStyleSheet("background-color: #0d1117; color: #c9d1d9; border: 1px solid #30363d; border-radius: 4px; padding: 5px;");
    connect(m_qualityCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ExportDialog::onQualityChanged);
    qualityLayout->addWidget(m_qualityCombo, 1);

    m_crfWidget = new QWidget(m_videoSectionWidget);
    QHBoxLayout *crfHBox = new QHBoxLayout(m_crfWidget);
    crfHBox->setContentsMargins(0, 0, 0, 0);
    crfHBox->setSpacing(4);
    crfHBox->addWidget(new QLabel("CRF:", m_crfWidget));
    m_crfSpin = new QSpinBox(m_crfWidget);
    m_crfSpin->setRange(1, 51);
    m_crfSpin->setValue(23);
    m_crfSpin->setStyleSheet("background-color: #0d1117; color: #c9d1d9; border: 1px solid #30363d; border-radius: 4px; padding: 3px;");
    crfHBox->addWidget(m_crfSpin);
    m_crfWidget->setVisible(false);
    qualityLayout->addWidget(m_crfWidget);

    videoForm->addRow("Calidad de video:", qualityLayout);
    form->addRow(m_videoSectionWidget);

    // Audio Section
    m_audioSectionWidget = new QWidget(this);
    QFormLayout *audioForm = new QFormLayout(m_audioSectionWidget);
    audioForm->setContentsMargins(0, 0, 0, 0);
    audioForm->setSpacing(8);

    m_audioCodecCombo = new QComboBox(m_audioSectionWidget);
    m_audioCodecCombo->setStyleSheet("background-color: #0d1117; color: #c9d1d9; border: 1px solid #30363d; border-radius: 4px; padding: 5px;");
    audioForm->addRow("Códec de audio:", m_audioCodecCombo);

    m_audioBitrateCombo = new QComboBox(m_audioSectionWidget);
    m_audioBitrateCombo->addItem("192 kbps (Alta calidad - Recomendado)", 192);
    m_audioBitrateCombo->addItem("128 kbps (Estándar)", 128);
    m_audioBitrateCombo->addItem("256 kbps (Muy alta calidad)", 256);
    m_audioBitrateCombo->addItem("320 kbps (Máxima fidelidad)", 320);
    m_audioBitrateCombo->setStyleSheet("background-color: #0d1117; color: #c9d1d9; border: 1px solid #30363d; border-radius: 4px; padding: 5px;");
    audioForm->addRow("Tasa de bits de audio:", m_audioBitrateCombo);
    form->addRow(m_audioSectionWidget);

    // Duration Mode
    double totalSec = m_model ? (m_model->totalDurationMs() / 1000.0) : 10.0;
    double playheadSec = m_playheadPositionMs > 0 ? (m_playheadPositionMs / 1000.0) : 0.0;

    m_durationModeCombo = new QComboBox(this);
    m_durationModeCombo->addItem(QString("Toda la línea de tiempo (%1)").arg(formatExportTime(totalSec)), 0);
    m_durationModeCombo->addItem("Duración personalizada...", 1);
    if (playheadSec > 0.1) {
        m_durationModeCombo->addItem(QString("Hasta el cabezal (%1)").arg(formatExportTime(playheadSec)), 2);
    }
    m_durationModeCombo->setStyleSheet("background-color: #0d1117; color: #c9d1d9; border: 1px solid #30363d; border-radius: 4px; padding: 5px;");
    connect(m_durationModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ExportDialog::onDurationModeChanged);
    form->addRow("Rango de exportación:", m_durationModeCombo);

    // Custom Duration Row
    m_customDurationWidget = new QWidget(this);
    QVBoxLayout *customVBox = new QVBoxLayout(m_customDurationWidget);
    customVBox->setContentsMargins(0, 0, 0, 0);
    customVBox->setSpacing(6);

    QHBoxLayout *customHBox = new QHBoxLayout();
    customHBox->setSpacing(6);

    customHBox->addWidget(new QLabel("Duración:", m_customDurationWidget));
    m_exportDurationSpin = new QDoubleSpinBox(m_customDurationWidget);
    m_exportDurationSpin->setRange(0.5, 7200.0);
    m_exportDurationSpin->setSingleStep(1.0);
    m_exportDurationSpin->setDecimals(1);
    m_exportDurationSpin->setValue(totalSec);
    m_exportDurationSpin->setSuffix(" s");
    m_exportDurationSpin->setStyleSheet("background-color: #0d1117; color: #c9d1d9; border: 1px solid #30363d; border-radius: 4px; padding: 4px;");
    connect(m_exportDurationSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ExportDialog::updateDurationSummary);
    customHBox->addWidget(m_exportDurationSpin);

    customHBox->addWidget(new QLabel("Inicio:", m_customDurationWidget));
    m_exportStartSpin = new QDoubleSpinBox(m_customDurationWidget);
    m_exportStartSpin->setRange(0.0, 7200.0);
    m_exportStartSpin->setSingleStep(1.0);
    m_exportStartSpin->setDecimals(1);
    m_exportStartSpin->setValue(0.0);
    m_exportStartSpin->setSuffix(" s");
    m_exportStartSpin->setStyleSheet("background-color: #0d1117; color: #c9d1d9; border: 1px solid #30363d; border-radius: 4px; padding: 4px;");
    connect(m_exportStartSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ExportDialog::updateDurationSummary);
    customHBox->addWidget(m_exportStartSpin);

    auto addQuickBtn = [&](const QString &label, double sec) {
        QPushButton *btn = new QPushButton(label, m_customDurationWidget);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet("QPushButton { background-color: #21262d; color: #c9d1d9; border: 1px solid #30363d; border-radius: 3px; padding: 3px 6px; font-size: 11px; } QPushButton:hover { background-color: #30363d; color: white; }");
        connect(btn, &QPushButton::clicked, this, [this, sec]() {
            m_exportDurationSpin->setValue(sec);
        });
        customHBox->addWidget(btn);
    };
    addQuickBtn("5s", 5.0);
    addQuickBtn("10s", 10.0);
    addQuickBtn("30s", 30.0);
    addQuickBtn("1m", 60.0);

    customVBox->addLayout(customHBox);
    m_customDurationWidget->setVisible(false);
    form->addRow("", m_customDurationWidget);

    m_durationSummaryLabel = new QLabel(this);
    m_durationSummaryLabel->setStyleSheet("color: #58a6ff; font-size: 11px; font-weight: bold; background-color: #0d1117; border: 1px solid #30363d; border-radius: 4px; padding: 6px;");
    m_durationSummaryLabel->setWordWrap(true);
    form->addRow("Resumen:", m_durationSummaryLabel);

    mainLayout->addLayout(form);

    // Progress Bar
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setStyleSheet(
        "QProgressBar {"
        "  background-color: #0d1117; border: 1px solid #30363d; border-radius: 4px; text-align: center; color: white;"
        "}"
        "QProgressBar::chunk {"
        "  background-color: #238636; border-radius: 3px;"
        "}"
    );
    mainLayout->addWidget(m_progressBar);

    m_statusLabel = new QLabel("Listo para exportar.", this);
    m_statusLabel->setStyleSheet("color: #8b949e; font-size: 11px;");
    mainLayout->addWidget(m_statusLabel);

    // Buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    m_cancelBtn = new QPushButton("Cerrar", this);
    m_cancelBtn->setStyleSheet("background-color: #21262d; color: #c9d1d9; border: 1px solid #30363d; border-radius: 4px; padding: 6px 14px;");
    connect(m_cancelBtn, &QPushButton::clicked, this, &ExportDialog::cancelExport);
    btnLayout->addWidget(m_cancelBtn);

    m_exportBtn = new QPushButton("Exportar Medios", this);
    m_exportBtn->setCursor(Qt::PointingHandCursor);
    m_exportBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #238636; color: white; border-radius: 4px; padding: 6px 18px; font-weight: bold;"
        "}"
        "QPushButton:hover { background-color: #2ea043; }"
        "QPushButton:pressed { background-color: #1a6b2c; }"
    );
    connect(m_exportBtn, &QPushButton::clicked, this, &ExportDialog::startExport);
    btnLayout->addWidget(m_exportBtn);

    mainLayout->addLayout(btnLayout);

    // Initialize codec lists for default container (MP4)
    populateVideoCodecs(ExportContainer::MP4);
    populateAudioCodecs(ExportContainer::MP4);
    updateDurationSummary();
}

void ExportDialog::onContainerChanged(int index)
{
    Q_UNUSED(index);
    ExportContainer container = static_cast<ExportContainer>(m_containerCombo->currentData().toInt());
    populateVideoCodecs(container);
    populateAudioCodecs(container);
    updateFormVisibility();
    updateOutputExtension(container);
    updateDurationSummary();
}

void ExportDialog::onVideoCodecChanged(int index)
{
    Q_UNUSED(index);
    VideoCodec vcodec = static_cast<VideoCodec>(m_videoCodecCombo->currentData().toInt());
#if defined(Q_OS_MACOS)
    bool isHwSupported = (vcodec == VideoCodec::H264 || vcodec == VideoCodec::H265_HEVC || vcodec == VideoCodec::ProRes || vcodec == VideoCodec::Auto);
#else
    bool isHwSupported = (vcodec == VideoCodec::H264 || vcodec == VideoCodec::H265_HEVC || vcodec == VideoCodec::Auto);
#endif
    m_hwAccelCheck->setVisible(isHwSupported);
    updateDurationSummary();
}

void ExportDialog::onQualityChanged(int index)
{
    Q_UNUSED(index);
    QualityPreset q = static_cast<QualityPreset>(m_qualityCombo->currentData().toInt());
    m_crfWidget->setVisible(q == QualityPreset::Custom);
    updateDurationSummary();
}

void ExportDialog::updateOutputExtension(ExportContainer container)
{
    QString currentPath = m_pathEdit->text();
    QString ext = VideoExporter::defaultExtension(container);
    QFileInfo fi(currentPath);
    QString baseName = fi.completeBaseName();
    if (baseName.isEmpty()) baseName = "proyecto_editado";
    QString newPath = fi.dir().filePath(baseName + "." + ext);
    m_pathEdit->setText(newPath);
}

void ExportDialog::updateFormVisibility()
{
    ExportContainer container = static_cast<ExportContainer>(m_containerCombo->currentData().toInt());
    bool isAudioOnly = (container == ExportContainer::MP3 || container == ExportContainer::WAV ||
                        container == ExportContainer::AAC || container == ExportContainer::FLAC ||
                        container == ExportContainer::OGG);
    bool isGif = (container == ExportContainer::GIF);

    m_videoSectionWidget->setVisible(!isAudioOnly);
    m_audioSectionWidget->setVisible(!isGif);

    if (isAudioOnly) {
        m_exportBtn->setText("Exportar Pista de Audio");
    } else if (isGif) {
        m_exportBtn->setText("Exportar GIF Animado");
    } else {
        m_exportBtn->setText("Exportar Video");
    }
}

void ExportDialog::populateVideoCodecs(ExportContainer container)
{
    m_videoCodecCombo->blockSignals(true);
    m_videoCodecCombo->clear();

    switch (container) {
    case ExportContainer::MP4:
        m_videoCodecCombo->addItem("H.264 / AVC (Recomendado - Máxima compatibilidad)", static_cast<int>(VideoCodec::H264));
        m_videoCodecCombo->addItem("H.265 / HEVC (Alta compresión / 4K)", static_cast<int>(VideoCodec::H265_HEVC));
        m_videoCodecCombo->addItem("MPEG-4 Part 2", static_cast<int>(VideoCodec::MPEG4));
        m_videoCodecCombo->addItem("AOMedia AV1 (Eficiencia de nueva generación)", static_cast<int>(VideoCodec::AV1));
        break;
    case ExportContainer::MOV:
        m_videoCodecCombo->addItem("Apple ProRes (Edición profesional de alta fidelidad)", static_cast<int>(VideoCodec::ProRes));
        m_videoCodecCombo->addItem("H.264 / AVC", static_cast<int>(VideoCodec::H264));
        m_videoCodecCombo->addItem("H.265 / HEVC", static_cast<int>(VideoCodec::H265_HEVC));
        break;
    case ExportContainer::MKV:
        m_videoCodecCombo->addItem("H.264 / AVC (Universal)", static_cast<int>(VideoCodec::H264));
        m_videoCodecCombo->addItem("H.265 / HEVC", static_cast<int>(VideoCodec::H265_HEVC));
        m_videoCodecCombo->addItem("Google VP9", static_cast<int>(VideoCodec::VP9));
        m_videoCodecCombo->addItem("Apple ProRes", static_cast<int>(VideoCodec::ProRes));
        m_videoCodecCombo->addItem("AOMedia AV1", static_cast<int>(VideoCodec::AV1));
        break;
    case ExportContainer::WebM:
        m_videoCodecCombo->addItem("Google VP9 (Recomendado para Web)", static_cast<int>(VideoCodec::VP9));
        m_videoCodecCombo->addItem("Google VP8 (Estándar WebM)", static_cast<int>(VideoCodec::VP8));
        break;
    case ExportContainer::AVI:
        m_videoCodecCombo->addItem("MPEG-4 Part 2 (Tradicional AVI)", static_cast<int>(VideoCodec::MPEG4));
        m_videoCodecCombo->addItem("H.264 / AVC", static_cast<int>(VideoCodec::H264));
        break;
    case ExportContainer::GIF:
        m_videoCodecCombo->addItem("GIF Animado con Paleta Óptima (PaletteGen)", static_cast<int>(VideoCodec::GIF));
        break;
    default:
        m_videoCodecCombo->addItem("No aplica (Solo Audio)", static_cast<int>(VideoCodec::Auto));
        break;
    }

    m_videoCodecCombo->blockSignals(false);
    onVideoCodecChanged(0);
}

void ExportDialog::populateAudioCodecs(ExportContainer container)
{
    m_audioCodecCombo->blockSignals(true);
    m_audioCodecCombo->clear();

    switch (container) {
    case ExportContainer::MP4:
        m_audioCodecCombo->addItem("AAC (Recomendado)", static_cast<int>(AudioCodec::AAC));
        m_audioCodecCombo->addItem("MP3", static_cast<int>(AudioCodec::MP3));
        m_audioCodecCombo->addItem("Sin Audio (Mudo)", static_cast<int>(AudioCodec::None));
        m_audioBitrateCombo->setVisible(true);
        break;
    case ExportContainer::MOV:
        m_audioCodecCombo->addItem("PCM 16-bit (Sin compresión)", static_cast<int>(AudioCodec::PCM_16));
        m_audioCodecCombo->addItem("AAC", static_cast<int>(AudioCodec::AAC));
        m_audioCodecCombo->addItem("Sin Audio (Mudo)", static_cast<int>(AudioCodec::None));
        m_audioBitrateCombo->setVisible(false);
        break;
    case ExportContainer::MKV:
        m_audioCodecCombo->addItem("AAC (Recomendado)", static_cast<int>(AudioCodec::AAC));
        m_audioCodecCombo->addItem("Opus (Alta fidelidad moderna)", static_cast<int>(AudioCodec::Opus));
        m_audioCodecCombo->addItem("FLAC (Lossless)", static_cast<int>(AudioCodec::FLAC));
        m_audioCodecCombo->addItem("MP3", static_cast<int>(AudioCodec::MP3));
        m_audioCodecCombo->addItem("PCM 16-bit", static_cast<int>(AudioCodec::PCM_16));
        m_audioCodecCombo->addItem("Sin Audio (Mudo)", static_cast<int>(AudioCodec::None));
        m_audioBitrateCombo->setVisible(true);
        break;
    case ExportContainer::WebM:
        m_audioCodecCombo->addItem("Opus (Recomendado WebM)", static_cast<int>(AudioCodec::Opus));
        m_audioCodecCombo->addItem("Vorbis", static_cast<int>(AudioCodec::Vorbis));
        m_audioCodecCombo->addItem("Sin Audio (Mudo)", static_cast<int>(AudioCodec::None));
        m_audioBitrateCombo->setVisible(true);
        break;
    case ExportContainer::AVI:
        m_audioCodecCombo->addItem("MP3 (Recomendado)", static_cast<int>(AudioCodec::MP3));
        m_audioCodecCombo->addItem("PCM 16-bit", static_cast<int>(AudioCodec::PCM_16));
        m_audioCodecCombo->addItem("Sin Audio (Mudo)", static_cast<int>(AudioCodec::None));
        m_audioBitrateCombo->setVisible(true);
        break;
    case ExportContainer::MP3:
        m_audioCodecCombo->addItem("MP3 (MPEG Audio Layer 3)", static_cast<int>(AudioCodec::MP3));
        m_audioBitrateCombo->setVisible(true);
        break;
    case ExportContainer::WAV:
        m_audioCodecCombo->addItem("PCM 16-bit Stereo (Sin pérdida)", static_cast<int>(AudioCodec::PCM_16));
        m_audioBitrateCombo->setVisible(false);
        break;
    case ExportContainer::AAC:
        m_audioCodecCombo->addItem("AAC (Advanced Audio Coding)", static_cast<int>(AudioCodec::AAC));
        m_audioBitrateCombo->setVisible(true);
        break;
    case ExportContainer::FLAC:
        m_audioCodecCombo->addItem("FLAC (Free Lossless Audio Codec)", static_cast<int>(AudioCodec::FLAC));
        m_audioBitrateCombo->setVisible(false);
        break;
    case ExportContainer::OGG:
        m_audioCodecCombo->addItem("Opus (Ogg Container)", static_cast<int>(AudioCodec::Opus));
        m_audioBitrateCombo->setVisible(true);
        break;
    case ExportContainer::GIF:
    default:
        m_audioCodecCombo->addItem("Sin Audio", static_cast<int>(AudioCodec::None));
        m_audioBitrateCombo->setVisible(false);
        break;
    }

    m_audioCodecCombo->blockSignals(false);
}

void ExportDialog::onDurationModeChanged(int index)
{
    m_customDurationWidget->setVisible(index == 1);
    updateDurationSummary();
}

void ExportDialog::updateDurationSummary()
{
    if (!m_durationSummaryLabel) return;

    int mode = m_durationModeCombo ? m_durationModeCombo->currentIndex() : 0;
    double durSec = 10.0;
    double startSec = 0.0;

    if (mode == 0) {
        durSec = m_model ? (m_model->totalDurationMs() / 1000.0) : 10.0;
    } else if (mode == 1) {
        durSec = m_exportDurationSpin ? m_exportDurationSpin->value() : 10.0;
        startSec = m_exportStartSpin ? m_exportStartSpin->value() : 0.0;
    } else if (mode == 2) {
        durSec = qMax(0.5, m_playheadPositionMs / 1000.0);
    }

    ExportContainer container = static_cast<ExportContainer>(m_containerCombo ? m_containerCombo->currentData().toInt() : 0);
    bool isAudioOnly = (container == ExportContainer::MP3 || container == ExportContainer::WAV ||
                        container == ExportContainer::AAC || container == ExportContainer::FLAC ||
                        container == ExportContainer::OGG);
    bool isGif = (container == ExportContainer::GIF);

    int fps = m_fpsCombo ? m_fpsCombo->currentData().toInt() : 30;
    int totalFrames = qMax(1, static_cast<int>(std::round(durSec * fps)));

    QString summary;
    if (isAudioOnly) {
        QString audioCodecStr = m_audioCodecCombo ? m_audioCodecCombo->currentText() : "Audio";
        summary = QString("🎵 Audio: %1 • Duración: %2 (%3 s)")
                      .arg(audioCodecStr)
                      .arg(formatExportTime(durSec))
                      .arg(durSec, 0, 'f', 1);
    } else if (isGif) {
        QSize res = m_resCombo ? m_resCombo->currentData().toSize() : QSize(640, 360);
        summary = QString("🎞️ GIF: %1x%2 @ %3 fps • %4 fotogramas • %5 (%6 s)")
                      .arg(res.width()).arg(res.height())
                      .arg(fps)
                      .arg(totalFrames)
                      .arg(formatExportTime(durSec))
                      .arg(durSec, 0, 'f', 1);
    } else {
        QSize res = m_resCombo ? m_resCombo->currentData().toSize() : QSize(1920, 1080);
        QString vcodecStr = m_videoCodecCombo ? m_videoCodecCombo->currentText() : "H.264";
        summary = QString("🎬 %1 • %2x%3 @ %4 fps • %5 fotogramas • %6 (%7 s)")
                      .arg(vcodecStr.split(" (").first())
                      .arg(res.width()).arg(res.height())
                      .arg(fps)
                      .arg(totalFrames)
                      .arg(formatExportTime(durSec))
                      .arg(durSec, 0, 'f', 1);
    }

    if (startSec > 0.05) {
        summary += QString(" • Inicio: %1s").arg(startSec, 0, 'f', 1);
    }

    m_durationSummaryLabel->setText(summary);
}

void ExportDialog::browseOutputFile()
{
    ExportContainer container = static_cast<ExportContainer>(m_containerCombo->currentData().toInt());
    QString ext = VideoExporter::defaultExtension(container);
    QString filterName;

    switch (container) {
    case ExportContainer::MP4: filterName = "Video MP4 (*.mp4)"; break;
    case ExportContainer::MOV: filterName = "QuickTime Video (*.mov)"; break;
    case ExportContainer::MKV: filterName = "Matroska Video (*.mkv)"; break;
    case ExportContainer::WebM: filterName = "WebM Video (*.webm)"; break;
    case ExportContainer::AVI: filterName = "AVI Video (*.avi)"; break;
    case ExportContainer::GIF: filterName = "GIF Animado (*.gif)"; break;
    case ExportContainer::MP3: filterName = "Audio MP3 (*.mp3)"; break;
    case ExportContainer::WAV: filterName = "Audio WAV (*.wav)"; break;
    case ExportContainer::AAC: filterName = "Audio AAC (*.m4a *.aac)"; break;
    case ExportContainer::FLAC: filterName = "Audio FLAC (*.flac)"; break;
    case ExportContainer::OGG: filterName = "Audio OGG (*.ogg)"; break;
    }

    QString path = QFileDialog::getSaveFileName(this, "Guardar Archivo de Medios", m_pathEdit->text(), filterName + ";;Todos los archivos (*.*)");
    if (!path.isEmpty()) {
        QString expectedExt = "." + ext;
        if (!path.endsWith(expectedExt, Qt::CaseInsensitive)) {
            path += expectedExt;
        }
        m_pathEdit->setText(path);
    }
}

void ExportDialog::startExport()
{
    QString outPath = m_pathEdit->text().trimmed();
    if (outPath.isEmpty()) {
        QMessageBox::warning(this, "Ruta requerida", "Por favor especifica una ruta válida de archivo.");
        return;
    }

    ExportConfig config;
    config.outputPath = outPath;
    config.container = static_cast<ExportContainer>(m_containerCombo->currentData().toInt());
    config.videoCodec = static_cast<VideoCodec>(m_videoCodecCombo->currentData().toInt());
    config.audioCodec = static_cast<AudioCodec>(m_audioCodecCombo->currentData().toInt());
    config.resolution = m_resCombo->currentData().toSize();
    config.fps = m_fpsCombo->currentData().toInt();
    config.useHardwareAcceleration = m_hwAccelCheck->isChecked();
    config.quality = static_cast<QualityPreset>(m_qualityCombo->currentData().toInt());
    config.crf = m_crfSpin->value();
    config.audioBitrateKbps = m_audioBitrateCombo->currentData().toInt();

    // Duration and start offset
    int durMode = m_durationModeCombo->currentIndex();
    if (durMode == 0) {
        config.durationMs = m_model ? m_model->totalDurationMs() : 10000;
        config.startMs = 0;
    } else if (durMode == 1) {
        config.durationMs = static_cast<qint64>(m_exportDurationSpin->value() * 1000);
        config.startMs = static_cast<qint64>(m_exportStartSpin->value() * 1000);
    } else if (durMode == 2) {
        config.durationMs = qMax<qint64>(500, m_playheadPositionMs);
        config.startMs = 0;
    }

    m_finalOutputPath = outPath;
    m_progressBar->setValue(0);
    m_statusLabel->setText("Iniciando exportación...");
    m_exportBtn->setEnabled(false);
    m_containerCombo->setEnabled(false);
    m_resCombo->setEnabled(false);
    m_fpsCombo->setEnabled(false);
    m_videoCodecCombo->setEnabled(false);
    m_audioCodecCombo->setEnabled(false);
    m_cancelBtn->setText("Cancelar");

    m_exporter->startExport(config);
}

void ExportDialog::cancelExport()
{
    if (m_exporter->isExporting()) {
        m_exporter->cancelExport();
        m_statusLabel->setText("Cancelando exportación...");
    } else {
        reject();
    }
}

void ExportDialog::onProgressUpdated(int percent, const QString &statusText)
{
    m_progressBar->setValue(percent);
    m_statusLabel->setText(statusText);
}

void ExportDialog::onExportFinished(bool success, const QString &outputPath, const QString &errorMessage)
{
    m_exportBtn->setEnabled(true);
    m_containerCombo->setEnabled(true);
    m_resCombo->setEnabled(true);
    m_fpsCombo->setEnabled(true);
    m_videoCodecCombo->setEnabled(true);
    m_audioCodecCombo->setEnabled(true);
    m_cancelBtn->setText("Cerrar");

    if (success) {
        m_statusLabel->setText(QString("¡Exportación completada!: %1").arg(outputPath));
        QMessageBox::StandardButton reply = QMessageBox::information(
            this,
            "Exportación Finalizada",
            QString("El archivo se ha exportado con éxito:\n\n%1\n\n¿Deseas abrir la carpeta contenedora?").arg(outputPath),
            QMessageBox::Yes | QMessageBox::No
        );

        if (reply == QMessageBox::Yes) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(outputPath).absolutePath()));
        }
    } else {
        m_statusLabel->setText("Error durante la exportación.");
        QMessageBox::critical(this, "Error de Exportación", errorMessage);
    }
}
