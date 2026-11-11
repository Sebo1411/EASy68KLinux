#include "mainwindow.h"
#include "codeeditor.h"
#include "syntaxhighlighter.h"
#include "registersview.h"
#include "memoryview.h"
#include "hardwarewidget.h"
#include "easybinwidget.h"
#include "helpwidget.h"
#include "terminalwidget.h"
#include "simulatormanager.h"
#include "updater.h"

#include <QMenuBar>
#include <QMenu>
#include <QToolBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QTimer>
#include <QFileInfo>
#include <QRegularExpression>
#include <QFont>
#include <QLabel>
#include <QSaveFile>
#include <QTextBlock>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    updater = new Updater(this);

    setupUi();
    createActions();
    createMenus();
    createToolBars();

    simManager = new SimulatorManager(this);
    connect(simManager, &SimulatorManager::stateChanged, this, &MainWindow::onSimulatorStateChanged);
    connect(simManager, &SimulatorManager::simulationHalted, this, &MainWindow::onSimulatorHalted);
    connect(simManager, &SimulatorManager::messageLogged, this, &MainWindow::onSimulatorMessage);

    // Route simulated I/O through the interactive GUI terminal
    setSimIO(terminalWidget);

    memoryView->setMemoryPointer(simManager->getMemory(), simManager->getMemorySize());

    setCurrentFile(QString());
    resize(1100, 750);

    if (Updater::autoCheckOnStartup() && QGuiApplication::platformName() != QStringLiteral("offscreen")) {
        QTimer::singleShot(2000, this, [this]() {
            updater->checkForUpdates(true);
        });
    }
}

MainWindow::~MainWindow()
{
    if (assemblerProcess && assemblerProcess->state() != QProcess::NotRunning) {
        assemblerProcess->kill();
        assemblerProcess->waitForFinished();
    }
}

