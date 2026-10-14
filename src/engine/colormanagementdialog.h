#pragma once

#include <QDialog>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include "colormanagement.h"

class ColorManagementDialog : public QDialog {
    Q_OBJECT
public:
    explicit ColorManagementDialog(QWidget *parent = nullptr);
    ~ColorManagementDialog() override = default;

private slots:
    void onColorSpaceChanged(int index);
    void onTransferFunctionChanged(int index);
    void onAcesToggled(bool checked);
    void onApplyClicked();

private:
    void setupUi();
    void updateSpecs();

    QComboBox *m_colorSpaceCombo = nullptr;
    QComboBox *m_transferCombo = nullptr;
    QCheckBox *m_acesCheck = nullptr;
    QLabel *m_specsLabel = nullptr;
};
