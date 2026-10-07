#include "inspectorwidget.h"
#include "../medialibrary/mediaitem.h"
#include "../timelinewidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QColorDialog>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QDrag>
#include <QMimeData>
#include <QMenu>
#include <QAction>
#include <QPainter>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>

// ---------------------------------------------------------------------------
// EffectItemWidget Implementation
// ---------------------------------------------------------------------------
EffectItemWidget::EffectItemWidget(int index, int totalCount, VisualFilter filter, QWidget *parent)
    : QWidget(parent)
    , m_index(index)
    , m_filter(filter)
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 5, 8, 5);
    layout->setSpacing(6);

    // Drag handle
    QLabel *handle = new QLabel("⠿", this);
    handle->setStyleSheet("color: #8b949e; font-size: 15px; font-weight: bold; padding-right: 2px;");
    handle->setCursor(Qt::SizeVerCursor);
    handle->setToolTip("Arrastrar para reordenar en la jerarquía");
    layout->addWidget(handle);

    // Hierarchy Level Badge
    QLabel *levelBadge = new QLabel(this);
    if (index == 0) {
        levelBadge->setText("▲ 1. Superior");
        levelBadge->setStyleSheet("background-color: #1f6feb; color: white; border-radius: 3px; padding: 2px 5px; font-size: 10px; font-weight: bold;");
        levelBadge->setToolTip("Capa Superior: Este efecto se aplica SOBRE todos los efectos inferiores");
    } else if (index == totalCount - 1 && totalCount > 1) {
        levelBadge->setText(QString("▼ %1. Base").arg(index + 1));
        levelBadge->setStyleSheet("background-color: #21262d; color: #8b949e; border: 1px solid #30363d; border-radius: 3px; padding: 2px 5px; font-size: 10px;");
        levelBadge->setToolTip("Capa Base: Primer efecto aplicado a la imagen del clip");
    } else {
        levelBadge->setText(QString("Cap. %1").arg(index + 1));
        levelBadge->setStyleSheet("background-color: #21262d; color: #c9d1d9; border: 1px solid #30363d; border-radius: 3px; padding: 2px 5px; font-size: 10px;");
        levelBadge->setToolTip(QString("Nivel de jerarquía %1").arg(index + 1));
    }
    layout->addWidget(levelBadge);

    // Effect Name
    QString filterName = visualFilterToString(filter);
    QLabel *nameLabel = new QLabel(filterName, this);
    nameLabel->setStyleSheet("color: #f0f6fc; font-weight: bold; font-size: 11px;");
    layout->addWidget(nameLabel, 1);

    // Move Up Button (▲)
    QPushButton *upBtn = new QPushButton("▲", this);
    upBtn->setFixedSize(24, 22);
    upBtn->setEnabled(index > 0);
    upBtn->setToolTip("Subir en jerarquía (Aplica sobre los efectos inferiores)");
    upBtn->setStyleSheet(
        "QPushButton { background-color: #21262d; color: #c9d1d9; border: 1px solid #30363d; border-radius: 3px; font-weight: bold; font-size: 10px; }"
        "QPushButton:hover:enabled { background-color: #388bfd; color: white; }"
        "QPushButton:disabled { color: #484f58; background-color: #161b22; border-color: #21262d; }"
    );
    connect(upBtn, &QPushButton::clicked, this, [this]() {
        emit moveUpRequested(m_index);
    });
    layout->addWidget(upBtn);

    // Move Down Button (▼)
    QPushButton *downBtn = new QPushButton("▼", this);
    downBtn->setFixedSize(24, 22);
    downBtn->setEnabled(index < totalCount - 1);
    downBtn->setToolTip("Bajar en jerarquía");
    downBtn->setStyleSheet(
        "QPushButton { background-color: #21262d; color: #c9d1d9; border: 1px solid #30363d; border-radius: 3px; font-weight: bold; font-size: 10px; }"
        "QPushButton:hover:enabled { background-color: #388bfd; color: white; }"
        "QPushButton:disabled { color: #484f58; background-color: #161b22; border-color: #21262d; }"
    );
    connect(downBtn, &QPushButton::clicked, this, [this]() {
        emit moveDownRequested(m_index);
    });
    layout->addWidget(downBtn);

    // Delete Button (🗑)
    QPushButton *delBtn = new QPushButton("🗑", this);
    delBtn->setFixedSize(24, 22);
    delBtn->setToolTip("Eliminar efecto de la pila");
    delBtn->setStyleSheet(
        "QPushButton { background-color: #21262d; color: #f85149; border: 1px solid #30363d; border-radius: 3px; font-weight: bold; font-size: 11px; }"
        "QPushButton:hover { background-color: #da3633; color: white; }"
    );
    connect(delBtn, &QPushButton::clicked, this, [this]() {
        emit removeRequested(m_index);
    });
    layout->addWidget(delBtn);

    setStyleSheet("EffectItemWidget { background-color: #161b22; border: 1px solid #30363d; border-radius: 4px; }");
}

// ---------------------------------------------------------------------------
// EffectsListWidget Implementation
// ---------------------------------------------------------------------------
EffectsListWidget::EffectsListWidget(QWidget *parent)
    : QListWidget(parent)
{
    setSelectionMode(QAbstractItemView::SingleSelection);
    setDragEnabled(true);
    setAcceptDrops(true);
    setDropIndicatorShown(true);
    setDefaultDropAction(Qt::MoveAction);
    setSpacing(3);
    setStyleSheet(
        "QListWidget { background-color: #0d1117; border: 1px solid #30363d; border-radius: 4px; padding: 3px; outline: none; }"
        "QListWidget::item { background: transparent; padding: 0px; border: none; }"
        "QListWidget::item:selected { background: transparent; }"
    );
}

void EffectsListWidget::startDrag(Qt::DropActions supportedActions)
{
    Q_UNUSED(supportedActions);
    QListWidgetItem *item = currentItem();
    if (!item) return;
    int fromRow = row(item);

    QByteArray itemData;
    QDataStream dataStream(&itemData, QIODevice::WriteOnly);
    dataStream << fromRow;

    QMimeData *mimeData = new QMimeData;
    mimeData->setData("application/x-editor-effect-index", itemData);

    QDrag *drag = new QDrag(this);
    drag->setMimeData(mimeData);

    QWidget *w = itemWidget(item);
    if (w) {
        QPixmap pix = w->grab();
        drag->setPixmap(pix);
        drag->setHotSpot(QPoint(pix.width() / 2, pix.height() / 2));
    }

    drag->exec(Qt::MoveAction);
}

void EffectsListWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat("application/x-editor-effect-index")) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void EffectsListWidget::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasFormat("application/x-editor-effect-index")) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void EffectsListWidget::dropEvent(QDropEvent *event)
{
    if (!event->mimeData()->hasFormat("application/x-editor-effect-index")) {
        event->ignore();
        return;
    }

    QByteArray itemData = event->mimeData()->data("application/x-editor-effect-index");
    QDataStream dataStream(&itemData, QIODevice::ReadOnly);
    int fromRow = -1;
    dataStream >> fromRow;

    QPoint pos = event->position().toPoint();
    QListWidgetItem *targetItem = itemAt(pos);
    int toRow = targetItem ? row(targetItem) : (count() - 1);

    if (fromRow >= 0 && toRow >= 0 && fromRow != toRow) {
        emit effectReordered(fromRow, toRow);
    }
    event->acceptProposedAction();
}

static QWidget* createChannelRow(QWidget *parent, const QString &label, const QString &colorAccent,
                                 QSlider *&slider, QSpinBox *&spin)
{
    QWidget *rowWidget = new QWidget(parent);
    QVBoxLayout *rowLayout = new QVBoxLayout(rowWidget);
    rowLayout->setContentsMargins(0, 2, 0, 2);
    rowLayout->setSpacing(2);

    QHBoxLayout *hdr = new QHBoxLayout();
    QLabel *lbl = new QLabel(label, rowWidget);
    lbl->setStyleSheet(QString("font-weight: bold; font-size: 11px; color: %1;").arg(colorAccent));
    hdr->addWidget(lbl);
    hdr->addStretch();

    spin = new QSpinBox(rowWidget);
    spin->setRange(-100, 100);
    spin->setValue(0);
    spin->setFixedWidth(64);
    spin->setAlignment(Qt::AlignCenter);
    spin->setStyleSheet("background-color: #0d1117; color: #c9d1d9; border: 1px solid #30363d; border-radius: 3px; font-weight: bold; font-size: 11px;");
    hdr->addWidget(spin);
    rowLayout->addLayout(hdr);

    slider = new QSlider(Qt::Horizontal, rowWidget);
    slider->setRange(-100, 100);
    slider->setValue(0);
    slider->setStyleSheet(QString(
        "QSlider::groove:horizontal { height: 4px; background: #21262d; border-radius: 2px; }"
        "QSlider::sub-page:horizontal { background: %1; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: #f0f6fc; border: 1px solid #30363d; width: 12px; margin-top: -4px; margin-bottom: -4px; border-radius: 6px; }"
        "QSlider::handle:horizontal:hover { background: %1; }"
    ).arg(colorAccent));
    rowLayout->addWidget(slider);

    return rowWidget;
}

