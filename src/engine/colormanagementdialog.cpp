#include "colormanagementdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>

ColorManagementDialog::ColorManagementDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("🎨 Gestión de Color del Proyecto (Color Management & HDR)");
    resize(560, 420);
    setStyleSheet(
        "QDialog { background-color: #0d1117; color: #c9d1d9; }"
        "QGroupBox { border: 1px solid #30363d; border-radius: 6px; margin-top: 10px; font-weight: bold; color: #58a6ff; padding: 12px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }"
        "QLabel { color: #c9d1d9; font-size: 11px; }"
        "QComboBox { background-color: #21262d; color: #c9d1d9; border: 1px solid #30363d; border-radius: 4px; padding: 5px 10px; }"
        "QComboBox QAbstractItemView { background-color: #161b22; color: #c9d1d9; selection-background-color: #1f6feb; }"
        "QCheckBox { color: #c9d1d9; font-size: 11px; font-weight: bold; }"
        "QPushButton { background-color: #21262d; color: #c9d1d9; border: 1px solid #30363d; border-radius: 4px; padding: 6px 14px; font-weight: bold; font-size: 11px; }"
        "QPushButton:hover { background-color: #30363d; color: white; }"
    );

    setupUi();
}

void ColorManagementDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    QGroupBox *settingsGroup = new QGroupBox("Espacio de Color y Curva de Transferencia", this);
    QGridLayout *gridLayout = new QGridLayout(settingsGroup);
    gridLayout->setSpacing(10);

    gridLayout->addWidget(new QLabel("Espacio de Trabajo (Working Gamut):", this), 0, 0);
    m_colorSpaceCombo = new QComboBox(this);
    m_colorSpaceCombo->addItem("Rec.709 (Estándar HDTV / SDR)", static_cast<int>(ColorSpaceStandard::Rec709));
    m_colorSpaceCombo->addItem("sRGB (Web & Pantallas de Computadora)", static_cast<int>(ColorSpaceStandard::sRGB));
    m_colorSpaceCombo->addItem("Display P3 (Apple Silicon / Wide Gamut)", static_cast<int>(ColorSpaceStandard::DisplayP3));
    m_colorSpaceCombo->addItem("DCI-P3 (Cine Digital / DCI)", static_cast<int>(ColorSpaceStandard::DciP3));
    m_colorSpaceCombo->addItem("Rec.2020 (Ultra HD 4K/8K • HDR)", static_cast<int>(ColorSpaceStandard::Rec2020));

    // Select current
    int csIdx = m_colorSpaceCombo->findData(static_cast<int>(ColorSpaceManager::instance().projectWorkingSpace()));
    if (csIdx >= 0) m_colorSpaceCombo->setCurrentIndex(csIdx);
    connect(m_colorSpaceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ColorManagementDialog::onColorSpaceChanged);
    gridLayout->addWidget(m_colorSpaceCombo, 0, 1);

    gridLayout->addWidget(new QLabel("Función de Transferencia (EOTF / Gamma):", this), 1, 0);
    m_transferCombo = new QComboBox(this);
    m_transferCombo->addItem("BT.1886 (Gamma 2.4 - Referencia de Estudio)", static_cast<int>(TransferFunction::Gamma24));
    m_transferCombo->addItem("Gamma 2.2 (Estándar PC)", static_cast<int>(TransferFunction::Gamma22));
    m_transferCombo->addItem("sRGB (Estándar Web)", static_cast<int>(TransferFunction::sRGB));
    m_transferCombo->addItem("PQ / SMPTE ST 2084 (HDR10 1000-10000 nits)", static_cast<int>(TransferFunction::PQ));
    m_transferCombo->addItem("HLG / ARIB STD-B67 (Hybrid Log-Gamma)", static_cast<int>(TransferFunction::HLG));
    m_transferCombo->addItem("Lineal (Linear Light)", static_cast<int>(TransferFunction::Linear));

    int tfIdx = m_transferCombo->findData(static_cast<int>(ColorSpaceManager::instance().projectTransferFunction()));
    if (tfIdx >= 0) m_transferCombo->setCurrentIndex(tfIdx);
    connect(m_transferCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ColorManagementDialog::onTransferFunctionChanged);
    gridLayout->addWidget(m_transferCombo, 1, 1);

    m_acesCheck = new QCheckBox("Habilitar Mapeo Tonal Fílmico ACES (ACES Filmic Tone Mapping)", this);
    m_acesCheck->setChecked(true);
    connect(m_acesCheck, &QCheckBox::toggled, this, &ColorManagementDialog::onAcesToggled);
    gridLayout->addWidget(m_acesCheck, 2, 0, 1, 2);

    mainLayout->addWidget(settingsGroup);

    // Technical specifications info box
    QGroupBox *infoGroup = new QGroupBox("Especificaciones Técnicas del Gamut", this);
    QVBoxLayout *infoLayout = new QVBoxLayout(infoGroup);

    m_specsLabel = new QLabel(this);
    m_specsLabel->setStyleSheet("color: #8b949e; line-height: 1.4;");
    infoLayout->addWidget(m_specsLabel);

    mainLayout->addWidget(infoGroup);

    // Buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    QPushButton *cancelBtn = new QPushButton("Cancelar", this);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(cancelBtn);

    QPushButton *applyBtn = new QPushButton("Aplicar", this);
    applyBtn->setStyleSheet("background-color: #238636; color: white; border: none; padding: 6px 20px; font-weight: bold; border-radius: 4px;");
    connect(applyBtn, &QPushButton::clicked, this, &ColorManagementDialog::onApplyClicked);
    btnLayout->addWidget(applyBtn);

    mainLayout->addLayout(btnLayout);

    updateSpecs();
}

