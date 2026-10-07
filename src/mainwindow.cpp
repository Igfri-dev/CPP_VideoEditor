#include "mainwindow.h"
#include "engine/exportdialog.h"
#include "core/projectserializer.h"
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QMenu>
#include <QToolBar>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QDoubleSpinBox>
#include <QToolButton>
#include <QFileDialog>
#include <QSettings>
#include <QCloseEvent>
#include <QComboBox>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("CPP VideoEditor - Editor de Video Profesional");
    resize(1366, 850);

    // Instantiate core model & audio engine
    m_timelineModel = new TimelineModel(this);
    m_audioEngine = new AudioEngine(m_timelineModel, this);

    // Instantiate views
    m_mediaLibrary = new MediaLibraryWidget(this);
    m_previewWidget = new PreviewWidget(m_timelineModel, m_audioEngine, this);
    m_inspectorWidget = new InspectorWidget(m_timelineModel, this);
    m_timelineWidget = new TimelineWidget(m_timelineModel, this);

    // Setup Layout & Menus
    setupMenus();
    setupToolbar();
    setupUiLayout();
    applyDarkTheme();

    // Wire signals and slots
    connect(m_mediaLibrary, &MediaLibraryWidget::addToTimelineRequested, this, &MainWindow::onMediaItemAddedToTimeline);
    connect(m_mediaLibrary, &MediaLibraryWidget::mediaItemDoubleClicked, this, [this](const MediaItem &item) {
        onMediaItemAddedToTimeline(item, false);
    });
    connect(m_mediaLibrary, &MediaLibraryWidget::addTextRequested, this, &MainWindow::onAddTextAction);

    connect(m_timelineWidget, &TimelineWidget::playheadSeekRequested, m_previewWidget, &PreviewWidget::setPosition);
    connect(m_previewWidget, &PreviewWidget::playheadMoved, m_timelineWidget, &TimelineWidget::setPlayheadPosition);
    connect(m_previewWidget, &PreviewWidget::playbackStateChanged, m_timelineWidget, &TimelineWidget::setIsPlaying);

    // Selection synchronization between Timeline, Inspector and Preview Gizmo
    connect(m_timelineWidget, &TimelineWidget::clipSelected, m_inspectorWidget, &InspectorWidget::setSelectedClip);
    connect(m_timelineWidget, &TimelineWidget::clipSelected, m_previewWidget, &PreviewWidget::setSelectedClipId);

    connect(m_previewWidget, &PreviewWidget::clipSelected, m_timelineWidget, &TimelineWidget::setSelectedClipId);
    connect(m_previewWidget, &PreviewWidget::clipSelected, m_inspectorWidget, &InspectorWidget::setSelectedClip);

    // Live transform synchronization
    connect(m_previewWidget, &PreviewWidget::clipTransformChanged, m_inspectorWidget, &InspectorWidget::updateTransformValues);
    connect(m_previewWidget, &PreviewWidget::clipTransformChanged, m_timelineWidget, [this]() { m_timelineWidget->update(); });

    connect(m_inspectorWidget, &InspectorWidget::clipPropertyModified, this, [this](qint64 clipId) {
        Q_UNUSED(clipId);
        m_previewWidget->updatePreview();
        m_timelineWidget->update();
    });

    connect(m_inspectorWidget, &InspectorWidget::separateAudioRequested, m_timelineModel, &TimelineModel::separateAudio);
    connect(m_inspectorWidget, &InspectorWidget::splitRequested, this, &MainWindow::onSplitAction);
    connect(m_inspectorWidget, &InspectorWidget::deleteRequested, this, &MainWindow::onDeleteAction);
    connect(m_inspectorWidget, &InspectorWidget::duplicateRequested, this, &MainWindow::onDuplicateAction);
    connect(m_inspectorWidget, &InspectorWidget::globalPropertyModified, this, [this]() {
        m_previewWidget->updatePreview();
    });
    connect(m_inspectorWidget, &InspectorWidget::exportRequested, this, &MainWindow::showExportDialog);

    // Undo / Redo and Selection UI state updates
    connect(m_timelineModel, &TimelineModel::undoRedoStateChanged, this, &MainWindow::updateUndoRedoUi);
    connect(m_timelineModel, &TimelineModel::durationChanged, this, &MainWindow::updateDurationUi);
    connect(m_timelineModel, &TimelineModel::timelineChanged, this, [this]() {
        m_isProjectModified = true;
        updateWindowTitle();
    });
    connect(m_timelineModel, &TimelineModel::markersChanged, this, [this]() {
        m_isProjectModified = true;
        updateWindowTitle();
    });
    connect(m_timelineModel, &TimelineModel::aspectRatioChanged, this, [this](ProjectAspectRatio ratio) {
        m_isProjectModified = true;
        updateWindowTitle();
        if (m_aspectRatioCombo) {
            int idx = m_aspectRatioCombo->findData(static_cast<int>(ratio));
            if (idx >= 0 && m_aspectRatioCombo->currentIndex() != idx) {
                m_aspectRatioCombo->setCurrentIndex(idx);
            }
        }
    });

    connect(m_timelineWidget, &TimelineWidget::selectionChanged, this, [this](const QSet<qint64> &ids) {
        if (m_joinBtn) m_joinBtn->setEnabled(ids.size() >= 2);
        if (m_actJoin) m_actJoin->setEnabled(ids.size() >= 2);
        if (m_inspectorWidget) m_inspectorWidget->setSelectedClips(ids);
    });
    updateUndoRedoUi();
    updateDurationUi();
    updateWindowTitle();

    // Autosave timer (every 3 minutes)
    connect(&m_autosaveTimer, &QTimer::timeout, this, &MainWindow::performAutosave);
    m_autosaveTimer.start(3 * 60 * 1000);

    // Auto-discover test assets in media and sample_assets directories
    const QStringList assetDirs = {"media", "sample_assets", "../media", "../sample_assets"};
    for (const QString &dirName : assetDirs) {
        QDir d(dirName);
        if (d.exists()) {
            const QStringList entries = d.entryList(QDir::Files);
            for (const QString &f : entries) {
                QString absPath = d.absoluteFilePath(f);
                if (QFileInfo(absPath).exists() && QFileInfo(absPath).size() > 0) {
                    m_mediaLibrary->addMediaFile(absPath);
                }
            }
        }
    }
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupMenus()
{
    QMenuBar *mb = menuBar();

    // Archivo
    QMenu *fileMenu = mb->addMenu("Archivo");
    QAction *actNew = fileMenu->addAction("Nuevo Proyecto");
    actNew->setShortcut(QKeySequence::New);
    connect(actNew, &QAction::triggered, this, &MainWindow::newProject);

    QAction *actOpen = fileMenu->addAction("Abrir Proyecto...");
    actOpen->setShortcut(QKeySequence::Open);
    connect(actOpen, &QAction::triggered, this, &MainWindow::openProject);

    m_recentProjectsMenu = fileMenu->addMenu("Proyectos Recientes");
    updateRecentProjectsMenu();

    fileMenu->addSeparator();

    m_actSave = fileMenu->addAction("Guardar Proyecto");
    m_actSave->setShortcut(QKeySequence::Save);
    connect(m_actSave, &QAction::triggered, this, &MainWindow::saveProject);

    m_actSaveAs = fileMenu->addAction("Guardar Proyecto Como...");
    m_actSaveAs->setShortcut(QKeySequence::SaveAs);
    connect(m_actSaveAs, &QAction::triggered, this, &MainWindow::saveProjectAs);

    fileMenu->addSeparator();

    QAction *actImport = fileMenu->addAction("Importar Medios...");
    actImport->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_I));
    connect(actImport, &QAction::triggered, m_mediaLibrary, [this]() {
        // Trigger dialog in media library
        QMetaObject::invokeMethod(m_mediaLibrary, "importFilesDialog");
    });

    fileMenu->addSeparator();
    QAction *actExport = fileMenu->addAction("Exportar Video (MP4)...");
    actExport->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    connect(actExport, &QAction::triggered, this, &MainWindow::showExportDialog);

    fileMenu->addSeparator();
    QAction *actExit = fileMenu->addAction("Salir");
    actExit->setShortcut(QKeySequence::Quit);
    connect(actExit, &QAction::triggered, qApp, &QApplication::quit);

    // Editar
    QMenu *editMenu = mb->addMenu("Editar");
    m_actUndo = editMenu->addAction("Deshacer");
    m_actUndo->setShortcut(QKeySequence::Undo);
    connect(m_actUndo, &QAction::triggered, this, &MainWindow::onUndoAction);

    m_actRedo = editMenu->addAction("Rehacer");
    m_actRedo->setShortcut(QKeySequence::Redo);
    connect(m_actRedo, &QAction::triggered, this, &MainWindow::onRedoAction);

    editMenu->addSeparator();

    QAction *actSplit = editMenu->addAction("Cortar en el cabezal (Split)");
    actSplit->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_K));
    connect(actSplit, &QAction::triggered, this, &MainWindow::onSplitAction);

    m_actJoin = editMenu->addAction("Unir clips seleccionados");
    m_actJoin->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_J));
    connect(m_actJoin, &QAction::triggered, this, &MainWindow::onJoinAction);

    QAction *actSeparate = editMenu->addAction("Separar Audio de Video");
    actSeparate->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_U));
    connect(actSeparate, &QAction::triggered, this, &MainWindow::onSeparateAudioAction);

    QAction *actDuplicate = editMenu->addAction("Duplicar clip");
    actDuplicate->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    connect(actDuplicate, &QAction::triggered, this, &MainWindow::onDuplicateAction);

    QAction *actAddText = editMenu->addAction("🔤 Insertar Cuadro de Texto");
    actAddText->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    connect(actAddText, &QAction::triggered, this, &MainWindow::onAddTextAction);

    QAction *actGlobalProps = editMenu->addAction("🌐 Propiedades Generales del Video...");
    actGlobalProps->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
    connect(actGlobalProps, &QAction::triggered, m_inspectorWidget, &InspectorWidget::showGlobalProperties);

    editMenu->addSeparator();

    QAction *actSelectAll = editMenu->addAction("Seleccionar todo");
    actSelectAll->setShortcut(QKeySequence::SelectAll);
    connect(actSelectAll, &QAction::triggered, m_timelineWidget, &TimelineWidget::selectAll);

    QAction *actDelete = editMenu->addAction("Eliminar clip");
    actDelete->setShortcut(QKeySequence::Delete);
    connect(actDelete, &QAction::triggered, this, &MainWindow::onDeleteAction);

    // Ver
    QMenu *viewMenu = mb->addMenu("Ver");
    QAction *actZoomIn = viewMenu->addAction("Acercar Zoom");
    actZoomIn->setShortcut(QKeySequence::ZoomIn);
    connect(actZoomIn, &QAction::triggered, this, &MainWindow::onZoomInAction);

    QAction *actZoomOut = viewMenu->addAction("Alejar Zoom");
    actZoomOut->setShortcut(QKeySequence::ZoomOut);
    connect(actZoomOut, &QAction::triggered, this, &MainWindow::onZoomOutAction);

    QAction *actZoomFit = viewMenu->addAction("Ajustar a Pantalla");
    actZoomFit->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    connect(actZoomFit, &QAction::triggered, this, &MainWindow::onZoomFitAction);

    viewMenu->addSeparator();

    m_actToggleSnap = viewMenu->addAction("🧲 Snapping Magnético");
    m_actToggleSnap->setCheckable(true);
    m_actToggleSnap->setChecked(true);
    m_actToggleSnap->setShortcut(QKeySequence(Qt::Key_S));
    connect(m_actToggleSnap, &QAction::triggered, this, &MainWindow::onToggleSnapping);

    m_actToggleSafeAreas = viewMenu->addAction("📐 Áreas Seguras (Safe Areas)");
    m_actToggleSafeAreas->setCheckable(true);
    m_actToggleSafeAreas->setChecked(false);
    m_actToggleSafeAreas->setShortcut(QKeySequence(Qt::ALT | Qt::Key_S));
    connect(m_actToggleSafeAreas, &QAction::triggered, this, &MainWindow::onToggleSafeAreas);

    // Efectos
    QMenu *effectsMenu = mb->addMenu("Efectos");
    TimelineWidget::populateEffectsMenu(effectsMenu, [this](VisualFilter f) {
        m_timelineWidget->applyFilterToSelectedClips(f);
    });

    // Ayuda
    QMenu *helpMenu = mb->addMenu("Ayuda");
    QAction *actAbout = helpMenu->addAction("Acerca de CPP VideoEditor");
    connect(actAbout, &QAction::triggered, this, [this]() {
        QMessageBox::about(
            this,
            "Acerca de CPP VideoEditor",
            "<b>CPP VideoEditor v2.0</b><br>"
            "Editor de video no lineal (NLE) moderno desarrollado con C++17, Qt 6 y FFmpeg.<br><br>"
            "Características:<br>"
            "• Separación independiente de audio y video.<br>"
            "• Línea de tiempo multipista con recorte (trimming) y formas de onda reales.<br>"
            "• Previsualizador 16:9 con composición en tiempo real.<br>"
            "• Barra lateral de recursos con miniaturas y filtros.<br>"
            "• Exportación de alta calidad a MP4 con FFmpeg."
        );
    });
}

