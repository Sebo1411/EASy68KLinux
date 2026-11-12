#include "mainwindow.h"
#include "codeeditor.h"
#include "syntaxhighlighter.h"
#include "listingwidget.h"
#include "registersview.h"
#include "memoryview.h"
#include "stackview.h"
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
#include <QDialog>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QTimer>
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <QFont>
#include <QLabel>
#include <QSaveFile>
#include <QInputDialog>
#include <QStyle>
#include <QIcon>
#include <QPixmap>
#include <QTextBlock>

static QIcon loadSimIcon(const QString &relPath, QStyle::StandardPixmap fallback, QWidget *parentWidget)
{
    QString appDir = QCoreApplication::applicationDirPath();
    QStringList candidates = {
        QDir::currentPath() + "/" + relPath,
        appDir + "/" + relPath,
        appDir + "/../" + relPath,
        appDir + "/../../" + relPath,
        appDir + "/../share/doc/easy68k/" + relPath,
        "/usr/share/doc/easy68k/" + relPath,
        "/usr/local/share/doc/easy68k/" + relPath
    };
    for (const QString &path : candidates) {
        if (QFileInfo::exists(path)) {
            QPixmap pm(path);
            if (!pm.isNull()) {
                return QIcon(pm);
            }
        }
    }
    if (parentWidget && parentWidget->style()) {
        return parentWidget->style()->standardIcon(fallback);
    }
    return QIcon();
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    updater = new Updater(this);
    simManager = new SimulatorManager(this);

    setupUi();
    createActions();
    createMenus();
    createToolBars();

    connect(simManager, &SimulatorManager::stateChanged, this, &MainWindow::onSimulatorStateChanged);
    connect(simManager, &SimulatorManager::simulationHalted, this, &MainWindow::onSimulatorHalted);
    connect(simManager, &SimulatorManager::messageLogged, this, &MainWindow::onSimulatorMessage);

    // Route simulated I/O through the interactive GUI terminal
    setSimIO(terminalWidget);

    memoryView->setMemoryPointer(simManager->getMemory(), simManager->getMemorySize());
    stackView->setSimulatorManager(simManager);
    registersView->setSimulatorManager(simManager);
    listingWidget->setSimulatorManager(simManager);

    // Deep interconnection: double-clicking registers jumps memory view
    connect(registersView, &RegistersView::addressDoubleClicked, this, [this](uint32_t addr) {
        simRightTabs->setCurrentWidget(memoryView);
        memoryView->setBaseAddress(addr);
    });

    // Double-clicking PC in registers scrolls to PC in listing
    connect(registersView, &RegistersView::pcDoubleClicked, this, [this](uint32_t pc) {
        listingWidget->highlightPC(pc, false);
    });

    // Double-clicking instruction line in listing sets PC
    connect(listingWidget, &ListingWidget::lineDoubleClicked, this, [this](uint32_t addr) {
        simManager->setPC(addr);
    });

    // Memory edits immediately notify hardware and stack views
    connect(memoryView, &MemoryView::memoryModified, this, [this](uint32_t /* addr */) {
        if (hardwareWidget) hardwareWidget->updateHardware();
        if (stackView) stackView->updateView();
    });

    connect(hardwareWidget, &HardwareWidget::hardwareResetRequested, this, &MainWindow::onSimulatorReset);

    auto *cursorPosLabel = new QLabel(tr("Line: 1, Col: 1"), this);
    statusBar()->addPermanentWidget(cursorPosLabel);
    connect(editor, &QPlainTextEdit::cursorPositionChanged, this, [this, cursorPosLabel]() {
        QTextCursor tc = editor->textCursor();
        cursorPosLabel->setText(tr("Line: %1, Col: %2").arg(tc.blockNumber() + 1).arg(tc.columnNumber() + 1));
    });

    setCurrentFile(QString());
    resize(1200, 800);

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

    // =========================================================================
    // Tab 0: Source Editor (Edit68K)
    // =========================================================================
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

    // =========================================================================
    // Tab 1: 68000 Simulator (Sim68K)
    // =========================================================================
    auto *simTab = new QWidget(this);
    auto *simLayout = new QVBoxLayout(simTab);
    simLayout->setContentsMargins(2, 2, 2, 2);
    simLayout->setSpacing(2);

    // 1. Top: Registers View (Compact, horizontal, always visible across the top)
    registersView = new RegistersView(simTab);
    simLayout->addWidget(registersView);

    // 2. Below Registers: Main Splitter
    auto *simSplitter = new QSplitter(Qt::Horizontal, simTab);

    // Left Pane: Code Listing + Execution Trace Log
    auto *simLeftSplitter = new QSplitter(Qt::Vertical, simSplitter);

    listingWidget = new ListingWidget(simLeftSplitter);
    simLeftSplitter->addWidget(listingWidget);

    auto *logBox = new QWidget(simLeftSplitter);
    auto *logLayout = new QVBoxLayout(logBox);
    logLayout->setContentsMargins(0, 0, 0, 0);
    logLayout->setSpacing(2);

    auto *logHeader = new QLabel(tr(" Execution Trace & Simulator Messages:"), logBox);
    QFont logHeaderFont("Monospace", 8, QFont::Bold);
    logHeader->setFont(logHeaderFont);
    logHeader->setStyleSheet("background-color: #E2E8F0; color: #334155; padding: 2px 4px; border-bottom: 1px solid #CBD5E1;");
    logLayout->addWidget(logHeader);

    executionLogView = new QPlainTextEdit(logBox);
    executionLogView->setReadOnly(true);
    executionLogView->setFont(monoFont);
    executionLogView->setLineWrapMode(QPlainTextEdit::NoWrap);
    logLayout->addWidget(executionLogView);

    simLeftSplitter->addWidget(logBox);
    simLeftSplitter->setStretchFactor(0, 4);
    simLeftSplitter->setStretchFactor(1, 1);

    simSplitter->addWidget(simLeftSplitter);

    // Right Pane: Simulator Windows Tabs
    simRightTabs = new QTabWidget(simSplitter);

    terminalWidget = new TerminalWidget(simRightTabs);
    simRightTabs->addTab(terminalWidget, tr("Output Window (TRAP #15)"));

    hardwareWidget = new HardwareWidget(simRightTabs);
    simRightTabs->addTab(hardwareWidget, tr("Hardware & 2D Graphics"));
    terminalWidget->setHardwareWidget(hardwareWidget);

    memoryView = new MemoryView(simRightTabs);
    simRightTabs->addTab(memoryView, tr("Memory"));

    stackView = new StackView(simRightTabs);
    simRightTabs->addTab(stackView, tr("Stack"));

    simSplitter->addWidget(simRightTabs);
    simSplitter->setStretchFactor(0, 3);
    simSplitter->setStretchFactor(1, 2);

    simLayout->addWidget(simSplitter, 1);
    mainTabs->addTab(simTab, tr("68000 Simulator"));

    // Other Tabs
    easyBinWidget = new EasyBinWidget(mainTabs);
    mainTabs->addTab(easyBinWidget, tr("EASyBIN Converter"));

    helpWidget = new HelpWidget(mainTabs);
    mainTabs->addTab(helpWidget, tr("Help & Reference"));

    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::createActions()
{
    // File Actions
    newAct = new QAction(loadSimIcon("docs/help/Images/ToolNew.GIF", QStyle::SP_FileIcon, this), tr("&New"), this);
    newAct->setShortcuts(QKeySequence::New);
    connect(newAct, &QAction::triggered, this, &MainWindow::newFile);

    openAct = new QAction(loadSimIcon("docs/help/Images/ToolOpen.GIF", QStyle::SP_DialogOpenButton, this), tr("&Open..."), this);
    openAct->setShortcuts(QKeySequence::Open);
    connect(openAct, &QAction::triggered, this, &MainWindow::openFile);

    saveAct = new QAction(loadSimIcon("docs/help/Images/ToolSave.GIF", QStyle::SP_DialogSaveButton, this), tr("&Save"), this);
    saveAct->setShortcuts(QKeySequence::Save);
    connect(saveAct, &QAction::triggered, this, &MainWindow::saveFile);

    saveAsAct = new QAction(tr("Save &As..."), this);
    saveAsAct->setShortcuts(QKeySequence::SaveAs);
    connect(saveAsAct, &QAction::triggered, this, &MainWindow::saveFileAs);

    exitAct = new QAction(tr("E&xit"), this);
    exitAct->setShortcuts(QKeySequence::Quit);
    connect(exitAct, &QAction::triggered, this, &QWidget::close);

    // Edit Actions (Edit68K)
    undoAct = new QAction(loadSimIcon("docs/help/Images/ToolUndo.GIF", QStyle::SP_ArrowBack, this), tr("&Undo"), this);
    undoAct->setShortcuts(QKeySequence::Undo);
    connect(undoAct, &QAction::triggered, editor, &QPlainTextEdit::undo);

    redoAct = new QAction(loadSimIcon("docs/help/Images/ToolRedo.GIF", QStyle::SP_ArrowForward, this), tr("&Redo"), this);
    redoAct->setShortcuts(QKeySequence::Redo);
    connect(redoAct, &QAction::triggered, editor, &QPlainTextEdit::redo);

    cutAct = new QAction(loadSimIcon("docs/help/Images/ToolCut.GIF", QStyle::SP_DialogDiscardButton, this), tr("Cu&t"), this);
    cutAct->setShortcuts(QKeySequence::Cut);
    connect(cutAct, &QAction::triggered, editor, &QPlainTextEdit::cut);

    copyAct = new QAction(loadSimIcon("docs/help/Images/ToolCopy.GIF", QStyle::SP_FileDialogDetailedView, this), tr("&Copy"), this);
    copyAct->setShortcuts(QKeySequence::Copy);
    connect(copyAct, &QAction::triggered, editor, &QPlainTextEdit::copy);

    pasteAct = new QAction(loadSimIcon("docs/help/Images/ToolPaste.GIF", QStyle::SP_FileDialogInfoView, this), tr("&Paste"), this);
    pasteAct->setShortcuts(QKeySequence::Paste);
    connect(pasteAct, &QAction::triggered, editor, &QPlainTextEdit::paste);

    commentAct = new QAction(loadSimIcon("docs/help/Images/ToolComment.GIF", QStyle::SP_FileDialogNewFolder, this), tr("Comment Lines (+*)"), this);
    connect(commentAct, &QAction::triggered, this, &MainWindow::onCommentLines);

    uncommentAct = new QAction(loadSimIcon("docs/help/Images/ToolUncomment.GIF", QStyle::SP_FileDialogBack, this), tr("Uncomment Lines (-*)"), this);
    connect(uncommentAct, &QAction::triggered, this, &MainWindow::onUncommentLines);

    insertTemplateAct = new QAction(tr("Insert 68000 &Template"), this);
    connect(insertTemplateAct, &QAction::triggered, this, &MainWindow::onInsertTemplate);

    // Project Actions
    assembleAct = new QAction(loadSimIcon("docs/help/Images/ToolAssemble.GIF", QStyle::SP_BrowserReload, this), tr("&Assemble"), this);
    assembleAct->setShortcut(QKeySequence(Qt::Key_F11));
    connect(assembleAct, &QAction::triggered, this, [this]() { assembleCurrentFile(false); });

    assembleAndLoadAct = new QAction(tr("Assemble && &Load in Simulator"), this);
    assembleAndLoadAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F8));
    connect(assembleAndLoadAct, &QAction::triggered, this, [this]() { assembleCurrentFile(true); });

    // Sim68K Run Actions
    runAct = new QAction(loadSimIcon("docs/help/SIMHELP/IMAGES/BasicOp/RunBtn.GIF", QStyle::SP_MediaPlay, this), tr("&Run"), this);
    runAct->setShortcut(QKeySequence(Qt::Key_F9));
    connect(runAct, &QAction::triggered, this, &MainWindow::onSimulatorRun);

    runToCursorAct = new QAction(loadSimIcon("docs/help/SIMHELP/IMAGES/BasicOp/RunToBtn.GIF", QStyle::SP_MediaSeekForward, this), tr("Run &To Cursor"), this);
    runToCursorAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F9));
    connect(runToCursorAct, &QAction::triggered, this, &MainWindow::onSimulatorRunToCursor);

    autoTraceAct = new QAction(loadSimIcon("docs/help/SIMHELP/IMAGES/BasicOp/AutoTraceBtn.GIF", QStyle::SP_ArrowForward, this), tr("&Auto Trace"), this);
    autoTraceAct->setShortcut(QKeySequence(Qt::Key_F10));
    connect(autoTraceAct, &QAction::triggered, this, &MainWindow::onSimulatorAutoTrace);

    stepOverAct = new QAction(loadSimIcon("docs/help/SIMHELP/IMAGES/BasicOp/StepBtn.GIF", QStyle::SP_MediaSkipForward, this), tr("&Step Over"), this);
    stepOverAct->setShortcut(QKeySequence(Qt::Key_F8));
    connect(stepOverAct, &QAction::triggered, this, &MainWindow::onSimulatorStepOver);

    traceIntoAct = new QAction(loadSimIcon("docs/help/SIMHELP/IMAGES/BasicOp/TraceBtn.GIF", QStyle::SP_MediaSeekForward, this), tr("&Trace Into"), this);
    traceIntoAct->setShortcut(QKeySequence(Qt::Key_F7));
    connect(traceIntoAct, &QAction::triggered, this, &MainWindow::onSimulatorTraceInto);

    pauseAct = new QAction(loadSimIcon("docs/help/SIMHELP/IMAGES/BasicOp/PauseBtn.GIF", QStyle::SP_MediaPause, this), tr("&Pause"), this);
    pauseAct->setShortcut(QKeySequence(Qt::Key_F6));
    connect(pauseAct, &QAction::triggered, this, &MainWindow::onSimulatorPause);

    rewindAct = new QAction(tr("Re&wind Program"), this);
    rewindAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F2));
    connect(rewindAct, &QAction::triggered, this, &MainWindow::onSimulatorRewind);

    reloadAct = new QAction(loadSimIcon("docs/help/SIMHELP/IMAGES/BasicOp/ReloadBtn.GIF", QStyle::SP_BrowserReload, this), tr("Re&load Program"), this);
    reloadAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F3));
    connect(reloadAct, &QAction::triggered, this, &MainWindow::onSimulatorReload);

    resetAct = new QAction(loadSimIcon("docs/help/SIMHELP/IMAGES/BasicOp/ResetBtn.GIF", QStyle::SP_MediaStop, this), tr("&Reset"), this);
    resetAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    connect(resetAct, &QAction::triggered, this, &MainWindow::onSimulatorReset);

    loadS68Act = new QAction(tr("Load &S68 File..."), this);
    connect(loadS68Act, &QAction::triggered, this, &MainWindow::onSimulatorLoadS68);

    toggleBpAct = new QAction(tr("Toggle &Breakpoint"), this);
    toggleBpAct->setShortcut(QKeySequence(Qt::Key_F2));
    connect(toggleBpAct, &QAction::triggered, this, [this]() {
        uint32_t targetAddr = 0;
        int sel = listingWidget->scrollArea()->getSelectedLine();
        if (sel >= 0 && sel < listingLines.size() && ListingScrollArea::isInstructionLine(listingLines[sel])) {
            targetAddr = ListingScrollArea::getLineAddress(listingLines[sel]);
        } else {
            targetAddr = static_cast<uint32_t>(simManager->getPC());
        }
        simManager->toggleBreakpoint(targetAddr);
    });

    clearBpsAct = new QAction(tr("Clear &All Breakpoints"), this);
    connect(clearBpsAct, &QAction::triggered, this, [this]() {
        simManager->clearBreakpoints();
    });

    // Search Actions
    findAct = new QAction(loadSimIcon("docs/help/Images/ToolFind.GIF", QStyle::SP_FileDialogContentsView, this), tr("&Find..."), this);
    findAct->setShortcut(QKeySequence::Find);
    connect(findAct, &QAction::triggered, this, &MainWindow::onFindText);

    goToPcAct = new QAction(tr("&Go to PC"), this);
    goToPcAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
    connect(goToPcAct, &QAction::triggered, this, &MainWindow::onGoToPC);

    // View Actions
    viewOutputAct = new QAction(tr("&Output Window"), this);
    connect(viewOutputAct, &QAction::triggered, this, [this]() {
        mainTabs->setCurrentIndex(1);
        simRightTabs->setCurrentWidget(terminalWidget);
    });

    viewHwAct = new QAction(tr("&Hardware & Graphics"), this);
    connect(viewHwAct, &QAction::triggered, this, [this]() {
        mainTabs->setCurrentIndex(1);
        simRightTabs->setCurrentWidget(hardwareWidget);
    });

    viewMemoryAct = new QAction(tr("&Memory"), this);
    connect(viewMemoryAct, &QAction::triggered, this, [this]() {
        mainTabs->setCurrentIndex(1);
        simRightTabs->setCurrentWidget(memoryView);
    });

    viewStackAct = new QAction(tr("&Stack"), this);
    connect(viewStackAct, &QAction::triggered, this, [this]() {
        mainTabs->setCurrentIndex(1);
        simRightTabs->setCurrentWidget(stackView);
    });

    viewEasyBinAct = new QAction(tr("&EASyBIN"), this);
    connect(viewEasyBinAct, &QAction::triggered, this, [this]() {
        mainTabs->setCurrentWidget(easyBinWidget);
    });

    viewHelpAct = new QAction(tr("&Help Window"), this);
    connect(viewHelpAct, &QAction::triggered, this, [this]() {
        mainTabs->setCurrentWidget(helpWidget);
    });

    // Help Actions
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

    auto *editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(undoAct);
    editMenu->addAction(redoAct);
    editMenu->addSeparator();
    editMenu->addAction(cutAct);
    editMenu->addAction(copyAct);
    editMenu->addAction(pasteAct);
    editMenu->addSeparator();
    editMenu->addAction(commentAct);
    editMenu->addAction(uncommentAct);
    editMenu->addAction(insertTemplateAct);

    auto *searchMenu = menuBar()->addMenu(tr("&Search"));
    searchMenu->addAction(findAct);
    searchMenu->addAction(goToPcAct);

    auto *viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(viewOutputAct);
    viewMenu->addAction(viewHwAct);
    viewMenu->addAction(viewMemoryAct);
    viewMenu->addAction(viewStackAct);
    viewMenu->addSeparator();
    viewMenu->addAction(viewEasyBinAct);
    viewMenu->addAction(viewHelpAct);

    auto *projectMenu = menuBar()->addMenu(tr("&Project"));
    projectMenu->addAction(assembleAct);
    projectMenu->addAction(assembleAndLoadAct);

    auto *runMenu = menuBar()->addMenu(tr("&Run"));
    runMenu->addAction(runAct);
    runMenu->addAction(runToCursorAct);
    runMenu->addAction(autoTraceAct);
    runMenu->addAction(stepOverAct);
    runMenu->addAction(traceIntoAct);
    runMenu->addAction(pauseAct);
    runMenu->addSeparator();
    runMenu->addAction(rewindAct);
    runMenu->addAction(reloadAct);
    runMenu->addAction(resetAct);
    runMenu->addSeparator();
    runMenu->addAction(toggleBpAct);
    runMenu->addAction(clearBpsAct);
    runMenu->addSeparator();
    runMenu->addAction(loadS68Act);

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
    mainToolBar->addAction(newAct);
    mainToolBar->addAction(openAct);
    mainToolBar->addAction(saveAct);
    mainToolBar->addAction(findAct);
    mainToolBar->addSeparator();
    mainToolBar->addAction(commentAct);
    mainToolBar->addAction(uncommentAct);
    mainToolBar->addSeparator();
    mainToolBar->addAction(assembleAct);
    mainToolBar->addSeparator();
    mainToolBar->addAction(runAct);
    mainToolBar->addAction(runToCursorAct);
    mainToolBar->addAction(autoTraceAct);
    mainToolBar->addAction(stepOverAct);
    mainToolBar->addAction(traceIntoAct);
    mainToolBar->addAction(pauseAct);
    mainToolBar->addSeparator();
    mainToolBar->addAction(resetAct);
    mainToolBar->addAction(reloadAct);
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

    int errorCount = (exitCode != 0) ? 1 : 0;
    int warningCount = 0;

    QStringList lines = fullOutput.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        auto *item = new QListWidgetItem(line, buildOutputList);

        if (line.contains("error", Qt::CaseInsensitive)) {
            errorCount++;
            item->setForeground(QColor(220, 38, 38));
        } else if (line.contains("warning", Qt::CaseInsensitive)) {
            warningCount++;
            item->setForeground(QColor(217, 119, 6));
        }

        static QRegularExpression reLine(QStringLiteral("[:\\(](\\d+)[:\\)]"));
        auto match = reLine.match(line);
        if (match.hasMatch()) {
            int lineNum = match.captured(1).toInt();
            item->setData(Qt::UserRole, lineNum);
            if (errorCount > 0) {
                item->setForeground(QColor(220, 38, 38));
            }
        }
    }

    if (exitCode == 0) {
        statusBar()->showMessage(tr("Assembly successful! 0 errors."), 4000);
        auto *okItem = new QListWidgetItem(tr("Assembly complete with 0 errors."), buildOutputList);
        okItem->setForeground(QColor(22, 163, 74));

        loadListing(currentListingPath);

        if (pendingAutoLoad) {
            mainTabs->setCurrentIndex(1); // Switch to Simulator tab
            simManager->loadS68(currentS68Path);
        } else {
            showAssemblerStatusDialog(0, warningCount);
        }
    } else {
        statusBar()->showMessage(tr("Assembly failed! See Build Output."), 4000);
        if (!pendingAutoLoad) {
            showAssemblerStatusDialog(std::max(1, errorCount), warningCount);
        }
    }

    pendingAutoLoad = false;
}

