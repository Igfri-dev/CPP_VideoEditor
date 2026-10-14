#include "audiomixerdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>

AudioMixerDialog::AudioMixerDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("🎛️ Mezclador de Audio & Master Suite (EQ + Compresor + Limitador)");
    resize(740, 560);
    setStyleSheet(
        "QDialog { background-color: #0d1117; color: #c9d1d9; }"
        "QGroupBox { border: 1px solid #30363d; border-radius: 6px; margin-top: 10px; font-weight: bold; color: #58a6ff; padding: 12px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }"
        "QLabel { color: #c9d1d9; font-size: 11px; }"
        "QSlider::groove:horizontal { height: 4px; background: #21262d; border-radius: 2px; }"
        "QSlider::sub-page:horizontal { background: #238636; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: #58a6ff; width: 14px; margin: -5px 0; border-radius: 7px; }"
        "QPushButton { background-color: #21262d; color: #c9d1d9; border: 1px solid #30363d; border-radius: 4px; padding: 6px 14px; font-weight: bold; font-size: 11px; }"
        "QPushButton:hover { background-color: #30363d; color: white; }"
    );

    setupUi();
}

void AudioMixerDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    // Preset & Actions Bar
    QHBoxLayout *topBar = new QHBoxLayout();
    QLabel *presetLbl = new QLabel("Presets de Mastering:", this);
    presetLbl->setStyleSheet("font-weight: bold; color: #e3b341;");
    topBar->addWidget(presetLbl);

    m_presetCombo = new QComboBox(this);
    m_presetCombo->addItem("Plano / Transparente (Flat)");
    m_presetCombo->addItem("Podcast & Voz Cristalina");
    m_presetCombo->addItem("Punchy Master (Pegada & Dinámica)");
    m_presetCombo->addItem("Broadcast Estándar (-0.2 dBFS True Peak)");
    m_presetCombo->setStyleSheet(
        "QComboBox { background-color: #21262d; color: #c9d1d9; border: 1px solid #30363d; border-radius: 4px; padding: 4px 8px; }"
    );
    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AudioMixerDialog::onPresetSelected);
    topBar->addWidget(m_presetCombo);

    topBar->addStretch();
    QPushButton *resetBtn = new QPushButton("Restablecer", this);
    connect(resetBtn, &QPushButton::clicked, this, &AudioMixerDialog::onResetDefaultsClicked);
    topBar->addWidget(resetBtn);

    mainLayout->addLayout(topBar);

    // 1. Equalizador Paramétrico de 4 Bandas
    QGroupBox *eqGroup = new QGroupBox("1. Ecualizador Paramétrico (4 Bandas)", this);
    QGridLayout *eqLayout = new QGridLayout(eqGroup);
    eqLayout->setSpacing(8);

    auto makeEqSlider = [this](int minVal, int maxVal, int defaultVal) {
        QSlider *s = new QSlider(Qt::Horizontal, this);
        s->setRange(minVal, maxVal);
        s->setValue(defaultVal);
        return s;
    };

    eqLayout->addWidget(new QLabel("Graves / Low Shelf (80 Hz):", this), 0, 0);
    m_eqLowSlider = makeEqSlider(-15, 15, static_cast<int>(AudioMixerSuite::instance().masterEq().lowGainDb()));
    eqLayout->addWidget(m_eqLowSlider, 0, 1);
    m_eqLowLbl = new QLabel("0 dB", this);
    m_eqLowLbl->setFixedWidth(50);
    eqLayout->addWidget(m_eqLowLbl, 0, 2);
    connect(m_eqLowSlider, &QSlider::valueChanged, this, &AudioMixerDialog::onEqLowChanged);

    eqLayout->addWidget(new QLabel("Medios-Graves (300 Hz Bell):", this), 1, 0);
    m_eqMidLowSlider = makeEqSlider(-15, 15, static_cast<int>(AudioMixerSuite::instance().masterEq().lowMidGainDb()));
    eqLayout->addWidget(m_eqMidLowSlider, 1, 1);
    m_eqMidLowLbl = new QLabel("0 dB", this);
    m_eqMidLowLbl->setFixedWidth(50);
    eqLayout->addWidget(m_eqMidLowLbl, 1, 2);
    connect(m_eqMidLowSlider, &QSlider::valueChanged, this, &AudioMixerDialog::onEqMidLowChanged);

    eqLayout->addWidget(new QLabel("Medios-Agudos (2.5 kHz Bell):", this), 2, 0);
    m_eqMidHighSlider = makeEqSlider(-15, 15, static_cast<int>(AudioMixerSuite::instance().masterEq().highMidGainDb()));
    eqLayout->addWidget(m_eqMidHighSlider, 2, 1);
    m_eqMidHighLbl = new QLabel("0 dB", this);
    m_eqMidHighLbl->setFixedWidth(50);
    eqLayout->addWidget(m_eqMidHighLbl, 2, 2);
    connect(m_eqMidHighSlider, &QSlider::valueChanged, this, &AudioMixerDialog::onEqMidHighChanged);

    eqLayout->addWidget(new QLabel("Agudos / High Shelf (10 kHz):", this), 3, 0);
    m_eqHighSlider = makeEqSlider(-15, 15, static_cast<int>(AudioMixerSuite::instance().masterEq().highGainDb()));
    eqLayout->addWidget(m_eqHighSlider, 3, 1);
    m_eqHighLbl = new QLabel("0 dB", this);
    m_eqHighLbl->setFixedWidth(50);
    eqLayout->addWidget(m_eqHighLbl, 3, 2);
    connect(m_eqHighSlider, &QSlider::valueChanged, this, &AudioMixerDialog::onEqHighChanged);

    mainLayout->addWidget(eqGroup);

    // 2. Compresor de Audio
    QGroupBox *compGroup = new QGroupBox("2. Compresor Dinámico Master", this);
    QGridLayout *compLayout = new QGridLayout(compGroup);
    compLayout->setSpacing(8);

    compLayout->addWidget(new QLabel("Umbral (Threshold):", this), 0, 0);
    m_compThresholdSlider = makeEqSlider(-60, 0, static_cast<int>(AudioMixerSuite::instance().masterCompressor().thresholdDb()));
    compLayout->addWidget(m_compThresholdSlider, 0, 1);
    m_compThresholdLbl = new QLabel("-18 dB", this);
    m_compThresholdLbl->setFixedWidth(50);
    compLayout->addWidget(m_compThresholdLbl, 0, 2);
    connect(m_compThresholdSlider, &QSlider::valueChanged, this, &AudioMixerDialog::onCompThresholdChanged);

    compLayout->addWidget(new QLabel("Relación (Ratio):", this), 1, 0);
    m_compRatioSlider = makeEqSlider(1, 20, static_cast<int>(AudioMixerSuite::instance().masterCompressor().ratio()));
    compLayout->addWidget(m_compRatioSlider, 1, 1);
    m_compRatioLbl = new QLabel("4:1", this);
    m_compRatioLbl->setFixedWidth(50);
    compLayout->addWidget(m_compRatioLbl, 1, 2);
    connect(m_compRatioSlider, &QSlider::valueChanged, this, &AudioMixerDialog::onCompRatioChanged);

    compLayout->addWidget(new QLabel("Ataque (Attack ms):", this), 2, 0);
    m_compAttackSlider = makeEqSlider(1, 100, static_cast<int>(AudioMixerSuite::instance().masterCompressor().attackMs()));
    compLayout->addWidget(m_compAttackSlider, 2, 1);
    m_compAttackLbl = new QLabel("15 ms", this);
    m_compAttackLbl->setFixedWidth(50);
    compLayout->addWidget(m_compAttackLbl, 2, 2);
    connect(m_compAttackSlider, &QSlider::valueChanged, this, &AudioMixerDialog::onCompAttackChanged);

    compLayout->addWidget(new QLabel("Liberación (Release ms):", this), 3, 0);
    m_compReleaseSlider = makeEqSlider(10, 500, static_cast<int>(AudioMixerSuite::instance().masterCompressor().releaseMs()));
    compLayout->addWidget(m_compReleaseSlider, 3, 1);
    m_compReleaseLbl = new QLabel("100 ms", this);
    m_compReleaseLbl->setFixedWidth(50);
    compLayout->addWidget(m_compReleaseLbl, 3, 2);
    connect(m_compReleaseSlider, &QSlider::valueChanged, this, &AudioMixerDialog::onCompReleaseChanged);

    compLayout->addWidget(new QLabel("Ganancia de Compensación (Makeup):", this), 4, 0);
    m_compMakeupSlider = makeEqSlider(0, 24, static_cast<int>(AudioMixerSuite::instance().masterCompressor().makeupGainDb()));
    compLayout->addWidget(m_compMakeupSlider, 4, 1);
    m_compMakeupLbl = new QLabel("0 dB", this);
    m_compMakeupLbl->setFixedWidth(50);
    compLayout->addWidget(m_compMakeupLbl, 4, 2);
    connect(m_compMakeupSlider, &QSlider::valueChanged, this, &AudioMixerDialog::onCompMakeupChanged);

    mainLayout->addWidget(compGroup);

    // 3. Limitador Brickwall
    QGroupBox *limGroup = new QGroupBox("3. Limitador Brickwall Master (Techo Pico Real)", this);
    QGridLayout *limLayout = new QGridLayout(limGroup);

    limLayout->addWidget(new QLabel("Techo Máximo (Ceiling dBFS):", this), 0, 0);
    m_limiterCeilingSlider = makeEqSlider(-120, 0, static_cast<int>(AudioMixerSuite::instance().masterLimiter().ceilingDb() * 10.0f));
    limLayout->addWidget(m_limiterCeilingSlider, 0, 1);
    m_limiterCeilingLbl = new QLabel("-0.2 dBFS", this);
    m_limiterCeilingLbl->setFixedWidth(65);
    limLayout->addWidget(m_limiterCeilingLbl, 0, 2);
    connect(m_limiterCeilingSlider, &QSlider::valueChanged, this, &AudioMixerDialog::onLimiterCeilingChanged);

    mainLayout->addWidget(limGroup);

    // Close button
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    QPushButton *closeBtn = new QPushButton("Aceptar", this);
    closeBtn->setStyleSheet("background-color: #238636; color: white; border: none; padding: 6px 20px; font-weight: bold; border-radius: 4px;");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(closeBtn);
    mainLayout->addLayout(btnLayout);

    updateLabels();
}

