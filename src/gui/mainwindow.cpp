#include "mainwindow.h"
#include "codeeditor.h"
#include "syntaxhighlighter.h"
#include "listingwidget.h"
#include "registersview.h"
#include "memoryview.h"
#include "variablesview.h"
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
#include <QPainter>
#include <QPainterPath>
#include <QTextBlock>

QIcon MainWindow::createAppWindowIcon()
{
    QIcon icon;
    const int sizes[] = {16, 24, 32, 48, 64, 128};
    for (int sz : sizes) {
        QPixmap pm(sz, sz);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing, true);

        qreal s = sz / 64.0;
        p.scale(s, s);

        // Gold/Silver IC pins on top & bottom
        p.setBrush(QColor(0x94, 0xA3, 0xB8));
        p.setPen(Qt::NoPen);
        for (int i = 0; i < 5; ++i) {
            qreal px = 12.0 + i * 9.0;
            p.drawRoundedRect(QRectF(px, 3.0, 4.0, 9.0), 1.5, 1.5);
            p.drawRoundedRect(QRectF(px, 52.0, 4.0, 9.0), 1.5, 1.5);
        }
        // Left & right pins
        for (int i = 0; i < 4; ++i) {
            qreal py = 15.0 + i * 9.5;
            p.drawRoundedRect(QRectF(3.0, py, 9.0, 4.0), 1.5, 1.5);
            p.drawRoundedRect(QRectF(52.0, py, 9.0, 4.0), 1.5, 1.5);
        }

        // Main Motorola 68000 ceramic/slate IC package
        QRectF body(9.0, 9.0, 46.0, 46.0);
        p.setBrush(QColor(0x1E, 0x29, 0x3B));
        p.setPen(QPen(QColor(0x38, 0xBD, 0xF8), 2.5));
        p.drawRoundedRect(body, 7.0, 7.0);

        // Pin 1 index dot (gold)
        p.setBrush(QColor(0xFA, 0xCC, 0x15));
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(16.0, 16.0), 2.5, 2.5);

        // "68K" typography
        QFont f("Monospace", 12, QFont::Black);
        f.setPixelSize(17);
        p.setFont(f);
        p.setPen(QColor(0xF8, 0xFA, 0xFC));
        p.drawText(body.adjusted(0, 2, 0, 0), Qt::AlignCenter, QStringLiteral("68K"));

        icon.addPixmap(pm);
    }
    return icon;
}

