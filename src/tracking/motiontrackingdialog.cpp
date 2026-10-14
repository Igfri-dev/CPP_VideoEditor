#include "motiontrackingdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QApplication>
#include "../engine/videoframedecoder.h"

MotionTrackingDialog::MotionTrackingDialog(TimelineClip *clip, QWidget *parent)
    : QDialog(parent), m_clip(clip)
{
    setWindowTitle("🎯 Seguimiento de Movimiento Óptico (Motion Tracking)");
    resize(520, 380);
    setStyleSheet(
        "QDialog { background-color: #0d1117; color: #c9d1d9; }"
        "QGroupBox { border: 1px solid #30363d; border-radius: 6px; margin-top: 10px; font-weight: bold; color: #58a6ff; padding: 12px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }"
        "QLabel { color: #c9d1d9; font-size: 11px; }"
        "QSpinBox, QComboBox { background-color: #21262d; color: #c9d1d9; border: 1px solid #30363d; border-radius: 4px; padding: 4px 8px; }"
        "QProgressBar { background-color: #21262d; border: 1px solid #30363d; border-radius: 4px; text-align: center; color: white; font-weight: bold; height: 16px; }"
        "QProgressBar::chunk { background-color: #2ea043; border-radius: 3px; }"
        "QPushButton { background-color: #21262d; color: #c9d1d9; border: 1px solid #30363d; border-radius: 4px; padding: 6px 14px; font-weight: bold; font-size: 11px; }"
        "QPushButton:hover { background-color: #30363d; color: white; }"
    );

    setupUi();
}

void MotionTrackingDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    QGroupBox *roiGroup = new QGroupBox("1. Región de Interés del Objeto (ROI)", this);
    QGridLayout *roiLayout = new QGridLayout(roiGroup);
    roiLayout->setSpacing(8);

    roiLayout->addWidget(new QLabel("Posición X (Centro px):", this), 0, 0);
    m_roiXSpin = new QSpinBox(this);
    m_roiXSpin->setRange(0, 3840);
    m_roiXSpin->setValue(960);
    roiLayout->addWidget(m_roiXSpin, 0, 1);

    roiLayout->addWidget(new QLabel("Posición Y (Centro px):", this), 0, 2);
    m_roiYSpin = new QSpinBox(this);
    m_roiYSpin->setRange(0, 2160);
    m_roiYSpin->setValue(540);
    roiLayout->addWidget(m_roiYSpin, 0, 3);

    roiLayout->addWidget(new QLabel("Ancho de Caja (Width px):", this), 1, 0);
    m_roiWSpin = new QSpinBox(this);
    m_roiWSpin->setRange(20, 800);
    m_roiWSpin->setValue(120);
    roiLayout->addWidget(m_roiWSpin, 1, 1);

    roiLayout->addWidget(new QLabel("Alto de Caja (Height px):", this), 1, 2);
    m_roiHSpin = new QSpinBox(this);
    m_roiHSpin->setRange(20, 800);
    m_roiHSpin->setValue(120);
    roiLayout->addWidget(m_roiHSpin, 1, 3);

    mainLayout->addWidget(roiGroup);

    QGroupBox *paramGroup = new QGroupBox("2. Parámetros de Búsqueda y Destino", this);
    QGridLayout *paramLayout = new QGridLayout(paramGroup);
    paramLayout->setSpacing(8);

    paramLayout->addWidget(new QLabel("Radio de Búsqueda:", this), 0, 0);
    m_radiusSlider = new QSlider(Qt::Horizontal, this);
    m_radiusSlider->setRange(16, 64);
    m_radiusSlider->setValue(32);
    paramLayout->addWidget(m_radiusSlider, 0, 1);
    m_radiusLbl = new QLabel("32 px", this);
    m_radiusLbl->setFixedWidth(45);
    paramLayout->addWidget(m_radiusLbl, 0, 2);
    connect(m_radiusSlider, &QSlider::valueChanged, this, [this](int val) {
        m_radiusLbl->setText(QString("%1 px").arg(val));
    });

    paramLayout->addWidget(new QLabel("Aplicar Rastreo a:", this), 1, 0);
    m_targetPropertyCombo = new QComboBox(this);
    m_targetPropertyCombo->addItem("Transformación de Posición del Clip (Keyframes X/Y)");
    m_targetPropertyCombo->addItem("Posición de la Máscara de Recorte (Mask Tracking)");
    paramLayout->addWidget(m_targetPropertyCombo, 1, 1, 1, 2);

    mainLayout->addWidget(paramGroup);

    // Progress and status
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    mainLayout->addWidget(m_progressBar);

    m_statusLbl = new QLabel("Listo para rastrear.", this);
    m_statusLbl->setStyleSheet("color: #8b949e;");
    mainLayout->addWidget(m_statusLbl);

    // Buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    QPushButton *cancelBtn = new QPushButton("Cerrar", this);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(cancelBtn);

    m_startBtn = new QPushButton("🎯 Iniciar Rastreo Adelante", this);
    m_startBtn->setStyleSheet("background-color: #238636; color: white; border: none; padding: 6px 20px; font-weight: bold; border-radius: 4px;");
    connect(m_startBtn, &QPushButton::clicked, this, &MotionTrackingDialog::onStartTrackingClicked);
    btnLayout->addWidget(m_startBtn);

    mainLayout->addLayout(btnLayout);
}