void AudioMixerDialog::updateLabels()
{
    m_eqLowLbl->setText(QString("%1 dB").arg(m_eqLowSlider->value()));
    m_eqMidLowLbl->setText(QString("%1 dB").arg(m_eqMidLowSlider->value()));
    m_eqMidHighLbl->setText(QString("%1 dB").arg(m_eqMidHighSlider->value()));
    m_eqHighLbl->setText(QString("%1 dB").arg(m_eqHighSlider->value()));

    m_compThresholdLbl->setText(QString("%1 dB").arg(m_compThresholdSlider->value()));
    m_compRatioLbl->setText(QString("%1:1").arg(m_compRatioSlider->value()));
    m_compAttackLbl->setText(QString("%1 ms").arg(m_compAttackSlider->value()));
    m_compReleaseLbl->setText(QString("%1 ms").arg(m_compReleaseSlider->value()));
    m_compMakeupLbl->setText(QString("+%1 dB").arg(m_compMakeupSlider->value()));

    m_limiterCeilingLbl->setText(QString("%1 dBFS").arg(m_limiterCeilingSlider->value() / 10.0f, 0, 'f', 1));
}

void AudioMixerDialog::onEqLowChanged(int value)
{
    AudioMixerSuite::instance().masterEq().setLowShelf(80.0f, static_cast<float>(value));
    updateLabels();
}

