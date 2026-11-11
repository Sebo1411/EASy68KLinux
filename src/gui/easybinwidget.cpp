#include "easybinwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <vector>

static uint32_t parseHexOrDec(QString str, uint32_t defaultVal = 0)
{
    str = str.trimmed();
    if (str.isEmpty()) return defaultVal;
    if (str.startsWith("$")) str = str.mid(1);
    else if (str.startsWith("0x", Qt::CaseInsensitive)) str = str.mid(2);

    bool ok = false;
    uint32_t val = str.toUInt(&ok, 16);
    return ok ? val : defaultVal;
}

EasyBinWidget::EasyBinWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void EasyBinWidget::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // Section 1: S-Record File Inspector
    auto *inspectBox = new QGroupBox(tr("S-Record (.s68) Inspector"), this);
    auto *inspectLayout = new QVBoxLayout(inspectBox);

    auto *fileRow = new QHBoxLayout();
    fileRow->addWidget(new QLabel(tr("S68 File:"), inspectBox));
    inspectFileEdit = new QLineEdit(inspectBox);
    auto *browseInspectBtn = new QPushButton(tr("Browse..."), inspectBox);
    auto *inspectBtn = new QPushButton(tr("Inspect"), inspectBox);
    connect(browseInspectBtn, &QPushButton::clicked, this, &EasyBinWidget::onBrowseInspectFile);
    connect(inspectBtn, &QPushButton::clicked, this, &EasyBinWidget::onInspectClicked);

    fileRow->addWidget(inspectFileEdit);
    fileRow->addWidget(browseInspectBtn);
    fileRow->addWidget(inspectBtn);
    inspectLayout->addLayout(fileRow);

    auto *infoGrid = new QGridLayout();
    infoGrid->setSpacing(4);

    infoGrid->addWidget(new QLabel(tr("Start Address:"), inspectBox), 0, 0);
    valStartAddr = new QLabel("-", inspectBox);
    valStartAddr->setStyleSheet("font-weight: bold; font-family: monospace;");
    infoGrid->addWidget(valStartAddr, 0, 1);

    infoGrid->addWidget(new QLabel(tr("End Address:"), inspectBox), 0, 2);
    valEndAddr = new QLabel("-", inspectBox);
    valEndAddr->setStyleSheet("font-weight: bold; font-family: monospace;");
    infoGrid->addWidget(valEndAddr, 0, 3);

    infoGrid->addWidget(new QLabel(tr("Entry Point:"), inspectBox), 1, 0);
    valEntryAddr = new QLabel("-", inspectBox);
    valEntryAddr->setStyleSheet("font-weight: bold; font-family: monospace;");
    infoGrid->addWidget(valEntryAddr, 1, 1);

    infoGrid->addWidget(new QLabel(tr("Total Length:"), inspectBox), 1, 2);
    valLength = new QLabel("-", inspectBox);
    valLength->setStyleSheet("font-weight: bold; font-family: monospace;");
    infoGrid->addWidget(valLength, 1, 3);

    infoGrid->addWidget(new QLabel(tr("Header (S0):"), inspectBox), 2, 0);
    valHeader = new QLabel("-", inspectBox);
    valHeader->setStyleSheet("font-family: monospace;");
    infoGrid->addWidget(valHeader, 2, 1, 1, 3);

    inspectLayout->addLayout(infoGrid);
    mainLayout->addWidget(inspectBox);

    // Section 2: Conversions (Split S68->BIN and BIN->S68)
    auto *convRow = new QHBoxLayout();

    // S68 -> BIN Box
    auto *s2bBox = new QGroupBox(tr("Convert S-Record to Binary (.s68 -> .bin)"), this);
    auto *s2bLayout = new QVBoxLayout(s2bBox);

    auto *s2bInLayout = new QHBoxLayout();
    s2bInLayout->addWidget(new QLabel(tr("Input S68:"), s2bBox));
    s2bInputEdit = new QLineEdit(s2bBox);
    auto *s2bInBtn = new QPushButton(tr("..."), s2bBox);
    connect(s2bInBtn, &QPushButton::clicked, this, &EasyBinWidget::onBrowseS2BInput);
    s2bInLayout->addWidget(s2bInputEdit);
    s2bInLayout->addWidget(s2bInBtn);
    s2bLayout->addLayout(s2bInLayout);

    auto *s2bOutLayout = new QHBoxLayout();
    s2bOutLayout->addWidget(new QLabel(tr("Output BIN:"), s2bBox));
    s2bOutputEdit = new QLineEdit(s2bBox);
    auto *s2bOutBtn = new QPushButton(tr("..."), s2bBox);
    connect(s2bOutBtn, &QPushButton::clicked, this, &EasyBinWidget::onBrowseS2BOutput);
    s2bOutLayout->addWidget(s2bOutputEdit);
    s2bOutLayout->addWidget(s2bOutBtn);
    s2bLayout->addLayout(s2bOutLayout);

    auto *s2bSplitLayout = new QHBoxLayout();
    s2bSplitLayout->addWidget(new QLabel(tr("Split Mode:"), s2bBox));
    s2bSplitCombo = new QComboBox(s2bBox);
    s2bSplitCombo->addItem(tr("Every Byte (Normal)"), 0);
    s2bSplitCombo->addItem(tr("Even Bytes"), 1);
    s2bSplitCombo->addItem(tr("Odd Bytes"), 2);
    s2bSplitCombo->addItem(tr("Word (16-bit)"), 3);
    s2bSplitCombo->addItem(tr("Long (32-bit)"), 4);
    s2bSplitLayout->addWidget(s2bSplitCombo);
    s2bLayout->addLayout(s2bSplitLayout);

    auto *convertS2BBtn = new QPushButton(tr("Convert S68 -> BIN"), s2bBox);
    connect(convertS2BBtn, &QPushButton::clicked, this, &EasyBinWidget::onConvertS2BClicked);
    s2bLayout->addWidget(convertS2BBtn);

    convRow->addWidget(s2bBox);

    // BIN -> S68 Box
    auto *b2sBox = new QGroupBox(tr("Convert Binary to S-Record (.bin -> .s68)"), this);
    auto *b2sLayout = new QVBoxLayout(b2sBox);

    auto *b2sInLayout = new QHBoxLayout();
    b2sInLayout->addWidget(new QLabel(tr("Input BIN:"), b2sBox));
    b2sInputEdit = new QLineEdit(b2sBox);
    auto *b2sInBtn = new QPushButton(tr("..."), b2sBox);
    connect(b2sInBtn, &QPushButton::clicked, this, &EasyBinWidget::onBrowseB2SInput);
    b2sInLayout->addWidget(b2sInputEdit);
    b2sInLayout->addWidget(b2sInBtn);
    b2sLayout->addLayout(b2sInLayout);

    auto *b2sOutLayout = new QHBoxLayout();
    b2sOutLayout->addWidget(new QLabel(tr("Output S68:"), b2sBox));
    b2sOutputEdit = new QLineEdit(b2sBox);
    auto *b2sOutBtn = new QPushButton(tr("..."), b2sBox);
    connect(b2sOutBtn, &QPushButton::clicked, this, &EasyBinWidget::onBrowseB2SOutput);
    b2sOutLayout->addWidget(b2sOutputEdit);
    b2sOutLayout->addWidget(b2sOutBtn);
    b2sLayout->addLayout(b2sOutLayout);

    auto *b2sAddrLayout = new QHBoxLayout();
    b2sAddrLayout->addWidget(new QLabel(tr("Load Addr:"), b2sBox));
    b2sLoadAddrEdit = new QLineEdit("$1000", b2sBox);
    b2sAddrLayout->addWidget(b2sLoadAddrEdit);
    b2sAddrLayout->addWidget(new QLabel(tr("Entry Point:"), b2sBox));
    b2sEntryAddrEdit = new QLineEdit("$1000", b2sBox);
    b2sAddrLayout->addWidget(b2sEntryAddrEdit);
    b2sLayout->addLayout(b2sAddrLayout);

    auto *b2sSplitLayout = new QHBoxLayout();
    b2sSplitLayout->addWidget(new QLabel(tr("Split Mode:"), b2sBox));
    b2sSplitCombo = new QComboBox(b2sBox);
    b2sSplitCombo->addItem(tr("Every Byte (Normal)"), 0);
    b2sSplitCombo->addItem(tr("Even Bytes"), 1);
    b2sSplitCombo->addItem(tr("Odd Bytes"), 2);
    b2sSplitCombo->addItem(tr("Word (16-bit)"), 3);
    b2sSplitCombo->addItem(tr("Long (32-bit)"), 4);
    b2sSplitLayout->addWidget(b2sSplitCombo);
    b2sLayout->addLayout(b2sSplitLayout);

    auto *convertB2SBtn = new QPushButton(tr("Convert BIN -> S68"), b2sBox);
    connect(convertB2SBtn, &QPushButton::clicked, this, &EasyBinWidget::onConvertB2SClicked);
    b2sLayout->addWidget(convertB2SBtn);

    convRow->addWidget(b2sBox);
    mainLayout->addLayout(convRow);

    // Section 3: Log View
    mainLayout->addWidget(new QLabel(tr("Operation Log:"), this));
    logView = new QPlainTextEdit(this);
    logView->setReadOnly(true);
    logView->setFont(QFont("Monospace", 9));
    mainLayout->addWidget(logView);

    logMessage(tr("EASyBIN Converter initialized."));
}

