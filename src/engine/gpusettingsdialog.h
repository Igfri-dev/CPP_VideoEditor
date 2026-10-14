#pragma once

#include <QDialog>
#include <QLabel>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QSlider>
#include <QPushButton>
#include "gpuaccelerationmanager.h"

class GpuSettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit GpuSettingsDialog(QWidget *parent = nullptr);
    ~GpuSettingsDialog() override = default;

private slots:
    void onApply();
    void onClearCacheClicked();
    void updateCacheLabel();

private:
    void setupUi();
    void loadCurrentSettings();

    QLabel *m_archLabel = nullptr;
    QLabel *m_gpuLabel = nullptr;
    QLabel *m_currentCacheLabel = nullptr;

    QCheckBox *m_hwDecodingCheck = nullptr;
    QComboBox *m_backendCombo = nullptr;
    QCheckBox *m_gpuRenderingCheck = nullptr;
    QComboBox *m_resolutionCombo = nullptr;

    QSlider *m_cacheSlider = nullptr;
    QSpinBox *m_cacheSpinBox = nullptr;
    QPushButton *m_clearCacheBtn = nullptr;

    QPushButton *m_okBtn = nullptr;
    QPushButton *m_cancelBtn = nullptr;
    QPushButton *m_applyBtn = nullptr;
};