void AudioMixerDialog::onEqMidLowChanged(int value)
{
    AudioMixerSuite::instance().masterEq().setLowMid(300.0f, static_cast<float>(value));
    updateLabels();
}

void AudioMixerDialog::onEqMidHighChanged(int value)
{
    AudioMixerSuite::instance().masterEq().setHighMid(2500.0f, static_cast<float>(value));
    updateLabels();
}

void AudioMixerDialog::onEqHighChanged(int value)
{
    AudioMixerSuite::instance().masterEq().setHighShelf(10000.0f, static_cast<float>(value));
    updateLabels();
}

void AudioMixerDialog::onCompThresholdChanged(int value)
{
    AudioMixerSuite::instance().masterCompressor().setThresholdDb(static_cast<float>(value));
    updateLabels();
}

void AudioMixerDialog::onCompRatioChanged(int value)
{
    AudioMixerSuite::instance().masterCompressor().setRatio(static_cast<float>(value));
    updateLabels();
}

void AudioMixerDialog::onCompAttackChanged(int value)
{
    AudioMixerSuite::instance().masterCompressor().setAttackMs(static_cast<float>(value));
    updateLabels();
}

void AudioMixerDialog::onCompReleaseChanged(int value)
{
    AudioMixerSuite::instance().masterCompressor().setReleaseMs(static_cast<float>(value));
    updateLabels();
}