void MainWindow::setupUi()
{
    mainTabs = new QTabWidget(this);
    setCentralWidget(mainTabs);

    // Tab 0: Editor Tab
    auto *editorTab = new QWidget(this);
    auto *editorLayout = new QVBoxLayout(editorTab);
    editorLayout->setContentsMargins(0, 0, 0, 0);

    auto *editorSplitter = new QSplitter(Qt::Vertical, editorTab);

    editor = new CodeEditor(editorSplitter);
    highlighter = new SyntaxHighlighter(editor->document());
    editorSplitter->addWidget(editor);

    auto *buildWidget = new QWidget(editorSplitter);
    auto *buildLayout = new QVBoxLayout(buildWidget);
    buildLayout->setContentsMargins(4, 4, 4, 4);
    buildLayout->addWidget(new QLabel(tr("Build Output (click an error to jump):"), buildWidget));

    buildOutputList = new QListWidget(buildWidget);
    QFont monoFont("Monospace", 9);
    monoFont.setStyleHint(QFont::Monospace);
    buildOutputList->setFont(monoFont);
    connect(buildOutputList, &QListWidget::itemClicked, this, &MainWindow::onBuildErrorClicked);
    buildLayout->addWidget(buildOutputList);

    editorSplitter->addWidget(buildWidget);
    editorSplitter->setStretchFactor(0, 4);
    editorSplitter->setStretchFactor(1, 1);

    editorLayout->addWidget(editorSplitter);
    mainTabs->addTab(editorTab, tr("Source Editor"));

    // Tab 1: Simulator Tab
    auto *simTab = new QWidget(this);
    auto *simLayout = new QHBoxLayout(simTab);
    simLayout->setContentsMargins(4, 4, 4, 4);

    auto *simSplitter = new QSplitter(Qt::Horizontal, simTab);

    // Left pane of simulator: Disassembly View + Terminal
    auto *simLeftPane = new QWidget(simSplitter);
    auto *simLeftLayout = new QVBoxLayout(simLeftPane);
    simLeftLayout->setContentsMargins(0, 0, 0, 0);

    auto *simLeftSplitter = new QSplitter(Qt::Vertical, simLeftPane);

    auto *disasmBox = new QWidget(simLeftSplitter);
    auto *disasmBoxLayout = new QVBoxLayout(disasmBox);
    disasmBoxLayout->setContentsMargins(2, 2, 2, 2);
    disasmBoxLayout->addWidget(new QLabel(tr("Execution Status & Log:"), disasmBox));

    disassemblyView = new QPlainTextEdit(disasmBox);
    disassemblyView->setReadOnly(true);
    disassemblyView->setFont(monoFont);
    disasmBoxLayout->addWidget(disassemblyView);
    simLeftSplitter->addWidget(disasmBox);

    auto *termBox = new QWidget(simLeftSplitter);
    auto *termBoxLayout = new QVBoxLayout(termBox);
    termBoxLayout->setContentsMargins(2, 2, 2, 2);
    termBoxLayout->addWidget(new QLabel(tr("Simulated I/O Terminal (TRAP #15):"), termBox));

    terminalWidget = new TerminalWidget(termBox);
    termBoxLayout->addWidget(terminalWidget);
    simLeftSplitter->addWidget(termBox);

    simLeftSplitter->setStretchFactor(0, 1);
    simLeftSplitter->setStretchFactor(1, 1);
    simLeftLayout->addWidget(simLeftSplitter);
    simSplitter->addWidget(simLeftPane);

    // Right pane of simulator: Tabbed Registers & Memory
    auto *simRightTabs = new QTabWidget(simSplitter);

    registersView = new RegistersView(simRightTabs);
    simRightTabs->addTab(registersView, tr("Registers"));

    memoryView = new MemoryView(simRightTabs);
    simRightTabs->addTab(memoryView, tr("Memory"));

    hardwareWidget = new HardwareWidget(simRightTabs);
    simRightTabs->addTab(hardwareWidget, tr("Hardware & Graphics"));
    terminalWidget->setHardwareWidget(hardwareWidget);

    simSplitter->addWidget(simRightTabs);
    simSplitter->setStretchFactor(0, 3);
    simSplitter->setStretchFactor(1, 2);

    simLayout->addWidget(simSplitter);
    mainTabs->addTab(simTab, tr("68000 Simulator"));

    easyBinWidget = new EasyBinWidget(mainTabs);
    mainTabs->addTab(easyBinWidget, tr("EASyBIN Converter"));

    helpWidget = new HelpWidget(mainTabs);
    mainTabs->addTab(helpWidget, tr("Help & Reference"));

    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::createActions()
{
    newAct = new QAction(tr("&New"), this);
    newAct->setShortcuts(QKeySequence::New);
    connect(newAct, &QAction::triggered, this, &MainWindow::newFile);

    openAct = new QAction(tr("&Open..."), this);
    openAct->setShortcuts(QKeySequence::Open);
    connect(openAct, &QAction::triggered, this, &MainWindow::openFile);

    saveAct = new QAction(tr("&Save"), this);
    saveAct->setShortcuts(QKeySequence::Save);
    connect(saveAct, &QAction::triggered, this, &MainWindow::saveFile);

    saveAsAct = new QAction(tr("Save &As..."), this);
    saveAsAct->setShortcuts(QKeySequence::SaveAs);
    connect(saveAsAct, &QAction::triggered, this, &MainWindow::saveFileAs);

    exitAct = new QAction(tr("E&xit"), this);
    exitAct->setShortcuts(QKeySequence::Quit);
    connect(exitAct, &QAction::triggered, this, &QWidget::close);

    assembleAct = new QAction(tr("&Assemble"), this);
    assembleAct->setShortcut(QKeySequence(Qt::Key_F8));
    connect(assembleAct, &QAction::triggered, this, [this]() { assembleCurrentFile(false); });

    assembleAndLoadAct = new QAction(tr("Assemble && &Load in Simulator"), this);
    assembleAndLoadAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F8));
    connect(assembleAndLoadAct, &QAction::triggered, this, [this]() { assembleCurrentFile(true); });

    stepAct = new QAction(tr("&Step"), this);
    stepAct->setShortcut(QKeySequence(Qt::Key_F7));
    connect(stepAct, &QAction::triggered, this, &MainWindow::onSimulatorStep);

    runAct = new QAction(tr("&Run"), this);
    runAct->setShortcut(QKeySequence(Qt::Key_F9));
    connect(runAct, &QAction::triggered, this, &MainWindow::onSimulatorRun);

    pauseAct = new QAction(tr("&Pause"), this);
    pauseAct->setShortcut(QKeySequence(Qt::Key_F6));
    connect(pauseAct, &QAction::triggered, this, &MainWindow::onSimulatorPause);

    resetAct = new QAction(tr("&Reset"), this);
    resetAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    connect(resetAct, &QAction::triggered, this, &MainWindow::onSimulatorReset);

    loadS68Act = new QAction(tr("Load &S68 File..."), this);
    connect(loadS68Act, &QAction::triggered, this, &MainWindow::onSimulatorLoadS68);

    toggleBpAct = new QAction(tr("Toggle &Breakpoint"), this);
    toggleBpAct->setShortcut(QKeySequence(Qt::Key_F2));
    connect(toggleBpAct, &QAction::triggered, this, [this]() {
        simManager->toggleBreakpoint(static_cast<uint32_t>(simManager->getPC()));
    });

    clearBpsAct = new QAction(tr("Clear &All Breakpoints"), this);
    connect(clearBpsAct, &QAction::triggered, this, [this]() {
        simManager->clearBreakpoints();
    });

    aboutAct = new QAction(tr("&About EASy68K"), this);
    connect(aboutAct, &QAction::triggered, this, &MainWindow::about);

    helpDocAct = new QAction(tr("&EASy68K Documentation"), this);
    helpDocAct->setShortcut(QKeySequence(Qt::Key_F1));
    connect(helpDocAct, &QAction::triggered, this, [this]() {
        mainTabs->setCurrentWidget(helpWidget);
        helpWidget->loadPage("main.htm");
    });

    helpHwAct = new QAction(tr("&Hardware & Peripheral Guide"), this);
    connect(helpHwAct, &QAction::triggered, this, [this]() {
        mainTabs->setCurrentWidget(helpWidget);
        helpWidget->loadPage("SIMHELP/Hardware.htm");
    });

    helpPdfAct = new QAction(tr("&Quick Reference PDF"), this);
    connect(helpPdfAct, &QAction::triggered, this, [this]() {
        helpWidget->openQuickRefPdf();
    });

    checkUpdatesAct = new QAction(tr("&Check for Updates..."), this);
    connect(checkUpdatesAct, &QAction::triggered, this, [this]() {
        updater->checkForUpdates(false);
    });

    autoUpdateCheckAct = new QAction(tr("Check for Updates on &Startup"), this);
    autoUpdateCheckAct->setCheckable(true);
    autoUpdateCheckAct->setChecked(Updater::autoCheckOnStartup());
    connect(autoUpdateCheckAct, &QAction::toggled, this, [](bool checked) {
        Updater::setAutoCheckOnStartup(checked);
    });

    repoSetupAct = new QAction(tr("Configure APT / RPM Auto-Update &Repository..."), this);
    connect(repoSetupAct, &QAction::triggered, this, [this]() {
        updater->showRepoSetupDialog();
    });
}