InspectorWidget::InspectorWidget(TimelineModel *model, QObject *parent)
    : QWidget(qobject_cast<QWidget*>(parent))
    , m_model(model)
{
    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(6, 6, 6, 6);
    rootLayout->setSpacing(6);

    QHBoxLayout *topBar = new QHBoxLayout();
    QLabel *panelTitle = new QLabel("Inspector", this);
    panelTitle->setStyleSheet("font-weight: bold; font-size: 13px; color: #c9d1d9;");
    topBar->addWidget(panelTitle);
    topBar->addStretch();

    // Mode tab buttons
    m_tabClipBtn = new QPushButton("🎬 Elemento", this);
    m_tabClipBtn->setCursor(Qt::PointingHandCursor);
    m_tabClipBtn->setStyleSheet("background-color: #1f6feb; color: white; border: 1px solid #388bfd; border-radius: 4px; padding: 4px 10px; font-weight: bold; font-size: 11px;");
    connect(m_tabClipBtn, &QPushButton::clicked, this, &InspectorWidget::showClipProperties);
    topBar->addWidget(m_tabClipBtn);

    m_tabGlobalBtn = new QPushButton("🌐 General / Master", this);
    m_tabGlobalBtn->setCursor(Qt::PointingHandCursor);
    m_tabGlobalBtn->setStyleSheet("background-color: #21262d; color: #8b949e; border: 1px solid #30363d; border-radius: 4px; padding: 4px 10px; font-weight: bold; font-size: 11px;");
    connect(m_tabGlobalBtn, &QPushButton::clicked, this, &InspectorWidget::showGlobalProperties);
    topBar->addWidget(m_tabGlobalBtn);

    rootLayout->addLayout(topBar);

    m_viewStack = new QStackedWidget(this);

    // ================= PAGE 0: CLIP PROPERTIES =================
    m_clipPage = new QWidget(m_viewStack);
    QVBoxLayout *clipPageLayout = new QVBoxLayout(m_clipPage);
    clipPageLayout->setContentsMargins(0, 0, 0, 0);
    clipPageLayout->setSpacing(6);

    m_noSelectionLabel = new QLabel("Selecciona un clip en la línea de tiempo para ver y editar sus propiedades.", m_clipPage);
    m_noSelectionLabel->setAlignment(Qt::AlignCenter);
    m_noSelectionLabel->setWordWrap(true);
    m_noSelectionLabel->setStyleSheet("color: #8b949e; font-size: 12px; padding: 20px;");
    clipPageLayout->addWidget(m_noSelectionLabel);

    m_openGlobalFromEmptyBtn = new QPushButton("🌐 Ver Ajustes Generales del Video", m_clipPage);
    m_openGlobalFromEmptyBtn->setCursor(Qt::PointingHandCursor);
    m_openGlobalFromEmptyBtn->setStyleSheet("QPushButton { background-color: #21262d; color: #58a6ff; border: 1px solid #30363d; border-radius: 4px; padding: 6px 12px; font-weight: bold; font-size: 11px; } QPushButton:hover { background-color: #30363d; color: white; }");
    connect(m_openGlobalFromEmptyBtn, &QPushButton::clicked, this, &InspectorWidget::showGlobalProperties);
    clipPageLayout->addWidget(m_openGlobalFromEmptyBtn, 0, Qt::AlignCenter);

    // Scroll area for content container
    QScrollArea *scrollArea = new QScrollArea(m_clipPage);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setStyleSheet("background: transparent;");

    m_contentContainer = new QWidget(scrollArea);
    m_contentContainer->setMinimumWidth(320);
    QVBoxLayout *layout = new QVBoxLayout(m_contentContainer);
    layout->setContentsMargins(0, 0, 4, 0);
    layout->setSpacing(8);

    // Header info box
    QGroupBox *infoGroup = new QGroupBox("Información del Clip", m_contentContainer);
    infoGroup->setStyleSheet("QGroupBox { color: #58a6ff; font-weight: bold; border: 1px solid #30363d; border-radius: 4px; margin-top: 8px; padding-top: 10px; } QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 3px; }");
    QVBoxLayout *infoLayout = new QVBoxLayout(infoGroup);
    infoLayout->setSpacing(4);

    QHBoxLayout *nameBadgeLayout = new QHBoxLayout();
    m_clipNameLabel = new QLabel(infoGroup);
    m_clipNameLabel->setStyleSheet("font-weight: bold; color: #ffffff;");
    m_clipTypeBadge = new QLabel(infoGroup);
    m_clipTypeBadge->setStyleSheet("background-color: #2f81f7; color: white; border-radius: 3px; padding: 2px 6px; font-size: 10px; font-weight: bold;");
    nameBadgeLayout->addWidget(m_clipNameLabel, 1);
    nameBadgeLayout->addWidget(m_clipTypeBadge);
    infoLayout->addLayout(nameBadgeLayout);

    m_trackLabel = new QLabel(infoGroup);
    m_trackLabel->setStyleSheet("color: #8b949e; font-size: 11px;");
    infoLayout->addWidget(m_trackLabel);

    m_timingLabel = new QLabel(infoGroup);
    m_timingLabel->setStyleSheet("color: #8b949e; font-size: 11px;");
    infoLayout->addWidget(m_timingLabel);

    m_techDetailsLabel = new QLabel(infoGroup);
    m_techDetailsLabel->setStyleSheet("color: #58a6ff; font-size: 11px; background-color: #0d1117; border: 1px solid #30363d; border-radius: 4px; padding: 6px;");
    m_techDetailsLabel->setWordWrap(true);
    infoLayout->addWidget(m_techDetailsLabel);

    // Prominent Separate Audio button
    m_separateAudioBtn = new QPushButton("Separar Audio de Video", infoGroup);
    m_separateAudioBtn->setCursor(Qt::PointingHandCursor);
    m_separateAudioBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #d29922; color: #0d1117; font-weight: bold; border-radius: 4px; padding: 6px 10px;"
        "}"
        "QPushButton:hover { background-color: #e3b341; }"
        "QPushButton:pressed { background-color: #b08018; }"
    );
    connect(m_separateAudioBtn, &QPushButton::clicked, this, &InspectorWidget::onSeparateAudioClicked);
    infoLayout->addWidget(m_separateAudioBtn);

    layout->addWidget(infoGroup);

    // Transform Section (Position, Scale, Rotation)
    m_transformSection = new QGroupBox("📐 Transformación (Posición, Escala, Rotación)", m_contentContainer);
    m_transformSection->setStyleSheet("QGroupBox { color: #58a6ff; font-weight: bold; border: 1px solid #30363d; border-radius: 4px; margin-top: 8px; padding-top: 10px; } QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 3px; }");
    QVBoxLayout *transLayout = new QVBoxLayout(m_transformSection);
    transLayout->setSpacing(6);

    // Scale
    QHBoxLayout *scaleHdr = new QHBoxLayout();
    scaleHdr->addWidget(new QLabel("Escala (Zoom):", m_transformSection));
    m_scaleSpin = new QSpinBox(m_transformSection);
    m_scaleSpin->setRange(10, 500);
    m_scaleSpin->setValue(100);
    m_scaleSpin->setSuffix("%");
    m_scaleSpin->setStyleSheet("background-color: #0d1117; color: #c9d1d9; border: 1px solid #30363d; padding: 2px 4px;");
    scaleHdr->addStretch();
    scaleHdr->addWidget(m_scaleSpin);
    transLayout->addLayout(scaleHdr);

    m_scaleSlider = new QSlider(Qt::Horizontal, m_transformSection);
    m_scaleSlider->setRange(10, 300);
    m_scaleSlider->setValue(100);
    transLayout->addWidget(m_scaleSlider);

    connect(m_scaleSlider, &QSlider::valueChanged, this, [this](int val) {
        if (!m_updatingUi) {
            m_scaleSpin->blockSignals(true);
            m_scaleSpin->setValue(val);
            m_scaleSpin->blockSignals(false);
            onScaleChanged(val);
        }
    });
    connect(m_scaleSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        if (!m_updatingUi) {
            m_scaleSlider->blockSignals(true);
            m_scaleSlider->setValue(qBound(10, val, 300));
            m_scaleSlider->blockSignals(false);
            onScaleChanged(val);
        }
    });

    // Position (X, Y)
    QHBoxLayout *posLayout = new QHBoxLayout();
    posLayout->addWidget(new QLabel("Pos X:", m_transformSection));
    m_posXSpin = new QSpinBox(m_transformSection);
    m_posXSpin->setRange(-1920, 1920);
    m_posXSpin->setSingleStep(5);
    m_posXSpin->setValue(0);
    m_posXSpin->setSuffix(" px");
    m_posXSpin->setStyleSheet("background-color: #0d1117; color: #c9d1d9; border: 1px solid #30363d; padding: 2px 4px;");
    posLayout->addWidget(m_posXSpin);

    posLayout->addWidget(new QLabel("Pos Y:", m_transformSection));
    m_posYSpin = new QSpinBox(m_transformSection);
    m_posYSpin->setRange(-1080, 1080);
    m_posYSpin->setSingleStep(5);
    m_posYSpin->setValue(0);
    m_posYSpin->setSuffix(" px");
    m_posYSpin->setStyleSheet("background-color: #0d1117; color: #c9d1d9; border: 1px solid #30363d; padding: 2px 4px;");
    posLayout->addWidget(m_posYSpin);
    transLayout->addLayout(posLayout);

    connect(m_posXSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &InspectorWidget::onPosXChanged);
    connect(m_posYSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &InspectorWidget::onPosYChanged);

    // Rotation
    QHBoxLayout *rotHdr = new QHBoxLayout();
    rotHdr->addWidget(new QLabel("Rotación:", m_transformSection));
    m_rotSpin = new QSpinBox(m_transformSection);
    m_rotSpin->setRange(-360, 360);
    m_rotSpin->setValue(0);
    m_rotSpin->setSuffix("°");
    m_rotSpin->setStyleSheet("background-color: #0d1117; color: #c9d1d9; border: 1px solid #30363d; padding: 2px 4px;");
    rotHdr->addStretch();
    rotHdr->addWidget(m_rotSpin);
    transLayout->addLayout(rotHdr);

    m_rotSlider = new QSlider(Qt::Horizontal, m_transformSection);
    m_rotSlider->setRange(-180, 180);
    m_rotSlider->setValue(0);
    transLayout->addWidget(m_rotSlider);

    connect(m_rotSlider, &QSlider::valueChanged, this, [this](int val) {
        if (!m_updatingUi) {
            m_rotSpin->blockSignals(true);
            m_rotSpin->setValue(val);
            m_rotSpin->blockSignals(false);
            onRotationChanged(val);
        }
    });
    connect(m_rotSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        if (!m_updatingUi) {
            m_rotSlider->blockSignals(true);
            m_rotSlider->setValue(qBound(-180, val, 180));
            m_rotSlider->blockSignals(false);
            onRotationChanged(val);
        }
    });

    // Z-Index (Capa y orden en la línea de tiempo)
    QHBoxLayout *zLayout = new QHBoxLayout();
    zLayout->addWidget(new QLabel("Capa (Z-Index):", m_transformSection));
    m_zIndexSpin = new QSpinBox(m_transformSection);
    m_zIndexSpin->setRange(1, 99);
    m_zIndexSpin->setValue(1);
    m_zIndexSpin->setToolTip("Nivel Z del elemento (1 = pista base inferior, mayor = más arriba y al frente)");
    m_zIndexSpin->setStyleSheet("background-color: #0d1117; color: #58a6ff; font-weight: bold; border: 1px solid #30363d; padding: 2px 6px;");
    zLayout->addWidget(m_zIndexSpin);

    m_zIndexInfoLabel = new QLabel("Base (1)", m_transformSection);
    m_zIndexInfoLabel->setStyleSheet("color: #8b949e; font-size: 11px;");
    zLayout->addWidget(m_zIndexInfoLabel);
    zLayout->addStretch();
    transLayout->addLayout(zLayout);

    connect(m_zIndexSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &InspectorWidget::onZIndexChanged);

    // Scale Mode (Ajuste al Lienzo)
    QHBoxLayout *scaleModeLayout = new QHBoxLayout();
    QLabel *smLabel = new QLabel("Ajuste al Lienzo:", m_transformSection);
    smLabel->setStyleSheet("color: #58a6ff; font-weight: bold; font-size: 11px;");
    scaleModeLayout->addWidget(smLabel);
    m_scaleModeCombo = new QComboBox(m_transformSection);
    m_scaleModeCombo->addItem(clipScaleModeDisplayName(ClipScaleMode::FillCrop), static_cast<int>(ClipScaleMode::FillCrop));
    m_scaleModeCombo->addItem(clipScaleModeDisplayName(ClipScaleMode::FitLetterbox), static_cast<int>(ClipScaleMode::FitLetterbox));
    m_scaleModeCombo->addItem(clipScaleModeDisplayName(ClipScaleMode::Stretch), static_cast<int>(ClipScaleMode::Stretch));
    m_scaleModeCombo->setToolTip("Define cómo se adapta el video a la relación de aspecto del lienzo (por ejemplo 16:9 en 9:16)");
    m_scaleModeCombo->setStyleSheet("background-color: #0d1117; color: #c9d1d9; border: 1px solid #30363d; border-radius: 4px; padding: 2px 6px; font-size: 11px;");
    connect(m_scaleModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &InspectorWidget::onScaleModeChanged);
    scaleModeLayout->addWidget(m_scaleModeCombo);
    transLayout->addLayout(scaleModeLayout);

    // Reset button
    m_resetTransformBtn = new QPushButton("Restablecer Transformación", m_transformSection);
    m_resetTransformBtn->setCursor(Qt::PointingHandCursor);
    m_resetTransformBtn->setStyleSheet("QPushButton { background-color: #21262d; color: #c9d1d9; border: 1px solid #30363d; border-radius: 4px; padding: 4px 8px; font-size: 11px; } QPushButton:hover { background-color: #30363d; }");
    connect(m_resetTransformBtn, &QPushButton::clicked, this, &InspectorWidget::onResetTransformClicked);
    transLayout->addWidget(m_resetTransformBtn);

    layout->addWidget(m_transformSection);

    // Motion Path Section (Desplazamiento y Animación)
    m_motionSection = new QGroupBox("🚀 Desplazamiento y Animación (Ruta Vectorial)", m_contentContainer);
    m_motionSection->setStyleSheet(
        "QGroupBox { color: #00d2ff; font-weight: bold; border: 1px solid #30363d; border-radius: 4px; margin-top: 8px; padding-top: 10px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 3px; }"
    );
    QVBoxLayout *motionLayout = new QVBoxLayout(m_motionSection);
    motionLayout->setSpacing(6);

    // Preset selector
    QHBoxLayout *presetRow = new QHBoxLayout();
    presetRow->addWidget(new QLabel("Trayectoria / Preset:", m_motionSection));
    m_motionPresetCombo = new QComboBox(m_motionSection);
    m_motionPresetCombo->addItem(motionPresetDisplayName(MotionPreset::None), static_cast<int>(MotionPreset::None));
    m_motionPresetCombo->addItem(motionPresetDisplayName(MotionPreset::LeftToRight), static_cast<int>(MotionPreset::LeftToRight));
    m_motionPresetCombo->addItem(motionPresetDisplayName(MotionPreset::RightToLeft), static_cast<int>(MotionPreset::RightToLeft));
    m_motionPresetCombo->addItem(motionPresetDisplayName(MotionPreset::TopToBottom), static_cast<int>(MotionPreset::TopToBottom));
    m_motionPresetCombo->addItem(motionPresetDisplayName(MotionPreset::BottomToTop), static_cast<int>(MotionPreset::BottomToTop));
    m_motionPresetCombo->addItem(motionPresetDisplayName(MotionPreset::DiagonalTLBR), static_cast<int>(MotionPreset::DiagonalTLBR));
    m_motionPresetCombo->addItem(motionPresetDisplayName(MotionPreset::DiagonalBLTR), static_cast<int>(MotionPreset::DiagonalBLTR));
    m_motionPresetCombo->addItem(motionPresetDisplayName(MotionPreset::Custom), static_cast<int>(MotionPreset::Custom));
    m_motionPresetCombo->setStyleSheet("background-color: #0d1117; color: #c9d1d9; border: 1px solid #30363d; border-radius: 4px; padding: 3px; font-weight: bold;");
    connect(m_motionPresetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &InspectorWidget::onMotionPresetChanged);
    presetRow->addWidget(m_motionPresetCombo);
    motionLayout->addLayout(presetRow);

    // Easing selector
    QHBoxLayout *easingRow = new QHBoxLayout();
    easingRow->addWidget(new QLabel("Suavizado (Easing):", m_motionSection));
    m_motionEasingCombo = new QComboBox(m_motionSection);
    m_motionEasingCombo->addItem(motionEasingDisplayName(MotionEasing::Linear), static_cast<int>(MotionEasing::Linear));
    m_motionEasingCombo->addItem(motionEasingDisplayName(MotionEasing::EaseInOut), static_cast<int>(MotionEasing::EaseInOut));
    m_motionEasingCombo->addItem(motionEasingDisplayName(MotionEasing::EaseIn), static_cast<int>(MotionEasing::EaseIn));
    m_motionEasingCombo->addItem(motionEasingDisplayName(MotionEasing::EaseOut), static_cast<int>(MotionEasing::EaseOut));
    m_motionEasingCombo->setStyleSheet("background-color: #0d1117; color: #c9d1d9; border: 1px solid #30363d; border-radius: 4px; padding: 3px; font-size: 11px;");
    connect(m_motionEasingCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &InspectorWidget::onMotionEasingChanged);
    easingRow->addWidget(m_motionEasingCombo);
    motionLayout->addLayout(easingRow);

    // Start Position (X, Y)
    QHBoxLayout *startHdr = new QHBoxLayout();
    QLabel *startLbl = new QLabel("▶ Posición Inicial (Start):", m_motionSection);
    startLbl->setStyleSheet("color: #2ea043; font-weight: bold; font-size: 11px;");
    startHdr->addWidget(startLbl);
    startHdr->addStretch();
    motionLayout->addLayout(startHdr);

    QHBoxLayout *startRow = new QHBoxLayout();
    startRow->addWidget(new QLabel("X:", m_motionSection));
    m_motionStartXSpin = new QDoubleSpinBox(m_motionSection);
    m_motionStartXSpin->setRange(-4000.0, 4000.0);
    m_motionStartXSpin->setDecimals(1);
    m_motionStartXSpin->setStyleSheet("background-color: #0d1117; color: #c9d1d9; border: 1px solid #30363d; border-radius: 3px; padding: 2px 4px;");
    connect(m_motionStartXSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) { onMotionStartChanged(); });
    startRow->addWidget(m_motionStartXSpin);

    startRow->addWidget(new QLabel("Y:", m_motionSection));
    m_motionStartYSpin = new QDoubleSpinBox(m_motionSection);
    m_motionStartYSpin->setRange(-4000.0, 4000.0);
    m_motionStartYSpin->setDecimals(1);
    m_motionStartYSpin->setStyleSheet("background-color: #0d1117; color: #c9d1d9; border: 1px solid #30363d; border-radius: 3px; padding: 2px 4px;");
    connect(m_motionStartYSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) { onMotionStartChanged(); });
    startRow->addWidget(m_motionStartYSpin);
    motionLayout->addLayout(startRow);

    // End Position (X, Y)
    QHBoxLayout *endHdr = new QHBoxLayout();
    QLabel *endLbl = new QLabel("■ Posición Final (End):", m_motionSection);
    endLbl->setStyleSheet("color: #f85149; font-weight: bold; font-size: 11px;");
    endHdr->addWidget(endLbl);
    endHdr->addStretch();
    motionLayout->addLayout(endHdr);

    QHBoxLayout *endRow = new QHBoxLayout();
    endRow->addWidget(new QLabel("X:", m_motionSection));
    m_motionEndXSpin = new QDoubleSpinBox(m_motionSection);
    m_motionEndXSpin->setRange(-4000.0, 4000.0);
    m_motionEndXSpin->setDecimals(1);
    m_motionEndXSpin->setStyleSheet("background-color: #0d1117; color: #c9d1d9; border: 1px solid #30363d; border-radius: 3px; padding: 2px 4px;");
    connect(m_motionEndXSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) { onMotionEndChanged(); });
    endRow->addWidget(m_motionEndXSpin);

    endRow->addWidget(new QLabel("Y:", m_motionSection));
    m_motionEndYSpin = new QDoubleSpinBox(m_motionSection);
    m_motionEndYSpin->setRange(-4000.0, 4000.0);
    m_motionEndYSpin->setDecimals(1);
    m_motionEndYSpin->setStyleSheet("background-color: #0d1117; color: #c9d1d9; border: 1px solid #30363d; border-radius: 3px; padding: 2px 4px;");
    connect(m_motionEndYSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) { onMotionEndChanged(); });
    endRow->addWidget(m_motionEndYSpin);
    motionLayout->addLayout(endRow);

    // Vector Waypoints & Curvature (Mano Alzada / Curvas Bézier)
    QHBoxLayout *wpSelectRow = new QHBoxLayout();
    wpSelectRow->addWidget(new QLabel("Punto Vectorial:", m_motionSection));
    m_motionWaypointCombo = new QComboBox(m_motionSection);
    m_motionWaypointCombo->setStyleSheet("background-color: #0d1117; color: #c9d1d9; border: 1px solid #30363d; border-radius: 4px; padding: 2px; font-size: 11px;");
    connect(m_motionWaypointCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &InspectorWidget::onMotionWaypointSelected);
    wpSelectRow->addWidget(m_motionWaypointCombo);
    motionLayout->addLayout(wpSelectRow);

    QHBoxLayout *wpCoordsRow = new QHBoxLayout();
    wpCoordsRow->addWidget(new QLabel("X:", m_motionSection));
    m_motionWaypointXSpin = new QDoubleSpinBox(m_motionSection);
    m_motionWaypointXSpin->setRange(-4000.0, 4000.0);
    m_motionWaypointXSpin->setDecimals(1);
    m_motionWaypointXSpin->setStyleSheet("background-color: #0d1117; color: #c9d1d9; border: 1px solid #30363d; border-radius: 3px; padding: 2px;");
    connect(m_motionWaypointXSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) { onMotionWaypointChanged(); });
    wpCoordsRow->addWidget(m_motionWaypointXSpin);

    wpCoordsRow->addWidget(new QLabel("Y:", m_motionSection));
    m_motionWaypointYSpin = new QDoubleSpinBox(m_motionSection);
    m_motionWaypointYSpin->setRange(-4000.0, 4000.0);
    m_motionWaypointYSpin->setDecimals(1);
    m_motionWaypointYSpin->setStyleSheet("background-color: #0d1117; color: #c9d1d9; border: 1px solid #30363d; border-radius: 3px; padding: 2px;");
    connect(m_motionWaypointYSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) { onMotionWaypointChanged(); });
    wpCoordsRow->addWidget(m_motionWaypointYSpin);
    motionLayout->addLayout(wpCoordsRow);

    QHBoxLayout *curveCheckRow = new QHBoxLayout();
    m_motionWaypointCurvedCheck = new QCheckBox("Curvatura Bézier (Tiradores)", m_motionSection);
    m_motionWaypointCurvedCheck->setStyleSheet("color: #58a6ff; font-weight: bold; font-size: 11px;");
    connect(m_motionWaypointCurvedCheck, &QCheckBox::toggled, this, [this](bool) { onMotionWaypointChanged(); });
    curveCheckRow->addWidget(m_motionWaypointCurvedCheck);
    motionLayout->addLayout(curveCheckRow);

    // Tangent handles
    QHBoxLayout *handleRow = new QHBoxLayout();
    handleRow->addWidget(new QLabel("Tirador Bézier (dx, dy):", m_motionSection));
    m_motionHandleOutXSpin = new QDoubleSpinBox(m_motionSection);
    m_motionHandleOutXSpin->setRange(-1000.0, 1000.0);
    m_motionHandleOutXSpin->setDecimals(1);
    m_motionHandleOutXSpin->setStyleSheet("background-color: #0d1117; color: #c9d1d9; border: 1px solid #30363d; border-radius: 3px; padding: 2px;");
    connect(m_motionHandleOutXSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) { onMotionWaypointChanged(); });
    handleRow->addWidget(m_motionHandleOutXSpin);

    m_motionHandleOutYSpin = new QDoubleSpinBox(m_motionSection);
    m_motionHandleOutYSpin->setRange(-1000.0, 1000.0);
    m_motionHandleOutYSpin->setDecimals(1);
    m_motionHandleOutYSpin->setStyleSheet("background-color: #0d1117; color: #c9d1d9; border: 1px solid #30363d; border-radius: 3px; padding: 2px;");
    connect(m_motionHandleOutYSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) { onMotionWaypointChanged(); });
    handleRow->addWidget(m_motionHandleOutYSpin);
    motionLayout->addLayout(handleRow);

    // Point Buttons (+ Add Point, - Remove Point, Auto-Smooth, Reverse)
    QHBoxLayout *pointBtnsRow = new QHBoxLayout();
    m_motionAddPointBtn = new QPushButton("➕ Añadir Punto", m_motionSection);
    m_motionAddPointBtn->setStyleSheet("QPushButton { background-color: #238636; color: white; border-radius: 3px; padding: 4px 6px; font-weight: bold; font-size: 11px; } QPushButton:hover { background-color: #2ea043; }");
    connect(m_motionAddPointBtn, &QPushButton::clicked, this, &InspectorWidget::onMotionAddPointClicked);
    pointBtnsRow->addWidget(m_motionAddPointBtn);

    m_motionRemovePointBtn = new QPushButton("🗑️ Eliminar", m_motionSection);
    m_motionRemovePointBtn->setStyleSheet("QPushButton { background-color: #da3633; color: white; border-radius: 3px; padding: 4px 6px; font-weight: bold; font-size: 11px; } QPushButton:hover { background-color: #f85149; }");
    connect(m_motionRemovePointBtn, &QPushButton::clicked, this, &InspectorWidget::onMotionRemovePointClicked);
    pointBtnsRow->addWidget(m_motionRemovePointBtn);
    motionLayout->addLayout(pointBtnsRow);

    QHBoxLayout *utilBtnsRow = new QHBoxLayout();
    m_motionAutoSmoothBtn = new QPushButton("〰️ Suavizar Curvas", m_motionSection);
    m_motionAutoSmoothBtn->setToolTip("Convertir automáticamente todos los puntos en curvas suaves con tiradores Bézier tipo pluma");
    m_motionAutoSmoothBtn->setStyleSheet("QPushButton { background-color: #1f6feb; color: white; border-radius: 3px; padding: 4px 6px; font-weight: bold; font-size: 11px; } QPushButton:hover { background-color: #388bfd; }");
    connect(m_motionAutoSmoothBtn, &QPushButton::clicked, this, &InspectorWidget::onMotionAutoSmoothClicked);
    utilBtnsRow->addWidget(m_motionAutoSmoothBtn);

    m_motionReverseBtn = new QPushButton("🔄 Invertir", m_motionSection);
    m_motionReverseBtn->setToolTip("Invertir el sentido de la trayectoria");
    m_motionReverseBtn->setStyleSheet("QPushButton { background-color: #21262d; color: #c9d1d9; border: 1px solid #30363d; border-radius: 3px; padding: 4px 6px; font-weight: bold; font-size: 11px; } QPushButton:hover { background-color: #30363d; }");
    connect(m_motionReverseBtn, &QPushButton::clicked, this, &InspectorWidget::onMotionReverseClicked);
    utilBtnsRow->addWidget(m_motionReverseBtn);
    motionLayout->addLayout(utilBtnsRow);

    // Tip label
    QLabel *tipLbl = new QLabel("💡 Arrastra los puntos [INICIO], [FIN] y tiradores Bézier directamente en la pantalla de previsualización, o haz doble clic sobre la trayectoria para añadir nuevos puntos.", m_motionSection);
    tipLbl->setStyleSheet("color: #8b949e; font-size: 10px; font-style: italic;");
    tipLbl->setWordWrap(true);
    motionLayout->addWidget(tipLbl);

    layout->addWidget(m_motionSection);

    // Text Section (Content, Typography, Formatting, Color, Box Size)
    m_textSection = new QGroupBox("🔤 Formato de Texto y Tipografía", m_contentContainer);
    m_textSection->setStyleSheet(
        "QGroupBox { color: #d29922; font-weight: bold; border: 1px solid #30363d; border-radius: 4px; margin-top: 8px; padding-top: 10px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 3px; }"
    );
    QVBoxLayout *textLayout = new QVBoxLayout(m_textSection);
    textLayout->setSpacing(6);

    // Text Editor
    textLayout->addWidget(new QLabel("Contenido del Texto (Selecciona palabras para estilo individual):", m_textSection));
    m_textEdit = new QTextEdit(m_textSection);
    m_textEdit->setPlaceholderText("Escribe tu texto aquí...");
    m_textEdit->setMinimumHeight(75);
    m_textEdit->setMaximumHeight(120);
    QFont editorFont = m_textEdit->font();
    editorFont.setPointSize(11);
    m_textEdit->setFont(editorFont);
    m_textEdit->document()->setDefaultFont(editorFont);
    m_textEdit->setStyleSheet("background-color: #0d1117; color: #ffffff; border: 1px solid #30363d; border-radius: 4px; padding: 4px; font-size: 13px;");
    connect(m_textEdit, &QTextEdit::textChanged, this, &InspectorWidget::onTextChanged);
    connect(m_textEdit, &QTextEdit::cursorPositionChanged, this, &InspectorWidget::onTextCursorPositionChanged);
    textLayout->addWidget(m_textEdit);

    // Font Family
    textLayout->addWidget(new QLabel("Fuente:", m_textSection));
    m_fontCombo = new QFontComboBox(m_textSection);
    m_fontCombo->setStyleSheet("background-color: #0d1117; color: #c9d1d9; border: 1px solid #30363d; padding: 3px;");
    connect(m_fontCombo, &QFontComboBox::currentFontChanged, this, &InspectorWidget::onFontFamilyChanged);
    textLayout->addWidget(m_fontCombo);

    // Font Size & Basic Styles (Bold, Italic, Underline)
    QHBoxLayout *styleRow = new QHBoxLayout();
    styleRow->setSpacing(4);
    styleRow->addWidget(new QLabel("Tamaño:", m_textSection));

    // Button to step down font size by 1 unit
    m_fontSizeDecBtn = new QPushButton("−", m_textSection);
    m_fontSizeDecBtn->setToolTip("Disminuir tamaño de fuente en 1 pt");
    m_fontSizeDecBtn->setFixedSize(26, 26);
    m_fontSizeDecBtn->setCursor(Qt::PointingHandCursor);
    m_fontSizeDecBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #21262d; color: #c9d1d9; border: 1px solid #30363d; border-radius: 3px; font-weight: bold; font-size: 15px;"
        "}"
        "QPushButton:hover { background-color: #30363d; color: white; }"
        "QPushButton:pressed { background-color: #388bfd; color: white; }"
    );
    connect(m_fontSizeDecBtn, &QPushButton::clicked, this, [this]() {
        m_fontSizeSpin->setValue(m_fontSizeSpin->value() - 1);
    });
    styleRow->addWidget(m_fontSizeDecBtn);

    m_fontSizeSpin = new QSpinBox(m_textSection);
    m_fontSizeSpin->setRange(8, 288);
    m_fontSizeSpin->setSingleStep(1);
    m_fontSizeSpin->setValue(48);
    m_fontSizeSpin->setSuffix(" pt");
    m_fontSizeSpin->setFixedWidth(66);
    m_fontSizeSpin->setAlignment(Qt::AlignCenter);
    m_fontSizeSpin->setStyleSheet(
        "QSpinBox { background-color: #0d1117; color: #c9d1d9; border: 1px solid #30363d; border-radius: 3px; padding: 2px 2px; font-weight: bold; }"
        "QSpinBox:focus { border-color: #58a6ff; }"
        "QSpinBox::up-button, QSpinBox::down-button { width: 14px; background-color: #21262d; border: none; }"
        "QSpinBox::up-button:hover, QSpinBox::down-button:hover { background-color: #30363d; }"
    );
    connect(m_fontSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &InspectorWidget::onFontSizeChanged);
    styleRow->addWidget(m_fontSizeSpin);

    // Button to step up font size by 1 unit
    m_fontSizeIncBtn = new QPushButton("+", m_textSection);
    m_fontSizeIncBtn->setToolTip("Aumentar tamaño de fuente en 1 pt");
    m_fontSizeIncBtn->setFixedSize(26, 26);
    m_fontSizeIncBtn->setCursor(Qt::PointingHandCursor);
    m_fontSizeIncBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #21262d; color: #c9d1d9; border: 1px solid #30363d; border-radius: 3px; font-weight: bold; font-size: 15px;"
        "}"
        "QPushButton:hover { background-color: #30363d; color: white; }"
        "QPushButton:pressed { background-color: #388bfd; color: white; }"
    );
    connect(m_fontSizeIncBtn, &QPushButton::clicked, this, [this]() {
        m_fontSizeSpin->setValue(m_fontSizeSpin->value() + 1);
    });
    styleRow->addWidget(m_fontSizeIncBtn);

    styleRow->addSpacing(6);

    auto createStyleBtn = [this](const QString &text, const QString &tooltip, const QString &customStyle) {
        QPushButton *btn = new QPushButton(text, m_textSection);
        btn->setToolTip(tooltip);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedSize(28, 26);
        btn->setStyleSheet(
            "QPushButton {"
            "  background-color: #21262d; color: #c9d1d9; border: 1px solid #30363d; border-radius: 3px; " + customStyle +
            "}"
            "QPushButton:hover { background-color: #30363d; color: white; }"
            "QPushButton:checked { background-color: #388bfd; color: white; border-color: #58a6ff; }"
        );
        return btn;
    };

    m_boldBtn = createStyleBtn("B", "Negrita (Bold)", "font-weight: bold; font-size: 13px;");
    connect(m_boldBtn, &QPushButton::toggled, this, &InspectorWidget::onBoldToggled);
    styleRow->addWidget(m_boldBtn);

    m_italicBtn = createStyleBtn("I", "Cursiva (Italic)", "font-style: italic; font-weight: bold; font-size: 13px;");
    connect(m_italicBtn, &QPushButton::toggled, this, &InspectorWidget::onItalicToggled);
    styleRow->addWidget(m_italicBtn);

    m_underlineBtn = createStyleBtn("U", "Subrayado (Underline)", "text-decoration: underline; font-size: 13px;");
    connect(m_underlineBtn, &QPushButton::toggled, this, &InspectorWidget::onUnderlineToggled);
    styleRow->addWidget(m_underlineBtn);

    styleRow->addStretch();
    textLayout->addLayout(styleRow);

    // Alignment Row
    QHBoxLayout *alignRow = new QHBoxLayout();
    alignRow->addWidget(new QLabel("Alineación:", m_textSection));

    auto createAlignBtn = [this](const QString &text, const QString &tooltip) {
        QPushButton *btn = new QPushButton(text, m_textSection);
        btn->setToolTip(tooltip);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedSize(40, 24);
        btn->setStyleSheet(
            "QPushButton { background-color: #21262d; color: #c9d1d9; border: 1px solid #30363d; border-radius: 3px; font-size: 11px; }"
            "QPushButton:hover { background-color: #30363d; color: white; }"
            "QPushButton:checked { background-color: #238636; color: white; border-color: #2ea043; }"
        );
        return btn;
    };

    m_alignLeftBtn = createAlignBtn("Izq", "Alinear a la izquierda");
    connect(m_alignLeftBtn, &QPushButton::clicked, this, &InspectorWidget::onAlignLeftClicked);
    alignRow->addWidget(m_alignLeftBtn);

    m_alignCenterBtn = createAlignBtn("Centro", "Centrar texto");
    connect(m_alignCenterBtn, &QPushButton::clicked, this, &InspectorWidget::onAlignCenterClicked);
    alignRow->addWidget(m_alignCenterBtn);

    m_alignRightBtn = createAlignBtn("Der", "Alinear a la derecha");
    connect(m_alignRightBtn, &QPushButton::clicked, this, &InspectorWidget::onAlignRightClicked);
    alignRow->addWidget(m_alignRightBtn);
    alignRow->addStretch();
    textLayout->addLayout(alignRow);

    // Text Color Row & Quick Palette
    QHBoxLayout *colorHdr = new QHBoxLayout();
    colorHdr->addWidget(new QLabel("Color del texto:", m_textSection));
    m_textColorSwatch = new QLabel(m_textSection);
    m_textColorSwatch->setFixedSize(18, 16);
    m_textColorSwatch->setStyleSheet("background-color: #ffffff; border: 1px solid #ffffff; border-radius: 2px;");
    colorHdr->addWidget(m_textColorSwatch);

    colorHdr->addStretch();
    m_textColorBtn = new QPushButton("🎨 Elegir Color...", m_textSection);
    m_textColorBtn->setCursor(Qt::PointingHandCursor);
    m_textColorBtn->setStyleSheet("QPushButton { background-color: #21262d; color: #c9d1d9; border: 1px solid #30363d; border-radius: 4px; padding: 3px 8px; font-size: 11px; } QPushButton:hover { background-color: #30363d; }");
    connect(m_textColorBtn, &QPushButton::clicked, this, &InspectorWidget::onPickTextColor);
    colorHdr->addWidget(m_textColorBtn);
    textLayout->addLayout(colorHdr);

    // Quick Color Palette
    QHBoxLayout *palRow = new QHBoxLayout();
    palRow->addWidget(new QLabel("Paleta rápida:", m_textSection));
    const QVector<QPair<QString, QString>> presets = {
        {"#ffffff", "Blanco"},
        {"#ffd700", "Amarillo"},
        {"#00d2ff", "Cian"},
        {"#ff4757", "Rojo"},
        {"#2ed573", "Verde"},
        {"#ffa502", "Naranja"},
        {"#a55eea", "Violeta"}
    };
    for (const auto &pColor : presets) {
        QPushButton *qBtn = new QPushButton(m_textSection);
        qBtn->setToolTip(pColor.second);
        qBtn->setCursor(Qt::PointingHandCursor);
        qBtn->setFixedSize(20, 20);
        qBtn->setStyleSheet(QString("QPushButton { background-color: %1; border: 1px solid #30363d; border-radius: 3px; } QPushButton:hover { border-color: white; }").arg(pColor.first));
        connect(qBtn, &QPushButton::clicked, this, [this, col = QColor(pColor.first)]() {
            onQuickColorClicked(col);
        });
        palRow->addWidget(qBtn);
    }
    palRow->addStretch();
    textLayout->addLayout(palRow);

    // Background & Box Width
    QHBoxLayout *boxRow = new QHBoxLayout();
    boxRow->setSpacing(6);
    boxRow->addWidget(new QLabel("Fondo:", m_textSection));
    m_bgColorSwatch = new QLabel(m_textSection);
    m_bgColorSwatch->setFixedSize(18, 16);
    m_bgColorSwatch->setStyleSheet("background-color: transparent; border: 1px dashed #8b949e; border-radius: 2px;");
    boxRow->addWidget(m_bgColorSwatch);

    m_bgColorBtn = new QPushButton("Color...", m_textSection);
    m_bgColorBtn->setCursor(Qt::PointingHandCursor);
    m_bgColorBtn->setStyleSheet("QPushButton { background-color: #21262d; color: #c9d1d9; border: 1px solid #30363d; border-radius: 4px; padding: 3px 6px; font-size: 11px; } QPushButton:hover { background-color: #30363d; }");
    connect(m_bgColorBtn, &QPushButton::clicked, this, &InspectorWidget::onPickBgColor);
    boxRow->addWidget(m_bgColorBtn);

    boxRow->addStretch();
    boxRow->addWidget(new QLabel("Ancho:", m_textSection));
    m_boxWidthSpin = new QSpinBox(m_textSection);
    m_boxWidthSpin->setRange(0, 1920);
    m_boxWidthSpin->setSingleStep(50);
    m_boxWidthSpin->setValue(0);
    m_boxWidthSpin->setSpecialValueText("Auto");
    m_boxWidthSpin->setSuffix(" px");
    m_boxWidthSpin->setFixedWidth(80);
    m_boxWidthSpin->setStyleSheet("background-color: #0d1117; color: #c9d1d9; border: 1px solid #30363d; padding: 2px 4px;");
    connect(m_boxWidthSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &InspectorWidget::onBoxWidthChanged);
    boxRow->addWidget(m_boxWidthSpin);

    textLayout->addLayout(boxRow);

    layout->addWidget(m_textSection);

    // Video Section
    m_videoSection = new QGroupBox("Video y Efectos Visuales", m_contentContainer);
    m_videoSection->setStyleSheet("QGroupBox { color: #58a6ff; font-weight: bold; border: 1px solid #30363d; border-radius: 4px; margin-top: 8px; padding-top: 10px; } QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 3px; }");
    QVBoxLayout *videoLayout = new QVBoxLayout(m_videoSection);
    videoLayout->setSpacing(6);

    // Opacity
    QHBoxLayout *opLayout = new QHBoxLayout();
    opLayout->addWidget(new QLabel("Opacidad:", m_videoSection));
    m_opacityLabel = new QLabel("100%", m_videoSection);
    opLayout->addStretch();
    opLayout->addWidget(m_opacityLabel);
    videoLayout->addLayout(opLayout);

    m_opacitySlider = new QSlider(Qt::Horizontal, m_videoSection);
    m_opacitySlider->setRange(0, 100);
    m_opacitySlider->setValue(100);
    connect(m_opacitySlider, &QSlider::valueChanged, this, &InspectorWidget::onOpacityChanged);
    videoLayout->addWidget(m_opacitySlider);

    // Filter
    videoLayout->addWidget(new QLabel("Filtro visual:", m_videoSection));
    m_filterCombo = new QComboBox(m_videoSection);
    for (const VisualFilterInfo &info : allVisualFilters()) {
        m_filterCombo->addItem(info.name, static_cast<int>(info.filter));
    }
    m_filterCombo->setStyleSheet("background-color: #0d1117; color: #c9d1d9; border: 1px solid #30363d; padding: 4px;");
    connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &InspectorWidget::onFilterChanged);
    videoLayout->addWidget(m_filterCombo);

    // Fades
    QHBoxLayout *fadesLayout = new QHBoxLayout();
    fadesLayout->addWidget(new QLabel("Fade In (s):", m_videoSection));
    m_videoFadeInSpin = new QDoubleSpinBox(m_videoSection);
    m_videoFadeInSpin->setRange(0.0, 5.0);
    m_videoFadeInSpin->setSingleStep(0.25);
    connect(m_videoFadeInSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &InspectorWidget::onFadeInChanged);
    fadesLayout->addWidget(m_videoFadeInSpin);

    fadesLayout->addWidget(new QLabel("Fade Out (s):", m_videoSection));
    m_videoFadeOutSpin = new QDoubleSpinBox(m_videoSection);
    m_videoFadeOutSpin->setRange(0.0, 5.0);
    m_videoFadeOutSpin->setSingleStep(0.25);
    connect(m_videoFadeOutSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &InspectorWidget::onFadeOutChanged);
    fadesLayout->addWidget(m_videoFadeOutSpin);
    videoLayout->addLayout(fadesLayout);

    layout->addWidget(m_videoSection);

    // Clip Color & Luminosity Adjustments Section
    m_clipColorSection = new QGroupBox("🎨 Ajustes de Color y Luminosidad", m_contentContainer);
    m_clipColorSection->setStyleSheet("QGroupBox { color: #58a6ff; font-weight: bold; border: 1px solid #30363d; border-radius: 4px; margin-top: 8px; padding-top: 10px; } QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 3px; }");
    QVBoxLayout *clipColorLayout = new QVBoxLayout(m_clipColorSection);
    clipColorLayout->setSpacing(6);

    // Mode Switcher Header
    QHBoxLayout *clipModeRow = new QHBoxLayout();
    clipModeRow->setSpacing(4);
    m_clipModeSlidersBtn = new QPushButton("🎚️ Deslizadores", m_clipColorSection);
    m_clipModeSlidersBtn->setCursor(Qt::PointingHandCursor);
    m_clipModeCurvesBtn = new QPushButton("📈 Gráficos / Curvas", m_clipColorSection);
    m_clipModeCurvesBtn->setCursor(Qt::PointingHandCursor);
    clipModeRow->addWidget(m_clipModeSlidersBtn);
    clipModeRow->addWidget(m_clipModeCurvesBtn);
    clipColorLayout->addLayout(clipModeRow);

    m_clipColorModeStack = new QStackedWidget(m_clipColorSection);

    // --- Page 0: Sliders ---
    m_clipSlidersPage = new QWidget(m_clipColorModeStack);
    QVBoxLayout *clipSlidersLayout = new QVBoxLayout(m_clipSlidersPage);
    clipSlidersLayout->setContentsMargins(0, 0, 0, 0);
    clipSlidersLayout->setSpacing(6);

    clipSlidersLayout->addWidget(createChannelRow(m_clipSlidersPage, "💡 Brillo:", "#f0883e", m_clipBrightnessSlider, m_clipBrightnessSpin));
    clipSlidersLayout->addWidget(createChannelRow(m_clipSlidersPage, "☀️ Luminosidad:", "#e3b341", m_clipLuminositySlider, m_clipLuminositySpin));
    clipSlidersLayout->addWidget(createChannelRow(m_clipSlidersPage, "🔴 Canal Rojo (R):", "#f85149", m_clipRedSlider, m_clipRedSpin));
    clipSlidersLayout->addWidget(createChannelRow(m_clipSlidersPage, "🟢 Canal Verde (G):", "#2ea043", m_clipGreenSlider, m_clipGreenSpin));
    clipSlidersLayout->addWidget(createChannelRow(m_clipSlidersPage, "🔵 Canal Azul (B):", "#388bfd", m_clipBlueSlider, m_clipBlueSpin));

    m_clipResetColorBtn = new QPushButton("↺ Restablecer Deslizadores", m_clipSlidersPage);
    m_clipResetColorBtn->setCursor(Qt::PointingHandCursor);
    m_clipResetColorBtn->setStyleSheet("QPushButton { background-color: #21262d; color: #c9d1d9; border: 1px solid #30363d; border-radius: 3px; padding: 4px; font-size: 11px; } QPushButton:hover { background-color: #30363d; color: white; }");
    connect(m_clipResetColorBtn, &QPushButton::clicked, this, &InspectorWidget::onClipResetColorClicked);
    clipSlidersLayout->addWidget(m_clipResetColorBtn);
    m_clipColorModeStack->addWidget(m_clipSlidersPage);

    // --- Page 1: Graphs / Curves ---
    m_clipCurvesPage = new QWidget(m_clipColorModeStack);
    QVBoxLayout *clipCurvesLayout = new QVBoxLayout(m_clipCurvesPage);
    clipCurvesLayout->setContentsMargins(0, 0, 0, 0);
    clipCurvesLayout->setSpacing(6);

    // Sub-tabs: Color Spectrum vs Luminosity vs Both
    QHBoxLayout *clipCurveTabRow = new QHBoxLayout();
    clipCurveTabRow->setSpacing(4);
    m_clipCurveTabColorBtn = new QPushButton("🎨 Color (Gradiente)", m_clipCurvesPage);
    m_clipCurveTabColorBtn->setCursor(Qt::PointingHandCursor);
    m_clipCurveTabLumaBtn = new QPushButton("☀️ Brillo y Luma", m_clipCurvesPage);
    m_clipCurveTabLumaBtn->setCursor(Qt::PointingHandCursor);
    m_clipCurveTabBothBtn = new QPushButton("📊 Ambos", m_clipCurvesPage);
    m_clipCurveTabBothBtn->setCursor(Qt::PointingHandCursor);
    clipCurveTabRow->addWidget(m_clipCurveTabColorBtn);
    clipCurveTabRow->addWidget(m_clipCurveTabLumaBtn);
    clipCurveTabRow->addWidget(m_clipCurveTabBothBtn);
    clipCurvesLayout->addLayout(clipCurveTabRow);

    auto updateClipCurveTabsStyle = [this](int activeTab) {
        auto styleTab = [](QPushButton *btn, bool active) {
            btn->setStyleSheet(active ? "QPushButton { background-color: #388bfd; color: white; border: 1px solid #58a6ff; border-radius: 3px; padding: 3px 6px; font-size: 10px; font-weight: bold; }"
                                      : "QPushButton { background-color: #21262d; color: #8b949e; border: 1px solid #30363d; border-radius: 3px; padding: 3px 6px; font-size: 10px; font-weight: bold; } QPushButton:hover { background-color: #30363d; color: white; }");
        };
        styleTab(m_clipCurveTabColorBtn, activeTab == 0);
        styleTab(m_clipCurveTabLumaBtn, activeTab == 1);
        styleTab(m_clipCurveTabBothBtn, activeTab == 2);
    };

    m_clipColorCurveWidget = new CurveEditorWidget(CurveType::ColorSpectrum, "🎨 Espectro de Color (Matiz vs Intensidad)", m_clipCurvesPage);
    m_clipLumaCurveWidget = new CurveEditorWidget(CurveType::Luma, "☀️ Brillo y Luminosidad (Curva de Tonos)", m_clipCurvesPage);

    clipCurvesLayout->addWidget(m_clipColorCurveWidget);
    clipCurvesLayout->addWidget(m_clipLumaCurveWidget);

    connect(m_clipCurveTabColorBtn, &QPushButton::clicked, this, [this, updateClipCurveTabsStyle]() {
        m_clipColorCurveWidget->show();
        m_clipLumaCurveWidget->hide();
        updateClipCurveTabsStyle(0);
    });
    connect(m_clipCurveTabLumaBtn, &QPushButton::clicked, this, [this, updateClipCurveTabsStyle]() {
        m_clipColorCurveWidget->hide();
        m_clipLumaCurveWidget->show();
        updateClipCurveTabsStyle(1);
    });
    connect(m_clipCurveTabBothBtn, &QPushButton::clicked, this, [this, updateClipCurveTabsStyle]() {
        m_clipColorCurveWidget->show();
        m_clipLumaCurveWidget->show();
        updateClipCurveTabsStyle(2);
    });
    updateClipCurveTabsStyle(0);
    m_clipLumaCurveWidget->hide(); // Initially show Color spectrum graph

    m_clipResetCurvesBtn = new QPushButton("↺ Restablecer Gráficos", m_clipCurvesPage);
    m_clipResetCurvesBtn->setCursor(Qt::PointingHandCursor);
    m_clipResetCurvesBtn->setStyleSheet("QPushButton { background-color: #21262d; color: #c9d1d9; border: 1px solid #30363d; border-radius: 3px; padding: 4px; font-size: 11px; } QPushButton:hover { background-color: #30363d; color: white; }");
    connect(m_clipResetCurvesBtn, &QPushButton::clicked, this, &InspectorWidget::onClipResetCurvesClicked);
    clipCurvesLayout->addWidget(m_clipResetCurvesBtn);

    m_clipColorModeStack->addWidget(m_clipCurvesPage);
    clipColorLayout->addWidget(m_clipColorModeStack);

    // Mode button connections
    connect(m_clipModeSlidersBtn, &QPushButton::clicked, this, [this]() {
        onClipColorGradeModeChanged(ColorGradeMode::Sliders);
    });
    connect(m_clipModeCurvesBtn, &QPushButton::clicked, this, [this]() {
        onClipColorGradeModeChanged(ColorGradeMode::Curves);
    });

    // Curve Widget signals
    connect(m_clipColorCurveWidget, &CurveEditorWidget::curveChanged, this, &InspectorWidget::onClipColorCurveChanged);
    connect(m_clipColorCurveWidget, &CurveEditorWidget::curveChangeCommitted, this, &InspectorWidget::onClipColorCurveCommitted);
    connect(m_clipLumaCurveWidget, &CurveEditorWidget::curveChanged, this, &InspectorWidget::onClipLumaCurveChanged);
    connect(m_clipLumaCurveWidget, &CurveEditorWidget::curveChangeCommitted, this, &InspectorWidget::onClipLumaCurveCommitted);

    layout->addWidget(m_clipColorSection);

    // Clip Time of Day Section (Interactive Day-to-Night Grading)
    m_clipTimeOfDayGroup = new QGroupBox("☀️ Hora del Día (Time of Day)", m_contentContainer);
    m_clipTimeOfDayGroup->setStyleSheet(
        "QGroupBox { color: #f0883e; font-weight: bold; border: 1px solid #30363d; border-radius: 4px; margin-top: 8px; padding-top: 10px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 3px; }"
    );
    QVBoxLayout *todLayout = new QVBoxLayout(m_clipTimeOfDayGroup);
    todLayout->setSpacing(6);

    QHBoxLayout *todTopRow = new QHBoxLayout();
    m_clipTimeOfDayCheck = new QCheckBox("Activar Efecto Hora del Día", m_clipTimeOfDayGroup);
    m_clipTimeOfDayCheck->setStyleSheet("QCheckBox { color: #c9d1d9; font-weight: bold; font-size: 11px; }");
    connect(m_clipTimeOfDayCheck, &QCheckBox::toggled, this, &InspectorWidget::onClipTimeOfDayToggled);
    todTopRow->addWidget(m_clipTimeOfDayCheck);
    todTopRow->addStretch();

    m_clipTimeOfDayBadge = new QLabel("☀️ Día (0.66)", m_clipTimeOfDayGroup);
    m_clipTimeOfDayBadge->setAlignment(Qt::AlignCenter);
    todTopRow->addWidget(m_clipTimeOfDayBadge);
    todLayout->addLayout(todTopRow);

    m_clipTimeOfDaySlider = new QSlider(Qt::Horizontal, m_clipTimeOfDayGroup);
    m_clipTimeOfDaySlider->setRange(0, 100);
    m_clipTimeOfDaySlider->setValue(66);
    m_clipTimeOfDaySlider->setStyleSheet(
        "QSlider::groove:horizontal { height: 6px; background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #0a1128, stop:0.33 #d47a32, stop:0.66 #79c0ff, stop:1 #e05338); border-radius: 3px; }"
        "QSlider::handle:horizontal { background: #ffffff; border: 2px solid #58a6ff; width: 14px; margin-top: -5px; margin-bottom: -5px; border-radius: 7px; }"
        "QSlider::handle:horizontal:hover { background: #58a6ff; }"
    );
    connect(m_clipTimeOfDaySlider, &QSlider::valueChanged, this, &InspectorWidget::onClipTimeOfDayChanged);
    connect(m_clipTimeOfDaySlider, &QSlider::sliderReleased, this, [this]() {
        if (m_model) m_model->saveState("Ajustar hora del día de clip");
    });
    todLayout->addWidget(m_clipTimeOfDaySlider);

    QHBoxLayout *todPresetsRow = new QHBoxLayout();
    todPresetsRow->setSpacing(4);
    auto createTodBtnHelper = [](const QString &txt, const QString &tooltip, const QString &color) {
        QPushButton *btn = new QPushButton(txt);
        btn->setToolTip(tooltip);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString(
            "QPushButton { background-color: #161b22; color: %1; border: 1px solid #30363d; border-radius: 3px; padding: 3px 4px; font-size: 10px; font-weight: bold; }"
            "QPushButton:hover { background-color: #21262d; border-color: %1; }"
        ).arg(color));
        return btn;
    };

    m_clipPresetNightBtn = createTodBtnHelper("🌙 Noche", "0.00 - Noche (-2.5 EV, Deep Midnight Blue)", "#79c0ff");
    m_clipPresetMorningBtn = createTodBtnHelper("🌅 Mañana", "0.33 - Mañana (-0.5 EV, Pastel Amber)", "#ffa657");
    m_clipPresetDayBtn = createTodBtnHelper("☀️ Día", "0.66 - Día pleno (0.0 EV, Neutro)", "#7ee787");
    m_clipPresetSunsetBtn = createTodBtnHelper("🌇 Atardecer", "1.00 - Atardecer (-0.8 EV, Crimson Sunset)", "#ff7b72");

    connect(m_clipPresetNightBtn, &QPushButton::clicked, this, [this]() {
        m_clipTimeOfDayCheck->setChecked(true);
        m_clipTimeOfDaySlider->setValue(0);
        if (m_model) m_model->saveState("Preset Hora del Día: Noche");
    });
    connect(m_clipPresetMorningBtn, &QPushButton::clicked, this, [this]() {
        m_clipTimeOfDayCheck->setChecked(true);
        m_clipTimeOfDaySlider->setValue(33);
        if (m_model) m_model->saveState("Preset Hora del Día: Mañana");
    });
    connect(m_clipPresetDayBtn, &QPushButton::clicked, this, [this]() {
        m_clipTimeOfDayCheck->setChecked(true);
        m_clipTimeOfDaySlider->setValue(66);
        if (m_model) m_model->saveState("Preset Hora del Día: Día");
    });
    connect(m_clipPresetSunsetBtn, &QPushButton::clicked, this, [this]() {
        m_clipTimeOfDayCheck->setChecked(true);
        m_clipTimeOfDaySlider->setValue(100);
        if (m_model) m_model->saveState("Preset Hora del Día: Atardecer");
    });

    todPresetsRow->addWidget(m_clipPresetNightBtn);
    todPresetsRow->addWidget(m_clipPresetMorningBtn);
    todPresetsRow->addWidget(m_clipPresetDayBtn);
    todPresetsRow->addWidget(m_clipPresetSunsetBtn);
    todLayout->addLayout(todPresetsRow);

    updateTimeOfDayBadge(m_clipTimeOfDayBadge, 0.66f);
    layout->addWidget(m_clipTimeOfDayGroup);

    // Connect Clip Color controls
    connect(m_clipBrightnessSlider, &QSlider::valueChanged, this, [this](int val) {
        if (!m_updatingUi) {
            m_clipBrightnessSpin->blockSignals(true);
            m_clipBrightnessSpin->setValue(val);
            m_clipBrightnessSpin->blockSignals(false);
            onClipBrightnessChanged(val);
        }
    });
    connect(m_clipBrightnessSlider, &QSlider::sliderReleased, this, [this]() {
        if (m_model) m_model->saveState("Ajustar brillo de clip");
    });
    connect(m_clipBrightnessSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        if (!m_updatingUi) {
            m_clipBrightnessSlider->blockSignals(true);
            m_clipBrightnessSlider->setValue(val);
            m_clipBrightnessSlider->blockSignals(false);
            onClipBrightnessChanged(val);
            if (m_model) m_model->saveState("Ajustar brillo de clip");
        }
    });

    connect(m_clipLuminositySlider, &QSlider::valueChanged, this, [this](int val) {
        if (!m_updatingUi) {
            m_clipLuminositySpin->blockSignals(true);
            m_clipLuminositySpin->setValue(val);
            m_clipLuminositySpin->blockSignals(false);
            onClipLuminosityChanged(val);
        }
    });
    connect(m_clipLuminositySlider, &QSlider::sliderReleased, this, [this]() {
        if (m_model) m_model->saveState("Ajustar luminosidad de clip");
    });
    connect(m_clipLuminositySpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        if (!m_updatingUi) {
            m_clipLuminositySlider->blockSignals(true);
            m_clipLuminositySlider->setValue(val);
            m_clipLuminositySlider->blockSignals(false);
            onClipLuminosityChanged(val);
            if (m_model) m_model->saveState("Ajustar luminosidad de clip");
        }
    });

    connect(m_clipRedSlider, &QSlider::valueChanged, this, [this](int val) {
        if (!m_updatingUi) {
            m_clipRedSpin->blockSignals(true);
            m_clipRedSpin->setValue(val);
            m_clipRedSpin->blockSignals(false);
            onClipRedChanged(val);
        }
    });
    connect(m_clipRedSlider, &QSlider::sliderReleased, this, [this]() {
        if (m_model) m_model->saveState("Ajustar canal rojo de clip");
    });
    connect(m_clipRedSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        if (!m_updatingUi) {
            m_clipRedSlider->blockSignals(true);
            m_clipRedSlider->setValue(val);
            m_clipRedSlider->blockSignals(false);
            onClipRedChanged(val);
            if (m_model) m_model->saveState("Ajustar canal rojo de clip");
        }
    });

    connect(m_clipGreenSlider, &QSlider::valueChanged, this, [this](int val) {
        if (!m_updatingUi) {
            m_clipGreenSpin->blockSignals(true);
            m_clipGreenSpin->setValue(val);
            m_clipGreenSpin->blockSignals(false);
            onClipGreenChanged(val);
        }
    });
    connect(m_clipGreenSlider, &QSlider::sliderReleased, this, [this]() {
        if (m_model) m_model->saveState("Ajustar canal verde de clip");
    });
    connect(m_clipGreenSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        if (!m_updatingUi) {
            m_clipGreenSlider->blockSignals(true);
            m_clipGreenSlider->setValue(val);
            m_clipGreenSpin->blockSignals(false);
            onClipGreenChanged(val);
            if (m_model) m_model->saveState("Ajustar canal verde de clip");
        }
    });

    connect(m_clipBlueSlider, &QSlider::valueChanged, this, [this](int val) {
        if (!m_updatingUi) {
            m_clipBlueSpin->blockSignals(true);
            m_clipBlueSpin->setValue(val);
            m_clipBlueSpin->blockSignals(false);
            onClipBlueChanged(val);
        }
    });
    connect(m_clipBlueSlider, &QSlider::sliderReleased, this, [this]() {
        if (m_model) m_model->saveState("Ajustar canal azul de clip");
    });
    connect(m_clipBlueSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        if (!m_updatingUi) {
            m_clipBlueSlider->blockSignals(true);
            m_clipBlueSlider->setValue(val);
            m_clipBlueSpin->blockSignals(false);
            onClipBlueChanged(val);
            if (m_model) m_model->saveState("Ajustar canal azul de clip");
        }
    });

    // Effects Stack Section
    m_effectsSection = new QGroupBox("✨ Pila de Efectos (Efectos Aplicados)", m_contentContainer);
    m_effectsSection->setStyleSheet("QGroupBox { color: #58a6ff; font-weight: bold; border: 1px solid #30363d; border-radius: 4px; margin-top: 8px; padding-top: 10px; } QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 3px; }");
    QVBoxLayout *effectsLayout = new QVBoxLayout(m_effectsSection);
    effectsLayout->setSpacing(6);

    QLabel *hierarchyInfo = new QLabel("⬆ Los efectos superiores aplican sobre los inferiores", m_effectsSection);
    hierarchyInfo->setStyleSheet("color: #8b949e; font-size: 10px; font-style: italic;");
    effectsLayout->addWidget(hierarchyInfo);

    QHBoxLayout *effectsHdr = new QHBoxLayout();
    m_addEffectBtn = new QPushButton("➕ Agregar Efecto", m_effectsSection);
    m_addEffectBtn->setStyleSheet(
        "QPushButton { background-color: #238636; color: white; border: 1px solid #2ea043; border-radius: 4px; padding: 5px 12px; font-weight: bold; font-size: 11px; }"
        "QPushButton:hover { background-color: #2ea043; }"
        "QPushButton:pressed { background-color: #1a6b2c; }"
    );
    m_addEffectBtn->setToolTip("Añadir un nuevo efecto a la pila");
    connect(m_addEffectBtn, &QPushButton::clicked, this, &InspectorWidget::onAddEffectClicked);
    effectsHdr->addWidget(m_addEffectBtn);

    effectsHdr->addStretch();

    m_effectsCountLabel = new QLabel("0 efectos", m_effectsSection);
    m_effectsCountLabel->setStyleSheet("color: #8b949e; font-size: 10px;");
    effectsHdr->addWidget(m_effectsCountLabel);
    effectsLayout->addLayout(effectsHdr);

    m_emptyEffectsLabel = new QLabel("Sin efectos aplicados.\nHaz clic en '➕ Agregar Efecto' para añadir uno a la pila.", m_effectsSection);
    m_emptyEffectsLabel->setAlignment(Qt::AlignCenter);
    m_emptyEffectsLabel->setStyleSheet("color: #8b949e; font-size: 11px; padding: 14px; border: 1px dashed #30363d; border-radius: 4px; background-color: #0d1117;");
    effectsLayout->addWidget(m_emptyEffectsLabel);

    m_effectsList = new EffectsListWidget(m_effectsSection);
    connect(m_effectsList, &EffectsListWidget::effectReordered, this, &InspectorWidget::onEffectReordered);
    effectsLayout->addWidget(m_effectsList);

    QHBoxLayout *effectsFooter = new QHBoxLayout();
    m_clearEffectsBtn = new QPushButton("Limpiar todos", m_effectsSection);
    m_clearEffectsBtn->setStyleSheet(
        "QPushButton { background-color: #21262d; color: #f85149; border: 1px solid #30363d; border-radius: 3px; padding: 3px 8px; font-size: 10px; }"
        "QPushButton:hover { background-color: #da3633; color: white; }"
    );
    connect(m_clearEffectsBtn, &QPushButton::clicked, this, &InspectorWidget::onClearEffectsClicked);
    effectsFooter->addStretch();
    effectsFooter->addWidget(m_clearEffectsBtn);
    effectsLayout->addLayout(effectsFooter);

    layout->addWidget(m_effectsSection);

    // Audio Section
    m_audioSection = new QGroupBox("Audio y Volumen", m_contentContainer);
    m_audioSection->setStyleSheet("QGroupBox { color: #58a6ff; font-weight: bold; border: 1px solid #30363d; border-radius: 4px; margin-top: 8px; padding-top: 10px; } QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 3px; }");
    QVBoxLayout *audioLayout = new QVBoxLayout(m_audioSection);
    audioLayout->setSpacing(6);

    QHBoxLayout *volLayout = new QHBoxLayout();
    volLayout->addWidget(new QLabel("Volumen:", m_audioSection));
    m_volumeLabel = new QLabel("100%", m_audioSection);
    volLayout->addStretch();
    volLayout->addWidget(m_volumeLabel);
    audioLayout->addLayout(volLayout);

    m_volumeSlider = new QSlider(Qt::Horizontal, m_audioSection);
    m_volumeSlider->setRange(0, 200);
    m_volumeSlider->setValue(100);
    connect(m_volumeSlider, &QSlider::valueChanged, this, &InspectorWidget::onVolumeChanged);
    audioLayout->addWidget(m_volumeSlider);

    QHBoxLayout *audioFadesLayout = new QHBoxLayout();
    audioFadesLayout->addWidget(new QLabel("Fade In (s):", m_audioSection));
    m_audioFadeInSpin = new QDoubleSpinBox(m_audioSection);
    m_audioFadeInSpin->setRange(0.0, 5.0);
    m_audioFadeInSpin->setSingleStep(0.25);
    connect(m_audioFadeInSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &InspectorWidget::onFadeInChanged);
    audioFadesLayout->addWidget(m_audioFadeInSpin);

    audioFadesLayout->addWidget(new QLabel("Fade Out (s):", m_audioSection));
    m_audioFadeOutSpin = new QDoubleSpinBox(m_audioSection);
    m_audioFadeOutSpin->setRange(0.0, 5.0);
    m_audioFadeOutSpin->setSingleStep(0.25);
    connect(m_audioFadeOutSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &InspectorWidget::onFadeOutChanged);
    audioFadesLayout->addWidget(m_audioFadeOutSpin);
    audioLayout->addLayout(audioFadesLayout);

    m_muteAudioCheck = new QCheckBox("Silenciar audio de este clip", m_audioSection);
    connect(m_muteAudioCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_updatingUi) return;
        if (TimelineClip *c = m_model->findClip(m_selectedClipId)) {
            c->setAudioMuted(checked);
            emit clipPropertyModified(m_selectedClipId);
        }
    });
    audioLayout->addWidget(m_muteAudioCheck);

    layout->addWidget(m_audioSection);

    // Speed Section
    QGroupBox *speedGroup = new QGroupBox("Velocidad de Reproducción", m_contentContainer);
    speedGroup->setStyleSheet("QGroupBox { color: #58a6ff; font-weight: bold; border: 1px solid #30363d; border-radius: 4px; margin-top: 8px; padding-top: 10px; } QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 3px; }");
    QVBoxLayout *speedLayout = new QVBoxLayout(speedGroup);

    QHBoxLayout *speedBtnsLayout = new QHBoxLayout();
    auto addSpeedBtn = [this, speedBtnsLayout, speedGroup](const QString &label, double speed) {
        QPushButton *btn = new QPushButton(label, speedGroup);
        btn->setStyleSheet("background-color: #21262d; color: #c9d1d9; border: 1px solid #30363d; border-radius: 3px; padding: 4px; font-size: 11px;");
        connect(btn, &QPushButton::clicked, this, [this, speed]() {
            onSpeedPresetClicked(speed);
        });
        speedBtnsLayout->addWidget(btn);
    };
    addSpeedBtn("0.5x", 0.5);
    addSpeedBtn("0.75x", 0.75);
    addSpeedBtn("1x", 1.0);
    addSpeedBtn("1.5x", 1.5);
    addSpeedBtn("2x", 2.0);
    speedLayout->addLayout(speedBtnsLayout);

    layout->addWidget(speedGroup);

    // Actions
    QGroupBox *actionsGroup = new QGroupBox("Acciones Rápidas", m_contentContainer);
    actionsGroup->setStyleSheet("QGroupBox { color: #58a6ff; font-weight: bold; border: 1px solid #30363d; border-radius: 4px; margin-top: 8px; padding-top: 10px; } QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 3px; }");
    QVBoxLayout *actionsLayout = new QVBoxLayout(actionsGroup);

    m_splitBtn = new QPushButton("Cortar en el cabezal (Split)", actionsGroup);
    m_splitBtn->setStyleSheet("background-color: #21262d; color: #c9d1d9; border: 1px solid #30363d; border-radius: 4px; padding: 6px;");
    connect(m_splitBtn, &QPushButton::clicked, this, [this]() {
        emit splitRequested(m_selectedClipId);
    });
    actionsLayout->addWidget(m_splitBtn);

    m_duplicateBtn = new QPushButton("Duplicar clip", actionsGroup);
    m_duplicateBtn->setStyleSheet("background-color: #21262d; color: #c9d1d9; border: 1px solid #30363d; border-radius: 4px; padding: 6px;");
    connect(m_duplicateBtn, &QPushButton::clicked, this, [this]() {
        emit duplicateRequested(m_selectedClipId);
    });
    actionsLayout->addWidget(m_duplicateBtn);

    m_deleteBtn = new QPushButton("Eliminar clip", actionsGroup);
    m_deleteBtn->setStyleSheet("background-color: #b62324; color: white; border-radius: 4px; padding: 6px; font-weight: bold;");
    connect(m_deleteBtn, &QPushButton::clicked, this, [this]() {
        emit deleteRequested(m_selectedClipId);
    });
    actionsLayout->addWidget(m_deleteBtn);

    layout->addWidget(actionsGroup);
    layout->addStretch();

    scrollArea->setWidget(m_contentContainer);
    clipPageLayout->addWidget(scrollArea, 1);
    m_contentContainer->hide();
    m_viewStack->addWidget(m_clipPage);

    // ================= PAGE 1: GLOBAL / MASTER PROPERTIES =================
    m_globalPage = new QWidget(m_viewStack);
    QVBoxLayout *globalPageLayout = new QVBoxLayout(m_globalPage);
    globalPageLayout->setContentsMargins(0, 0, 0, 0);
    globalPageLayout->setSpacing(6);

    QScrollArea *globalScrollArea = new QScrollArea(m_globalPage);
    globalScrollArea->setWidgetResizable(true);
    globalScrollArea->setFrameShape(QFrame::NoFrame);
    globalScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    globalScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    globalScrollArea->setStyleSheet("background: transparent;");

    m_globalContainer = new QWidget(globalScrollArea);
    m_globalContainer->setMinimumWidth(320);
    QVBoxLayout *gLayout = new QVBoxLayout(m_globalContainer);
    gLayout->setContentsMargins(0, 0, 4, 0);
    gLayout->setSpacing(8);

    // Global Title Header
    QGroupBox *headerGroup = new QGroupBox("🌐 Cuadro de Propiedades Generales", m_globalContainer);
    headerGroup->setStyleSheet("QGroupBox { color: #58a6ff; font-weight: bold; border: 1px solid #30363d; border-radius: 4px; margin-top: 8px; padding-top: 10px; } QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 3px; }");
    QVBoxLayout *hLayout = new QVBoxLayout(headerGroup);
    QLabel *gDesc = new QLabel("Ajustes globales de luminosidad, brillo y presencia RGB aplicados a todo el video (tanto en previsualizador como en exportación final).", headerGroup);
    gDesc->setStyleSheet("color: #8b949e; font-size: 11px;");
    gDesc->setWordWrap(true);
    hLayout->addWidget(gDesc);
    gLayout->addWidget(headerGroup);

    // Color Correction Group (Global)
    QGroupBox *gColorGroup = new QGroupBox("🎨 Corrección de Color y Luminosidad General", m_globalContainer);
    gColorGroup->setStyleSheet("QGroupBox { color: #58a6ff; font-weight: bold; border: 1px solid #30363d; border-radius: 4px; margin-top: 8px; padding-top: 10px; } QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 3px; }");
    QVBoxLayout *gcLayout = new QVBoxLayout(gColorGroup);
    gcLayout->setSpacing(6);

    // Global Mode Switcher Header
    QHBoxLayout *globalModeRow = new QHBoxLayout();
    globalModeRow->setSpacing(4);
    m_globalModeSlidersBtn = new QPushButton("🎚️ Deslizadores", gColorGroup);
    m_globalModeSlidersBtn->setCursor(Qt::PointingHandCursor);
    m_globalModeCurvesBtn = new QPushButton("📈 Gráficos / Curvas", gColorGroup);
    m_globalModeCurvesBtn->setCursor(Qt::PointingHandCursor);
    globalModeRow->addWidget(m_globalModeSlidersBtn);
    globalModeRow->addWidget(m_globalModeCurvesBtn);
    gcLayout->addLayout(globalModeRow);

    m_globalColorModeStack = new QStackedWidget(gColorGroup);

    // --- Page 0: Global Sliders ---
    m_globalSlidersPage = new QWidget(m_globalColorModeStack);
    QVBoxLayout *globalSlidersLayout = new QVBoxLayout(m_globalSlidersPage);
    globalSlidersLayout->setContentsMargins(0, 0, 0, 0);
    globalSlidersLayout->setSpacing(6);

    globalSlidersLayout->addWidget(createChannelRow(m_globalSlidersPage, "💡 Brillo General:", "#f0883e", m_globalBrightnessSlider, m_globalBrightnessSpin));
    globalSlidersLayout->addWidget(createChannelRow(m_globalSlidersPage, "☀️ Luminosidad General:", "#e3b341", m_globalLuminositySlider, m_globalLuminositySpin));
    globalSlidersLayout->addWidget(createChannelRow(m_globalSlidersPage, "🔴 Presencia Canal Rojo (R):", "#f85149", m_globalRedSlider, m_globalRedSpin));
    globalSlidersLayout->addWidget(createChannelRow(m_globalSlidersPage, "🟢 Presencia Canal Verde (G):", "#2ea043", m_globalGreenSlider, m_globalGreenSpin));
    globalSlidersLayout->addWidget(createChannelRow(m_globalSlidersPage, "🔵 Presencia Canal Azul (B):", "#388bfd", m_globalBlueSlider, m_globalBlueSpin));

    m_globalResetColorBtn = new QPushButton("↺ Restablecer Deslizadores Generales", m_globalSlidersPage);
    m_globalResetColorBtn->setCursor(Qt::PointingHandCursor);
    m_globalResetColorBtn->setStyleSheet("QPushButton { background-color: #21262d; color: #c9d1d9; border: 1px solid #30363d; border-radius: 3px; padding: 5px; font-weight: bold; font-size: 11px; } QPushButton:hover { background-color: #30363d; color: white; }");
    connect(m_globalResetColorBtn, &QPushButton::clicked, this, &InspectorWidget::onGlobalResetColorClicked);
    globalSlidersLayout->addWidget(m_globalResetColorBtn);
    m_globalColorModeStack->addWidget(m_globalSlidersPage);

    // --- Page 1: Global Curves ---
    m_globalCurvesPage = new QWidget(m_globalColorModeStack);
    QVBoxLayout *globalCurvesLayout = new QVBoxLayout(m_globalCurvesPage);
    globalCurvesLayout->setContentsMargins(0, 0, 0, 0);
    globalCurvesLayout->setSpacing(6);

    QHBoxLayout *globalCurveTabRow = new QHBoxLayout();
    globalCurveTabRow->setSpacing(4);
    m_globalCurveTabColorBtn = new QPushButton("🎨 Color (Gradiente)", m_globalCurvesPage);
    m_globalCurveTabColorBtn->setCursor(Qt::PointingHandCursor);
    m_globalCurveTabLumaBtn = new QPushButton("☀️ Brillo y Luma", m_globalCurvesPage);
    m_globalCurveTabLumaBtn->setCursor(Qt::PointingHandCursor);
    m_globalCurveTabBothBtn = new QPushButton("📊 Ambos", m_globalCurvesPage);
    m_globalCurveTabBothBtn->setCursor(Qt::PointingHandCursor);
    globalCurveTabRow->addWidget(m_globalCurveTabColorBtn);
    globalCurveTabRow->addWidget(m_globalCurveTabLumaBtn);
    globalCurveTabRow->addWidget(m_globalCurveTabBothBtn);
    globalCurvesLayout->addLayout(globalCurveTabRow);

    auto updateGlobalCurveTabsStyle = [this](int activeTab) {
        auto styleTab = [](QPushButton *btn, bool active) {
            btn->setStyleSheet(active ? "QPushButton { background-color: #388bfd; color: white; border: 1px solid #58a6ff; border-radius: 3px; padding: 3px 6px; font-size: 10px; font-weight: bold; }"
                                      : "QPushButton { background-color: #21262d; color: #8b949e; border: 1px solid #30363d; border-radius: 3px; padding: 3px 6px; font-size: 10px; font-weight: bold; } QPushButton:hover { background-color: #30363d; color: white; }");
        };
        styleTab(m_globalCurveTabColorBtn, activeTab == 0);
        styleTab(m_globalCurveTabLumaBtn, activeTab == 1);
        styleTab(m_globalCurveTabBothBtn, activeTab == 2);
    };

    m_globalColorCurveWidget = new CurveEditorWidget(CurveType::ColorSpectrum, "🎨 Espectro de Color General (Matiz vs Intensidad)", m_globalCurvesPage);
    m_globalLumaCurveWidget = new CurveEditorWidget(CurveType::Luma, "☀️ Brillo y Luminosidad General (Tonos)", m_globalCurvesPage);

    globalCurvesLayout->addWidget(m_globalColorCurveWidget);
    globalCurvesLayout->addWidget(m_globalLumaCurveWidget);

    connect(m_globalCurveTabColorBtn, &QPushButton::clicked, this, [this, updateGlobalCurveTabsStyle]() {
        m_globalColorCurveWidget->show();
        m_globalLumaCurveWidget->hide();
        updateGlobalCurveTabsStyle(0);
    });
    connect(m_globalCurveTabLumaBtn, &QPushButton::clicked, this, [this, updateGlobalCurveTabsStyle]() {
        m_globalColorCurveWidget->hide();
        m_globalLumaCurveWidget->show();
        updateGlobalCurveTabsStyle(1);
    });
    connect(m_globalCurveTabBothBtn, &QPushButton::clicked, this, [this, updateGlobalCurveTabsStyle]() {
        m_globalColorCurveWidget->show();
        m_globalLumaCurveWidget->show();
        updateGlobalCurveTabsStyle(2);
    });
    updateGlobalCurveTabsStyle(0);
    m_globalLumaCurveWidget->hide();

    m_globalResetCurvesBtn = new QPushButton("↺ Restablecer Gráficos Generales", m_globalCurvesPage);
    m_globalResetCurvesBtn->setCursor(Qt::PointingHandCursor);
    m_globalResetCurvesBtn->setStyleSheet("QPushButton { background-color: #21262d; color: #c9d1d9; border: 1px solid #30363d; border-radius: 3px; padding: 5px; font-weight: bold; font-size: 11px; } QPushButton:hover { background-color: #30363d; color: white; }");
    connect(m_globalResetCurvesBtn, &QPushButton::clicked, this, &InspectorWidget::onGlobalResetCurvesClicked);
    globalCurvesLayout->addWidget(m_globalResetCurvesBtn);

    m_globalColorModeStack->addWidget(m_globalCurvesPage);
    gcLayout->addWidget(m_globalColorModeStack);

    // Mode connections
    connect(m_globalModeSlidersBtn, &QPushButton::clicked, this, [this]() {
        onGlobalColorGradeModeChanged(ColorGradeMode::Sliders);
    });
    connect(m_globalModeCurvesBtn, &QPushButton::clicked, this, [this]() {
        onGlobalColorGradeModeChanged(ColorGradeMode::Curves);
    });

    // Curve Widget signals
    connect(m_globalColorCurveWidget, &CurveEditorWidget::curveChanged, this, &InspectorWidget::onGlobalColorCurveChanged);
    connect(m_globalColorCurveWidget, &CurveEditorWidget::curveChangeCommitted, this, &InspectorWidget::onGlobalColorCurveCommitted);
    connect(m_globalLumaCurveWidget, &CurveEditorWidget::curveChanged, this, &InspectorWidget::onGlobalLumaCurveChanged);
    connect(m_globalLumaCurveWidget, &CurveEditorWidget::curveChangeCommitted, this, &InspectorWidget::onGlobalLumaCurveCommitted);

    gLayout->addWidget(gColorGroup);

    // Global Time of Day Section (Interactive Day-to-Night Grading Master)
    m_globalTimeOfDayGroup = new QGroupBox("☀️ Hora del Día Global (Master)", m_globalContainer);
    m_globalTimeOfDayGroup->setStyleSheet(
        "QGroupBox { color: #f0883e; font-weight: bold; border: 1px solid #30363d; border-radius: 4px; margin-top: 8px; padding-top: 10px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 3px; }"
    );
    QVBoxLayout *gTodLayout = new QVBoxLayout(m_globalTimeOfDayGroup);
    gTodLayout->setSpacing(6);

    QHBoxLayout *gTodTopRow = new QHBoxLayout();
    m_globalTimeOfDayCheck = new QCheckBox("Activar Efecto Hora del Día Master", m_globalTimeOfDayGroup);
    m_globalTimeOfDayCheck->setStyleSheet("QCheckBox { color: #c9d1d9; font-weight: bold; font-size: 11px; }");
    connect(m_globalTimeOfDayCheck, &QCheckBox::toggled, this, &InspectorWidget::onGlobalTimeOfDayToggled);
    gTodTopRow->addWidget(m_globalTimeOfDayCheck);
    gTodTopRow->addStretch();

    m_globalTimeOfDayBadge = new QLabel("☀️ Día (0.66)", m_globalTimeOfDayGroup);
    m_globalTimeOfDayBadge->setAlignment(Qt::AlignCenter);
    gTodTopRow->addWidget(m_globalTimeOfDayBadge);
    gTodLayout->addLayout(gTodTopRow);

    m_globalTimeOfDaySlider = new QSlider(Qt::Horizontal, m_globalTimeOfDayGroup);
    m_globalTimeOfDaySlider->setRange(0, 100);
    m_globalTimeOfDaySlider->setValue(66);
    m_globalTimeOfDaySlider->setStyleSheet(
        "QSlider::groove:horizontal { height: 6px; background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #0a1128, stop:0.33 #d47a32, stop:0.66 #79c0ff, stop:1 #e05338); border-radius: 3px; }"
        "QSlider::handle:horizontal { background: #ffffff; border: 2px solid #58a6ff; width: 14px; margin-top: -5px; margin-bottom: -5px; border-radius: 7px; }"
        "QSlider::handle:horizontal:hover { background: #58a6ff; }"
    );
    connect(m_globalTimeOfDaySlider, &QSlider::valueChanged, this, &InspectorWidget::onGlobalTimeOfDayChanged);
    connect(m_globalTimeOfDaySlider, &QSlider::sliderReleased, this, [this]() {
        if (m_model) m_model->saveState("Ajustar hora del día general");
    });
    gTodLayout->addWidget(m_globalTimeOfDaySlider);

    QHBoxLayout *gTodPresetsRow = new QHBoxLayout();
    gTodPresetsRow->setSpacing(4);
    auto createGTodBtnHelper = [](const QString &txt, const QString &tooltip, const QString &color) {
        QPushButton *btn = new QPushButton(txt);
        btn->setToolTip(tooltip);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString(
            "QPushButton { background-color: #161b22; color: %1; border: 1px solid #30363d; border-radius: 3px; padding: 3px 4px; font-size: 10px; font-weight: bold; }"
            "QPushButton:hover { background-color: #21262d; border-color: %1; }"
        ).arg(color));
        return btn;
    };

    m_globalPresetNightBtn = createGTodBtnHelper("🌙 Noche", "0.00 - Noche profunda (-2.5 EV, Deep Midnight Blue)", "#79c0ff");
    m_globalPresetMorningBtn = createGTodBtnHelper("🌅 Mañana", "0.33 - Amanecer (-0.5 EV, Pastel Amber)", "#ffa657");
    m_globalPresetDayBtn = createGTodBtnHelper("☀️ Día", "0.66 - Día pleno (-0.0 EV, Neutro)", "#7ee787");
    m_globalPresetSunsetBtn = createGTodBtnHelper("🌇 Atardecer", "1.00 - Atardecer (-0.8 EV, Crimson Sunset)", "#ff7b72");

    connect(m_globalPresetNightBtn, &QPushButton::clicked, this, [this]() {
        m_globalTimeOfDayCheck->setChecked(true);
        m_globalTimeOfDaySlider->setValue(0);
        if (m_model) m_model->saveState("Preset Hora del Día General: Noche");
    });
    connect(m_globalPresetMorningBtn, &QPushButton::clicked, this, [this]() {
        m_globalTimeOfDayCheck->setChecked(true);
        m_globalTimeOfDaySlider->setValue(33);
        if (m_model) m_model->saveState("Preset Hora del Día General: Mañana");
    });
    connect(m_globalPresetDayBtn, &QPushButton::clicked, this, [this]() {
        m_globalTimeOfDayCheck->setChecked(true);
        m_globalTimeOfDaySlider->setValue(66);
        if (m_model) m_model->saveState("Preset Hora del Día General: Día");
    });
    connect(m_globalPresetSunsetBtn, &QPushButton::clicked, this, [this]() {
        m_globalTimeOfDayCheck->setChecked(true);
        m_globalTimeOfDaySlider->setValue(100);
        if (m_model) m_model->saveState("Preset Hora del Día General: Atardecer");
    });

    gTodPresetsRow->addWidget(m_globalPresetNightBtn);
    gTodPresetsRow->addWidget(m_globalPresetMorningBtn);
    gTodPresetsRow->addWidget(m_globalPresetDayBtn);
    gTodPresetsRow->addWidget(m_globalPresetSunsetBtn);
    gTodLayout->addLayout(gTodPresetsRow);

    updateTimeOfDayBadge(m_globalTimeOfDayBadge, 0.66f);
    gLayout->addWidget(m_globalTimeOfDayGroup);

    // Global signals
    connect(m_globalBrightnessSlider, &QSlider::valueChanged, this, [this](int val) {
        if (!m_updatingUi) {
            m_globalBrightnessSpin->blockSignals(true);
            m_globalBrightnessSpin->setValue(val);
            m_globalBrightnessSpin->blockSignals(false);
            onGlobalBrightnessChanged(val);
        }
    });
    connect(m_globalBrightnessSlider, &QSlider::sliderReleased, this, [this]() {
        if (m_model) m_model->saveState("Ajustar brillo general");
    });
    connect(m_globalBrightnessSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        if (!m_updatingUi) {
            m_globalBrightnessSlider->blockSignals(true);
            m_globalBrightnessSlider->setValue(val);
            m_globalBrightnessSlider->blockSignals(false);
            onGlobalBrightnessChanged(val);
            if (m_model) m_model->saveState("Ajustar brillo general");
        }
    });

    connect(m_globalLuminositySlider, &QSlider::valueChanged, this, [this](int val) {
        if (!m_updatingUi) {
            m_globalLuminositySpin->blockSignals(true);
            m_globalLuminositySpin->setValue(val);
            m_globalLuminositySpin->blockSignals(false);
            onGlobalLuminosityChanged(val);
        }
    });
    connect(m_globalLuminositySlider, &QSlider::sliderReleased, this, [this]() {
        if (m_model) m_model->saveState("Ajustar luminosidad general");
    });
    connect(m_globalLuminositySpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        if (!m_updatingUi) {
            m_globalLuminositySlider->blockSignals(true);
            m_globalLuminositySlider->setValue(val);
            m_globalLuminositySlider->blockSignals(false);
            onGlobalLuminosityChanged(val);
            if (m_model) m_model->saveState("Ajustar luminosidad general");
        }
    });

    connect(m_globalRedSlider, &QSlider::valueChanged, this, [this](int val) {
        if (!m_updatingUi) {
            m_globalRedSpin->blockSignals(true);
            m_globalRedSpin->setValue(val);
            m_globalRedSpin->blockSignals(false);
            onGlobalRedChanged(val);
        }
    });
    connect(m_globalRedSlider, &QSlider::sliderReleased, this, [this]() {
        if (m_model) m_model->saveState("Ajustar canal rojo general");
    });
    connect(m_globalRedSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        if (!m_updatingUi) {
            m_globalRedSlider->blockSignals(true);
            m_globalRedSlider->setValue(val);
            m_globalRedSlider->blockSignals(false);
            onGlobalRedChanged(val);
            if (m_model) m_model->saveState("Ajustar canal rojo general");
        }
    });

    connect(m_globalGreenSlider, &QSlider::valueChanged, this, [this](int val) {
        if (!m_updatingUi) {
            m_globalGreenSpin->blockSignals(true);
            m_globalGreenSpin->setValue(val);
            m_globalGreenSpin->blockSignals(false);
            onGlobalGreenChanged(val);
        }
    });
    connect(m_globalGreenSlider, &QSlider::sliderReleased, this, [this]() {
        if (m_model) m_model->saveState("Ajustar canal verde general");
    });
    connect(m_globalGreenSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        if (!m_updatingUi) {
            m_globalGreenSlider->blockSignals(true);
            m_globalGreenSlider->setValue(val);
            m_globalGreenSpin->blockSignals(false);
            onGlobalGreenChanged(val);
            if (m_model) m_model->saveState("Ajustar canal verde general");
        }
    });

    connect(m_globalBlueSlider, &QSlider::valueChanged, this, [this](int val) {
        if (!m_updatingUi) {
            m_globalBlueSpin->blockSignals(true);
            m_globalBlueSpin->setValue(val);
            m_globalBlueSpin->blockSignals(false);
            onGlobalBlueChanged(val);
        }
    });
    connect(m_globalBlueSlider, &QSlider::sliderReleased, this, [this]() {
        if (m_model) m_model->saveState("Ajustar canal azul general");
    });
    connect(m_globalBlueSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        if (!m_updatingUi) {
            m_globalBlueSlider->blockSignals(true);
            m_globalBlueSlider->setValue(val);
            m_globalBlueSpin->blockSignals(false);
            onGlobalBlueChanged(val);
            if (m_model) m_model->saveState("Ajustar canal azul general");
        }
    });

    // Project Info Group
    QGroupBox *projGroup = new QGroupBox("ℹ️ Información del Proyecto", m_globalContainer);
    projGroup->setStyleSheet("QGroupBox { color: #58a6ff; font-weight: bold; border: 1px solid #30363d; border-radius: 4px; margin-top: 8px; padding-top: 10px; } QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 3px; }");
    QVBoxLayout *pLayout = new QVBoxLayout(projGroup);
    pLayout->setSpacing(4);

    m_projectDurationLabel = new QLabel(projGroup);
    m_projectDurationLabel->setStyleSheet("color: #c9d1d9; font-size: 11px;");
    pLayout->addWidget(m_projectDurationLabel);

    m_projectTracksLabel = new QLabel(projGroup);
    m_projectTracksLabel->setStyleSheet("color: #8b949e; font-size: 11px;");
    pLayout->addWidget(m_projectTracksLabel);

    m_projectClipsLabel = new QLabel(projGroup);
    m_projectClipsLabel->setStyleSheet("color: #8b949e; font-size: 11px;");
    pLayout->addWidget(m_projectClipsLabel);

    m_globalExportBtn = new QPushButton("🎬 Exportar Video...", projGroup);
    m_globalExportBtn->setCursor(Qt::PointingHandCursor);
    m_globalExportBtn->setStyleSheet("QPushButton { background-color: #238636; color: white; border-radius: 4px; padding: 6px 12px; font-weight: bold; font-size: 12px; } QPushButton:hover { background-color: #2ea043; }");
    connect(m_globalExportBtn, &QPushButton::clicked, this, &InspectorWidget::exportRequested);
    pLayout->addWidget(m_globalExportBtn);

    gLayout->addWidget(projGroup);
    gLayout->addStretch();

    globalScrollArea->setWidget(m_globalContainer);
    globalPageLayout->addWidget(globalScrollArea, 1);
    m_viewStack->addWidget(m_globalPage);

    rootLayout->addWidget(m_viewStack, 1);

    if (m_model) {
        connect(m_model, &TimelineModel::globalColorAdjustmentsChanged, this, &InspectorWidget::updateGlobalPropertiesUi);
        connect(m_model, &TimelineModel::timelineChanged, this, &InspectorWidget::updateGlobalPropertiesUi);
    }

    updateGlobalPropertiesUi();
}