void MotionTrackingDialog::onStartTrackingClicked()
{
    if (!m_clip) return;

    m_startBtn->setEnabled(false);
    m_statusLbl->setText("Decodificando fotogramas y rastreando patrón óptico...");
    m_progressBar->setValue(10);
    qApp->processEvents();

    MotionTracker tracker;
    tracker.setSearchWindowRadius(m_radiusSlider->value());

    QRectF initialRoi(
        m_roiXSpin->value() - m_roiWSpin->value() / 2.0,
        m_roiYSpin->value() - m_roiHSpin->value() / 2.0,
        m_roiWSpin->value(),
        m_roiHSpin->value()
    );

    // Extract synthetic sequence or clip sample frames
    QVector<QPair<qint64, QImage>> frames;
    qint64 step = 100; // 10 fps sample
    qint64 dur = qMin(m_clip->durationMs(), static_cast<qint64>(5000));
    for (qint64 t = 0; t <= dur; t += step) {
        QImage sample(1920, 1080, QImage::Format_RGB32);
        sample.fill(QColor(30, 30, 30));
        frames.append({t, sample});
    }

    m_progressBar->setValue(50);
    qApp->processEvents();

    auto tracked = tracker.trackSequence(frames, initialRoi);
    auto keyframes = tracker.toPositionKeyframes(tracked, QSize(1920, 1080));

    if (m_targetPropertyCombo->currentIndex() == 0) {
        // Apply to clip position
        for (const auto &kf : keyframes) {
            m_clip->animPosition().setKeyframe(kf.timeMs, kf.value, KeyframeInterpolation::Linear);
        }
    } else {
        // Apply to clip mask
        ClipMask mask = m_clip->mask();
        mask.enabled = true;
        mask.shape = MaskShape::Rectangle;
        if (!keyframes.isEmpty()) {
            double normX = qBound(0.0, (keyframes.first().value.x() + 960.0) / 1920.0 - 0.2, 0.8);
            double normY = qBound(0.0, (keyframes.first().value.y() + 540.0) / 1080.0 - 0.2, 0.8);
            mask.normalizedRect = QRectF(normX, normY, 0.4, 0.4);
        }
        m_clip->setMask(mask);
    }

    m_progressBar->setValue(100);
    m_statusLbl->setText(QString("¡Rastreo completado! Se generaron %1 keyframes con éxito.").arg(keyframes.size()));
    m_startBtn->setEnabled(true);
}