void MainWindow::createMenus()
{
    auto *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(newAct);
    fileMenu->addAction(openAct);
    fileMenu->addAction(saveAct);
    fileMenu->addAction(saveAsAct);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAct);

    auto *projectMenu = menuBar()->addMenu(tr("&Project"));
    projectMenu->addAction(assembleAct);
    projectMenu->addAction(assembleAndLoadAct);

    auto *simMenu = menuBar()->addMenu(tr("&Simulator"));
    simMenu->addAction(stepAct);
    simMenu->addAction(runAct);
    simMenu->addAction(pauseAct);
    simMenu->addAction(resetAct);
    simMenu->addSeparator();
    simMenu->addAction(toggleBpAct);
    simMenu->addAction(clearBpsAct);
    simMenu->addSeparator();
    simMenu->addAction(loadS68Act);

    auto *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(helpDocAct);
    helpMenu->addAction(helpHwAct);
    helpMenu->addAction(helpPdfAct);
    helpMenu->addSeparator();
    helpMenu->addAction(checkUpdatesAct);
    helpMenu->addAction(autoUpdateCheckAct);
    helpMenu->addAction(repoSetupAct);
    helpMenu->addSeparator();
    helpMenu->addAction(aboutAct);
}

void MainWindow::createToolBars()
{
    auto *mainToolBar = addToolBar(tr("Main Toolbar"));
    mainToolBar->addAction(openAct);
    mainToolBar->addAction(saveAct);
    mainToolBar->addSeparator();
    mainToolBar->addAction(assembleAct);
    mainToolBar->addAction(assembleAndLoadAct);
    mainToolBar->addSeparator();
    mainToolBar->addAction(stepAct);
    mainToolBar->addAction(runAct);
    mainToolBar->addAction(pauseAct);
    mainToolBar->addAction(resetAct);
    mainToolBar->addSeparator();
    mainToolBar->addAction(toggleBpAct);
}