void MainWindow::setupToolbar()
{
    QToolBar *toolbar = addToolBar("Herramientas de Edición");
    toolbar->setMovable(false);
    toolbar->setStyleSheet("QToolBar { background-color: #161b22; border-bottom: 1px solid #30363d; padding: 4px; spacing: 6px; }");

    auto createToolBtn = [this](const QString &text, const QString &tooltip) {
        QPushButton *btn = new QPushButton(text, this);
        btn->setToolTip(tooltip);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            "QPushButton {"
            "  background-color: #21262d; color: #c9d1d9; border: 1px solid #30363d; border-radius: 4px; padding: 5px 10px; font-weight: bold; font-size: 11px;"
            "}"
            "QPushButton:hover { background-color: #30363d; color: white; }"
            "QPushButton:pressed { background-color: #161b22; }"
        );
        return btn;
    };

    m_undoBtn = createToolBtn("↶ Deshacer", "Deshacer la última acción (Ctrl+Z)");
    connect(m_undoBtn, &QPushButton::clicked, this, &MainWindow::onUndoAction);
    toolbar->addWidget(m_undoBtn);

    m_redoBtn = createToolBtn("↷ Rehacer", "Rehacer la última acción deshecha (Ctrl+Shift+Z)");
    connect(m_redoBtn, &QPushButton::clicked, this, &MainWindow::onRedoAction);
    toolbar->addWidget(m_redoBtn);

    toolbar->addSeparator();

    QPushButton *splitBtn = createToolBtn("✂ Cortar (Split)", "Dividir clip(s) seleccionados en el cabezal (Ctrl+K)");
    connect(splitBtn, &QPushButton::clicked, this, &MainWindow::onSplitAction);
    toolbar->addWidget(splitBtn);

    m_joinBtn = createToolBtn("🔗 Unir", "Unir/fusionar clips seleccionados (Ctrl+J)");
    m_joinBtn->setEnabled(false);
    connect(m_joinBtn, &QPushButton::clicked, this, &MainWindow::onJoinAction);
    toolbar->addWidget(m_joinBtn);

    QPushButton *separateAudioBtn = createToolBtn("🎵 Separar Audio", "Separar y desvincular el audio del video seleccionado");
    separateAudioBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #d29922; color: #0d1117; border-radius: 4px; padding: 5px 12px; font-weight: bold; font-size: 11px;"
        "}"
        "QPushButton:hover { background-color: #e3b341; }"
        "QPushButton:pressed { background-color: #b08018; }"
    );
    connect(separateAudioBtn, &QPushButton::clicked, this, &MainWindow::onSeparateAudioAction);
    toolbar->addWidget(separateAudioBtn);

    QPushButton *addTextBtn = createToolBtn("🔤 Añadir Texto", "Insertar cuadro de texto en la posición del cabezal (Ctrl+T)");
    addTextBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #238636; color: white; border-radius: 4px; padding: 5px 12px; font-weight: bold; font-size: 11px;"
        "}"
        "QPushButton:hover { background-color: #2ea043; }"
        "QPushButton:pressed { background-color: #1a6328; }"
    );
    connect(addTextBtn, &QPushButton::clicked, this, &MainWindow::onAddTextAction);
    toolbar->addWidget(addTextBtn);

    QToolButton *effectsBtn = new QToolButton(this);
    effectsBtn->setText("✨ Efectos ▾");
    effectsBtn->setToolTip("Aplicar efectos y filtros visuales a los clips seleccionados");
    effectsBtn->setPopupMode(QToolButton::InstantPopup);
    QMenu *effectsPopup = new QMenu(effectsBtn);
    TimelineWidget::populateEffectsMenu(effectsPopup, [this](VisualFilter f) {
        m_timelineWidget->applyFilterToSelectedClips(f);
    });
    effectsBtn->setMenu(effectsPopup);
    effectsBtn->setStyleSheet(
        "QToolButton {"
        "  background-color: #1f6feb; color: white; border-radius: 4px; padding: 5px 12px; font-weight: bold; font-size: 11px;"
        "}"
        "QToolButton:hover { background-color: #388bfd; }"
        "QToolButton:pressed { background-color: #1158c7; }"
        "QToolButton::menu-indicator { image: none; }"
    );
    toolbar->addWidget(effectsBtn);

    QPushButton *duplicateBtn = createToolBtn("📋 Duplicar", "Duplicar clip seleccionado (Ctrl+D)");
    connect(duplicateBtn, &QPushButton::clicked, this, &MainWindow::onDuplicateAction);
    toolbar->addWidget(duplicateBtn);

    QPushButton *deleteBtn = createToolBtn("🗑 Eliminar", "Eliminar clip seleccionado (Delete)");
    connect(deleteBtn, &QPushButton::clicked, this, &MainWindow::onDeleteAction);
    toolbar->addWidget(deleteBtn);

    QPushButton *rippleDeleteBtn = createToolBtn("⏪ Ripple Delete", "Eliminar clip y cerrar hueco (Shift+Delete)");
    connect(rippleDeleteBtn, &QPushButton::clicked, this, [this]() {
        m_timelineWidget->deleteSelectedClip(true);
    });
    toolbar->addWidget(rippleDeleteBtn);

    toolbar->addSeparator();

    QPushButton *addVideoTrackBtn = createToolBtn("+ Pista Video", "Añadir una nueva pista de video en la parte superior");
    connect(addVideoTrackBtn, &QPushButton::clicked, m_timelineWidget, &TimelineWidget::addVideoTrack);
    toolbar->addWidget(addVideoTrackBtn);

    QPushButton *addAudioTrackBtn = createToolBtn("+ Pista Audio", "Añadir una nueva pista de audio");
    connect(addAudioTrackBtn, &QPushButton::clicked, m_timelineWidget, &TimelineWidget::addAudioTrack);
    toolbar->addWidget(addAudioTrackBtn);

    toolbar->addSeparator();

    QPushButton *zoomInBtn = createToolBtn("🔍 +", "Acercar Zoom");
    connect(zoomInBtn, &QPushButton::clicked, this, &MainWindow::onZoomInAction);
    toolbar->addWidget(zoomInBtn);

    QPushButton *zoomOutBtn = createToolBtn("🔍 -", "Alejar Zoom");
    connect(zoomOutBtn, &QPushButton::clicked, this, &MainWindow::onZoomOutAction);
    toolbar->addWidget(zoomOutBtn);

    QPushButton *zoomFitBtn = createToolBtn("Ajustar", "Ajustar la vista completa a la ventana");
    connect(zoomFitBtn, &QPushButton::clicked, this, &MainWindow::onZoomFitAction);
    toolbar->addWidget(zoomFitBtn);

    toolbar->addSeparator();

    m_snapBtn = createToolBtn("🧲 Snap: ON", "Alternar Snapping magnético en monitor y línea de tiempo (S)");
    m_snapBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #238636; color: white; border: 1px solid #2ea043; border-radius: 4px; padding: 5px 12px; font-weight: bold; font-size: 11px;"
        "}"
        "QPushButton:hover { background-color: #2ea043; }"
        "QPushButton:pressed { background-color: #1a6b2c; }"
    );
    connect(m_snapBtn, &QPushButton::clicked, this, &MainWindow::onToggleSnapping);
    toolbar->addWidget(m_snapBtn);

    QPushButton *safeAreasBtn = createToolBtn("📐 Guías", "Mostrar u ocultar guías de Áreas Seguras (Alt+S)");
    connect(safeAreasBtn, &QPushButton::clicked, this, &MainWindow::onToggleSafeAreas);
    toolbar->addWidget(safeAreasBtn);

    QPushButton *globalPropsBtn = createToolBtn("🌐 General", "Abrir cuadro de propiedades generales del video (Luminosidad, Brillo, RGB)");
    connect(globalPropsBtn, &QPushButton::clicked, m_inspectorWidget, &InspectorWidget::showGlobalProperties);
    toolbar->addWidget(globalPropsBtn);

    toolbar->addSeparator();

    QLabel *durLabel = new QLabel("⏱ Duración:", this);
    durLabel->setStyleSheet("color: #d29922; font-weight: bold; font-size: 11px; margin-left: 4px;");
    toolbar->addWidget(durLabel);

    m_durationSpin = new QDoubleSpinBox(this);
    m_durationSpin->setRange(0.5, 7200.0);
    m_durationSpin->setSingleStep(1.0);
    m_durationSpin->setDecimals(1);
    m_durationSpin->setValue(m_timelineModel ? m_timelineModel->totalDurationMs() / 1000.0 : 10.0);
    m_durationSpin->setSuffix(" s");
    m_durationSpin->setToolTip("Duración del video / secuencia en la línea de tiempo (segundos)");
    m_durationSpin->setStyleSheet(
        "QDoubleSpinBox {"
        "  background-color: #0d1117; color: #c9d1d9; border: 1px solid #30363d; border-radius: 4px; padding: 4px 6px; font-weight: bold; font-size: 11px; min-width: 65px;"
        "}"
        "QDoubleSpinBox:focus { border-color: #58a6ff; }"
    );
    connect(m_durationSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::onDurationSpinChanged);
    toolbar->addWidget(m_durationSpin);

    m_autoDurationBtn = createToolBtn("Auto", "Ajustar la duración automáticamente al final de los clips");
    m_autoDurationBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #21262d; color: #8b949e; border: 1px solid #30363d; border-radius: 4px; padding: 4px 8px; font-size: 11px;"
        "}"
        "QPushButton:hover { background-color: #30363d; color: white; }"
    );
    connect(m_autoDurationBtn, &QPushButton::clicked, this, &MainWindow::onAutoDurationClicked);
    toolbar->addWidget(m_autoDurationBtn);

    toolbar->addSeparator();

    QLabel *ratioLabel = new QLabel("📐 Formato:", this);
    ratioLabel->setStyleSheet("color: #58a6ff; font-weight: bold; font-size: 11px; margin-left: 4px;");
    toolbar->addWidget(ratioLabel);

    m_aspectRatioCombo = new QComboBox(this);
    m_aspectRatioCombo->addItem("16:9 Panorámico (1920x1080)", static_cast<int>(ProjectAspectRatio::Landscape_16_9));
    m_aspectRatioCombo->addItem("9:16 Vertical (1080x1920)", static_cast<int>(ProjectAspectRatio::Vertical_9_16));
    m_aspectRatioCombo->addItem("1:1 Cuadrado (1080x1080)", static_cast<int>(ProjectAspectRatio::Square_1_1));
    m_aspectRatioCombo->addItem("4:3 Clásico (1440x1080)", static_cast<int>(ProjectAspectRatio::Classic_4_3));
    m_aspectRatioCombo->addItem("21:9 Cine (2560x1080)", static_cast<int>(ProjectAspectRatio::Cinema_21_9));
    m_aspectRatioCombo->setToolTip("Formato y relación de aspecto del proyecto (Canvas)");
    m_aspectRatioCombo->setStyleSheet(
        "QComboBox {"
        "  background-color: #0d1117; color: #c9d1d9; border: 1px solid #30363d; border-radius: 4px; padding: 3px 8px; font-weight: bold; font-size: 11px; min-width: 140px;"
        "}"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background-color: #161b22; color: #c9d1d9; selection-background-color: #1f6feb; }"
    );
    if (m_timelineModel) {
        int idx = m_aspectRatioCombo->findData(static_cast<int>(m_timelineModel->aspectRatio()));
        if (idx >= 0) m_aspectRatioCombo->setCurrentIndex(idx);
    }
    connect(m_aspectRatioCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onAspectRatioChanged);
    toolbar->addWidget(m_aspectRatioCombo);

    // Spacer
    QWidget *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar->addWidget(spacer);

    // Export button
    QPushButton *exportBtn = createToolBtn("🎬 Exportar Video (MP4)", "Renderizar el proyecto editado a archivo MP4");
    exportBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #238636; color: white; border-radius: 4px; padding: 6px 16px; font-weight: bold; font-size: 12px;"
        "}"
        "QPushButton:hover { background-color: #2ea043; }"
        "QPushButton:pressed { background-color: #1a6b2c; }"
    );
    connect(exportBtn, &QPushButton::clicked, this, &MainWindow::showExportDialog);
    toolbar->addWidget(exportBtn);
}