void ColorManagementDialog::updateSpecs()
{
    ColorSpaceStandard cs = static_cast<ColorSpaceStandard>(m_colorSpaceCombo->currentData().toInt());
    TransferFunction tf = static_cast<TransferFunction>(m_transferCombo->currentData().toInt());

    QString csName = colorSpaceStandardToString(cs);
    QString tfName = transferFunctionToString(tf);
    bool isHdr = (tf == TransferFunction::PQ || tf == TransferFunction::HLG);

    QString text = QString(
        "<b>Espacio de Color:</b> %1<br>"
        "<b>Curva EOTF:</b> %2<br>"
        "<b>Rango Dinámico:</b> %3<br>"
        "<b>Punto Blanco:</b> CIE D65 (0.3127, 0.3290)<br>"
        "<b>Cobertura de Color:</b> %4"
    ).arg(csName)
     .arg(tfName)
     .arg(isHdr ? "<span style='color:#f0883e;'>HDR (Alto Rango Dinámico)</span>" : "<span style='color:#2ea043;'>SDR (Rango Dinámico Estándar)</span>")
     .arg(cs == ColorSpaceStandard::Rec2020 ? "99.9% Rec.2020 Gamut Extendido" : (cs == ColorSpaceStandard::DisplayP3 ? "100% DCI-P3 Gamut Apple" : "100% Rec.709 Broadcast"));

    m_specsLabel->setText(text);
}

void ColorManagementDialog::onColorSpaceChanged(int index)
{
    Q_UNUSED(index);
    updateSpecs();
}

void ColorManagementDialog::onTransferFunctionChanged(int index)
{
    Q_UNUSED(index);
    updateSpecs();
}

void ColorManagementDialog::onAcesToggled(bool checked)
{
    Q_UNUSED(checked);
}

void ColorManagementDialog::onApplyClicked()
{
    ColorSpaceStandard cs = static_cast<ColorSpaceStandard>(m_colorSpaceCombo->currentData().toInt());
    TransferFunction tf = static_cast<TransferFunction>(m_transferCombo->currentData().toInt());

    ColorSpaceManager::instance().setProjectWorkingSpace(cs);
    ColorSpaceManager::instance().setProjectTransferFunction(tf);

    accept();
}