void MainWindow::newFile()
{
    if (maybeSave()) {
        editor->clear();
        setCurrentFile(QString());
    }
}

void MainWindow::openFile()
{
    if (maybeSave()) {
        QString fileName = QFileDialog::getOpenFileName(this, tr("Open 68000 Assembly File"),
                                                        QString(), tr("68000 Assembly (*.x68 *.asm *.s);;All Files (*)"));
        if (!fileName.isEmpty())
            loadFile(fileName);
    }
}

bool MainWindow::saveFile()
{
    if (currentFilePath.isEmpty()) {
        return saveFileAs();
    }

    QSaveFile file(currentFilePath);
    if (file.open(QFile::WriteOnly | QFile::Text)) {
        QTextStream out(&file);
        out << editor->toPlainText();
        if (!file.commit()) {
            QMessageBox::warning(this, tr("EASy68K"), tr("Cannot write file %1:\n%2.")
                                 .arg(QDir::toNativeSeparators(currentFilePath), file.errorString()));
            return false;
        }
    } else {
        QMessageBox::warning(this, tr("EASy68K"), tr("Cannot open file %1 for writing:\n%2.")
                             .arg(QDir::toNativeSeparators(currentFilePath), file.errorString()));
        return false;
    }

    setCurrentFile(currentFilePath);
    statusBar()->showMessage(tr("File saved"), 2000);
    return true;
}

bool MainWindow::saveFileAs()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save 68000 Assembly File"),
                                                    QString(), tr("68000 Assembly (*.x68 *.asm *.s);;All Files (*)"));
    if (fileName.isEmpty())
        return false;

    currentFilePath = fileName;
    return saveFile();
}

bool MainWindow::maybeSave()
{
    if (!editor->document()->isModified())
        return true;

    const QMessageBox::StandardButton ret
        = QMessageBox::warning(this, tr("EASy68K"),
                               tr("The document has been modified.\nDo you want to save your changes?"),
                               QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    switch (ret) {
    case QMessageBox::Save:
        return saveFile();
    case QMessageBox::Cancel:
        return false;
    default:
        break;
    }
    return true;
}

void MainWindow::loadFile(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        QMessageBox::warning(this, tr("EASy68K"),
                             tr("Cannot read file %1:\n%2.")
                             .arg(QDir::toNativeSeparators(fileName), file.errorString()));
        return;
    }

    QTextStream in(&file);
    editor->setPlainText(in.readAll());
    setCurrentFile(fileName);
    statusBar()->showMessage(tr("File loaded"), 2000);
}

void MainWindow::setCurrentFile(const QString &fileName)
{
    currentFilePath = fileName;
    editor->document()->setModified(false);

    QString shownName = currentFilePath;
    if (currentFilePath.isEmpty())
        shownName = "untitled.x68";
    setWindowFilePath(shownName);
    setWindowTitle(QString("%1[*] - EASy68K Native Linux IDE").arg(QFileInfo(shownName).fileName()));
}

