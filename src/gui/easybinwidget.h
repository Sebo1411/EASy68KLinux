#ifndef EASYBINWIDGET_H
#define EASYBINWIDGET_H

#include <QWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QLabel>
#include "fileio.h"

class EasyBinWidget : public QWidget
{
    Q_OBJECT

public:
    explicit EasyBinWidget(QWidget *parent = nullptr);
    ~EasyBinWidget() override = default;

    bool inspectSRecordFile(const QString &filePath);
    bool convertS68ToBin(const QString &s68Path, const QString &binPath, int splitMode);
    bool convertBinToS68(const QString &binPath, const QString &s68Path, uint32_t loadAddr, uint32_t entryAddr, int splitMode);

private slots:
    void onBrowseInspectFile();
    void onInspectClicked();
    void onBrowseS2BInput();
    void onBrowseS2BOutput();
    void onConvertS2BClicked();
    void onBrowseB2SInput();
    void onBrowseB2SOutput();
    void onConvertB2SClicked();

private:
    void setupUi();
    void logMessage(const QString &msg);

    // Inspect fields
    QLineEdit *inspectFileEdit;
    QLabel *valStartAddr;
    QLabel *valEndAddr;
    QLabel *valEntryAddr;
    QLabel *valLength;
    QLabel *valHeader;

    // S68 -> BIN fields
    QLineEdit *s2bInputEdit;
    QLineEdit *s2bOutputEdit;
    QComboBox *s2bSplitCombo;

    // BIN -> S68 fields
    QLineEdit *b2sInputEdit;
    QLineEdit *b2sOutputEdit;
    QLineEdit *b2sLoadAddrEdit;
    QLineEdit *b2sEntryAddrEdit;
    QComboBox *b2sSplitCombo;

    // Log
    QPlainTextEdit *logView;
};

#endif // EASYBINWIDGET_H