void EasyBinWidget::logMessage(const QString &msg)
{
    logView->appendPlainText(msg);
}

void EasyBinWidget::onBrowseInspectFile()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Select S-Record File"), "",
                                                tr("S-Record Files (*.s68 *.s28 *.s19 *.hex);;All Files (*)"));
    if (!path.isEmpty()) {
        inspectFileEdit->setText(path);
        inspectSRecordFile(path);
    }
}

void EasyBinWidget::onInspectClicked()
{
    inspectSRecordFile(inspectFileEdit->text());
}

bool EasyBinWidget::inspectSRecordFile(const QString &filePath)
{
    if (filePath.isEmpty()) {
        logMessage(tr("Error: No file specified for inspection."));
        return false;
    }

    const size_t memSize = 0x01000000;
    std::vector<unsigned char> mem(memSize, 0);
    SrecInfo info;
    std::string errMsg;

    if (!loadSrec(filePath.toStdString(), mem.data(), memSize, info, errMsg)) {
        logMessage(tr("Failed to inspect %1: %2").arg(filePath, QString::fromStdString(errMsg)));
        valStartAddr->setText("-");
        valEndAddr->setText("-");
        valEntryAddr->setText("-");
        valLength->setText("-");
        valHeader->setText("-");
        return false;
    }

    valStartAddr->setText(QString("0x%1").arg(info.startAddr, 8, 16, QLatin1Char('0')).toUpper());
    valEndAddr->setText(QString("0x%1").arg(info.endAddr, 8, 16, QLatin1Char('0')).toUpper());
    valEntryAddr->setText(QString("0x%1").arg(info.entryAddr, 8, 16, QLatin1Char('0')).toUpper());
    valLength->setText(QString("%1 bytes (0x%2)").arg(info.length).arg(info.length, 6, 16, QLatin1Char('0')).toUpper());
    valHeader->setText(info.headerS0.empty() ? QString("<none>") : QString::fromStdString(info.headerS0));

    logMessage(tr("Inspected %1: Start=0x%2, End=0x%3, Entry=0x%4, Length=%5 bytes")
               .arg(QFileInfo(filePath).fileName())
               .arg(info.startAddr, 8, 16, QLatin1Char('0')).toUpper()
               .arg(info.endAddr, 8, 16, QLatin1Char('0')).toUpper()
               .arg(info.entryAddr, 8, 16, QLatin1Char('0')).toUpper()
               .arg(info.length));
    return true;
}

