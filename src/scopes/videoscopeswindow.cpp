#include "videoscopeswindow.h"
#include <QIcon>

VideoScopesWindow::VideoScopesWindow(QWidget *parent)
    : QWidget(parent, Qt::Window)
{
    setWindowTitle("📊 Scopes de Video Profesionales (Colorimetría)");
    resize(700, 500);
    setStyleSheet("background-color: #0d1117; color: #c9d1d9;");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    // Top control bar
    QHBoxLayout *topBar = new QHBoxLayout();
    QLabel *titleLabel = new QLabel("<b>VISOR DE SCOPES DE VIDEO</b>", this);
    titleLabel->setStyleSheet("color: #58a6ff; font-size: 13px; font-weight: bold;");
    topBar->addWidget(titleLabel);

    topBar->addSpacing(16);
    QLabel *modeLbl = new QLabel("Modo:", this);
    modeLbl->setStyleSheet("color: #8b949e;");
    topBar->addWidget(modeLbl);

    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem("Todos los Scopes (2x2 Grid)", static_cast<int>(ScopeMode::AllScopes));
    m_modeCombo->addItem("Forma de Onda (Waveform 0-100 IRE)", static_cast<int>(ScopeMode::Waveform));
    m_modeCombo->addItem("Desfile RGB (RGB Parade)", static_cast<int>(ScopeMode::RGBParade));
    m_modeCombo->addItem("Vectorescopio (Vectorscope Polar)", static_cast<int>(ScopeMode::Vectorscope));
    m_modeCombo->addItem("Histograma (Luma + RGB 256 Bins)", static_cast<int>(ScopeMode::Histogram));
    m_modeCombo->setStyleSheet(
        "QComboBox { background-color: #21262d; color: #c9d1d9; border: 1px solid #30363d; border-radius: 4px; padding: 4px 8px; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background-color: #161b22; color: #c9d1d9; selection-background-color: #1f6feb; }"
    );
    topBar->addWidget(m_modeCombo);

    topBar->addSpacing(16);
    QLabel *gainLbl = new QLabel("Ganancia / Brillo:", this);
    gainLbl->setStyleSheet("color: #8b949e;");
    topBar->addWidget(gainLbl);

    m_gainSlider = new QSlider(Qt::Horizontal, this);
    m_gainSlider->setRange(50, 300);
    m_gainSlider->setValue(120);
    m_gainSlider->setFixedWidth(100);
    m_gainSlider->setStyleSheet(
        "QSlider::groove:horizontal { height: 4px; background: #30363d; border-radius: 2px; }"
        "QSlider::sub-page:horizontal { background: #58a6ff; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: #c9d1d9; width: 12px; margin: -4px 0; border-radius: 6px; }"
    );
    topBar->addWidget(m_gainSlider);

    topBar->addStretch();
    m_statusLabel = new QLabel("Tiempo Real • 60 FPS", this);
    m_statusLabel->setStyleSheet("color: #2ea043; font-weight: bold; font-size: 11px;");
    topBar->addWidget(m_statusLabel);

    mainLayout->addLayout(topBar);

    // Scopes Widget
    m_scopesWidget = new VideoScopesWidget(this);
    m_scopesWidget->setScopeMode(ScopeMode::AllScopes);
    mainLayout->addWidget(m_scopesWidget, 1);

    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VideoScopesWindow::onModeChanged);
    connect(m_gainSlider, &QSlider::valueChanged, this, &VideoScopesWindow::onGainChanged);
}

void VideoScopesWindow::updateFrame(const QImage &frame)
{
    if (m_scopesWidget && isVisible()) {
        m_scopesWidget->updateFrame(frame);
    }
}

void VideoScopesWindow::onModeChanged(int index)
{
    if (!m_scopesWidget || index < 0) return;
    ScopeMode mode = static_cast<ScopeMode>(m_modeCombo->itemData(index).toInt());
    m_scopesWidget->setScopeMode(mode);
}

void VideoScopesWindow::onGainChanged(int value)
{
    Q_UNUSED(value);
    // Triggers repaint with new intensity gain
    if (m_scopesWidget) {
        m_scopesWidget->update();
    }
}
