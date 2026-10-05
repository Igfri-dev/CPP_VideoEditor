#pragma once

#include <QWidget>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QTextEdit>
#include <QFontComboBox>
#include <QListWidget>
#include <QStackedWidget>
#include "curveeditorwidget.h"
#include "../core/timelinemodel.h"

class EffectItemWidget : public QWidget {
    Q_OBJECT
public:
    EffectItemWidget(int index, int totalCount, VisualFilter filter, QWidget *parent = nullptr);

    int index() const { return m_index; }
    VisualFilter filter() const { return m_filter; }

signals:
    void moveUpRequested(int index);
    void moveDownRequested(int index);
    void removeRequested(int index);

private:
    int m_index = 0;
    VisualFilter m_filter = VisualFilter::None;
};

class EffectsListWidget : public QListWidget {
    Q_OBJECT
public:
    explicit EffectsListWidget(QWidget *parent = nullptr);

signals:
    void effectReordered(int fromIndex, int toIndex);

protected:
    void startDrag(Qt::DropActions supportedActions) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
};

class InspectorWidget : public QWidget {
    Q_OBJECT

public:
    explicit InspectorWidget(TimelineModel *model, QObject *parent = nullptr);

    void setSelectedClip(qint64 clipId);
    void setSelectedClips(const QSet<qint64> &clipIds);
    qint64 selectedClipId() const { return m_selectedClipId; }
    const QSet<qint64>& selectedClipIds() const { return m_selectedClipIds; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void clipPropertyModified(qint64 clipId);
    void globalPropertyModified();
    void exportRequested();
    void splitRequested(qint64 clipId);
    void deleteRequested(qint64 clipId);
    void duplicateRequested(qint64 clipId);
    void separateAudioRequested(qint64 clipId);

public slots:
    void showGlobalProperties();
    void showClipProperties();
    void updateGlobalPropertiesUi();
    void updateTransformValues();
    void onAddEffectClicked();
    void onEffectMoveUp(int index);
    void onEffectMoveDown(int index);
    void onEffectRemove(int index);
    void onEffectReordered(int fromIndex, int toIndex);
    void onClearEffectsClicked();

    // Clip Color Adjustment slots
    void onClipBrightnessChanged(int value);
    void onClipLuminosityChanged(int value);
    void onClipRedChanged(int value);
    void onClipGreenChanged(int value);
    void onClipBlueChanged(int value);
    void onClipResetColorClicked();
    void onClipColorGradeModeChanged(ColorGradeMode mode);
    void onClipColorCurveChanged(const ColorCurve &curve);
    void onClipColorCurveCommitted(const ColorCurve &curve);
    void onClipLumaCurveChanged(const ColorCurve &curve);
    void onClipLumaCurveCommitted(const ColorCurve &curve);
    void onClipResetCurvesClicked();

    // Global Color Adjustment slots
    void onGlobalBrightnessChanged(int value);
    void onGlobalLuminosityChanged(int value);
    void onGlobalRedChanged(int value);
    void onGlobalGreenChanged(int value);
    void onGlobalBlueChanged(int value);
    void onGlobalResetColorClicked();
    void onGlobalColorGradeModeChanged(ColorGradeMode mode);
    void onGlobalColorCurveChanged(const ColorCurve &curve);
    void onGlobalColorCurveCommitted(const ColorCurve &curve);
    void onGlobalLumaCurveChanged(const ColorCurve &curve);
    void onGlobalLumaCurveCommitted(const ColorCurve &curve);
    void onGlobalResetCurvesClicked();

private slots:
    void onOpacityChanged(int value);
    void onVolumeChanged(int value);
    void onFadeInChanged(double value);
    void onFadeOutChanged(double value);
    void onFilterChanged(int index);
    void onSpeedPresetClicked(double speed);
    void onSeparateAudioClicked();

    // Transform slots
    void onScaleChanged(int value);
    void onPosXChanged(int value);
    void onPosYChanged(int value);
    void onRotationChanged(int value);
    void onScaleModeChanged(int index);
    void onResetTransformClicked();

    // Motion Path slots
    void onMotionPresetChanged(int index);
    void onMotionStartChanged();
    void onMotionEndChanged();
    void onMotionWaypointSelected(int index);
    void onMotionWaypointChanged();
    void onMotionAddPointClicked();
    void onMotionRemovePointClicked();
    void onMotionAutoSmoothClicked();
    void onMotionReverseClicked();
    void onMotionEasingChanged(int index);

    // Text slots
    void onTextChanged();
    void onTextCursorPositionChanged();
    void onFontFamilyChanged(const QFont &font);
    void onFontSizeChanged(int size);
    void onBoldToggled(bool checked);
    void onItalicToggled(bool checked);
    void onUnderlineToggled(bool checked);
    void onAlignLeftClicked();
    void onAlignCenterClicked();
    void onAlignRightClicked();
    void onPickTextColor();
    void onQuickColorClicked(const QColor &color);
    void onPickBgColor();
    void onBoxWidthChanged(int width);
    void onZIndexChanged(int z);

private:
    void refreshUi();

    TimelineModel *m_model = nullptr;
    qint64 m_selectedClipId = -1;
    QSet<qint64> m_selectedClipIds;
    bool m_updatingUi = false;

    // View switcher tabs
    QPushButton *m_tabClipBtn = nullptr;
    QPushButton *m_tabGlobalBtn = nullptr;
    QStackedWidget *m_viewStack = nullptr;
    QWidget *m_clipPage = nullptr;
    QWidget *m_globalPage = nullptr;

    QLabel *m_noSelectionLabel = nullptr;
    QPushButton *m_openGlobalFromEmptyBtn = nullptr;
    QWidget *m_contentContainer = nullptr;

    QLabel *m_clipNameLabel = nullptr;
    QLabel *m_clipTypeBadge = nullptr;
    QLabel *m_trackLabel = nullptr;
    QLabel *m_timingLabel = nullptr;
    QLabel *m_techDetailsLabel = nullptr;

    QPushButton *m_separateAudioBtn = nullptr;

    // Transform controls (Scale, Position, Rotation, Z-Index)
    QWidget *m_transformSection = nullptr;
    QSlider *m_scaleSlider = nullptr;
    QSpinBox *m_scaleSpin = nullptr;
    QSpinBox *m_posXSpin = nullptr;
    QSpinBox *m_posYSpin = nullptr;
    QSlider *m_rotSlider = nullptr;
    QSpinBox *m_rotSpin = nullptr;
    QSpinBox *m_zIndexSpin = nullptr;
    QLabel *m_zIndexInfoLabel = nullptr;
    QComboBox *m_scaleModeCombo = nullptr;
    QPushButton *m_resetTransformBtn = nullptr;

    // Motion Path controls (Desplazamiento y Animación)
    QWidget *m_motionSection = nullptr;
    QComboBox *m_motionPresetCombo = nullptr;
    QDoubleSpinBox *m_motionStartXSpin = nullptr;
    QDoubleSpinBox *m_motionStartYSpin = nullptr;
    QDoubleSpinBox *m_motionEndXSpin = nullptr;
    QDoubleSpinBox *m_motionEndYSpin = nullptr;
    QComboBox *m_motionWaypointCombo = nullptr;
    QDoubleSpinBox *m_motionWaypointXSpin = nullptr;
    QDoubleSpinBox *m_motionWaypointYSpin = nullptr;
    QCheckBox *m_motionWaypointCurvedCheck = nullptr;
    QDoubleSpinBox *m_motionHandleOutXSpin = nullptr;
    QDoubleSpinBox *m_motionHandleOutYSpin = nullptr;
    QPushButton *m_motionAddPointBtn = nullptr;
    QPushButton *m_motionRemovePointBtn = nullptr;
    QPushButton *m_motionAutoSmoothBtn = nullptr;
    QPushButton *m_motionReverseBtn = nullptr;
    QComboBox *m_motionEasingCombo = nullptr;

    // Text controls
    QWidget *m_textSection = nullptr;
    QTextEdit *m_textEdit = nullptr;
    QFontComboBox *m_fontCombo = nullptr;
    QSpinBox *m_fontSizeSpin = nullptr;
    QPushButton *m_fontSizeDecBtn = nullptr;
    QPushButton *m_fontSizeIncBtn = nullptr;
    QPushButton *m_boldBtn = nullptr;
    QPushButton *m_italicBtn = nullptr;
    QPushButton *m_underlineBtn = nullptr;
    QPushButton *m_alignLeftBtn = nullptr;
    QPushButton *m_alignCenterBtn = nullptr;
    QPushButton *m_alignRightBtn = nullptr;
    QPushButton *m_textColorBtn = nullptr;
    QLabel *m_textColorSwatch = nullptr;
    QPushButton *m_bgColorBtn = nullptr;
    QLabel *m_bgColorSwatch = nullptr;
    QSpinBox *m_boxWidthSpin = nullptr;

    // Video controls
    QWidget *m_videoSection = nullptr;
    QSlider *m_opacitySlider = nullptr;
    QLabel *m_opacityLabel = nullptr;
    QDoubleSpinBox *m_videoFadeInSpin = nullptr;
    QDoubleSpinBox *m_videoFadeOutSpin = nullptr;
    QComboBox *m_filterCombo = nullptr;

    // Clip Color & Luminosity Adjustments controls
    QWidget *m_clipColorSection = nullptr;
    QPushButton *m_clipModeSlidersBtn = nullptr;
    QPushButton *m_clipModeCurvesBtn = nullptr;
    QStackedWidget *m_clipColorModeStack = nullptr;
    QWidget *m_clipSlidersPage = nullptr;
    QWidget *m_clipCurvesPage = nullptr;

    // Clip Sliders
    QSlider *m_clipBrightnessSlider = nullptr;
    QSpinBox *m_clipBrightnessSpin = nullptr;
    QSlider *m_clipLuminositySlider = nullptr;
    QSpinBox *m_clipLuminositySpin = nullptr;
    QSlider *m_clipRedSlider = nullptr;
    QSpinBox *m_clipRedSpin = nullptr;
    QSlider *m_clipGreenSlider = nullptr;
    QSpinBox *m_clipGreenSpin = nullptr;
    QSlider *m_clipBlueSlider = nullptr;
    QSpinBox *m_clipBlueSpin = nullptr;
    QPushButton *m_clipResetColorBtn = nullptr;

    // Clip Curves
    QPushButton *m_clipCurveTabColorBtn = nullptr;
    QPushButton *m_clipCurveTabLumaBtn = nullptr;
    QPushButton *m_clipCurveTabBothBtn = nullptr;
    CurveEditorWidget *m_clipColorCurveWidget = nullptr;
    CurveEditorWidget *m_clipLumaCurveWidget = nullptr;
    QPushButton *m_clipResetCurvesBtn = nullptr;

    // Effects Stack controls
    QWidget *m_effectsSection = nullptr;
    QPushButton *m_addEffectBtn = nullptr;
    EffectsListWidget *m_effectsList = nullptr;
    QLabel *m_emptyEffectsLabel = nullptr;
    QPushButton *m_clearEffectsBtn = nullptr;
    QLabel *m_effectsCountLabel = nullptr;
    void refreshEffectsStack();

    // Audio controls
    QWidget *m_audioSection = nullptr;
    QSlider *m_volumeSlider = nullptr;
    QLabel *m_volumeLabel = nullptr;
    QDoubleSpinBox *m_audioFadeInSpin = nullptr;
    QDoubleSpinBox *m_audioFadeOutSpin = nullptr;
    QCheckBox *m_muteAudioCheck = nullptr;

    // Actions
    QPushButton *m_splitBtn = nullptr;
    QPushButton *m_duplicateBtn = nullptr;
    QPushButton *m_deleteBtn = nullptr;

    // Global / Master Properties Box controls
    QWidget *m_globalContainer = nullptr;
    QPushButton *m_globalModeSlidersBtn = nullptr;
    QPushButton *m_globalModeCurvesBtn = nullptr;
    QStackedWidget *m_globalColorModeStack = nullptr;
    QWidget *m_globalSlidersPage = nullptr;
    QWidget *m_globalCurvesPage = nullptr;

    // Global Sliders
    QSlider *m_globalBrightnessSlider = nullptr;
    QSpinBox *m_globalBrightnessSpin = nullptr;
    QSlider *m_globalLuminositySlider = nullptr;
    QSpinBox *m_globalLuminositySpin = nullptr;
    QSlider *m_globalRedSlider = nullptr;
    QSpinBox *m_globalRedSpin = nullptr;
    QSlider *m_globalGreenSlider = nullptr;
    QSpinBox *m_globalGreenSpin = nullptr;
    QSlider *m_globalBlueSlider = nullptr;
    QSpinBox *m_globalBlueSpin = nullptr;
    QPushButton *m_globalResetColorBtn = nullptr;

    // Global Curves
    QPushButton *m_globalCurveTabColorBtn = nullptr;
    QPushButton *m_globalCurveTabLumaBtn = nullptr;
    QPushButton *m_globalCurveTabBothBtn = nullptr;
    CurveEditorWidget *m_globalColorCurveWidget = nullptr;
    CurveEditorWidget *m_globalLumaCurveWidget = nullptr;
    QPushButton *m_globalResetCurvesBtn = nullptr;

    QLabel *m_projectDurationLabel = nullptr;
    QLabel *m_projectTracksLabel = nullptr;
    QLabel *m_projectClipsLabel = nullptr;
    QPushButton *m_globalExportBtn = nullptr;
};