void EasyBinWidget::onBrowseS2BInput()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Select S-Record File"), "",
                                                tr("S-Record Files (*.s68 *.s28 *.s19);;All Files (*)"));
    if (!path.isEmpty()) {
        s2bInputEdit->setText(path);
        QFileInfo fi(path);
        s2bOutputEdit->setText(fi.path() + "/" + fi.completeBaseName() + ".bin");
    }
}

void EasyBinWidget::onBrowseS2BOutput()
{
    QString path = QFileDialog::getSaveFileName(this, tr("Select Output Binary File"), s2bOutputEdit->text(),
                                                tr("Binary Files (*.bin);;All Files (*)"));
    if (!path.isEmpty()) {
        s2bOutputEdit->setText(path);
    }
}

void EasyBinWidget::onConvertS2BClicked()
{
    int split = s2bSplitCombo->currentData().toInt();
    convertS68ToBin(s2bInputEdit->text(), s2bOutputEdit->text(), split);
}

bool EasyBinWidget::convertS68ToBin(const QString &s68Path, const QString &binPath, int splitMode)
{
    if (s68Path.isEmpty() || binPath.isEmpty()) {
        logMessage(tr("Error: S68 to BIN requires both input and output paths."));
        return false;
    }

    const size_t memSize = 0x01000000;
    std::vector<unsigned char> mem(memSize, 0);
    SrecInfo info;
    std::string errMsg;

    if (!loadSrec(s68Path.toStdString(), mem.data(), memSize, info, errMsg)) {
        logMessage(tr("Error loading S-Record %1: %2").arg(s68Path, QString::fromStdString(errMsg)));
        return false;
    }

    if (!saveBinary(binPath.toStdString(), mem.data(), info.startAddr, info.length, memSize, splitMode, errMsg)) {
        logMessage(tr("Error saving binary %1: %2").arg(binPath, QString::fromStdString(errMsg)));
        return false;
    }

    logMessage(tr("Successfully converted %1 -> %2 (%3 bytes, split=%4)")
               .arg(s68Path, binPath).arg(info.length).arg(splitMode));
    return true;
}