void MainWindow::showAssemblerStatusDialog(int errors, int warnings)
{
    if (QGuiApplication::platformName() == QStringLiteral("offscreen")) {
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Assembler Status"));
    dlg.setMinimumWidth(320);

    auto *layout = new QVBoxLayout(&dlg);
    layout->setSpacing(12);

    auto *statusLabel = new QLabel(errors == 0 ? tr("<b>Assembly completed successfully!</b>")
                                               : tr("<b>Assembly completed with errors.</b>"), &dlg);
    layout->addWidget(statusLabel);

    auto *infoLabel = new QLabel(tr("Errors: %1\nWarnings: %2").arg(errors).arg(warnings), &dlg);
    if (errors > 0) {
        infoLabel->setStyleSheet("color: #DC2626; font-weight: bold; font-size: 11pt;");
    } else {
        infoLabel->setStyleSheet("color: #16A34A; font-weight: bold; font-size: 11pt;");
    }
    layout->addWidget(infoLabel);

    auto *btnBox = new QHBoxLayout();
    btnBox->setSpacing(8);

    auto *executeBtn = new QPushButton(tr("Execute"), &dlg);
    executeBtn->setEnabled(errors == 0);
    if (errors == 0) {
        executeBtn->setDefault(true);
    }
    btnBox->addWidget(executeBtn);

    auto *loadL68Btn = new QPushButton(tr("Load L68"), &dlg);
    btnBox->addWidget(loadL68Btn);

    auto *closeBtn = new QPushButton(tr("Close"), &dlg);
    btnBox->addWidget(closeBtn);

    layout->addLayout(btnBox);

    connect(executeBtn, &QPushButton::clicked, &dlg, [&dlg, this]() {
        dlg.accept();
        mainTabs->setCurrentIndex(1); // Simulator tab
        simManager->loadS68(currentS68Path);
    });

    connect(loadL68Btn, &QPushButton::clicked, &dlg, [&dlg, this]() {
        dlg.accept();
        mainTabs->setCurrentIndex(1); // Simulator tab
        loadListing(currentListingPath);
    });

    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::reject);

    dlg.exec();
}

