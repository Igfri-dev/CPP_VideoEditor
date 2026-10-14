#pragma once

#include <QWidget>
#include <QImage>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QSlider>
#include "videoscopeswidget.h"

class VideoScopesWindow : public QWidget {
    Q_OBJECT
public:
    explicit VideoScopesWindow(QWidget *parent = nullptr);
    ~VideoScopesWindow() override = default;

public slots:
    void updateFrame(const QImage &frame);

private slots:
    void onModeChanged(int index);
    void onGainChanged(int value);

private:
    VideoScopesWidget *m_scopesWidget = nullptr;
    QComboBox *m_modeCombo = nullptr;
    QSlider *m_gainSlider = nullptr;
    QLabel *m_statusLabel = nullptr;
};