QSize InspectorWidget::sizeHint() const
{
    return QSize(360, 650);
}

QSize InspectorWidget::minimumSizeHint() const
{
    return QSize(330, 400);
}

void InspectorWidget::setSelectedClip(qint64 clipId)
{
    m_selectedClipId = clipId;
    m_selectedClipIds.clear();
    if (clipId > 0) {
        m_selectedClipIds.insert(clipId);
        showClipProperties();
    }
    refreshUi();
}

void InspectorWidget::setSelectedClips(const QSet<qint64> &clipIds)
{
    m_selectedClipIds = clipIds;
    if (clipIds.isEmpty()) {
        m_selectedClipId = -1;
    } else {
        m_selectedClipId = *clipIds.begin();
        showClipProperties();
    }
    refreshUi();
}

void InspectorWidget::refreshUi()
{
    if (m_selectedClipIds.isEmpty() || !m_model) {
        m_noSelectionLabel->show();
        if (m_openGlobalFromEmptyBtn) m_openGlobalFromEmptyBtn->show();
        m_contentContainer->hide();
        return;
    }

    if (m_openGlobalFromEmptyBtn) m_openGlobalFromEmptyBtn->hide();

    if (m_selectedClipIds.size() > 1) {
        m_updatingUi = true;
        m_noSelectionLabel->hide();
        m_contentContainer->show();

        m_clipNameLabel->setText(QString("%1 clips seleccionados").arg(m_selectedClipIds.size()));
        m_clipTypeBadge->setText("SELECCIÓN MÚLTIPLE");
        m_clipTypeBadge->setStyleSheet("background-color: #8957e5; color: white; border-radius: 3px; padding: 2px 6px; font-size: 10px; font-weight: bold;");
        m_trackLabel->setText("Múltiples pistas");
        m_timingLabel->setText("Efectos y propiedades compartidas");
        if (m_techDetailsLabel) {
            m_techDetailsLabel->setText(QString("Aplicando efectos y propiedades en lote a %1 clips seleccionados.").arg(m_selectedClipIds.size()));
        }

        m_separateAudioBtn->hide();
        m_textSection->hide();
        m_transformSection->hide();
        m_motionSection->hide();
        m_videoSection->show();
        m_clipColorSection->show();
        m_effectsSection->show();
        m_audioSection->show();
        refreshEffectsStack();

        // Check common adjustments
        ColorAdjustments commonAdj;
        if (!m_selectedClipIds.isEmpty()) {
            if (TimelineClip *c = m_model->findClip(*m_selectedClipIds.begin())) {
                commonAdj = c->colorAdjustments();
            }
        }
        m_clipBrightnessSlider->setValue(commonAdj.brightness);
        m_clipBrightnessSpin->setValue(commonAdj.brightness);
        m_clipLuminositySlider->setValue(commonAdj.luminosity);
        m_clipLuminositySpin->setValue(commonAdj.luminosity);
        m_clipRedSlider->setValue(commonAdj.red);
        m_clipRedSpin->setValue(commonAdj.red);
        m_clipGreenSlider->setValue(commonAdj.green);
        m_clipGreenSpin->setValue(commonAdj.green);
        m_clipBlueSlider->setValue(commonAdj.blue);
        m_clipBlueSpin->setValue(commonAdj.blue);

        // Check common filter
        VisualFilter commonFilter = VisualFilter::None;
        bool first = true;
        bool sameFilter = true;
        for (qint64 cid : m_selectedClipIds) {
            if (TimelineClip *c = m_model->findClip(cid)) {
                if (first) {
                    commonFilter = c->filter();
                    first = false;
                } else if (commonFilter != c->filter()) {
                    sameFilter = false;
                    break;
                }
            }
        }
        int filterIdx = m_filterCombo->findData(static_cast<int>(sameFilter ? commonFilter : VisualFilter::None));
        if (filterIdx >= 0) {
            m_filterCombo->setCurrentIndex(filterIdx);
        }

        m_splitBtn->setEnabled(true);
        m_duplicateBtn->setEnabled(false);
        m_deleteBtn->setEnabled(true);

        m_updatingUi = false;
        return;
    }

    TimelineClip *clip = m_model->findClip(m_selectedClipId);
    if (!clip) {
        m_selectedClipId = -1;
        m_selectedClipIds.clear();
        m_noSelectionLabel->show();
        if (m_openGlobalFromEmptyBtn) m_openGlobalFromEmptyBtn->show();
        m_contentContainer->hide();
        return;
    }

    m_updatingUi = true;
    m_noSelectionLabel->hide();
    m_contentContainer->show();

    m_clipNameLabel->setText(clip->name());
    m_clipTypeBadge->setText(clipTypeToString(clip->type()).toUpper());
    if (clip->type() == ClipType::Video) {
        m_clipTypeBadge->setStyleSheet("background-color: #2f81f7; color: white; border-radius: 3px; padding: 2px 6px; font-size: 10px; font-weight: bold;");
    } else if (clip->type() == ClipType::Audio) {
        m_clipTypeBadge->setStyleSheet("background-color: #d29922; color: white; border-radius: 3px; padding: 2px 6px; font-size: 10px; font-weight: bold;");
    } else if (clip->type() == ClipType::Text) {
        m_clipTypeBadge->setStyleSheet("background-color: #d29922; color: #0d1117; border-radius: 3px; padding: 2px 6px; font-size: 10px; font-weight: bold;");
    } else {
        m_clipTypeBadge->setStyleSheet("background-color: #8957e5; color: white; border-radius: 3px; padding: 2px 6px; font-size: 10px; font-weight: bold;");
    }

    TimelineTrack *track = m_model->findTrackForClip(clip->id());
    m_trackLabel->setText(QString("Pista: %1").arg(track ? track->name() : "N/A"));

    qint64 inSec = clip->timelineInMs() / 1000;
    qint64 inMsRem = clip->timelineInMs() % 1000;
    qint64 durSec = clip->durationMs() / 1000;
    qint64 durMsRem = clip->durationMs() % 1000;
    m_timingLabel->setText(QString("Inicio: %1.%2s • Duración: %3.%4s")
                               .arg(inSec).arg(inMsRem / 100)
                               .arg(durSec).arg(durMsRem / 100));

    if (m_techDetailsLabel) {
        if (clip->type() == ClipType::Text) {
            m_techDetailsLabel->setText("🔤 Cuadro de Texto Vectorial (Enriquecido)");
        } else if (!clip->filePath().isEmpty()) {
            MediaItem mi(clip->filePath());
            m_techDetailsLabel->setText(mi.technicalSummary());
        } else {
            m_techDetailsLabel->setText("Sin archivo fuente asociado");
        }
    }

    // Show or hide separate audio button
    // It is shown if it's a video clip or linked clip
    if (clip->type() == ClipType::Video || clip->isLinked()) {
        m_separateAudioBtn->show();
        if (clip->isLinked()) {
            m_separateAudioBtn->setText("Separar Audio de Video (Desvincular)");
        } else {
            m_separateAudioBtn->setText("Separar y Extraer Audio a Pista Independiente");
        }
    } else {
        m_separateAudioBtn->hide();
    }

    // Transform Section (Video, Image and Text)
    if (clip->type() == ClipType::Video || clip->type() == ClipType::Image || clip->type() == ClipType::Text) {
        m_transformSection->show();
        updateTransformValues();

        m_motionSection->show();
        const MotionPath &path = clip->motionPath();

        m_motionPresetCombo->blockSignals(true);
        int presetIdx = m_motionPresetCombo->findData(static_cast<int>(path.preset()));
        m_motionPresetCombo->setCurrentIndex(presetIdx >= 0 ? presetIdx : 0);
        m_motionPresetCombo->blockSignals(false);

        m_motionEasingCombo->blockSignals(true);
        int easingIdx = m_motionEasingCombo->findData(static_cast<int>(path.easing()));
        m_motionEasingCombo->setCurrentIndex(easingIdx >= 0 ? easingIdx : 1);
        m_motionEasingCombo->blockSignals(false);

        bool enabled = path.isEnabled();
        m_motionStartXSpin->setEnabled(enabled);
        m_motionStartYSpin->setEnabled(enabled);
        m_motionEndXSpin->setEnabled(enabled);
        m_motionEndYSpin->setEnabled(enabled);
        m_motionWaypointCombo->setEnabled(enabled);
        m_motionWaypointXSpin->setEnabled(enabled);
        m_motionWaypointYSpin->setEnabled(enabled);
        m_motionWaypointCurvedCheck->setEnabled(enabled);
        m_motionHandleOutXSpin->setEnabled(enabled);
        m_motionHandleOutYSpin->setEnabled(enabled);
        m_motionAddPointBtn->setEnabled(true);
        m_motionRemovePointBtn->setEnabled(enabled && path.waypointCount() > 2);
        m_motionAutoSmoothBtn->setEnabled(enabled);
        m_motionReverseBtn->setEnabled(enabled);

        if (enabled && path.waypointCount() >= 2) {
            m_motionStartXSpin->blockSignals(true);
            m_motionStartYSpin->blockSignals(true);
            m_motionStartXSpin->setValue(path.startPoint().x());
            m_motionStartYSpin->setValue(path.startPoint().y());
            m_motionStartXSpin->blockSignals(false);
            m_motionStartYSpin->blockSignals(false);

            m_motionEndXSpin->blockSignals(true);
            m_motionEndYSpin->blockSignals(true);
            m_motionEndXSpin->setValue(path.endPoint().x());
            m_motionEndYSpin->setValue(path.endPoint().y());
            m_motionEndXSpin->blockSignals(false);
            m_motionEndYSpin->blockSignals(false);

            m_motionWaypointCombo->blockSignals(true);
            int prevIdx = m_motionWaypointCombo->currentIndex();
            m_motionWaypointCombo->clear();
            for (int i = 0; i < path.waypointCount(); ++i) {
                QString label;
                if (i == 0) label = "Punto 1 (▶ INICIO)";
                else if (i == path.waypointCount() - 1) label = QString("Punto %1 (■ FIN)").arg(i + 1);
                else label = QString("Punto %1 (Intermedio)").arg(i + 1);
                m_motionWaypointCombo->addItem(label, i);
            }
            int newIdx = qBound(0, prevIdx, path.waypointCount() - 1);
            m_motionWaypointCombo->setCurrentIndex(newIdx);
            m_motionWaypointCombo->blockSignals(false);

            MotionWaypoint wp = path.waypoint(newIdx);
            m_motionWaypointXSpin->blockSignals(true);
            m_motionWaypointYSpin->blockSignals(true);
            m_motionWaypointCurvedCheck->blockSignals(true);
            m_motionHandleOutXSpin->blockSignals(true);
            m_motionHandleOutYSpin->blockSignals(true);

            m_motionWaypointXSpin->setValue(wp.pos.x());
            m_motionWaypointYSpin->setValue(wp.pos.y());
            m_motionWaypointCurvedCheck->setChecked(wp.isCurved);
            m_motionHandleOutXSpin->setValue(wp.handleOut.x());
            m_motionHandleOutYSpin->setValue(wp.handleOut.y());

            m_motionWaypointXSpin->blockSignals(false);
            m_motionWaypointYSpin->blockSignals(false);
            m_motionWaypointCurvedCheck->blockSignals(false);
            m_motionHandleOutXSpin->blockSignals(false);
            m_motionHandleOutYSpin->blockSignals(false);
        } else {
            m_motionStartXSpin->setValue(clip->posX());
            m_motionStartYSpin->setValue(clip->posY());
            m_motionEndXSpin->setValue(clip->posX());
            m_motionEndYSpin->setValue(clip->posY());
            m_motionWaypointCombo->clear();
        }
    } else {
        m_transformSection->hide();
        m_motionSection->hide();
    }

    // Text Section (Content, Typography, Formatting, Color, Box Size)
    if (clip->type() == ClipType::Text) {
        m_textSection->show();
        if (!clip->richTextHtml().isEmpty()) {
            QString uiHtml = clip->richTextHtml();
            uiHtml.replace(QRegularExpression("font-size:\\s*\\d+(?:\\.\\d+)?(?:pt|px)?", QRegularExpression::CaseInsensitiveOption), "font-size:11pt");
            m_textEdit->setHtml(uiHtml);
        } else {
            m_textEdit->setPlainText(clip->textContent());
        }

        m_fontCombo->setCurrentFont(QFont(clip->fontFamily().isEmpty() ? "Arial" : clip->fontFamily()));
        m_fontSizeSpin->setValue(clip->fontSize());
        m_boldBtn->setChecked(clip->isBold());
        m_italicBtn->setChecked(clip->isItalic());
        m_underlineBtn->setChecked(clip->isUnderline());

        Qt::Alignment align = static_cast<Qt::Alignment>(clip->textAlignment());
        m_alignLeftBtn->setChecked((align & Qt::AlignHorizontal_Mask) == Qt::AlignLeft);
        m_alignCenterBtn->setChecked((align & Qt::AlignHorizontal_Mask) == Qt::AlignHCenter);
        m_alignRightBtn->setChecked((align & Qt::AlignHorizontal_Mask) == Qt::AlignRight);

        m_textColorSwatch->setStyleSheet(QString("background-color: %1; border: 1px solid #ffffff; border-radius: 2px;").arg(clip->textColor().name()));
        if (clip->backgroundColor().alpha() > 0) {
            m_bgColorSwatch->setStyleSheet(QString("background-color: rgba(%1,%2,%3,%4); border: 1px solid #8b949e; border-radius: 2px;")
                .arg(clip->backgroundColor().red()).arg(clip->backgroundColor().green()).arg(clip->backgroundColor().blue()).arg(clip->backgroundColor().alphaF(), 0, 'f', 2));
        } else {
            m_bgColorSwatch->setStyleSheet("background-color: transparent; border: 1px dashed #8b949e; border-radius: 2px;");
        }
        m_boxWidthSpin->setValue(clip->textBoxWidth());
    } else {
        m_textSection->hide();
    }

    // Video Section & Effects Stack
    if (clip->type() == ClipType::Video || clip->type() == ClipType::Image || clip->type() == ClipType::Text) {
        m_videoSection->show();
        m_clipColorSection->show();
        m_effectsSection->show();
        int opVal = qRound(clip->opacity() * 100);
        m_opacitySlider->setValue(opVal);
        m_opacityLabel->setText(QString("%1%").arg(opVal));
        m_videoFadeInSpin->setValue(clip->fadeInMs() / 1000.0);
        m_videoFadeOutSpin->setValue(clip->fadeOutMs() / 1000.0);
        if (m_filterCombo) {
            m_filterCombo->setCurrentText(visualFilterToString(clip->filter()));
        }
        const ColorAdjustments &adj = clip->colorAdjustments();
        m_clipBrightnessSlider->setValue(adj.brightness);
        m_clipBrightnessSpin->setValue(adj.brightness);
        m_clipLuminositySlider->setValue(adj.luminosity);
        m_clipLuminositySpin->setValue(adj.luminosity);
        m_clipRedSlider->setValue(adj.red);
        m_clipRedSpin->setValue(adj.red);
        m_clipGreenSlider->setValue(adj.green);
        m_clipGreenSpin->setValue(adj.green);
        m_clipBlueSlider->setValue(adj.blue);
        m_clipBlueSpin->setValue(adj.blue);

        if (m_clipColorCurveWidget) {
            m_clipColorCurveWidget->blockSignals(true);
            m_clipColorCurveWidget->setCurve(adj.colorCurve);
            m_clipColorCurveWidget->blockSignals(false);
        }
        if (m_clipLumaCurveWidget) {
            m_clipLumaCurveWidget->blockSignals(true);
            m_clipLumaCurveWidget->setCurve(adj.lumaCurve);
            m_clipLumaCurveWidget->blockSignals(false);
        }
        if (m_clipColorModeStack) {
            m_clipColorModeStack->setCurrentIndex(adj.mode == ColorGradeMode::Curves ? 1 : 0);
        }
        if (m_clipModeSlidersBtn && m_clipModeCurvesBtn) {
            bool isCurves = (adj.mode == ColorGradeMode::Curves);
            m_clipModeSlidersBtn->setStyleSheet(isCurves ? "QPushButton { background-color: #21262d; color: #8b949e; border: 1px solid #30363d; border-radius: 4px; padding: 4px 8px; font-weight: bold; font-size: 11px; } QPushButton:hover { background-color: #30363d; color: white; }"
                                                         : "QPushButton { background-color: #1f6feb; color: white; border: 1px solid #388bfd; border-radius: 4px; padding: 4px 8px; font-weight: bold; font-size: 11px; }");
            m_clipModeCurvesBtn->setStyleSheet(isCurves ? "QPushButton { background-color: #1f6feb; color: white; border: 1px solid #388bfd; border-radius: 4px; padding: 4px 8px; font-weight: bold; font-size: 11px; }"
                                                        : "QPushButton { background-color: #21262d; color: #8b949e; border: 1px solid #30363d; border-radius: 4px; padding: 4px 8px; font-weight: bold; font-size: 11px; } QPushButton:hover { background-color: #30363d; color: white; }");
        }

        refreshEffectsStack();

        if (m_clipTimeOfDayGroup) {
            m_clipTimeOfDayGroup->show();
            m_clipTimeOfDayCheck->blockSignals(true);
            m_clipTimeOfDayCheck->setChecked(adj.timeOfDayEnabled);
            m_clipTimeOfDayCheck->blockSignals(false);

            m_clipTimeOfDaySlider->blockSignals(true);
            m_clipTimeOfDaySlider->setValue(qRound(adj.timeOfDay * 100.0f));
            m_clipTimeOfDaySlider->blockSignals(false);

            updateTimeOfDayBadge(m_clipTimeOfDayBadge, adj.timeOfDay);
        }
    } else {
        m_videoSection->hide();
        m_clipColorSection->hide();
        if (m_clipTimeOfDayGroup) {
            m_clipTimeOfDayGroup->hide();
        }
        m_effectsSection->hide();
    }

    // Audio Section
    if (clip->type() == ClipType::Audio || clip->type() == ClipType::Video) {
        m_audioSection->show();
        int volVal = qRound(clip->volume() * 100);
        m_volumeSlider->setValue(volVal);
        m_volumeLabel->setText(QString("%1%").arg(volVal));
        m_audioFadeInSpin->setValue(clip->fadeInMs() / 1000.0);
        m_audioFadeOutSpin->setValue(clip->fadeOutMs() / 1000.0);
        m_muteAudioCheck->setChecked(clip->isAudioMuted());
    } else {
        m_audioSection->hide();
    }

    m_updatingUi = false;
}