void MainWindow::onBuildErrorClicked(QListWidgetItem *item)
{
    if (!item) return;
    int lineNum = item->data(Qt::UserRole).toInt();
    if (lineNum > 0) {
        editor->highlightLine(lineNum);
    }
}

void MainWindow::onSimulatorTraceInto()
{
    mainTabs->setCurrentIndex(1);
    simManager->traceInto();
}

void MainWindow::onSimulatorStepOver()
{
    mainTabs->setCurrentIndex(1);
    simManager->stepOver();
}

void MainWindow::onSimulatorRun()
{
    mainTabs->setCurrentIndex(1);
    simManager->run();
}

void MainWindow::onSimulatorRunToCursor()
{
    mainTabs->setCurrentIndex(1);
    uint32_t targetAddr = 0;
    int sel = listingWidget->scrollArea()->getSelectedLine();
    if (sel >= 0 && sel < listingLines.size() && ListingScrollArea::isInstructionLine(listingLines[sel])) {
        targetAddr = ListingScrollArea::getLineAddress(listingLines[sel]);
    } else {
        int hl = listingWidget->scrollArea()->getHighlightedLine();
        if (hl >= 0 && hl < listingLines.size()) {
            targetAddr = ListingScrollArea::getLineAddress(listingLines[hl]);
        }
    }
    if (targetAddr == 0) {
        targetAddr = static_cast<uint32_t>(simManager->getPC());
    }
    simManager->runToCursor(targetAddr);
}