void AudioMixerDialog::onCompMakeupChanged(int value)
{
    AudioMixerSuite::instance().masterCompressor().setMakeupGainDb(static_cast<float>(value));
    updateLabels();
}

void AudioMixerDialog::onLimiterCeilingChanged(int value)
{
    AudioMixerSuite::instance().masterLimiter().setCeilingDb(static_cast<float>(value) / 10.0f);
    updateLabels();
}

void AudioMixerDialog::onMasterVolChanged(int value)
{
    Q_UNUSED(value);
}

void AudioMixerDialog::onPresetSelected(int index)
{
    if (index == 0) { // Flat
        m_eqLowSlider->setValue(0);
        m_eqMidLowSlider->setValue(0);
        m_eqMidHighSlider->setValue(0);
        m_eqHighSlider->setValue(0);
        m_compThresholdSlider->setValue(-18);
        m_compRatioSlider->setValue(4);
        m_compMakeupSlider->setValue(0);
        m_limiterCeilingSlider->setValue(-2);
    } else if (index == 1) { // Podcast
        m_eqLowSlider->setValue(-3);
        m_eqMidLowSlider->setValue(-2);
        m_eqMidHighSlider->setValue(4);
        m_eqHighSlider->setValue(3);
        m_compThresholdSlider->setValue(-22);
        m_compRatioSlider->setValue(5);
        m_compMakeupSlider->setValue(4);
        m_limiterCeilingSlider->setValue(-1);
    } else if (index == 2) { // Punchy Master
        m_eqLowSlider->setValue(4);
        m_eqMidLowSlider->setValue(-1);
        m_eqMidHighSlider->setValue(2);
        m_eqHighSlider->setValue(5);
        m_compThresholdSlider->setValue(-16);
        m_compRatioSlider->setValue(3);
        m_compMakeupSlider->setValue(3);
        m_limiterCeilingSlider->setValue(-2);
    } else if (index == 3) { // Broadcast
        m_eqLowSlider->setValue(0);
        m_eqMidLowSlider->setValue(0);
        m_eqMidHighSlider->setValue(0);
        m_eqHighSlider->setValue(0);
        m_compThresholdSlider->setValue(-20);
        m_compRatioSlider->setValue(4);
        m_compMakeupSlider->setValue(2);
        m_limiterCeilingSlider->setValue(-2);
    }
}

void AudioMixerDialog::onResetDefaultsClicked()
{
    m_presetCombo->setCurrentIndex(0);
    onPresetSelected(0);
}