void InspectorWidget::onOpacityChanged(int value)
{
    if (m_updatingUi) return;
    double op = value / 100.0;
    m_opacityLabel->setText(QString("%1%").arg(value));

    if (m_selectedClipIds.size() > 1) {
        for (qint64 cid : m_selectedClipIds) {
            if (TimelineClip *c = m_model->findClip(cid)) {
                c->setOpacity(op);
                emit clipPropertyModified(cid);
            }
        }
    } else if (TimelineClip *clip = m_model->findClip(m_selectedClipId)) {
        clip->setOpacity(op);
        emit clipPropertyModified(m_selectedClipId);
    }
}

void InspectorWidget::onVolumeChanged(int value)
{
    if (m_updatingUi) return;
    double vol = value / 100.0;
    m_volumeLabel->setText(QString("%1%").arg(value));

    if (m_selectedClipIds.size() > 1) {
        for (qint64 cid : m_selectedClipIds) {
            if (TimelineClip *c = m_model->findClip(cid)) {
                c->setVolume(vol);
                emit clipPropertyModified(cid);
            }
        }
    } else if (TimelineClip *clip = m_model->findClip(m_selectedClipId)) {
        clip->setVolume(vol);
        emit clipPropertyModified(m_selectedClipId);
    }
}

void InspectorWidget::onFadeInChanged(double value)
{
    if (m_updatingUi) return;
    if (TimelineClip *clip = m_model->findClip(m_selectedClipId)) {
        clip->setFadeInMs(static_cast<qint64>(value * 1000));
        emit clipPropertyModified(m_selectedClipId);
    }
}