void MainWindow::assembleCurrentFile(bool autoLoad)
{
    if (currentFilePath.isEmpty()) {
        if (!saveFileAs()) return;
    } else {
        if (!saveFile()) return;
    }

    pendingAutoLoad = autoLoad;
    buildOutputList->clear();

    // Locate assembler executable
    QString appDir = QCoreApplication::applicationDirPath();
    QString asmBinary = appDir + "/easy68k-asm";
    if (!QFileInfo::exists(asmBinary)) {
        asmBinary = appDir + "/../assembler/easy68k-asm";
    }
    if (!QFileInfo::exists(asmBinary)) {
        asmBinary = "easy68k-asm";
    }

    QFileInfo fi(currentFilePath);
    QString basePath = fi.path() + "/" + fi.completeBaseName();
    currentListingPath = basePath + ".l68";
    currentS68Path = basePath + ".s68";

    if (!assemblerProcess) {
        assemblerProcess = new QProcess(this);
        connect(assemblerProcess, &QProcess::finished, this, &MainWindow::onAssemblerFinished);
    }

    QStringList args;
    args << "-o" << currentS68Path << "--listing" << currentListingPath << currentFilePath;

    statusBar()->showMessage(tr("Assembling %1...").arg(fi.fileName()));
    assemblerProcess->start(asmBinary, args);
}

void MainWindow::onAssemblerFinished(int exitCode, QProcess::ExitStatus /* exitStatus */)
{
    QString output = QString::fromUtf8(assemblerProcess->readAllStandardOutput());
    QString errOutput = QString::fromUtf8(assemblerProcess->readAllStandardError());
    QString fullOutput = output + errOutput;

    QStringList lines = fullOutput.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        auto *item = new QListWidgetItem(line, buildOutputList);

        // Detect line number: e.g. "file(12):" or "line 12" or ":12:"
        static QRegularExpression reLine(QStringLiteral("[:\\(](\\d+)[:\\)]"));
        auto match = reLine.match(line);
        if (match.hasMatch()) {
            int lineNum = match.captured(1).toInt();
            item->setData(Qt::UserRole, lineNum);
            item->setForeground(QColor(200, 0, 0));
        }
    }

    if (exitCode == 0) {
        statusBar()->showMessage(tr("Assembly successful! 0 errors."), 4000);
        auto *okItem = new QListWidgetItem(tr("Assembly complete with 0 errors."), buildOutputList);
        okItem->setForeground(QColor(0, 150, 0));

        loadListing(currentListingPath);

        if (pendingAutoLoad) {
            mainTabs->setCurrentIndex(1); // Switch to Simulator tab
            simManager->loadS68(currentS68Path);
        }
    } else {
        statusBar()->showMessage(tr("Assembly failed! See Build Output."), 4000);
    }
    pendingAutoLoad = false;
}

void MainWindow::onBuildErrorClicked(QListWidgetItem *item)
{
    if (!item) return;
    int lineNum = item->data(Qt::UserRole).toInt();
    if (lineNum > 0) {
        editor->highlightLine(lineNum);
    }
}

void MainWindow::onSimulatorStep()
{
    mainTabs->setCurrentIndex(1);
    simManager->step();
}

void MainWindow::onSimulatorRun()
{
    mainTabs->setCurrentIndex(1);
    simManager->run();
}

void MainWindow::onSimulatorPause()
{
    simManager->pause();
}

void MainWindow::onSimulatorReset()
{
    simManager->reset();
    if (hardwareWidget) {
        hardwareWidget->resetHardware();
    }
}

void MainWindow::onSimulatorLoadS68()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open Motorola S-Record"),
                                                    QString(), tr("Motorola S-Record (*.s68 *.s19 *.s28 *.s);;All Files (*)"));
    if (!fileName.isEmpty()) {
        mainTabs->setCurrentIndex(1);
        currentS68Path = fileName;
        QFileInfo fi(fileName);
        QString listCandidate = fi.path() + "/" + fi.completeBaseName() + ".l68";
        if (QFileInfo::exists(listCandidate)) {
            loadListing(listCandidate);
        } else {
            listingLines.clear();
        }
        simManager->loadS68(fileName);
    }
}

