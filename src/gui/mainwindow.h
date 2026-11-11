#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTabWidget>
#include <QPlainTextEdit>
#include <QListWidget>
#include <QString>
#include <QProcess>

class CodeEditor;
class SyntaxHighlighter;
class RegistersView;
class MemoryView;
class TerminalWidget;
class SimulatorManager;
class HardwareWidget;
class EasyBinWidget;
class HelpWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    HardwareWidget* getHardwareWidget() const { return hardwareWidget; }
    EasyBinWidget* getEasyBinWidget() const { return easyBinWidget; }
    HelpWidget* getHelpWidget() const { return helpWidget; }
    TerminalWidget* getTerminalWidget() const { return terminalWidget; }
    SimulatorManager* getSimulatorManager() const { return simManager; }

private slots:
    void newFile();
    void openFile();
    bool saveFile();
    bool saveFileAs();
    void assembleCurrentFile(bool autoLoad = false);
    void onAssemblerFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onBuildErrorClicked(QListWidgetItem *item);

    void onSimulatorStep();
    void onSimulatorRun();
    void onSimulatorPause();
    void onSimulatorReset();
    void onSimulatorLoadS68();
    void onSimulatorStateChanged();
    void onSimulatorHalted(const QString &reason);
    void onSimulatorMessage(const QString &msg);

    void about();

private:
    void setupUi();
    void createActions();
    void createMenus();
    void createToolBars();
    void updateDisassemblyView();
    bool maybeSave();
    void loadFile(const QString &fileName);
    void setCurrentFile(const QString &fileName);

    // Widgets
    QTabWidget *mainTabs;
    CodeEditor *editor;
    SyntaxHighlighter *highlighter;
    QListWidget *buildOutputList;

    RegistersView *registersView;
    MemoryView *memoryView;
    HardwareWidget *hardwareWidget;
    TerminalWidget *terminalWidget;
    EasyBinWidget *easyBinWidget;
    HelpWidget *helpWidget;
    QPlainTextEdit *disassemblyView;

    SimulatorManager *simManager;
    QProcess *assemblerProcess = nullptr;

    QString currentFilePath;
    QString currentS68Path;
    QString currentListingPath;
    QStringList listingLines;
    bool pendingAutoLoad = false;

    void loadListing(const QString &listingPath);

    // Actions
    QAction *newAct;
    QAction *openAct;
    QAction *saveAct;
    QAction *saveAsAct;
    QAction *exitAct;

    QAction *assembleAct;
    QAction *assembleAndLoadAct;

    QAction *stepAct;
    QAction *runAct;
    QAction *pauseAct;
    QAction *resetAct;
    QAction *loadS68Act;
    QAction *toggleBpAct;
    QAction *clearBpsAct;

    QAction *aboutAct;
    QAction *helpDocAct;
    QAction *helpHwAct;
    QAction *helpPdfAct;
};

#endif // MAINWINDOW_H