void InspectorWidget::onFadeOutChanged(double value)
{
    if (m_updatingUi) return;
    if (TimelineClip *clip = m_model->findClip(m_selectedClipId)) {
        clip->setFadeOutMs(static_cast<qint64>(value * 1000));
        emit clipPropertyModified(m_selectedClipId);
    }
}

void InspectorWidget::onFilterChanged(int index)
{
    Q_UNUSED(index);
    if (m_updatingUi || !m_model || !m_filterCombo) return;
    VisualFilter filter = static_cast<VisualFilter>(m_filterCombo->currentData().toInt());

    if (m_selectedClipIds.size() > 1) {
        m_model->setClipsFilter(m_selectedClipIds.values(), filter);
        for (qint64 cid : m_selectedClipIds) {
            emit clipPropertyModified(cid);
        }
    } else if (m_selectedClipId > 0) {
        m_model->setClipFilter(m_selectedClipId, filter);
        emit clipPropertyModified(m_selectedClipId);
    }
    refreshEffectsStack();
}

void InspectorWidget::onAddEffectClicked()
{
    if (!m_model) return;

    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background-color: #161b22; color: #c9d1d9; border: 1px solid #30363d; }"
        "QMenu::item:selected { background-color: #1f293d; color: #58a6ff; }"
    );

    TimelineWidget::populateEffectsMenu(&menu, [this](VisualFilter f) {
        if (f == VisualFilter::None) return;
        if (m_selectedClipIds.size() > 1) {
            m_model->addClipsFilter(m_selectedClipIds.values(), f);
            for (qint64 cid : m_selectedClipIds) {
                emit clipPropertyModified(cid);
            }
        } else if (m_selectedClipId > 0) {
            m_model->addClipFilter(m_selectedClipId, f);
            emit clipPropertyModified(m_selectedClipId);
        }
        refreshEffectsStack();
    });

    menu.exec(m_addEffectBtn->mapToGlobal(QPoint(0, m_addEffectBtn->height())));
}