void MainWindow::onSimulatorAutoTrace()
{
    mainTabs->setCurrentIndex(1);
    simManager->toggleAutoTrace();
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

void MainWindow::onSimulatorRewind()
{
    mainTabs->setCurrentIndex(1);
    simManager->rewindProgram();
    if (hardwareWidget) {
        hardwareWidget->resetHardware();
    }
}

void MainWindow::onSimulatorReload()
{
    mainTabs->setCurrentIndex(1);
    simManager->reloadProgram();
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
            listingWidget->clear();
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
    listingWidget->loadListing(listingLines);
    simManager->loadListingDirectives(listingLines);
}

void MainWindow::onSimulatorStateChanged()
{
    registersView->updateRegisters(simManager->getD(), simManager->getA(),
                                   simManager->getPC(), simManager->getSR(),
                                   simManager->getCycles());
    memoryView->updateView();
    if (stackView) {
        stackView->updateView();
    }
    if (hardwareWidget) {
        hardwareWidget->updateHardware();
    }
    listingWidget->highlightPC(static_cast<uint32_t>(simManager->getPC()), false);
}

void MainWindow::onSimulatorHalted(const QString &reason)
{
    statusBar()->showMessage(reason, 4000);
    onSimulatorMessage(reason);
    listingWidget->highlightPC(static_cast<uint32_t>(simManager->getPC()), true);
}

void MainWindow::onSimulatorMessage(const QString &msg)
{
    if (executionLogView) {
        executionLogView->appendPlainText(msg);
        executionLogView->ensureCursorVisible();
    }
}

void MainWindow::onGoToPC()
{
    mainTabs->setCurrentIndex(1);
    listingWidget->highlightPC(static_cast<uint32_t>(simManager->getPC()), false);
}

void MainWindow::onFindText()
{
    bool ok = false;
    QString query = QInputDialog::getText(this, tr("Find Text"), tr("Search string:"), QLineEdit::Normal, QString(), &ok);
    if (ok && !query.isEmpty()) {
        if (mainTabs->currentIndex() == 0) {
            editor->find(query);
        }
    }
}

void MainWindow::onCommentLines()
{
    QTextCursor cursor = editor->textCursor();
    int startPos = cursor.selectionStart();
    int endPos = cursor.selectionEnd();

    cursor.beginEditBlock();
    cursor.setPosition(startPos);
    int startBlock = cursor.blockNumber();
    cursor.setPosition(endPos);
    int endBlock = cursor.blockNumber();
    if (cursor.atBlockStart() && endBlock > startBlock) {
        endBlock--;
    }

    for (int b = startBlock; b <= endBlock; ++b) {
        QTextBlock block = editor->document()->findBlockByNumber(b);
        QTextCursor lineCursor(block);
        lineCursor.insertText("*");
    }
    cursor.endEditBlock();
}

void MainWindow::onUncommentLines()
{
    QTextCursor cursor = editor->textCursor();
    int startPos = cursor.selectionStart();
    int endPos = cursor.selectionEnd();

    cursor.beginEditBlock();
    cursor.setPosition(startPos);
    int startBlock = cursor.blockNumber();
    cursor.setPosition(endPos);
    int endBlock = cursor.blockNumber();
    if (cursor.atBlockStart() && endBlock > startBlock) {
        endBlock--;
    }

    for (int b = startBlock; b <= endBlock; ++b) {
        QTextBlock block = editor->document()->findBlockByNumber(b);
        if (block.text().startsWith('*')) {
            QTextCursor lineCursor(block);
            lineCursor.deleteChar();
        }
    }
    cursor.endEditBlock();
}

void MainWindow::onInsertTemplate()
{
    static const char *kTemplate =
        "*-----------------------------------------------------------\n"
        "* Title      :\n"
        "* Written by :\n"
        "* Date       :\n"
        "* Description:\n"
        "*-----------------------------------------------------------\n"
        "    ORG    $1000\n"
        "START:                  ; first instruction of program\n"
        "\n"
        "* Put program code here\n"
        "\n"
        "    SIMHALT             ; halt simulator\n"
        "\n"
        "* Put variables and constants here\n"
        "\n"
        "    END    START        ; last line of source\n";
    editor->insertPlainText(QString::fromLatin1(kTemplate));
}

void MainWindow::about()
{
    QMessageBox::about(this, tr("About EASy68K"),
                       tr("<h3>EASy68K Native Linux IDE & Simulator</h3>"
                          "<p>Version %1</p>"
                          "<p>Motorola 68000 cross-assembler and simulator originally developed by Chuck Kelly.</p>"
                          "<p>Modern native Linux edition with zero-overhead GUI and complete Sim68K / Edit68K fidelity.</p>")
                       .arg(EASY68K_VERSION_STRING));
}
