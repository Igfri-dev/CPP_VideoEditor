#pragma once

#include <QMainWindow>
#include "core/timelinemodel.h"
#include "engine/audioengine.h"
#include "medialibrary/medialibrarywidget.h"
#include "inspector/inspectorwidget.h"
#include "previewwidget.h"
#include "timelinewidget.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void newProject();
    void openProject();
    void saveProject();
    void saveProjectAs();
    void openRecentProject(const QString &filePath);
    void updateRecentProjectsMenu();
    void performAutosave();
    void showExportDialog();
    void onMediaItemAddedToTimeline(const MediaItem &item, bool separateAudio);
    void onAddTextAction();
    void onSeparateAudioAction();
    void onSplitAction();
    void onJoinAction();
    void onDeleteAction();
    void onDuplicateAction();
    void onUndoAction();
    void onRedoAction();
    void updateUndoRedoUi();
    void onZoomInAction();
    void onZoomOutAction();
    void onZoomFitAction();
    void onToggleSnapping();
    void onToggleSafeAreas();
    void onAspectRatioChanged(int index);
    void onDurationSpinChanged(double seconds);
    void onAutoDurationClicked();
    void updateDurationUi();
    void updateWindowTitle();

    // 13 Professional Features slots
    void onOpenScopes();
    void onToggleProxyMode();
    void onAddAdjustmentLayerAction();
    void onAddSolidColorAction();
    void onCreateCompoundClipAction();
    void onDecomposeCompoundClipAction();
    void onOpenAudioMixerAction();
    void onOpenColorManagementAction();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void setupMenus();
    void setupToolbar();
    void setupUiLayout();
    void applyDarkTheme();
    void addRecentProject(const QString &filePath);

    TimelineModel *m_timelineModel = nullptr;
    AudioEngine *m_audioEngine = nullptr;

    MediaLibraryWidget *m_mediaLibrary = nullptr;
    PreviewWidget *m_previewWidget = nullptr;
    InspectorWidget *m_inspectorWidget = nullptr;
    TimelineWidget *m_timelineWidget = nullptr;

    class VideoScopesWindow *m_scopesWindow = nullptr;

    QAction *m_actSave = nullptr;
    QAction *m_actSaveAs = nullptr;
    QMenu *m_recentProjectsMenu = nullptr;
    QAction *m_actUndo = nullptr;
    QAction *m_actRedo = nullptr;
    QAction *m_actJoin = nullptr;
    QAction *m_actToggleSnap = nullptr;
    QAction *m_actToggleSafeAreas = nullptr;

    // 13 Pro Features Actions & Buttons
    QAction *m_actScopes = nullptr;
    QAction *m_actProxyMode = nullptr;
    QAction *m_actAddAdjustmentLayer = nullptr;
    QAction *m_actAddSolidColor = nullptr;
    QAction *m_actCompoundClip = nullptr;
    QAction *m_actDecomposeCompound = nullptr;
    QAction *m_actAudioMixer = nullptr;
    QAction *m_actColorManagement = nullptr;

    QPushButton *m_undoBtn = nullptr;
    QPushButton *m_redoBtn = nullptr;
    QPushButton *m_joinBtn = nullptr;
    QPushButton *m_snapBtn = nullptr;

    QPushButton *m_scopesBtn = nullptr;
    QPushButton *m_proxyBtn = nullptr;
    QPushButton *m_adjLayerBtn = nullptr;
    QPushButton *m_compoundBtn = nullptr;
    QPushButton *m_audioMixerBtn = nullptr;
    QPushButton *m_colorMgmtBtn = nullptr;

    QComboBox *m_aspectRatioCombo = nullptr;
    QDoubleSpinBox *m_durationSpin = nullptr;
    QPushButton *m_autoDurationBtn = nullptr;
    bool m_updatingDurationUi = false;

    QString m_currentProjectPath;
    bool m_isProjectModified = false;
    QTimer m_autosaveTimer;
};