void InspectorWidget::onEffectMoveUp(int index)
{
    if (index <= 0 || !m_model) return;
    if (m_selectedClipIds.size() > 1) {
        for (qint64 cid : m_selectedClipIds) {
            m_model->moveClipFilter(cid, index, index - 1);
            emit clipPropertyModified(cid);
        }
    } else if (m_selectedClipId > 0) {
        m_model->moveClipFilter(m_selectedClipId, index, index - 1);
        emit clipPropertyModified(m_selectedClipId);
    }
    refreshEffectsStack();
}

void InspectorWidget::onEffectMoveDown(int index)
{
    if (!m_model) return;
    if (m_selectedClipIds.size() > 1) {
        for (qint64 cid : m_selectedClipIds) {
            TimelineClip *clip = m_model->findClip(cid);
            if (clip && index < clip->filterStack().size() - 1) {
                m_model->moveClipFilter(cid, index, index + 1);
                emit clipPropertyModified(cid);
            }
        }
    } else if (m_selectedClipId > 0) {
        TimelineClip *clip = m_model->findClip(m_selectedClipId);
        if (clip && index < clip->filterStack().size() - 1) {
            m_model->moveClipFilter(m_selectedClipId, index, index + 1);
            emit clipPropertyModified(m_selectedClipId);
        }
    }
    refreshEffectsStack();
}

void InspectorWidget::onEffectRemove(int index)
{
    if (!m_model) return;
    if (m_selectedClipIds.size() > 1) {
        for (qint64 cid : m_selectedClipIds) {
            m_model->removeClipFilterAt(cid, index);
            emit clipPropertyModified(cid);
        }
    } else if (m_selectedClipId > 0) {
        m_model->removeClipFilterAt(m_selectedClipId, index);
        emit clipPropertyModified(m_selectedClipId);
    }
    refreshEffectsStack();
}

void InspectorWidget::onEffectReordered(int fromIndex, int toIndex)
{
    if (!m_model || fromIndex == toIndex) return;
    if (m_selectedClipIds.size() > 1) {
        for (qint64 cid : m_selectedClipIds) {
            m_model->moveClipFilter(cid, fromIndex, toIndex);
            emit clipPropertyModified(cid);
        }
    } else if (m_selectedClipId > 0) {
        m_model->moveClipFilter(m_selectedClipId, fromIndex, toIndex);
        emit clipPropertyModified(m_selectedClipId);
    }
    refreshEffectsStack();
}

void InspectorWidget::onClearEffectsClicked()
{
    if (!m_model) return;
    if (m_selectedClipIds.size() > 1) {
        m_model->clearClipsFilters(m_selectedClipIds.values());
        for (qint64 cid : m_selectedClipIds) {
            emit clipPropertyModified(cid);
        }
    } else if (m_selectedClipId > 0) {
        m_model->clearClipFilters(m_selectedClipId);
        emit clipPropertyModified(m_selectedClipId);
    }
    refreshEffectsStack();
}

void InspectorWidget::refreshEffectsStack()
{
    if (!m_effectsList) return;
    m_effectsList->clear();

    if (!m_model) {
        m_emptyEffectsLabel->show();
        m_effectsList->hide();
        m_clearEffectsBtn->hide();
        m_effectsCountLabel->setText("0 efectos");
        return;
    }

    QVector<VisualFilter> stack;
    if (m_selectedClipIds.size() > 1) {
        bool first = true;
        bool allSame = true;
        for (qint64 cid : m_selectedClipIds) {
            TimelineClip *c = m_model->findClip(cid);
            if (!c) continue;
            if (first) {
                stack = c->filterStack();
                first = false;
            } else if (stack != c->filterStack()) {
                allSame = false;
                break;
            }
        }
        if (!allSame) {
            m_emptyEffectsLabel->setText("Varios clips seleccionados con efectos distintos.\nHaz clic en '➕ Agregar Efecto' para aplicar a todos.");
            m_emptyEffectsLabel->show();
            m_effectsList->hide();
            m_clearEffectsBtn->show();
            m_effectsCountLabel->setText(QString("%1 clips").arg(m_selectedClipIds.size()));
            return;
        }
    } else if (m_selectedClipId > 0) {
        TimelineClip *clip = m_model->findClip(m_selectedClipId);
        if (clip) {
            stack = clip->filterStack();
        }
    }

    if (stack.isEmpty()) {
        m_emptyEffectsLabel->setText("Sin efectos aplicados.\nHaz clic en '➕ Agregar Efecto' para añadir uno a la pila.");
        m_emptyEffectsLabel->show();
        m_effectsList->hide();
        m_clearEffectsBtn->hide();
        m_effectsCountLabel->setText("0 efectos");
    } else {
        m_emptyEffectsLabel->hide();
        m_effectsList->show();
        m_clearEffectsBtn->show();
        m_effectsCountLabel->setText(QString("%1 efecto(s)").arg(stack.size()));

        for (int i = 0; i < stack.size(); ++i) {
            QListWidgetItem *item = new QListWidgetItem(m_effectsList);
            EffectItemWidget *w = new EffectItemWidget(i, stack.size(), stack[i], m_effectsList);
            connect(w, &EffectItemWidget::moveUpRequested, this, &InspectorWidget::onEffectMoveUp);
            connect(w, &EffectItemWidget::moveDownRequested, this, &InspectorWidget::onEffectMoveDown);
            connect(w, &EffectItemWidget::removeRequested, this, &InspectorWidget::onEffectRemove);
            item->setSizeHint(w->sizeHint());
            m_effectsList->setItemWidget(item, w);
        }
        int listHeight = qBound(60, stack.size() * 38 + 12, 240);
        m_effectsList->setFixedHeight(listHeight);
    }
}