void MainWindow::loadListing(const QString &listingPath)
{
    currentListingPath = listingPath;
    listingLines.clear();
    QFile file(listingPath);
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            listingLines.append(in.readLine());
        }
    }
}

void MainWindow::onSimulatorStateChanged()
{
    registersView->updateRegisters(simManager->getD(), simManager->getA(),
                                   simManager->getPC(), simManager->getSR(),
                                   simManager->getCycles());
    memoryView->updateView();
    if (hardwareWidget) {
        hardwareWidget->updateHardware();
    }
    updateDisassemblyView();
}

void MainWindow::updateDisassemblyView()
{
    uint32_t pc = static_cast<uint32_t>(simManager->getPC());
    QString header = QString("PC: 0x%1 | SR: 0x%2 | Cycles: %3\n--------------------------------------------------\n")
                     .arg(pc, 8, 16, QLatin1Char('0'))
                     .arg(static_cast<uint16_t>(simManager->getSR()), 4, 16, QLatin1Char('0'))
                     .arg(simManager->getCycles());

    if (!listingLines.isEmpty()) {
        QString out = header;
        QString pcPrefix = QString("%1").arg(pc, 8, 16, QLatin1Char('0')).toUpper();
        int activeLineIndex = -1;
        int lineCounter = 0;

        for (const QString &line : listingLines) {
            bool isActive = line.startsWith(pcPrefix, Qt::CaseInsensitive);

            bool isBp = false;
            if (line.length() >= 8) {
                bool ok = false;
                uint32_t lineAddr = line.left(8).toUInt(&ok, 16);
                if (ok && simManager->hasBreakpoint(lineAddr)) {
                    isBp = true;
                }
            }

            QString tag;
            if (isActive && isBp) {
                tag = "=>* ";
                if (activeLineIndex == -1) activeLineIndex = lineCounter;
            } else if (isActive) {
                tag = "==> ";
                if (activeLineIndex == -1) activeLineIndex = lineCounter;
            } else if (isBp) {
                tag = " *  ";
            } else {
                tag = "    ";
            }

            out += tag + line + "\n";
            lineCounter++;
        }
        disassemblyView->setPlainText(out);

        if (activeLineIndex != -1) {
            QTextBlock block = disassemblyView->document()->findBlockByLineNumber(activeLineIndex + 2);
            if (block.isValid()) {
                QTextCursor cursor(block);
                disassemblyView->setTextCursor(cursor);
                disassemblyView->ensureCursorVisible();
            }
        }
    } else {
        const char *mem = simManager->getMemory();
        uint32_t memSize = simManager->getMemorySize();
        QString opcodes;
        if (mem && pc + 1 < memSize) {
            uint16_t op = (static_cast<unsigned char>(mem[pc]) << 8) | static_cast<unsigned char>(mem[pc + 1]);
            opcodes = QString("Instruction Word at PC: 0x%1\n").arg(op, 4, 16, QLatin1Char('0')).toUpper();
        }
        disassemblyView->setPlainText(header + opcodes);
    }
}

void MainWindow::onSimulatorHalted(const QString &reason)
{
    statusBar()->showMessage(reason, 4000);
    onSimulatorMessage(reason);
}

void MainWindow::onSimulatorMessage(const QString &msg)
{
    disassemblyView->appendPlainText(msg);
}

void MainWindow::about()
{
    QMessageBox::about(this, tr("About EASy68K Linux Native"),
                       tr("<h2>EASy68K v5.16.1 (Linux Native)</h2>"
                          "<p>Motorola 68000 Cross-Assembler and Simulator for Linux.</p>"
                          "<p>Compiled with GCC 15 on Kubuntu with Qt 6.</p>"
                          "<p>Author: Chuck Kelly &middot; Ported to Native Linux.</p>"));
}