void MainWindow::setupUiLayout()
{
    // Main vertical splitter separating Top Panels from Bottom Timeline
    QSplitter *verticalSplitter = new QSplitter(Qt::Vertical, this);

    // Top horizontal splitter separating Media Library, Preview Monitor, and Inspector
    QSplitter *topHorizontalSplitter = new QSplitter(Qt::Horizontal, verticalSplitter);

    m_mediaLibrary->setMinimumWidth(240);
    m_mediaLibrary->setMaximumWidth(360);
    topHorizontalSplitter->addWidget(m_mediaLibrary);

    m_previewWidget->setMinimumWidth(400);
    topHorizontalSplitter->addWidget(m_previewWidget);

    m_inspectorWidget->setMinimumWidth(340);
    m_inspectorWidget->setMaximumWidth(650);
    topHorizontalSplitter->addWidget(m_inspectorWidget);

    topHorizontalSplitter->setStretchFactor(0, 0);
    topHorizontalSplitter->setStretchFactor(1, 1);
    topHorizontalSplitter->setStretchFactor(2, 0);
    topHorizontalSplitter->setCollapsible(0, false);
    topHorizontalSplitter->setCollapsible(1, false);
    topHorizontalSplitter->setCollapsible(2, false);
    topHorizontalSplitter->setSizes({260, 720, 360});

    verticalSplitter->addWidget(topHorizontalSplitter);

    m_timelineWidget->setMinimumHeight(240);
    verticalSplitter->addWidget(m_timelineWidget);

    verticalSplitter->setStretchFactor(0, 1);
    verticalSplitter->setStretchFactor(1, 1);
    verticalSplitter->setCollapsible(0, false);
    verticalSplitter->setCollapsible(1, false);
    verticalSplitter->setSizes({500, 350});

    setCentralWidget(verticalSplitter);
}

