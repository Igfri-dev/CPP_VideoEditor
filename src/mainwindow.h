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

    QAction *m_actSave = nullptr;
    QAction *m_actSaveAs = nullptr;
    QMenu *m_recentProjectsMenu = nullptr;
    QAction *m_actUndo = nullptr;
    QAction *m_actRedo = nullptr;
    QAction *m_actJoin = nullptr;
    QAction *m_actToggleSnap = nullptr;
    QAction *m_actToggleSafeAreas = nullptr;
    QPushButton *m_undoBtn = nullptr;
    QPushButton *m_redoBtn = nullptr;
    QPushButton *m_joinBtn = nullptr;
    QPushButton *m_snapBtn = nullptr;

    QComboBox *m_aspectRatioCombo = nullptr;
    QDoubleSpinBox *m_durationSpin = nullptr;
    QPushButton *m_autoDurationBtn = nullptr;
    bool m_updatingDurationUi = false;

    QString m_currentProjectPath;
    bool m_isProjectModified = false;
    QTimer m_autosaveTimer;
};