void EasyBinWidget::onBrowseB2SInput()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Select Binary File"), "",
                                                tr("Binary Files (*.bin);;All Files (*)"));
    if (!path.isEmpty()) {
        b2sInputEdit->setText(path);
        QFileInfo fi(path);
        b2sOutputEdit->setText(fi.path() + "/" + fi.completeBaseName() + ".s68");
    }
}

void EasyBinWidget::onBrowseB2SOutput()
{
    QString path = QFileDialog::getSaveFileName(this, tr("Select Output S68 File"), b2sOutputEdit->text(),
                                                tr("S-Record Files (*.s68);;All Files (*)"));
    if (!path.isEmpty()) {
        b2sOutputEdit->setText(path);
    }
}

void EasyBinWidget::onConvertB2SClicked()
{
    uint32_t loadAddr = parseHexOrDec(b2sLoadAddrEdit->text(), 0x1000);
    uint32_t entryAddr = parseHexOrDec(b2sEntryAddrEdit->text(), 0x1000);
    int split = b2sSplitCombo->currentData().toInt();
    convertBinToS68(b2sInputEdit->text(), b2sOutputEdit->text(), loadAddr, entryAddr, split);
}

bool EasyBinWidget::convertBinToS68(const QString &binPath, const QString &s68Path, uint32_t loadAddr, uint32_t entryAddr, int splitMode)
{
    if (binPath.isEmpty() || s68Path.isEmpty()) {
        logMessage(tr("Error: BIN to S68 requires both input and output paths."));
        return false;
    }

    const size_t memSize = 0x01000000;
    std::vector<unsigned char> mem(memSize, 0);
    size_t bytesLoaded = 0;
    std::string errMsg;

    if (!loadBinary(binPath.toStdString(), mem.data(), memSize, loadAddr, splitMode, bytesLoaded, errMsg)) {
        logMessage(tr("Error loading binary %1: %2").arg(binPath, QString::fromStdString(errMsg)));
        return false;
    }

    uint32_t toAddr = loadAddr + static_cast<uint32_t>(bytesLoaded) - 1;
    if (!saveSRecord(s68Path.toStdString(), mem.data(), loadAddr, toAddr, entryAddr, memSize, errMsg)) {
        logMessage(tr("Error saving S-Record %1: %2").arg(s68Path, QString::fromStdString(errMsg)));
        return false;
    }

    logMessage(tr("Successfully converted %1 -> %2 (%3 bytes loaded at 0x%4, entry=0x%5)")
               .arg(binPath, s68Path).arg(bytesLoaded)
               .arg(loadAddr, 8, 16, QLatin1Char('0')).toUpper()
               .arg(entryAddr, 8, 16, QLatin1Char('0')).toUpper());
    return true;
}
