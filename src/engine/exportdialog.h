#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QProgressBar>
#include <QPushButton>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include "videoexporter.h"
#include "../core/timelinemodel.h"

class ExportDialog : public QDialog {
    Q_OBJECT
    Q_DISABLE_COPY(ExportDialog)

public:
    explicit ExportDialog(TimelineModel *model, qint64 playheadPositionMs = 0, QWidget *parent = nullptr);

private slots:
    void browseOutputFile();
    void startExport();
    void cancelExport();
    void onProgressUpdated(int percent, const QString &statusText);
    void onExportFinished(bool success, const QString &outputPath, const QString &errorMessage);
    void onDurationModeChanged(int index);
    void onContainerChanged(int index);
    void onVideoCodecChanged(int index);
    void onQualityChanged(int index);
    void updateDurationSummary();

private:
    void populateVideoCodecs(ExportContainer container);
    void populateAudioCodecs(ExportContainer container);
    void updateFormVisibility();
    void updateOutputExtension(ExportContainer container);

    TimelineModel *m_model = nullptr;
    VideoExporter *m_exporter = nullptr;
    qint64 m_playheadPositionMs = 0;

    QLineEdit *m_pathEdit = nullptr;
    QPushButton *m_browseBtn = nullptr;

    QComboBox *m_containerCombo = nullptr;
    QComboBox *m_videoCodecCombo = nullptr;
    QCheckBox *m_hwAccelCheck = nullptr;
    QComboBox *m_resCombo = nullptr;
    QComboBox *m_fpsCombo = nullptr;
    QComboBox *m_qualityCombo = nullptr;
    QSpinBox *m_crfSpin = nullptr;
    QWidget *m_crfWidget = nullptr;

    QComboBox *m_audioCodecCombo = nullptr;
    QComboBox *m_audioBitrateCombo = nullptr;

    QComboBox *m_durationModeCombo = nullptr;
    QWidget *m_customDurationWidget = nullptr;
    QDoubleSpinBox *m_exportDurationSpin = nullptr;
    QDoubleSpinBox *m_exportStartSpin = nullptr;
    QLabel *m_durationSummaryLabel = nullptr;

    QProgressBar *m_progressBar = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_exportBtn = nullptr;
    QPushButton *m_cancelBtn = nullptr;
    QString m_finalOutputPath;

    QWidget *m_videoSectionWidget = nullptr;
    QWidget *m_audioSectionWidget = nullptr;
};