void InspectorWidget::onSpeedPresetClicked(double speed)
{
    if (TimelineClip *clip = m_model->findClip(m_selectedClipId)) {
        clip->setSpeed(speed);
        // Recalculate duration
        qint64 newDuration = qRound64((clip->sourceOutMs() - clip->sourceInMs()) / speed);
        clip->setTimelineOutMs(clip->timelineInMs() + newDuration);
        emit clipPropertyModified(m_selectedClipId);
        refreshUi();
    }
}

void InspectorWidget::onSeparateAudioClicked()
{
    if (m_selectedClipId > 0) {
        emit separateAudioRequested(m_selectedClipId);
        refreshUi();
    }
}

void InspectorWidget::updateTransformValues()
{
    if (m_selectedClipId <= 0 || !m_model) return;
    TimelineClip *clip = m_model->findClip(m_selectedClipId);
    if (!clip) return;

    bool prevUpdating = m_updatingUi;
    m_updatingUi = true;

    int scalePercent = qRound(clip->scale() * 100.0);
    m_scaleSpin->setValue(scalePercent);
    m_scaleSlider->setValue(qBound(10, scalePercent, 300));

    m_posXSpin->setValue(qRound(clip->posX()));
    m_posYSpin->setValue(qRound(clip->posY()));

    int rotDeg = qRound(clip->rotation());
    m_rotSpin->setValue(rotDeg);
    m_rotSlider->setValue(qBound(-180, rotDeg, 180));

    if (clip->type() == ClipType::Text && m_boxWidthSpin) {
        m_boxWidthSpin->setValue(clip->textBoxWidth());
    }

    if (m_zIndexSpin && m_model) {
        int z = m_model->clipZIndex(m_selectedClipId);
        int totalZ = m_model->totalVideoZLevels();
        m_zIndexSpin->setRange(1, qMax(totalZ + 5, 20));
        m_zIndexSpin->setValue(z);
        if (z == 1) {
            m_zIndexInfoLabel->setText("Base (1 - Fondo)");
        } else if (z == totalZ) {
            m_zIndexInfoLabel->setText(QString("Cúspide (%1 - Frente)").arg(totalZ));
        } else {
            m_zIndexInfoLabel->setText(QString("Nivel %1 de %2").arg(z).arg(totalZ));
        }
    }

    if (m_scaleModeCombo) {
        int idx = m_scaleModeCombo->findData(static_cast<int>(clip->scaleMode()));
        if (idx >= 0) m_scaleModeCombo->setCurrentIndex(idx);
    }

    m_updatingUi = prevUpdating;
}

void InspectorWidget::onScaleChanged(int value)
{
    if (m_updatingUi) return;
    if (TimelineClip *clip = m_model->findClip(m_selectedClipId)) {
        clip->setScale(value / 100.0);
        emit clipPropertyModified(m_selectedClipId);
    }
}

void InspectorWidget::onPosXChanged(int value)
{
    if (m_updatingUi) return;
    if (TimelineClip *clip = m_model->findClip(m_selectedClipId)) {
        clip->setPosX(value);
        emit clipPropertyModified(m_selectedClipId);
    }
}

void InspectorWidget::onPosYChanged(int value)
{
    if (m_updatingUi) return;
    if (TimelineClip *clip = m_model->findClip(m_selectedClipId)) {
        clip->setPosY(value);
        emit clipPropertyModified(m_selectedClipId);
    }
}

void InspectorWidget::onRotationChanged(int value)
{
    if (m_updatingUi) return;
    if (TimelineClip *clip = m_model->findClip(m_selectedClipId)) {
        clip->setRotation(value);
        emit clipPropertyModified(m_selectedClipId);
    }
}

void InspectorWidget::onZIndexChanged(int z)
{
    if (m_updatingUi || !m_model || m_selectedClipId <= 0) return;
    int currentZ = m_model->clipZIndex(m_selectedClipId);
    if (currentZ == z) return;

    if (m_model->setClipZIndex(m_selectedClipId, z)) {
        emit clipPropertyModified(m_selectedClipId);
        refreshUi();
    }
}

void InspectorWidget::onResetTransformClicked()
{
    if (TimelineClip *clip = m_model->findClip(m_selectedClipId)) {
        clip->resetTransform();
        updateTransformValues();
        emit clipPropertyModified(m_selectedClipId);
    }
}

void InspectorWidget::onScaleModeChanged(int index)
{
    if (m_updatingUi || !m_model || m_selectedClipId <= 0) return;
    TimelineClip *clip = m_model->findClip(m_selectedClipId);
    if (!clip) return;
    ClipScaleMode mode = static_cast<ClipScaleMode>(m_scaleModeCombo->itemData(index).toInt());
    if (clip->scaleMode() != mode) {
        m_model->saveState(QString("Cambiar ajuste a lienzo: %1").arg(clipScaleModeDisplayName(mode)));
        clip->setScaleMode(mode);
        emit clipPropertyModified(m_selectedClipId);
    }
}

void InspectorWidget::onTextChanged()
{
    if (m_updatingUi || !m_model || m_selectedClipId <= 0) return;
    TimelineClip *clip = m_model->findClip(m_selectedClipId);
    if (!clip || clip->type() != ClipType::Text) return;

    clip->setRichTextHtml(m_textEdit->toHtml());
    clip->setTextContent(m_textEdit->toPlainText());
    clip->setName(m_textEdit->toPlainText().left(20).simplified());
    emit clipPropertyModified(clip->id());
}

void InspectorWidget::onTextCursorPositionChanged()
{
    if (m_updatingUi) return;
    m_updatingUi = true;
    QTextCursor cursor = m_textEdit->textCursor();
    QTextCharFormat fmt = cursor.charFormat();

    m_boldBtn->setChecked(fmt.fontWeight() == QFont::Bold || fmt.font().bold());
    m_italicBtn->setChecked(fmt.fontItalic());
    m_underlineBtn->setChecked(fmt.fontUnderline());
    if (!fmt.font().family().isEmpty()) {
        m_fontCombo->setCurrentFont(QFont(fmt.font().family()));
    }
    if (fmt.foreground().color().isValid()) {
        m_textColorSwatch->setStyleSheet(QString("background-color: %1; border: 1px solid #ffffff; border-radius: 2px;").arg(fmt.foreground().color().name()));
    }
    Qt::Alignment align = m_textEdit->alignment();
    m_alignLeftBtn->setChecked((align & Qt::AlignHorizontal_Mask) == Qt::AlignLeft);
    m_alignCenterBtn->setChecked((align & Qt::AlignHorizontal_Mask) == Qt::AlignHCenter);
    m_alignRightBtn->setChecked((align & Qt::AlignHorizontal_Mask) == Qt::AlignRight);
    m_updatingUi = false;
}

void InspectorWidget::onFontFamilyChanged(const QFont &font)
{
    if (m_updatingUi || !m_model || m_selectedClipId <= 0) return;
    TimelineClip *clip = m_model->findClip(m_selectedClipId);
    if (!clip || clip->type() != ClipType::Text) return;

    clip->setFontFamily(font.family());

    QTextCursor cursor = m_textEdit->textCursor();
    bool hadSelection = cursor.hasSelection();
    if (!hadSelection) {
        cursor.select(QTextCursor::Document);
    }
    QTextCharFormat fmt;
    fmt.setFontFamilies({font.family()});
    cursor.mergeCharFormat(fmt);

    if (!hadSelection) {
        cursor.clearSelection();
        m_textEdit->setTextCursor(cursor);
    }

    clip->setRichTextHtml(m_textEdit->toHtml());
    emit clipPropertyModified(clip->id());
}

void InspectorWidget::onFontSizeChanged(int size)
{
    if (m_updatingUi || !m_model || m_selectedClipId <= 0) return;
    TimelineClip *clip = m_model->findClip(m_selectedClipId);
    if (!clip || clip->type() != ClipType::Text) return;

    clip->setFontSize(size);
    emit clipPropertyModified(clip->id());
}

void InspectorWidget::onBoldToggled(bool checked)
{
    if (m_updatingUi || !m_model || m_selectedClipId <= 0) return;
    TimelineClip *clip = m_model->findClip(m_selectedClipId);
    if (!clip || clip->type() != ClipType::Text) return;

    clip->setBold(checked);

    QTextCursor cursor = m_textEdit->textCursor();
    bool hadSelection = cursor.hasSelection();
    if (!hadSelection) {
        cursor.select(QTextCursor::Document);
    }
    QTextCharFormat fmt;
    fmt.setFontWeight(checked ? QFont::Bold : QFont::Normal);
    cursor.mergeCharFormat(fmt);

    if (!hadSelection) {
        cursor.clearSelection();
        m_textEdit->setTextCursor(cursor);
    }

    clip->setRichTextHtml(m_textEdit->toHtml());
    emit clipPropertyModified(clip->id());
}

void InspectorWidget::onItalicToggled(bool checked)
{
    if (m_updatingUi || !m_model || m_selectedClipId <= 0) return;
    TimelineClip *clip = m_model->findClip(m_selectedClipId);
    if (!clip || clip->type() != ClipType::Text) return;

    clip->setItalic(checked);

    QTextCursor cursor = m_textEdit->textCursor();
    bool hadSelection = cursor.hasSelection();
    if (!hadSelection) {
        cursor.select(QTextCursor::Document);
    }
    QTextCharFormat fmt;
    fmt.setFontItalic(checked);
    cursor.mergeCharFormat(fmt);

    if (!hadSelection) {
        cursor.clearSelection();
        m_textEdit->setTextCursor(cursor);
    }

    clip->setRichTextHtml(m_textEdit->toHtml());
    emit clipPropertyModified(clip->id());
}

void InspectorWidget::onUnderlineToggled(bool checked)
{
    if (m_updatingUi || !m_model || m_selectedClipId <= 0) return;
    TimelineClip *clip = m_model->findClip(m_selectedClipId);
    if (!clip || clip->type() != ClipType::Text) return;

    clip->setUnderline(checked);

    QTextCursor cursor = m_textEdit->textCursor();
    bool hadSelection = cursor.hasSelection();
    if (!hadSelection) {
        cursor.select(QTextCursor::Document);
    }
    QTextCharFormat fmt;
    fmt.setFontUnderline(checked);
    cursor.mergeCharFormat(fmt);

    if (!hadSelection) {
        cursor.clearSelection();
        m_textEdit->setTextCursor(cursor);
    }

    clip->setRichTextHtml(m_textEdit->toHtml());
    emit clipPropertyModified(clip->id());
}

void InspectorWidget::onAlignLeftClicked()
{
    if (m_updatingUi || !m_model || m_selectedClipId <= 0) return;
    TimelineClip *clip = m_model->findClip(m_selectedClipId);
    if (!clip || clip->type() != ClipType::Text) return;

    m_textEdit->setAlignment(Qt::AlignLeft);
    clip->setTextAlignment(Qt::AlignLeft);
    m_alignLeftBtn->setChecked(true);
    m_alignCenterBtn->setChecked(false);
    m_alignRightBtn->setChecked(false);
    clip->setRichTextHtml(m_textEdit->toHtml());
    emit clipPropertyModified(clip->id());
}

void InspectorWidget::onAlignCenterClicked()
{
    if (m_updatingUi || !m_model || m_selectedClipId <= 0) return;
    TimelineClip *clip = m_model->findClip(m_selectedClipId);
    if (!clip || clip->type() != ClipType::Text) return;

    m_textEdit->setAlignment(Qt::AlignCenter);
    clip->setTextAlignment(Qt::AlignCenter);
    m_alignLeftBtn->setChecked(false);
    m_alignCenterBtn->setChecked(true);
    m_alignRightBtn->setChecked(false);
    clip->setRichTextHtml(m_textEdit->toHtml());
    emit clipPropertyModified(clip->id());
}

void InspectorWidget::onAlignRightClicked()
{
    if (m_updatingUi || !m_model || m_selectedClipId <= 0) return;
    TimelineClip *clip = m_model->findClip(m_selectedClipId);
    if (!clip || clip->type() != ClipType::Text) return;

    m_textEdit->setAlignment(Qt::AlignRight);
    clip->setTextAlignment(Qt::AlignRight);
    m_alignLeftBtn->setChecked(false);
    m_alignCenterBtn->setChecked(false);
    m_alignRightBtn->setChecked(true);
    clip->setRichTextHtml(m_textEdit->toHtml());
    emit clipPropertyModified(clip->id());
}

void InspectorWidget::onPickTextColor()
{
    if (!m_model || m_selectedClipId <= 0) return;
    TimelineClip *clip = m_model->findClip(m_selectedClipId);
    if (!clip || clip->type() != ClipType::Text) return;

    QColor col = QColorDialog::getColor(clip->textColor(), this, "Seleccionar Color de Texto");
    if (col.isValid()) {
        onQuickColorClicked(col);
    }
}

void InspectorWidget::onQuickColorClicked(const QColor &color)
{
    if (!m_model || m_selectedClipId <= 0) return;
    TimelineClip *clip = m_model->findClip(m_selectedClipId);
    if (!clip || clip->type() != ClipType::Text) return;

    clip->setTextColor(color);
    m_textColorSwatch->setStyleSheet(QString("background-color: %1; border: 1px solid #ffffff; border-radius: 2px;").arg(color.name()));

    QTextCursor cursor = m_textEdit->textCursor();
    bool hadSelection = cursor.hasSelection();
    if (!hadSelection) {
        cursor.select(QTextCursor::Document);
    }
    QTextCharFormat fmt;
    fmt.setForeground(color);
    cursor.mergeCharFormat(fmt);

    if (!hadSelection) {
        cursor.clearSelection();
        m_textEdit->setTextCursor(cursor);
    }

    clip->setRichTextHtml(m_textEdit->toHtml());
    emit clipPropertyModified(clip->id());
}

void InspectorWidget::onPickBgColor()
{
    if (!m_model || m_selectedClipId <= 0) return;
    TimelineClip *clip = m_model->findClip(m_selectedClipId);
    if (!clip || clip->type() != ClipType::Text) return;

    QColor col = QColorDialog::getColor(
        clip->backgroundColor().isValid() && clip->backgroundColor().alpha() > 0 ? clip->backgroundColor() : QColor(0, 0, 0, 160),
        this, "Seleccionar Color de Fondo de Caja", QColorDialog::ShowAlphaChannel
    );
    if (col.isValid()) {
        clip->setBackgroundColor(col);
        m_bgColorSwatch->setStyleSheet(QString("background-color: rgba(%1,%2,%3,%4); border: 1px solid #8b949e; border-radius: 2px;")
            .arg(col.red()).arg(col.green()).arg(col.blue()).arg(col.alphaF(), 0, 'f', 2));
        emit clipPropertyModified(clip->id());
    }
}

void InspectorWidget::onBoxWidthChanged(int width)
{
    if (m_updatingUi || !m_model || m_selectedClipId <= 0) return;
    TimelineClip *clip = m_model->findClip(m_selectedClipId);
    if (!clip || clip->type() != ClipType::Text) return;

    clip->setTextBoxWidth(width);
    emit clipPropertyModified(clip->id());
}

void InspectorWidget::onMotionPresetChanged(int index)
{
    if (m_updatingUi || !m_model || m_selectedClipId <= 0) return;
    TimelineClip *clip = m_model->findClip(m_selectedClipId);
    if (!clip) return;

    MotionPreset preset = static_cast<MotionPreset>(m_motionPresetCombo->currentData().toInt());
    m_model->setClipMotionPreset(m_selectedClipId, preset);
    refreshUi();
    emit clipPropertyModified(m_selectedClipId);
}

void InspectorWidget::onMotionStartChanged()
{
    if (m_updatingUi || !m_model || m_selectedClipId <= 0) return;
    TimelineClip *clip = m_model->findClip(m_selectedClipId);
    if (!clip) return;

    QPointF startP(m_motionStartXSpin->value(), m_motionStartYSpin->value());
    m_model->setClipMotionStartPoint(m_selectedClipId, startP);
    emit clipPropertyModified(m_selectedClipId);
}

void InspectorWidget::onMotionEndChanged()
{
    if (m_updatingUi || !m_model || m_selectedClipId <= 0) return;
    TimelineClip *clip = m_model->findClip(m_selectedClipId);
    if (!clip) return;

    QPointF endP(m_motionEndXSpin->value(), m_motionEndYSpin->value());
    m_model->setClipMotionEndPoint(m_selectedClipId, endP);
    emit clipPropertyModified(m_selectedClipId);
}

void InspectorWidget::onMotionWaypointSelected(int index)
{
    if (m_updatingUi || !m_model || m_selectedClipId <= 0 || index < 0) return;
    TimelineClip *clip = m_model->findClip(m_selectedClipId);
    if (!clip) return;

    const MotionPath &path = clip->motionPath();
    if (index >= path.waypointCount()) return;

    MotionWaypoint wp = path.waypoint(index);

    m_updatingUi = true;
    m_motionWaypointXSpin->setValue(wp.pos.x());
    m_motionWaypointYSpin->setValue(wp.pos.y());
    m_motionWaypointCurvedCheck->setChecked(wp.isCurved);
    m_motionHandleOutXSpin->setValue(wp.handleOut.x());
    m_motionHandleOutYSpin->setValue(wp.handleOut.y());
    m_updatingUi = false;
}

void InspectorWidget::onMotionWaypointChanged()
{
    if (m_updatingUi || !m_model || m_selectedClipId <= 0) return;
    TimelineClip *clip = m_model->findClip(m_selectedClipId);
    if (!clip) return;

    int idx = m_motionWaypointCombo->currentIndex();
    MotionPath path = clip->motionPath();
    if (idx < 0 || idx >= path.waypointCount()) return;

    auto wps = path.waypoints();
    wps[idx].pos = QPointF(m_motionWaypointXSpin->value(), m_motionWaypointYSpin->value());
    wps[idx].isCurved = m_motionWaypointCurvedCheck->isChecked();
    wps[idx].handleOut = QPointF(m_motionHandleOutXSpin->value(), m_motionHandleOutYSpin->value());
    if (wps[idx].isCurved && wps[idx].handleOut.isNull()) {
        wps[idx].handleOut = QPointF(80.0, 0.0);
    }
    wps[idx].handleIn = -wps[idx].handleOut;
    path.setWaypoints(wps);
    m_model->setClipMotionPath(m_selectedClipId, path);

    // Update start/end spins if first or last waypoint was changed
    if (idx == 0) {
        m_motionStartXSpin->blockSignals(true);
        m_motionStartYSpin->blockSignals(true);
        m_motionStartXSpin->setValue(wps[idx].pos.x());
        m_motionStartYSpin->setValue(wps[idx].pos.y());
        m_motionStartXSpin->blockSignals(false);
        m_motionStartYSpin->blockSignals(false);
    } else if (idx == wps.size() - 1) {
        m_motionEndXSpin->blockSignals(true);
        m_motionEndYSpin->blockSignals(true);
        m_motionEndXSpin->setValue(wps[idx].pos.x());
        m_motionEndYSpin->setValue(wps[idx].pos.y());
        m_motionEndXSpin->blockSignals(false);
        m_motionEndYSpin->blockSignals(false);
    }

    emit clipPropertyModified(m_selectedClipId);
}

void InspectorWidget::onMotionAddPointClicked()
{
    if (m_updatingUi || !m_model || m_selectedClipId <= 0) return;
    TimelineClip *clip = m_model->findClip(m_selectedClipId);
    if (!clip) return;

    MotionPath path = clip->motionPath();
    if (!path.isEnabled()) {
        path.setPreset(MotionPreset::Custom, QPointF(clip->posX(), clip->posY()));
    }
    QPointF mid = path.evaluate(0.5);
    int insertIdx = qMax(1, path.waypointCount() - 1);
    MotionWaypoint newWp(mid, true, QPointF(-70, 0), QPointF(70, 0));
    path.insertWaypoint(insertIdx, newWp);
    m_model->setClipMotionPath(m_selectedClipId, path);
    refreshUi();
    emit clipPropertyModified(m_selectedClipId);
}

void InspectorWidget::onMotionRemovePointClicked()
{
    if (m_updatingUi || !m_model || m_selectedClipId <= 0) return;
    TimelineClip *clip = m_model ? m_model->findClip(m_selectedClipId) : nullptr;
    if (!clip) return;

    MotionPath path = clip->motionPath();
    if (path.waypointCount() <= 2) return;

    int idx = m_motionWaypointCombo->currentIndex();
    if (idx <= 0 || idx >= path.waypointCount() - 1) {
        idx = qMax(1, path.waypointCount() - 2);
    }
    path.removeWaypoint(idx);
    m_model->setClipMotionPath(m_selectedClipId, path);
    refreshUi();
    emit clipPropertyModified(m_selectedClipId);
}

void InspectorWidget::onMotionAutoSmoothClicked()
{
    if (m_updatingUi || !m_model || m_selectedClipId <= 0) return;
    TimelineClip *clip = m_model ? m_model->findClip(m_selectedClipId) : nullptr;
    if (!clip) return;

    MotionPath path = clip->motionPath();
    path.autoSmoothHandles(0.33);
    m_model->setClipMotionPath(m_selectedClipId, path);
    refreshUi();
    emit clipPropertyModified(m_selectedClipId);
}

void InspectorWidget::onMotionReverseClicked()
{
    if (m_updatingUi || !m_model || m_selectedClipId <= 0) return;
    TimelineClip *clip = m_model ? m_model->findClip(m_selectedClipId) : nullptr;
    if (!clip) return;

    MotionPath path = clip->motionPath();
    path.reverse();
    m_model->setClipMotionPath(m_selectedClipId, path);
    refreshUi();
    emit clipPropertyModified(m_selectedClipId);
}

void InspectorWidget::onMotionEasingChanged(int index)
{
    if (m_updatingUi || !m_model || m_selectedClipId <= 0) return;
    TimelineClip *clip = m_model ? m_model->findClip(m_selectedClipId) : nullptr;
    if (!clip) return;

    MotionEasing easing = static_cast<MotionEasing>(m_motionEasingCombo->currentData().toInt());
    MotionPath path = clip->motionPath();
    path.setEasing(easing);
    m_model->setClipMotionPath(m_selectedClipId, path);
    emit clipPropertyModified(m_selectedClipId);
}