void MainWindow::applyDarkTheme()
{
    setStyleSheet(
        "QMainWindow, QWidget {"
        "  background-color: #0d1117; color: #c9d1d9; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;"
        "}"
        "QMenuBar {"
        "  background-color: #161b22; color: #c9d1d9; border-bottom: 1px solid #30363d; padding: 2px;"
        "}"
        "QMenuBar::item:selected {"
        "  background-color: #1f293d; color: #58a6ff;"
        "}"
        "QSplitter::handle {"
        "  background-color: #21262d;"
        "}"
        "QSplitter::handle:hover {"
        "  background-color: #58a6ff;"
        "}"
    );
}

void MainWindow::newProject()
{
    if (m_isProjectModified) {
        auto res = QMessageBox::question(this, "Guardar cambios",
            "¿Deseas guardar los cambios del proyecto actual antes de crear uno nuevo?",
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (res == QMessageBox::Save) {
            saveProject();
        } else if (res == QMessageBox::Cancel) {
            return;
        }
    }

    m_timelineModel->resetProject();
    m_mediaLibrary->clearItems();
    m_currentProjectPath.clear();
    m_isProjectModified = false;
    updateWindowTitle();
    m_previewWidget->setPosition(0);
    m_timelineWidget->setPlayheadPosition(0);
    m_inspectorWidget->setSelectedClip(-1);
    m_previewWidget->updatePreview();
}

void MainWindow::showExportDialog()
{
    ExportDialog dialog(m_timelineModel, m_timelineWidget ? m_timelineWidget->playheadPosition() : 0, this);
    dialog.exec();
}

void MainWindow::onMediaItemAddedToTimeline(const MediaItem &item, bool separateAudio)
{
    qint64 playhead = m_timelineWidget->playheadPosition();
    qint64 targetTrackId = -1; // auto detect
    m_timelineModel->addMediaClip(item.filePath(), item.type(), targetTrackId, playhead, item.durationMs(), separateAudio);
}

void MainWindow::onAddTextAction()
{
    qint64 playhead = m_timelineWidget ? m_timelineWidget->playheadPosition() : 0;
    qint64 textId = m_timelineModel->addTextClip("Texto de ejemplo", playhead);
    if (textId > 0) {
        if (m_timelineWidget) {
            m_timelineWidget->setSelectedClipId(textId);
        }
        if (m_previewWidget) {
            m_previewWidget->setSelectedClipId(textId);
            m_previewWidget->updatePreview();
        }
        if (m_inspectorWidget) {
            m_inspectorWidget->setSelectedClip(textId);
        }
    }
}

void MainWindow::onSeparateAudioAction()
{
    qint64 selId = m_timelineWidget->selectedClipId();
    if (selId > 0) {
        m_timelineModel->separateAudio(selId);
    } else {
        QMessageBox::information(this, "Selecciona un clip", "Selecciona primero un clip de video en la línea de tiempo para separar su audio.");
    }
}

void MainWindow::onSplitAction()
{
    m_timelineWidget->splitSelectedClips();
}

void MainWindow::onJoinAction()
{
    m_timelineWidget->joinSelectedClips();
}

void MainWindow::onDeleteAction()
{
    m_timelineWidget->deleteSelectedClips(false);
}

void MainWindow::onDuplicateAction()
{
    m_timelineWidget->duplicateSelectedClip();
}

void MainWindow::onUndoAction()
{
    m_timelineWidget->undo();
    m_previewWidget->updatePreview();
}

void MainWindow::onRedoAction()
{
    m_timelineWidget->redo();
    m_previewWidget->updatePreview();
}

void MainWindow::updateUndoRedoUi()
{
    bool canUndo = m_timelineModel ? m_timelineModel->canUndo() : false;
    bool canRedo = m_timelineModel ? m_timelineModel->canRedo() : false;

    if (m_actUndo) {
        m_actUndo->setEnabled(canUndo);
        if (canUndo && !m_timelineModel->undoText().isEmpty()) {
            m_actUndo->setText(QString("Deshacer %1").arg(m_timelineModel->undoText()));
        } else {
            m_actUndo->setText("Deshacer");
        }
    }
    if (m_actRedo) {
        m_actRedo->setEnabled(canRedo);
        if (canRedo && !m_timelineModel->redoText().isEmpty()) {
            m_actRedo->setText(QString("Rehacer %1").arg(m_timelineModel->redoText()));
        } else {
            m_actRedo->setText("Rehacer");
        }
    }
    if (m_undoBtn) {
        m_undoBtn->setEnabled(canUndo);
        m_undoBtn->setToolTip(canUndo && !m_timelineModel->undoText().isEmpty() ?
            QString("Deshacer %1 (Ctrl+Z)").arg(m_timelineModel->undoText()) : "Deshacer (Ctrl+Z)");
    }
    if (m_redoBtn) {
        m_redoBtn->setEnabled(canRedo);
        m_redoBtn->setToolTip(canRedo && !m_timelineModel->redoText().isEmpty() ?
            QString("Rehacer %1 (Ctrl+Shift+Z)").arg(m_timelineModel->redoText()) : "Rehacer (Ctrl+Shift+Z)");
    }
}

void MainWindow::onZoomInAction()
{
    m_timelineWidget->zoomIn();
}

void MainWindow::onZoomOutAction()
{
    m_timelineWidget->zoomOut();
}

void MainWindow::onZoomFitAction()
{
    m_timelineWidget->zoomToFit();
}

void MainWindow::onToggleSnapping()
{
    bool enabled = !m_timelineWidget->isSnappingEnabled();
    m_timelineWidget->setSnappingEnabled(enabled);
    m_previewWidget->setSnappingEnabled(enabled);

    if (m_actToggleSnap) {
        m_actToggleSnap->setChecked(enabled);
    }

    if (m_snapBtn) {
        if (enabled) {
            m_snapBtn->setText("🧲 Snap: ON");
            m_snapBtn->setStyleSheet(
                "QPushButton {"
                "  background-color: #238636; color: white; border: 1px solid #2ea043; border-radius: 4px; padding: 5px 12px; font-weight: bold; font-size: 11px;"
                "}"
                "QPushButton:hover { background-color: #2ea043; }"
                "QPushButton:pressed { background-color: #1a6b2c; }"
            );
        } else {
            m_snapBtn->setText("🧲 Snap: OFF");
            m_snapBtn->setStyleSheet(
                "QPushButton {"
                "  background-color: #30363d; color: #8b949e; border: 1px solid #484f58; border-radius: 4px; padding: 5px 12px; font-weight: bold; font-size: 11px;"
                "}"
                "QPushButton:hover { background-color: #484f58; color: white; }"
                "QPushButton:pressed { background-color: #21262d; }"
            );
        }
    }
}

void MainWindow::onToggleSafeAreas()
{
    bool show = !m_previewWidget->showSafeAreas();
    m_previewWidget->setShowSafeAreas(show);

    if (m_actToggleSafeAreas) {
        m_actToggleSafeAreas->setChecked(show);
    }
}

void MainWindow::onDurationSpinChanged(double seconds)
{
    if (m_updatingDurationUi || !m_timelineModel) return;
    m_timelineModel->setCustomDurationMs(qRound(seconds * 1000.0));
    updateDurationUi();
}

void MainWindow::onAutoDurationClicked()
{
    if (!m_timelineModel) return;
    m_timelineModel->setCustomDurationMs(0); // 0 = Auto
    updateDurationUi();
}

void MainWindow::updateDurationUi()
{
    if (!m_timelineModel || !m_durationSpin) return;
    m_updatingDurationUi = true;
    double sec = m_timelineModel->totalDurationMs() / 1000.0;
    m_durationSpin->setValue(sec);

    if (m_autoDurationBtn) {
        if (m_timelineModel->isCustomDuration()) {
            m_autoDurationBtn->setText("Fijada (Auto)");
            m_autoDurationBtn->setStyleSheet(
                "QPushButton {"
                "  background-color: #d29922; color: #0d1117; border: 1px solid #d29922; border-radius: 4px; padding: 4px 8px; font-weight: bold; font-size: 11px;"
                "}"
                "QPushButton:hover { background-color: #e3b341; }"
            );
        } else {
            m_autoDurationBtn->setText("Auto");
            m_autoDurationBtn->setStyleSheet(
                "QPushButton {"
                "  background-color: #21262d; color: #8b949e; border: 1px solid #30363d; border-radius: 4px; padding: 4px 8px; font-size: 11px;"
                "}"
                "QPushButton:hover { background-color: #30363d; color: white; }"
            );
        }
    }
    m_updatingDurationUi = false;
}

void MainWindow::openProject()
{
    if (m_isProjectModified) {
        auto res = QMessageBox::question(this, "Guardar cambios",
            "¿Deseas guardar los cambios del proyecto actual antes de abrir otro?",
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (res == QMessageBox::Save) {
            saveProject();
        } else if (res == QMessageBox::Cancel) {
            return;
        }
    }

    QString filePath = QFileDialog::getOpenFileName(
        this, "Abrir Proyecto de Video", QString(), "Proyectos CPP VideoEditor (*.veproj);;Todos los archivos (*.*)"
    );
    if (filePath.isEmpty()) return;

    QStringList mediaFiles;
    if (ProjectSerializer::loadProject(filePath, m_timelineModel, mediaFiles)) {
        m_mediaLibrary->clearItems();
        for (const QString &mf : mediaFiles) {
            if (!mf.isEmpty()) {
                m_mediaLibrary->addMediaFile(mf);
            }
        }
        m_currentProjectPath = filePath;
        m_isProjectModified = false;
        addRecentProject(filePath);
        updateWindowTitle();
        if (m_aspectRatioCombo) {
            int idx = m_aspectRatioCombo->findData(static_cast<int>(m_timelineModel->aspectRatio()));
            if (idx >= 0) m_aspectRatioCombo->setCurrentIndex(idx);
        }
        m_previewWidget->setPosition(0);
        m_timelineWidget->setPlayheadPosition(0);
        m_timelineWidget->zoomToFit();
        m_previewWidget->updatePreview();
    } else {
        QMessageBox::critical(this, "Error", "No se pudo abrir el proyecto seleccionado.");
    }
}

void MainWindow::saveProject()
{
    if (m_currentProjectPath.isEmpty()) {
        saveProjectAs();
        return;
    }

    QStringList mediaFiles;
    for (const MediaItem &item : m_mediaLibrary->items()) {
        mediaFiles << item.filePath();
    }

    if (ProjectSerializer::saveProject(m_currentProjectPath, m_timelineModel, mediaFiles)) {
        m_isProjectModified = false;
        updateWindowTitle();
        addRecentProject(m_currentProjectPath);
    } else {
        QMessageBox::critical(this, "Error al guardar", "No se pudo guardar el archivo del proyecto:\n" + m_currentProjectPath);
    }
}

void MainWindow::saveProjectAs()
{
    QString defaultPath = m_currentProjectPath.isEmpty() ? "mi_proyecto.veproj" : m_currentProjectPath;
    QString filePath = QFileDialog::getSaveFileName(
        this, "Guardar Proyecto Como", defaultPath, "Proyectos CPP VideoEditor (*.veproj);;Todos los archivos (*.*)"
    );
    if (filePath.isEmpty()) return;

    if (!filePath.endsWith(".veproj", Qt::CaseInsensitive)) {
        filePath += ".veproj";
    }

    m_currentProjectPath = filePath;
    saveProject();
}

void MainWindow::openRecentProject(const QString &filePath)
{
    if (m_isProjectModified) {
        auto res = QMessageBox::question(this, "Guardar cambios",
            "¿Deseas guardar los cambios del proyecto actual antes de abrir otro?",
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (res == QMessageBox::Save) {
            saveProject();
        } else if (res == QMessageBox::Cancel) {
            return;
        }
    }

    if (!QFile::exists(filePath)) {
        QMessageBox::warning(this, "Archivo no encontrado", "El archivo de proyecto especificado no existe:\n" + filePath);
        return;
    }

    QStringList mediaFiles;
    if (ProjectSerializer::loadProject(filePath, m_timelineModel, mediaFiles)) {
        m_mediaLibrary->clearItems();
        for (const QString &mf : mediaFiles) {
            if (!mf.isEmpty()) {
                m_mediaLibrary->addMediaFile(mf);
            }
        }
        m_currentProjectPath = filePath;
        m_isProjectModified = false;
        addRecentProject(filePath);
        updateWindowTitle();
        if (m_aspectRatioCombo) {
            int idx = m_aspectRatioCombo->findData(static_cast<int>(m_timelineModel->aspectRatio()));
            if (idx >= 0) m_aspectRatioCombo->setCurrentIndex(idx);
        }
        m_previewWidget->setPosition(0);
        m_timelineWidget->setPlayheadPosition(0);
        m_timelineWidget->zoomToFit();
        m_previewWidget->updatePreview();
    } else {
        QMessageBox::critical(this, "Error", "No se pudo abrir el proyecto reciente.");
    }
}

void MainWindow::addRecentProject(const QString &filePath)
{
    if (filePath.isEmpty()) return;
    QSettings settings("CPP_VideoEditor", "CPP_VideoEditor");
    QStringList recents = settings.value("recentProjects").toStringList();
    recents.removeAll(filePath);
    recents.prepend(filePath);
    while (recents.size() > 10) {
        recents.removeLast();
    }
    settings.setValue("recentProjects", recents);
    updateRecentProjectsMenu();
}

void MainWindow::updateRecentProjectsMenu()
{
    if (!m_recentProjectsMenu) return;
    m_recentProjectsMenu->clear();
    QSettings settings("CPP_VideoEditor", "CPP_VideoEditor");
    QStringList recents = settings.value("recentProjects").toStringList();

    if (recents.isEmpty()) {
        QAction *emptyAct = m_recentProjectsMenu->addAction("(Sin proyectos recientes)");
        emptyAct->setEnabled(false);
        return;
    }

    for (const QString &path : recents) {
        QAction *act = m_recentProjectsMenu->addAction(QFileInfo(path).fileName());
        act->setToolTip(path);
        connect(act, &QAction::triggered, this, [this, path]() {
            openRecentProject(path);
        });
    }

    m_recentProjectsMenu->addSeparator();
    QAction *clearAct = m_recentProjectsMenu->addAction("Borrar historial");
    connect(clearAct, &QAction::triggered, this, [this]() {
        QSettings settings("CPP_VideoEditor", "CPP_VideoEditor");
        settings.remove("recentProjects");
        updateRecentProjectsMenu();
    });
}

void MainWindow::performAutosave()
{
    if (!m_isProjectModified || !m_timelineModel) return;

    QString autosavePath;
    if (!m_currentProjectPath.isEmpty()) {
        QFileInfo fi(m_currentProjectPath);
        autosavePath = fi.dir().filePath(fi.baseName() + ".autosave.veproj");
    } else {
        autosavePath = QDir::temp().filePath("CPP_VideoEditor_untitled.autosave.veproj");
    }

    QStringList mediaFiles;
    for (const MediaItem &item : m_mediaLibrary->items()) {
        mediaFiles << item.filePath();
    }

    ProjectSerializer::saveProject(autosavePath, m_timelineModel, mediaFiles);
}

void MainWindow::onAspectRatioChanged(int index)
{
    if (!m_aspectRatioCombo || !m_timelineModel) return;
    ProjectAspectRatio ratio = static_cast<ProjectAspectRatio>(m_aspectRatioCombo->itemData(index).toInt());
    if (m_timelineModel->aspectRatio() != ratio) {
        m_timelineModel->setAspectRatio(ratio);
        m_previewWidget->updatePreview();
    }
}

void MainWindow::updateWindowTitle()
{
    QString name = m_currentProjectPath.isEmpty() ? "Sin título" : QFileInfo(m_currentProjectPath).fileName();
    if (m_isProjectModified) {
        name += " *";
    }
    setWindowTitle(QString("%1 - CPP VideoEditor").arg(name));
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_isProjectModified) {
        auto res = QMessageBox::question(this, "Guardar cambios",
            "¿Deseas guardar los cambios antes de salir?",
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (res == QMessageBox::Save) {
            saveProject();
            event->accept();
        } else if (res == QMessageBox::Discard) {
            event->accept();
        } else {
            event->ignore();
        }
    } else {
        event->accept();
    }
}
