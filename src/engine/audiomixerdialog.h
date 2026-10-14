#pragma once

#include <QDialog>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include "audioeffects.h"

class AudioMixerDialog : public QDialog {
    Q_OBJECT
public:
    explicit AudioMixerDialog(QWidget *parent = nullptr);
    ~AudioMixerDialog() override = default;

private slots:
    void onMasterVolChanged(int value);
    void onEqLowChanged(int value);
    void onEqMidLowChanged(int value);
    void onEqMidHighChanged(int value);
    void onEqHighChanged(int value);

    void onCompThresholdChanged(int value);
    void onCompRatioChanged(int value);
    void onCompAttackChanged(int value);
    void onCompReleaseChanged(int value);
    void onCompMakeupChanged(int value);

    void onLimiterCeilingChanged(int value);

    void onPresetSelected(int index);
    void onResetDefaultsClicked();

private:
    void setupUi();
    void updateLabels();

    QSlider *m_masterVolSlider = nullptr;
    QLabel *m_masterVolLbl = nullptr;

    // EQ
    QSlider *m_eqLowSlider = nullptr;
    QSlider *m_eqMidLowSlider = nullptr;
    QSlider *m_eqMidHighSlider = nullptr;
    QSlider *m_eqHighSlider = nullptr;
    QLabel *m_eqLowLbl = nullptr;
    QLabel *m_eqMidLowLbl = nullptr;
    QLabel *m_eqMidHighLbl = nullptr;
    QLabel *m_eqHighLbl = nullptr;

    // Compressor
    QSlider *m_compThresholdSlider = nullptr;
    QSlider *m_compRatioSlider = nullptr;
    QSlider *m_compAttackSlider = nullptr;
    QSlider *m_compReleaseSlider = nullptr;
    QSlider *m_compMakeupSlider = nullptr;
    QLabel *m_compThresholdLbl = nullptr;
    QLabel *m_compRatioLbl = nullptr;
    QLabel *m_compAttackLbl = nullptr;
    QLabel *m_compReleaseLbl = nullptr;
    QLabel *m_compMakeupLbl = nullptr;

    // Limiter
    QSlider *m_limiterCeilingSlider = nullptr;
    QLabel *m_limiterCeilingLbl = nullptr;

    QComboBox *m_presetCombo = nullptr;
};