QIcon MainWindow::createModernToolbarIcon(const QString &name)
{
    QIcon icon;
    const int sizes[] = {24, 48};
    for (int sz : sizes) {
        QPixmap pm(sz, sz);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing, true);
        qreal s = sz / 24.0;
        p.scale(s, s);

        if (name == QLatin1String("new")) {
            // Document outline (#D4D4D4) + green '+' badge
            QPainterPath doc;
            doc.moveTo(5, 3);
            doc.lineTo(14, 3);
            doc.lineTo(19, 8);
            doc.lineTo(19, 21);
            doc.lineTo(5, 21);
            doc.closeSubpath();
            p.setBrush(QColor(0x25, 0x25, 0x26));
            p.setPen(QPen(QColor(0xD4, 0xD4, 0xD4), 1.8));
            p.drawPath(doc);
            p.setPen(QPen(QColor(0x89, 0xD1, 0x85), 2.2, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(12, 11, 12, 17);
            p.drawLine(9, 14, 15, 14);
        } else if (name == QLatin1String("open")) {
            // Warm VS Code folder (#DCB67A)
            QPainterPath folder;
            folder.moveTo(3, 6);
            folder.lineTo(9, 6);
            folder.lineTo(11, 8);
            folder.lineTo(21, 8);
            folder.lineTo(21, 19);
            folder.lineTo(3, 19);
            folder.closeSubpath();
            p.setBrush(QColor(0xDC, 0xB6, 0x7A));
            p.setPen(QPen(QColor(0xE8, 0xC9, 0x95), 1.4));
            p.drawPath(folder);
        } else if (name == QLatin1String("save")) {
            // VS Code blue disk (#569CD6)
            p.setBrush(QColor(0x37, 0x94, 0xFF));
            p.setPen(QPen(QColor(0x75, 0xBE, 0xFF), 1.4));
            p.drawRoundedRect(QRectF(4, 4, 16, 16), 2.5, 2.5);
            p.setBrush(QColor(0x1E, 0x1E, 0x1E));
            p.drawRect(QRectF(7, 4, 9, 6));
            p.setBrush(QColor(0xE2, 0xE8, 0xF0));
            p.drawRect(QRectF(7, 13, 10, 7));
        } else if (name == QLatin1String("find")) {
            // Sky-blue magnifying glass (#9CDCFE)
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor(0x9C, 0xDC, 0xFE), 2.2));
            p.drawEllipse(QPointF(10.5, 10.5), 5.5, 5.5);
            p.setPen(QPen(QColor(0x9C, 0xDC, 0xFE), 2.5, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(QPointF(14.8, 14.8), QPointF(20.0, 20.0));
        } else if (name == QLatin1String("undo")) {
            p.setPen(QPen(QColor(0x75, 0xBE, 0xFF), 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.setBrush(Qt::NoBrush);
            p.drawArc(QRectF(6, 7, 13, 11), 0 * 16, 180 * 16);
            p.drawLine(QPointF(6, 12), QPointF(4, 8));
            p.drawLine(QPointF(6, 12), QPointF(10, 10));
        } else if (name == QLatin1String("redo")) {
            p.setPen(QPen(QColor(0x75, 0xBE, 0xFF), 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.setBrush(Qt::NoBrush);
            p.drawArc(QRectF(5, 7, 13, 11), 0 * 16, 180 * 16);
            p.drawLine(QPointF(18, 12), QPointF(20, 8));
            p.drawLine(QPointF(18, 12), QPointF(14, 10));
        } else if (name == QLatin1String("cut") || name == QLatin1String("copy") || name == QLatin1String("paste")) {
            p.setPen(QPen(QColor(0xD4, 0xD4, 0xD4), 1.8));
            p.setBrush(QColor(0x25, 0x25, 0x26));
            p.drawRoundedRect(QRectF(5, 6, 11, 14), 1.5, 1.5);
            p.drawRoundedRect(QRectF(8, 3, 11, 14), 1.5, 1.5);
        } else if (name == QLatin1String("comment")) {
            p.setBrush(QColor(0x25, 0x25, 0x26));
            p.setPen(QPen(QColor(0x6A, 0x99, 0x55), 1.6));
            p.drawRoundedRect(QRectF(2.5, 4.5, 19.0, 15.0), 3.5, 3.5);
            QFont f("Monospace", 9, QFont::Bold);
            f.setPixelSize(12);
            p.setFont(f);
            p.setPen(QColor(0x89, 0xD1, 0x85));
            p.drawText(QRectF(2.5, 4.5, 19.0, 15.0), Qt::AlignCenter, QStringLiteral("+*"));
        } else if (name == QLatin1String("uncomment")) {
            p.setBrush(QColor(0x25, 0x25, 0x26));
            p.setPen(QPen(QColor(0xCE, 0x91, 0x78), 1.6));
            p.drawRoundedRect(QRectF(2.5, 4.5, 19.0, 15.0), 3.5, 3.5);
            QFont f("Monospace", 9, QFont::Bold);
            f.setPixelSize(12);
            p.setFont(f);
            p.setPen(QColor(0xCE, 0x91, 0x78));
            p.drawText(QRectF(2.5, 4.5, 19.0, 15.0), Qt::AlignCenter, QStringLiteral("-*"));
        } else if (name == QLatin1String("assemble")) {
            // Teal build hexagon/gear + play (#4EC9B0)
            p.setBrush(QColor(0x1E, 0x3A, 0x3A));
            p.setPen(QPen(QColor(0x4E, 0xC9, 0xB0), 2.0));
            p.drawRoundedRect(QRectF(3.5, 3.5, 17.0, 17.0), 4.0, 4.0);
            QPainterPath tri;
            tri.moveTo(9.0, 7.5);
            tri.lineTo(17.0, 12.0);
            tri.lineTo(9.0, 16.5);
            tri.closeSubpath();
            p.setBrush(QColor(0x4E, 0xC9, 0xB0));
            p.setPen(Qt::NoPen);
            p.drawPath(tri);
        } else if (name == QLatin1String("run")) {
            // Vibrant VS Code Debug Run green play triangle (#89D185)
            QPainterPath tri;
            tri.moveTo(6.0, 4.0);
            tri.lineTo(19.5, 12.0);
            tri.lineTo(6.0, 20.0);
            tri.closeSubpath();
            p.setBrush(QColor(0x89, 0xD1, 0x85));
            p.setPen(QPen(QColor(0x4E, 0xC9, 0xB0), 1.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.drawPath(tri);
        } else if (name == QLatin1String("runto")) {
            // Green play triangle pointing to gold vertical cursor bar
            QPainterPath tri;
            tri.moveTo(4.5, 5.0);
            tri.lineTo(15.5, 12.0);
            tri.lineTo(4.5, 19.0);
            tri.closeSubpath();
            p.setBrush(QColor(0x89, 0xD1, 0x85));
            p.setPen(Qt::NoPen);
            p.drawPath(tri);
            p.setPen(QPen(QColor(0xFF, 0xCC, 0x00), 2.8, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(QPointF(18.5, 5.0), QPointF(18.5, 19.0));
        } else if (name == QLatin1String("autotrace")) {
            // Double fast-forward triangles (#C586C0)
            QPainterPath t1, t2;
            t1.moveTo(3.5, 5.5);
            t1.lineTo(12.0, 12.0);
            t1.lineTo(3.5, 18.5);
            t1.closeSubpath();
            t2.moveTo(11.5, 5.5);
            t2.lineTo(20.5, 12.0);
            t2.lineTo(11.5, 18.5);
            t2.closeSubpath();
            p.setBrush(QColor(0xC5, 0x86, 0xC0));
            p.setPen(Qt::NoPen);
            p.drawPath(t1);
            p.drawPath(t2);
        } else if (name == QLatin1String("stepover")) {
            // VS Code Step Over: curved blue arc jumping over dot
            p.setBrush(QColor(0xD4, 0xD4, 0xD4));
            p.setPen(Qt::NoPen);
            p.drawEllipse(QPointF(12.0, 17.5), 2.5, 2.5);
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor(0x75, 0xBE, 0xFF), 2.3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.drawArc(QRectF(5.0, 5.5, 14.0, 12.0), 20 * 16, 140 * 16);
            p.drawLine(QPointF(18.5, 10.5), QPointF(14.5, 9.5));
            p.drawLine(QPointF(18.5, 10.5), QPointF(19.5, 6.5));
        } else if (name == QLatin1String("traceinto")) {
            // VS Code Step Into: downward blue arrow pointing into dot
            p.setBrush(QColor(0xD4, 0xD4, 0xD4));
            p.setPen(Qt::NoPen);
            p.drawEllipse(QPointF(12.0, 18.5), 2.5, 2.5);
            p.setPen(QPen(QColor(0x75, 0xBE, 0xFF), 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.drawLine(QPointF(12.0, 4.0), QPointF(12.0, 13.5));
            p.drawLine(QPointF(8.0, 9.5), QPointF(12.0, 13.8));
            p.drawLine(QPointF(16.0, 9.5), QPointF(12.0, 13.8));
        } else if (name == QLatin1String("pause")) {
            // VS Code Pause: two golden vertical bars (#FFCC00)
            p.setBrush(QColor(0xFF, 0xCC, 0x00));
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(QRectF(6.5, 4.5, 4.0, 15.0), 1.2, 1.2);
            p.drawRoundedRect(QRectF(13.5, 4.5, 4.0, 15.0), 1.2, 1.2);
        } else if (name == QLatin1String("reset")) {
            // VS Code Reset/Stop: circular coral red reset arrow (#F14C4C)
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor(0xF1, 0x4C, 0x4C), 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.drawArc(QRectF(5.0, 5.0, 14.0, 14.0), 45 * 16, 275 * 16);
            p.drawLine(QPointF(16.5, 6.5), QPointF(12.5, 6.5));
            p.drawLine(QPointF(16.5, 6.5), QPointF(16.5, 10.5));
        } else if (name == QLatin1String("reload")) {
            // Teal circular reload arrow (#4EC9B0)
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor(0x4E, 0xC9, 0xB0), 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.drawArc(QRectF(5.0, 5.0, 14.0, 14.0), 30 * 16, 290 * 16);
            p.drawLine(QPointF(17.5, 7.5), QPointF(13.5, 7.5));
            p.drawLine(QPointF(17.5, 7.5), QPointF(17.5, 3.5));
        }
        icon.addPixmap(pm);
    }
    return icon;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowIcon(createAppWindowIcon());
    setStyleSheet(QStringLiteral(
        "QMainWindow, QWidget { background-color: #1E1E1E; color: #D4D4D4; }"
        "QMenuBar { background-color: #252526; color: #CCCCCC; border-bottom: 1px solid #333337; }"
        "QMenuBar::item:selected { background-color: #094771; color: #FFFFFF; }"
        "QMenu { background-color: #252526; color: #CCCCCC; border: 1px solid #454545; }"
        "QMenu::item:selected { background-color: #094771; color: #FFFFFF; }"
        "QToolBar { background-color: #252526; border-bottom: 1px solid #333337; spacing: 4px; padding: 3px; }"
        "QToolButton { background-color: transparent; border: 1px solid transparent; border-radius: 4px; padding: 4px; }"
        "QToolButton:hover { background-color: #2A2D2E; border: 1px solid #3C3C3C; }"
        "QToolButton:pressed { background-color: #094771; }"
        "QTabWidget::pane { border: 1px solid #333337; background-color: #1E1E1E; }"
        "QTabBar::tab { background-color: #2D2D2D; color: #969696; padding: 6px 14px; border: 1px solid #252526; border-bottom: none; }"
        "QTabBar::tab:selected { background-color: #1E1E1E; color: #FFFFFF; border-top: 2px solid #007ACC; font-weight: bold; }"
        "QTabBar::tab:hover:!selected { background-color: #2A2D2E; color: #CCCCCC; }"
        "QGroupBox { border: 1px solid #3C3C3C; border-radius: 4px; margin-top: 8px; padding-top: 6px; font-weight: bold; color: #9CDCFE; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }"
        "QLineEdit { background-color: #3C3C3C; color: #F0F0F0; border: 1px solid #555555; border-radius: 3px; padding: 2px 4px; selection-background-color: #264F78; }"
        "QLineEdit:focus { border: 1px solid #007ACC; background-color: #252526; }"
        "QPushButton { background-color: #333337; color: #E0E0E0; border: 1px solid #555555; border-radius: 3px; padding: 4px 10px; }"
        "QPushButton:hover { background-color: #094771; border-color: #007ACC; color: #FFFFFF; }"
        "QPushButton:pressed { background-color: #0E639C; }"
        "QComboBox { background-color: #3C3C3C; color: #F0F0F0; border: 1px solid #555555; border-radius: 3px; padding: 2px 6px; }"
        "QListWidget, QPlainTextEdit, QTextBrowser { background-color: #1E1E1E; color: #D4D4D4; border: 1px solid #333337; selection-background-color: #264F78; }"
        "QSplitter::handle { background-color: #333337; }"
        "QStatusBar { background-color: #007ACC; color: #FFFFFF; font-weight: bold; }"
        "QStatusBar QLabel { background-color: transparent; color: #FFFFFF; padding: 0 6px; }"
    ));

    updater = new Updater(this);
    simManager = new SimulatorManager(this);

    setupUi();
    createActions();
    createMenus();
    createToolBars();

    connect(simManager, &SimulatorManager::stateChanged, this, &MainWindow::onSimulatorStateChanged);
    connect(simManager, &SimulatorManager::programLoadedChanged, this, [this](uint32_t startAddr) {
        if (memoryView) {
            memoryView->setProgramStartAddress(startAddr);
        }
    });
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

    // Memory edits immediately notify hardware, stack, and variables views
    connect(memoryView, &MemoryView::memoryModified, this, [this](uint32_t /* addr */) {
        if (hardwareWidget) hardwareWidget->updateHardware();
        if (stackView) stackView->updateView();
        if (variablesView) variablesView->updateVariables();
    });

    if (variablesView) {
        connect(variablesView, &VariablesView::memoryModified, this, [this](uint32_t /* addr */) {
            if (hardwareWidget) hardwareWidget->updateHardware();
            if (stackView) stackView->updateView();
            if (memoryView) memoryView->updateView();
        });
    }

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
    logHeader->setStyleSheet("background-color: #252526; color: #CCCCCC; padding: 3px 6px; border-bottom: 1px solid #3C3C3C;");
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

    variablesView = new VariablesView(simRightTabs);
    simRightTabs->addTab(variablesView, tr("Variables & Watch"));
    variablesView->setSimulatorManager(simManager);
    variablesView->setMemoryView(memoryView);

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
    newAct = new QAction(createModernToolbarIcon("new"), tr("&New"), this);
    newAct->setShortcuts(QKeySequence::New);
    connect(newAct, &QAction::triggered, this, &MainWindow::newFile);

    openAct = new QAction(createModernToolbarIcon("open"), tr("&Open..."), this);
    openAct->setShortcuts(QKeySequence::Open);
    connect(openAct, &QAction::triggered, this, &MainWindow::openFile);

    saveAct = new QAction(createModernToolbarIcon("save"), tr("&Save"), this);
    saveAct->setShortcuts(QKeySequence::Save);
    connect(saveAct, &QAction::triggered, this, &MainWindow::saveFile);

    saveAsAct = new QAction(tr("Save &As..."), this);
    saveAsAct->setShortcuts(QKeySequence::SaveAs);
    connect(saveAsAct, &QAction::triggered, this, &MainWindow::saveFileAs);

    exitAct = new QAction(tr("E&xit"), this);
    exitAct->setShortcuts(QKeySequence::Quit);
    connect(exitAct, &QAction::triggered, this, &QWidget::close);

    // Edit Actions (Edit68K)
    undoAct = new QAction(createModernToolbarIcon("undo"), tr("&Undo"), this);
    undoAct->setShortcuts(QKeySequence::Undo);
    connect(undoAct, &QAction::triggered, editor, &QPlainTextEdit::undo);

    redoAct = new QAction(createModernToolbarIcon("redo"), tr("&Redo"), this);
    redoAct->setShortcuts(QKeySequence::Redo);
    connect(redoAct, &QAction::triggered, editor, &QPlainTextEdit::redo);

    cutAct = new QAction(createModernToolbarIcon("cut"), tr("Cu&t"), this);
    cutAct->setShortcuts(QKeySequence::Cut);
    connect(cutAct, &QAction::triggered, editor, &QPlainTextEdit::cut);

    copyAct = new QAction(createModernToolbarIcon("copy"), tr("&Copy"), this);
    copyAct->setShortcuts(QKeySequence::Copy);
    connect(copyAct, &QAction::triggered, editor, &QPlainTextEdit::copy);

    pasteAct = new QAction(createModernToolbarIcon("paste"), tr("&Paste"), this);
    pasteAct->setShortcuts(QKeySequence::Paste);
    connect(pasteAct, &QAction::triggered, editor, &QPlainTextEdit::paste);

    commentAct = new QAction(createModernToolbarIcon("comment"), tr("Comment Lines (+*)"), this);
    connect(commentAct, &QAction::triggered, this, &MainWindow::onCommentLines);

    uncommentAct = new QAction(createModernToolbarIcon("uncomment"), tr("Uncomment Lines (-*)"), this);
    connect(uncommentAct, &QAction::triggered, this, &MainWindow::onUncommentLines);

    insertTemplateAct = new QAction(tr("Insert 68000 &Template"), this);
    connect(insertTemplateAct, &QAction::triggered, this, &MainWindow::onInsertTemplate);

    // Project Actions
    assembleAct = new QAction(createModernToolbarIcon("assemble"), tr("&Assemble"), this);
    assembleAct->setShortcut(QKeySequence(Qt::Key_F11));
    connect(assembleAct, &QAction::triggered, this, [this]() { assembleCurrentFile(false); });

    assembleAndLoadAct = new QAction(createModernToolbarIcon("assemble"), tr("Assemble && &Load in Simulator"), this);
    assembleAndLoadAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F8));
    connect(assembleAndLoadAct, &QAction::triggered, this, [this]() { assembleCurrentFile(true); });

    // Sim68K Run Actions
    runAct = new QAction(createModernToolbarIcon("run"), tr("&Run"), this);
    runAct->setShortcut(QKeySequence(Qt::Key_F9));
    connect(runAct, &QAction::triggered, this, &MainWindow::onSimulatorRun);

    runToCursorAct = new QAction(createModernToolbarIcon("runto"), tr("Run &To Cursor"), this);
    runToCursorAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F9));
    connect(runToCursorAct, &QAction::triggered, this, &MainWindow::onSimulatorRunToCursor);

    autoTraceAct = new QAction(createModernToolbarIcon("autotrace"), tr("&Auto Trace"), this);
    autoTraceAct->setShortcut(QKeySequence(Qt::Key_F10));
    connect(autoTraceAct, &QAction::triggered, this, &MainWindow::onSimulatorAutoTrace);

    stepOverAct = new QAction(createModernToolbarIcon("stepover"), tr("&Step Over"), this);
    stepOverAct->setShortcut(QKeySequence(Qt::Key_F8));
    connect(stepOverAct, &QAction::triggered, this, &MainWindow::onSimulatorStepOver);

    traceIntoAct = new QAction(createModernToolbarIcon("traceinto"), tr("&Trace Into"), this);
    traceIntoAct->setShortcut(QKeySequence(Qt::Key_F7));
    connect(traceIntoAct, &QAction::triggered, this, &MainWindow::onSimulatorTraceInto);

    pauseAct = new QAction(createModernToolbarIcon("pause"), tr("&Pause"), this);
    pauseAct->setShortcut(QKeySequence(Qt::Key_F6));
    connect(pauseAct, &QAction::triggered, this, &MainWindow::onSimulatorPause);

    rewindAct = new QAction(createModernToolbarIcon("reset"), tr("Re&wind Program"), this);
    rewindAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F2));
    connect(rewindAct, &QAction::triggered, this, &MainWindow::onSimulatorRewind);

    reloadAct = new QAction(createModernToolbarIcon("reload"), tr("Re&load Program"), this);
    reloadAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F3));
    connect(reloadAct, &QAction::triggered, this, &MainWindow::onSimulatorReload);

    resetAct = new QAction(createModernToolbarIcon("reset"), tr("&Reset"), this);
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
    findAct = new QAction(createModernToolbarIcon("find"), tr("&Find..."), this);
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

    viewVariablesAct = new QAction(tr("&Variables && Watch"), this);
    connect(viewVariablesAct, &QAction::triggered, this, [this]() {
        mainTabs->setCurrentIndex(1);
        simRightTabs->setCurrentWidget(variablesView);
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
    viewMenu->addAction(viewVariablesAct);
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
    mainToolBar->setIconSize(QSize(22, 22));
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

    int errorCount = 0;
    int warningCount = 0;
    QStringList diagnosticDetails;
    int firstErrorLine = -1;

    QStringList lines = fullOutput.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        auto *item = new QListWidgetItem(line, buildOutputList);
        QString trimmedLine = line.trimmed();

        bool isSummaryLine = trimmedLine.startsWith("Assembly failed with", Qt::CaseInsensitive) ||
                             trimmedLine.startsWith("Assembly completed successfully", Qt::CaseInsensitive);

        if (!isSummaryLine) {
            if (trimmedLine.contains(": ERROR:", Qt::CaseInsensitive) || trimmedLine.contains("ERROR:")) {
                errorCount++;
                item->setForeground(QColor(220, 38, 38));
                diagnosticDetails.append(trimmedLine);
            } else if (trimmedLine.contains(": WARNING:", Qt::CaseInsensitive) || trimmedLine.contains("WARNING:")) {
                warningCount++;
                item->setForeground(QColor(217, 119, 6));
                diagnosticDetails.append(trimmedLine);
            }
        }

        static QRegularExpression reLine(QStringLiteral("[:\\(](\\d+)[:\\)]"));
        auto match = reLine.match(trimmedLine);
        if (match.hasMatch()) {
            int lineNum = match.captured(1).toInt();
            item->setData(Qt::UserRole, lineNum);
            if (firstErrorLine < 0 && trimmedLine.contains("ERROR:", Qt::CaseInsensitive)) {
                firstErrorLine = lineNum;
            }
        }
    }

    if (exitCode != 0 && errorCount == 0) {
        errorCount = 1;
    }

    if (exitCode == 0) {
        statusBar()->showMessage(tr("Assembly successful! 0 errors."), 4000);
        auto *okItem = new QListWidgetItem(tr("Assembly complete with 0 errors."), buildOutputList);
        okItem->setForeground(QColor(22, 163, 74));

        if (pendingAutoLoad) {
            mainTabs->setCurrentIndex(1); // Switch to Simulator tab
            simManager->loadS68(currentS68Path);
        }

        // Apply *[sim68k] listing directives after loading the S-Record so
        // they are not reset by initSim().
        loadListing(currentListingPath);

        if (!pendingAutoLoad) {
            showAssemblerStatusDialog(0, warningCount, diagnosticDetails);
        }
    } else {
        statusBar()->showMessage(tr("Assembly failed! See Build Output."), 4000);
        if (firstErrorLine > 0 && editor) {
            editor->highlightLine(firstErrorLine);
        }
        if (QFileInfo::exists(currentListingPath)) {
            loadListing(currentListingPath);
        }
        if (!pendingAutoLoad) {
            showAssemblerStatusDialog(errorCount, warningCount, diagnosticDetails);
        }
    }

    pendingAutoLoad = false;
}

void MainWindow::showAssemblerStatusDialog(int errors, int warnings, const QStringList &details)
{
    if (QGuiApplication::platformName() == QStringLiteral("offscreen")) {
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Assembler Status"));
    dlg.setMinimumWidth(details.isEmpty() ? 320 : 520);

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

    if (!details.isEmpty()) {
        auto *detailEdit = new QTextEdit(&dlg);
        detailEdit->setReadOnly(true);
        detailEdit->setPlainText(details.join(QLatin1Char('\n')));
        detailEdit->setMinimumHeight(120);
        detailEdit->setMaximumHeight(220);
        detailEdit->setStyleSheet("background-color: #1E1E1E; color: #D4D4D4; border: 1px solid #3C3C3C; font-family: Monospace; font-size: 9pt;");
        layout->addWidget(detailEdit);
    }

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
        // loadS68() resets simulator defaults; re-apply *[sim68k] directives.
        loadListing(currentListingPath);
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
    if (variablesView) {
        variablesView->applyPersistentInitialValues();
    }
    if (!currentListingPath.isEmpty()) {
        loadListing(currentListingPath); // re-apply *[sim68k] directives
    }
}

void MainWindow::onSimulatorRewind()
{
    mainTabs->setCurrentIndex(1);
    simManager->rewindProgram();
    if (hardwareWidget) {
        hardwareWidget->resetHardware();
    }
    if (variablesView) {
        variablesView->applyPersistentInitialValues();
    }
}

void MainWindow::onSimulatorReload()
{
    mainTabs->setCurrentIndex(1);
    simManager->reloadProgram();
    if (hardwareWidget) {
        hardwareWidget->resetHardware();
    }
    if (variablesView) {
        variablesView->applyPersistentInitialValues();
    }
    if (!currentListingPath.isEmpty()) {
        loadListing(currentListingPath); // re-apply *[sim68k] directives
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
        simManager->loadS68(fileName);
        // Load the listing after the S-Record: loadS68() calls initSim() which
        // restores default simulator options, so *[sim68k] directives must be
        // applied last or they would be lost.
        if (QFileInfo::exists(listCandidate)) {
            loadListing(listCandidate);
        } else {
            listingLines.clear();
            listingWidget->clear();
        }
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

    // Parse symbols and variables from listing
    QMap<QString, uint32_t> symbols;
    QList<VariableEntry> discoveredVars;

    QString pendingLabel;
    uint32_t pendingLabelAddr = 0;

    static const QRegularExpression equRe(QStringLiteral("^([A-Za-z_][A-Za-z0-9_]*)\\s+EQU"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression labelRe(QStringLiteral("^([A-Za-z_][A-Za-z0-9_]*):?(.*)$"));
    static const QRegularExpression varOpRe(QStringLiteral("^(DS|DC|DCB)(?:\\.([BWLbwl]))?\\s+(.*)$"), QRegularExpression::CaseInsensitiveOption);

    for (const QString &line : listingLines) {
        if (line.length() < 35) continue;

        // Check for EQU lines: e.g. "00000000  =00005000                  2  MY_CONST EQU $5000"
        int eqIdx = line.indexOf('=');
        if (eqIdx >= 8 && eqIdx <= 15) {
            bool okVal = false;
            uint32_t equVal = line.mid(eqIdx + 1, 8).trimmed().toUInt(&okVal, 16);
            if (okVal && line.length() > 40) {
                QString src = line.mid(40).trimmed();
                auto match = equRe.match(src);
                if (match.hasMatch()) {
                    QString sym = match.captured(1).toUpper();
                    symbols[sym] = equVal;
                    continue;
                }
            }
        }

        // Check if first 8 chars are a valid hex address
        bool okAddr = false;
        uint32_t addr = line.left(8).trimmed().toUInt(&okAddr, 16);
        if (!okAddr) continue;

        if (line.length() <= 40) continue;
        QString src = line.mid(40);
        if (src.trimmed().isEmpty()) continue;

        bool col1HasToken = !line.at(40).isSpace();
        QString trimmedSrc = src.trimmed();
        if (trimmedSrc.startsWith('*') || trimmedSrc.startsWith(';')) continue;

        QString lineLabel;
        QString restOfLine;
        if (col1HasToken) {
            auto lMatch = labelRe.match(trimmedSrc);
            if (lMatch.hasMatch()) {
                lineLabel = lMatch.captured(1);
                restOfLine = lMatch.captured(2).trimmed();
                symbols[lineLabel.toUpper()] = addr;
            }
        }

        QString activeLabel;
        uint32_t varAddr = addr;
        QString toMatch = col1HasToken ? restOfLine : trimmedSrc;

        if (!lineLabel.isEmpty() && !restOfLine.isEmpty()) {
            activeLabel = lineLabel;
        } else if (!pendingLabel.isEmpty() && (!col1HasToken || lineLabel.isEmpty())) {
            activeLabel = pendingLabel;
            varAddr = pendingLabelAddr;
        }

        auto varMatch = varOpRe.match(toMatch);
        if (varMatch.hasMatch() && !activeLabel.isEmpty()) {
            QString op = varMatch.captured(1).toUpper();
            QString sizeChar = varMatch.captured(2).toUpper();
            QString operands = varMatch.captured(3).trimmed();

            int byteSize = 2; // Default .W for 68000
            if (sizeChar == "B") byteSize = 1;
            else if (sizeChar == "L") byteSize = 4;
            else if (sizeChar == "W") byteSize = 2;

            int count = 1;
            if (op == "DS") {
                bool okCount = false;
                int c = operands.toInt(&okCount);
                if (okCount && c > 0) count = c;
            }

            VariableEntry entry;
            entry.name = activeLabel;
            entry.address = varAddr;
            entry.byteSize = byteSize;
            entry.elementCount = count;
            entry.isCustomWatch = false;

            discoveredVars.append(entry);
            pendingLabel.clear();
        } else {
            if (col1HasToken && !lineLabel.isEmpty() && restOfLine.isEmpty()) {
                pendingLabel = lineLabel;
                pendingLabelAddr = addr;
            } else {
                pendingLabel.clear();
            }
        }
    }

    if (memoryView) {
        memoryView->setSymbolTable(symbols);
    }
    if (variablesView) {
        variablesView->setDiscoveredVariables(discoveredVars);
    }

    // Loading the listing can happen after loadS68() (so *[sim68k] directives
    // survive initSim()); restore the PC highlight for the current position.
    listingWidget->highlightPC(static_cast<uint32_t>(simManager->getPC()), false);
}

void MainWindow::onSimulatorStateChanged()
{
    registersView->updateRegisters(simManager->getD(), simManager->getA(),
                                   simManager->getPC(), simManager->getSR(),
                                   simManager->getCycles());
    memoryView->updateView();
    if (variablesView) {
        variablesView->updateVariables();
    }
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
