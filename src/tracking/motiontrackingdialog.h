#pragma once

#include <QDialog>
#include <QSpinBox>
#include <QSlider>
#include <QComboBox>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include "motiontracker.h"
#include "../core/clip.h"

class MotionTrackingDialog : public QDialog {
    Q_OBJECT
public:
    explicit MotionTrackingDialog(TimelineClip *clip, QWidget *parent = nullptr);
    ~MotionTrackingDialog() override = default;

private slots:
    void onStartTrackingClicked();

private:
    void setupUi();

    TimelineClip *m_clip = nullptr;
    QSpinBox *m_roiXSpin = nullptr;
    QSpinBox *m_roiYSpin = nullptr;
    QSpinBox *m_roiWSpin = nullptr;
    QSpinBox *m_roiHSpin = nullptr;
    QSlider *m_radiusSlider = nullptr;
    QLabel *m_radiusLbl = nullptr;
    QComboBox *m_targetPropertyCombo = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QLabel *m_statusLbl = nullptr;
    QPushButton *m_startBtn = nullptr;
};