void InspectorWidget::showClipProperties()
{
    if (m_viewStack && m_clipPage) {
        m_viewStack->setCurrentWidget(m_clipPage);
    }
    if (m_tabClipBtn && m_tabGlobalBtn) {
        m_tabClipBtn->setStyleSheet("background-color: #1f6feb; color: white; border: 1px solid #388bfd; border-radius: 4px; padding: 4px 10px; font-weight: bold; font-size: 11px;");
        m_tabGlobalBtn->setStyleSheet("background-color: #21262d; color: #8b949e; border: 1px solid #30363d; border-radius: 4px; padding: 4px 10px; font-weight: bold; font-size: 11px;");
    }
}

void InspectorWidget::showGlobalProperties()
{
    if (m_viewStack && m_globalPage) {
        m_viewStack->setCurrentWidget(m_globalPage);
    }
    if (m_tabClipBtn && m_tabGlobalBtn) {
        m_tabGlobalBtn->setStyleSheet("background-color: #1f6feb; color: white; border: 1px solid #388bfd; border-radius: 4px; padding: 4px 10px; font-weight: bold; font-size: 11px;");
        m_tabClipBtn->setStyleSheet("background-color: #21262d; color: #8b949e; border: 1px solid #30363d; border-radius: 4px; padding: 4px 10px; font-weight: bold; font-size: 11px;");
    }
    updateGlobalPropertiesUi();
}

void InspectorWidget::updateGlobalPropertiesUi()
{
    if (!m_model) return;

    bool prevUpdating = m_updatingUi;
    m_updatingUi = true;

    const ColorAdjustments &adj = m_model->globalColorAdjustments();
    if (m_globalBrightnessSlider && m_globalBrightnessSpin) {
        m_globalBrightnessSlider->setValue(adj.brightness);
        m_globalBrightnessSpin->setValue(adj.brightness);
    }
    if (m_globalLuminositySlider && m_globalLuminositySpin) {
        m_globalLuminositySlider->setValue(adj.luminosity);
        m_globalLuminositySpin->setValue(adj.luminosity);
    }
    if (m_globalRedSlider && m_globalRedSpin) {
        m_globalRedSlider->setValue(adj.red);
        m_globalRedSpin->setValue(adj.red);
    }
    if (m_globalGreenSlider && m_globalGreenSpin) {
        m_globalGreenSlider->setValue(adj.green);
        m_globalGreenSpin->setValue(adj.green);
    }
    if (m_globalBlueSlider && m_globalBlueSpin) {
        m_globalBlueSlider->setValue(adj.blue);
        m_globalBlueSpin->setValue(adj.blue);
    }

    if (m_globalColorCurveWidget) {
        m_globalColorCurveWidget->blockSignals(true);
        m_globalColorCurveWidget->setCurve(adj.colorCurve);
        m_globalColorCurveWidget->blockSignals(false);
    }
    if (m_globalLumaCurveWidget) {
        m_globalLumaCurveWidget->blockSignals(true);
        m_globalLumaCurveWidget->setCurve(adj.lumaCurve);
        m_globalLumaCurveWidget->blockSignals(false);
    }
    if (m_globalColorModeStack) {
        m_globalColorModeStack->setCurrentIndex(adj.mode == ColorGradeMode::Curves ? 1 : 0);
    }
    if (m_globalModeSlidersBtn && m_globalModeCurvesBtn) {
        bool isCurves = (adj.mode == ColorGradeMode::Curves);
        m_globalModeSlidersBtn->setStyleSheet(isCurves ? "QPushButton { background-color: #21262d; color: #8b949e; border: 1px solid #30363d; border-radius: 4px; padding: 4px 8px; font-weight: bold; font-size: 11px; } QPushButton:hover { background-color: #30363d; color: white; }"
                                                       : "QPushButton { background-color: #1f6feb; color: white; border: 1px solid #388bfd; border-radius: 4px; padding: 4px 8px; font-weight: bold; font-size: 11px; }");
        m_globalModeCurvesBtn->setStyleSheet(isCurves ? "QPushButton { background-color: #1f6feb; color: white; border: 1px solid #388bfd; border-radius: 4px; padding: 4px 8px; font-weight: bold; font-size: 11px; }"
                                                      : "QPushButton { background-color: #21262d; color: #8b949e; border: 1px solid #30363d; border-radius: 4px; padding: 4px 8px; font-weight: bold; font-size: 11px; } QPushButton:hover { background-color: #30363d; color: white; }");
    }

    if (m_globalTimeOfDayCheck && m_globalTimeOfDaySlider) {
        m_globalTimeOfDayCheck->blockSignals(true);
        m_globalTimeOfDayCheck->setChecked(adj.timeOfDayEnabled);
        m_globalTimeOfDayCheck->blockSignals(false);

        m_globalTimeOfDaySlider->blockSignals(true);
        m_globalTimeOfDaySlider->setValue(qRound(adj.timeOfDay * 100.0f));
        m_globalTimeOfDaySlider->blockSignals(false);

        updateTimeOfDayBadge(m_globalTimeOfDayBadge, adj.timeOfDay);
    }

    if (m_projectDurationLabel) {
        qint64 totalMs = m_model->totalDurationMs();
        qint64 sec = totalMs / 1000;
        qint64 msRem = (totalMs % 1000) / 10;
        qint64 min = sec / 60;
        sec = sec % 60;
        qint64 hrs = min / 60;
        min = min % 60;
        QString durStr = QString("%1:%2:%3.%4")
                             .arg(hrs, 2, 10, QChar('0'))
                             .arg(min, 2, 10, QChar('0'))
                             .arg(sec, 2, 10, QChar('0'))
                             .arg(msRem, 2, 10, QChar('0'));
        m_projectDurationLabel->setText(QString("⏱ Duración del Proyecto: <b>%1</b>").arg(durStr));
    }
    if (m_projectTracksLabel) {
        m_projectTracksLabel->setText(QString("🎬 Pistas: %1 Video • %2 Audio")
                                          .arg(m_model->videoTracks().size())
                                          .arg(m_model->audioTracks().size()));
    }
    if (m_projectClipsLabel) {
        int totalClips = 0;
        for (const auto &t : m_model->videoTracks()) totalClips += t.clips().size();
        for (const auto &t : m_model->audioTracks()) totalClips += t.clips().size();
        m_projectClipsLabel->setText(QString("📦 Elementos en línea de tiempo: %1").arg(totalClips));
    }

    m_updatingUi = prevUpdating;
}

void InspectorWidget::onClipBrightnessChanged(int value)
{
    if (m_updatingUi || !m_model) return;
    if (m_selectedClipIds.size() > 1) {
        for (qint64 cid : m_selectedClipIds) {
            m_model->setClipBrightness(cid, value, false);
            emit clipPropertyModified(cid);
        }
    } else if (m_selectedClipId > 0) {
        m_model->setClipBrightness(m_selectedClipId, value, false);
        emit clipPropertyModified(m_selectedClipId);
    }
}

void InspectorWidget::onClipLuminosityChanged(int value)
{
    if (m_updatingUi || !m_model) return;
    if (m_selectedClipIds.size() > 1) {
        for (qint64 cid : m_selectedClipIds) {
            m_model->setClipLuminosity(cid, value, false);
            emit clipPropertyModified(cid);
        }
    } else if (m_selectedClipId > 0) {
        m_model->setClipLuminosity(m_selectedClipId, value, false);
        emit clipPropertyModified(m_selectedClipId);
    }
}

void InspectorWidget::onClipRedChanged(int value)
{
    if (m_updatingUi || !m_model) return;
    if (m_selectedClipIds.size() > 1) {
        for (qint64 cid : m_selectedClipIds) {
            m_model->setClipRedPresence(cid, value, false);
            emit clipPropertyModified(cid);
        }
    } else if (m_selectedClipId > 0) {
        m_model->setClipRedPresence(m_selectedClipId, value, false);
        emit clipPropertyModified(m_selectedClipId);
    }
}

void InspectorWidget::onClipGreenChanged(int value)
{
    if (m_updatingUi || !m_model) return;
    if (m_selectedClipIds.size() > 1) {
        for (qint64 cid : m_selectedClipIds) {
            m_model->setClipGreenPresence(cid, value, false);
            emit clipPropertyModified(cid);
        }
    } else if (m_selectedClipId > 0) {
        m_model->setClipGreenPresence(m_selectedClipId, value, false);
        emit clipPropertyModified(m_selectedClipId);
    }
}

void InspectorWidget::onClipBlueChanged(int value)
{
    if (m_updatingUi || !m_model) return;
    if (m_selectedClipIds.size() > 1) {
        for (qint64 cid : m_selectedClipIds) {
            m_model->setClipBluePresence(cid, value, false);
            emit clipPropertyModified(cid);
        }
    } else if (m_selectedClipId > 0) {
        m_model->setClipBluePresence(m_selectedClipId, value, false);
        emit clipPropertyModified(m_selectedClipId);
    }
}

void InspectorWidget::onClipResetColorClicked()
{
    if (!m_model) return;
    m_updatingUi = true;
    m_clipBrightnessSlider->setValue(0);
    m_clipBrightnessSpin->setValue(0);
    m_clipLuminositySlider->setValue(0);
    m_clipLuminositySpin->setValue(0);
    m_clipRedSlider->setValue(0);
    m_clipRedSpin->setValue(0);
    m_clipGreenSlider->setValue(0);
    m_clipGreenSpin->setValue(0);
    m_clipBlueSlider->setValue(0);
    m_clipBlueSpin->setValue(0);
    m_updatingUi = false;

    if (m_selectedClipIds.size() > 1) {
        m_model->saveState("Restablecer color de clips seleccionados");
        for (qint64 cid : m_selectedClipIds) {
            m_model->resetClipColorAdjustments(cid, false);
            emit clipPropertyModified(cid);
        }
    } else if (m_selectedClipId > 0) {
        m_model->resetClipColorAdjustments(m_selectedClipId, true);
        emit clipPropertyModified(m_selectedClipId);
    }
}

void InspectorWidget::onGlobalBrightnessChanged(int value)
{
    if (m_updatingUi || !m_model) return;
    m_model->setGlobalBrightness(value, false);
    emit globalPropertyModified();
}

void InspectorWidget::onGlobalLuminosityChanged(int value)
{
    if (m_updatingUi || !m_model) return;
    m_model->setGlobalLuminosity(value, false);
    emit globalPropertyModified();
}

void InspectorWidget::onGlobalRedChanged(int value)
{
    if (m_updatingUi || !m_model) return;
    m_model->setGlobalRedPresence(value, false);
    emit globalPropertyModified();
}

void InspectorWidget::onGlobalGreenChanged(int value)
{
    if (m_updatingUi || !m_model) return;
    m_model->setGlobalGreenPresence(value, false);
    emit globalPropertyModified();
}

void InspectorWidget::onGlobalBlueChanged(int value)
{
    if (m_updatingUi || !m_model) return;
    m_model->setGlobalBluePresence(value, false);
    emit globalPropertyModified();
}

void InspectorWidget::onGlobalResetColorClicked()
{
    if (!m_model) return;
    m_updatingUi = true;
    m_globalBrightnessSlider->setValue(0);
    m_globalBrightnessSpin->setValue(0);
    m_globalLuminositySlider->setValue(0);
    m_globalLuminositySpin->setValue(0);
    m_globalRedSlider->setValue(0);
    m_globalRedSpin->setValue(0);
    m_globalGreenSlider->setValue(0);
    m_globalGreenSpin->setValue(0);
    m_globalBlueSlider->setValue(0);
    m_globalBlueSpin->setValue(0);
    m_updatingUi = false;

    m_model->resetGlobalColorAdjustments(true);
    emit globalPropertyModified();
}

void InspectorWidget::onClipColorGradeModeChanged(ColorGradeMode mode)
{
    if (m_updatingUi || !m_model) return;
    if (m_selectedClipIds.size() > 1) {
        m_model->saveState("Cambiar modo de ajuste de color");
        for (qint64 cid : m_selectedClipIds) {
            m_model->setClipColorGradeMode(cid, mode, false);
            emit clipPropertyModified(cid);
        }
    } else if (m_selectedClipId > 0) {
        m_model->setClipColorGradeMode(m_selectedClipId, mode, true);
        emit clipPropertyModified(m_selectedClipId);
    }
    refreshUi();
}

void InspectorWidget::onClipColorCurveChanged(const ColorCurve &curve)
{
    if (m_updatingUi || !m_model) return;
    if (m_selectedClipIds.size() > 1) {
        for (qint64 cid : m_selectedClipIds) {
            if (TimelineClip *c = m_model->findClip(cid)) {
                c->setColorCurve(curve);
                emit clipPropertyModified(cid);
            }
        }
    } else if (m_selectedClipId > 0) {
        if (TimelineClip *c = m_model->findClip(m_selectedClipId)) {
            c->setColorCurve(curve);
            emit clipPropertyModified(m_selectedClipId);
        }
    }
}

void InspectorWidget::onClipColorCurveCommitted(const ColorCurve &curve)
{
    if (m_updatingUi || !m_model) return;
    if (m_selectedClipIds.size() > 1) {
        m_model->saveState("Modificar curva de espectro de color");
        for (qint64 cid : m_selectedClipIds) {
            m_model->setClipColorCurve(cid, curve, false);
            emit clipPropertyModified(cid);
        }
    } else if (m_selectedClipId > 0) {
        m_model->setClipColorCurve(m_selectedClipId, curve, true);
        emit clipPropertyModified(m_selectedClipId);
    }
}

void InspectorWidget::onClipLumaCurveChanged(const ColorCurve &curve)
{
    if (m_updatingUi || !m_model) return;
    if (m_selectedClipIds.size() > 1) {
        for (qint64 cid : m_selectedClipIds) {
            if (TimelineClip *c = m_model->findClip(cid)) {
                c->setLumaCurve(curve);
                emit clipPropertyModified(cid);
            }
        }
    } else if (m_selectedClipId > 0) {
        if (TimelineClip *c = m_model->findClip(m_selectedClipId)) {
            c->setLumaCurve(curve);
            emit clipPropertyModified(m_selectedClipId);
        }
    }
}

void InspectorWidget::onClipLumaCurveCommitted(const ColorCurve &curve)
{
    if (m_updatingUi || !m_model) return;
    if (m_selectedClipIds.size() > 1) {
        m_model->saveState("Modificar curva de brillo y luma");
        for (qint64 cid : m_selectedClipIds) {
            m_model->setClipLumaCurve(cid, curve, false);
            emit clipPropertyModified(cid);
        }
    } else if (m_selectedClipId > 0) {
        m_model->setClipLumaCurve(m_selectedClipId, curve, true);
        emit clipPropertyModified(m_selectedClipId);
    }
}

void InspectorWidget::onClipResetCurvesClicked()
{
    if (!m_model) return;
    ColorCurve defLuma = ColorCurve::defaultLuma();
    ColorCurve defColor = ColorCurve::defaultColorSpectrum();

    if (m_clipColorCurveWidget) {
        m_clipColorCurveWidget->blockSignals(true);
        m_clipColorCurveWidget->setCurve(defColor);
        m_clipColorCurveWidget->blockSignals(false);
    }
    if (m_clipLumaCurveWidget) {
        m_clipLumaCurveWidget->blockSignals(true);
        m_clipLumaCurveWidget->setCurve(defLuma);
        m_clipLumaCurveWidget->blockSignals(false);
    }

    if (m_selectedClipIds.size() > 1) {
        m_model->saveState("Restablecer gráficos de color de clips");
        for (qint64 cid : m_selectedClipIds) {
            m_model->setClipLumaCurve(cid, defLuma, false);
            m_model->setClipColorCurve(cid, defColor, false);
            emit clipPropertyModified(cid);
        }
    } else if (m_selectedClipId > 0) {
        m_model->saveState("Restablecer gráficos de color");
        m_model->setClipLumaCurve(m_selectedClipId, defLuma, false);
        m_model->setClipColorCurve(m_selectedClipId, defColor, false);
        emit clipPropertyModified(m_selectedClipId);
    }
}

void InspectorWidget::onGlobalColorGradeModeChanged(ColorGradeMode mode)
{
    if (m_updatingUi || !m_model) return;
    m_model->setGlobalColorGradeMode(mode, true);
    emit globalPropertyModified();
    updateGlobalPropertiesUi();
}

void InspectorWidget::onGlobalColorCurveChanged(const ColorCurve &curve)
{
    if (m_updatingUi || !m_model) return;
    m_model->setGlobalColorCurve(curve, false);
    emit globalPropertyModified();
}

void InspectorWidget::onGlobalColorCurveCommitted(const ColorCurve &curve)
{
    if (m_updatingUi || !m_model) return;
    m_model->setGlobalColorCurve(curve, true);
    emit globalPropertyModified();
}

void InspectorWidget::onGlobalLumaCurveChanged(const ColorCurve &curve)
{
    if (m_updatingUi || !m_model) return;
    m_model->setGlobalLumaCurve(curve, false);
    emit globalPropertyModified();
}

void InspectorWidget::onGlobalLumaCurveCommitted(const ColorCurve &curve)
{
    if (m_updatingUi || !m_model) return;
    m_model->setGlobalLumaCurve(curve, true);
    emit globalPropertyModified();
}

void InspectorWidget::onGlobalResetCurvesClicked()
{
    if (!m_model) return;
    ColorCurve defLuma = ColorCurve::defaultLuma();
    ColorCurve defColor = ColorCurve::defaultColorSpectrum();

    if (m_globalColorCurveWidget) {
        m_globalColorCurveWidget->blockSignals(true);
        m_globalColorCurveWidget->setCurve(defColor);
        m_globalColorCurveWidget->blockSignals(false);
    }
    if (m_globalLumaCurveWidget) {
        m_globalLumaCurveWidget->blockSignals(true);
        m_globalLumaCurveWidget->setCurve(defLuma);
        m_globalLumaCurveWidget->blockSignals(false);
    }

    m_model->saveState("Restablecer gráficos de color generales");
    m_model->setGlobalLumaCurve(defLuma, false);
    m_model->setGlobalColorCurve(defColor, false);
    emit globalPropertyModified();
}

void InspectorWidget::updateTimeOfDayBadge(QLabel *badge, float val)
{
    if (!badge) return;
    if (val <= 0.16f) {
        badge->setText(QString("🌙 Noche (%1)").arg(val, 0, 'f', 2));
        badge->setStyleSheet("color: #79c0ff; font-weight: bold; background-color: #0d1117; border: 1px solid #1f6feb; border-radius: 4px; padding: 2px 6px; font-size: 11px;");
    } else if (val <= 0.49f) {
        badge->setText(QString("🌅 Mañana (%1)").arg(val, 0, 'f', 2));
        badge->setStyleSheet("color: #ffa657; font-weight: bold; background-color: #1f160e; border: 1px solid #d29922; border-radius: 4px; padding: 2px 6px; font-size: 11px;");
    } else if (val <= 0.82f) {
        badge->setText(QString("☀️ Día (%1%2)").arg(val, 0, 'f', 2).arg(qAbs(val - 0.66f) < 0.03f ? " - Neutro" : ""));
        badge->setStyleSheet("color: #7ee787; font-weight: bold; background-color: #0c1f17; border: 1px solid #238636; border-radius: 4px; padding: 2px 6px; font-size: 11px;");
    } else {
        badge->setText(QString("🌇 Atardecer (%1)").arg(val, 0, 'f', 2));
        badge->setStyleSheet("color: #ff7b72; font-weight: bold; background-color: #251214; border: 1px solid #f85149; border-radius: 4px; padding: 2px 6px; font-size: 11px;");
    }
}

void InspectorWidget::onClipTimeOfDayToggled(bool enabled)
{
    if (m_updatingUi || !m_model) return;
    float val = m_clipTimeOfDaySlider->value() / 100.0f;
    if (m_selectedClipIds.size() > 1) {
        m_model->setClipsTimeOfDay(m_selectedClipIds.values(), enabled, val, true);
        for (qint64 cid : m_selectedClipIds) {
            emit clipPropertyModified(cid);
        }
    } else if (m_selectedClipId > 0) {
        m_model->setClipTimeOfDay(m_selectedClipId, enabled, val, true);
        emit clipPropertyModified(m_selectedClipId);
    }
}

void InspectorWidget::onClipTimeOfDayChanged(int value)
{
    float val = value / 100.0f;
    updateTimeOfDayBadge(m_clipTimeOfDayBadge, val);
    if (m_updatingUi || !m_model) return;
    bool enabled = m_clipTimeOfDayCheck->isChecked();
    if (m_selectedClipIds.size() > 1) {
        m_model->setClipsTimeOfDay(m_selectedClipIds.values(), enabled, val, false);
        for (qint64 cid : m_selectedClipIds) {
            emit clipPropertyModified(cid);
        }
    } else if (m_selectedClipId > 0) {
        m_model->setClipTimeOfDay(m_selectedClipId, enabled, val, false);
        emit clipPropertyModified(m_selectedClipId);
    }
}

void InspectorWidget::onGlobalTimeOfDayToggled(bool enabled)
{
    if (m_updatingUi || !m_model) return;
    float val = m_globalTimeOfDaySlider->value() / 100.0f;
    m_model->setGlobalTimeOfDay(enabled, val, true);
    emit globalPropertyModified();
}

void InspectorWidget::onGlobalTimeOfDayChanged(int value)
{
    float val = value / 100.0f;
    updateTimeOfDayBadge(m_globalTimeOfDayBadge, val);
    if (m_updatingUi || !m_model) return;
    bool enabled = m_globalTimeOfDayCheck->isChecked();
    m_model->setGlobalTimeOfDay(enabled, val, false);
    emit globalPropertyModified();
}

