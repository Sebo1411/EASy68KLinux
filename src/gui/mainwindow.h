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
class ListingWidget;
class RegistersView;
class MemoryView;
class StackView;
class TerminalWidget;
class SimulatorManager;
class HardwareWidget;
class EasyBinWidget;
class HelpWidget;
class VariablesView;
class Updater;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    static QIcon createAppWindowIcon();
    static QIcon createModernToolbarIcon(const QString &name);

    ListingWidget* getListingWidget() const { return listingWidget; }
    QPlainTextEdit* getExecutionLogView() const { return executionLogView; }
    RegistersView* getRegistersView() const { return registersView; }
    MemoryView* getMemoryView() const { return memoryView; }
    VariablesView* getVariablesView() const { return variablesView; }
    StackView* getStackView() const { return stackView; }
    HardwareWidget* getHardwareWidget() const { return hardwareWidget; }
    EasyBinWidget* getEasyBinWidget() const { return easyBinWidget; }
    HelpWidget* getHelpWidget() const { return helpWidget; }
    TerminalWidget* getTerminalWidget() const { return terminalWidget; }
    SimulatorManager* getSimulatorManager() const { return simManager; }
    Updater* getUpdater() const { return updater; }

public slots:
    void newFile();
    void openFile();
    bool saveFile();
    bool saveFileAs();
    void assembleCurrentFile(bool autoLoad = false);
    void onAssemblerFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onBuildErrorClicked(QListWidgetItem *item);

    // Sim68K Execution Actions
    void onSimulatorTraceInto();
    void onSimulatorStepOver();
    void onSimulatorStep() { onSimulatorTraceInto(); }
    void onSimulatorRun();
    void onSimulatorRunToCursor();
    void onSimulatorAutoTrace();
    void onSimulatorPause();
    void onSimulatorReset();
    void onSimulatorRewind();
    void onSimulatorReload();
    void onSimulatorLoadS68();
    void onSimulatorStateChanged();
    void onSimulatorHalted(const QString &reason);
    void onSimulatorMessage(const QString &msg);
    void onGoToPC();
    void onFindText();
    void onCommentLines();
    void onUncommentLines();
    void onInsertTemplate();

    void about();

private:
    void setupUi();
    void createActions();
    void createMenus();
    void createToolBars();
    void showAssemblerStatusDialog(int errors, int warnings, const QStringList &details = {});
    bool maybeSave();
    void loadFile(const QString &fileName);
    void setCurrentFile(const QString &fileName);
    void loadListing(const QString &listingPath);

    // Widgets
    QTabWidget *mainTabs;
    CodeEditor *editor;
    SyntaxHighlighter *highlighter;
    QListWidget *buildOutputList;

    RegistersView *registersView;
    ListingWidget *listingWidget;
    QPlainTextEdit *executionLogView;
    QTabWidget *simRightTabs;
    MemoryView *memoryView;
    VariablesView *variablesView;
    StackView *stackView;
    HardwareWidget *hardwareWidget;
    TerminalWidget *terminalWidget;
    EasyBinWidget *easyBinWidget;
    HelpWidget *helpWidget;

    SimulatorManager *simManager;
    Updater *updater = nullptr;
    QProcess *assemblerProcess = nullptr;

    QString currentFilePath;
    QString currentS68Path;
    QString currentListingPath;
    QStringList listingLines;
    bool pendingAutoLoad = false;

    // Actions
    QAction *newAct;
    QAction *openAct;
    QAction *saveAct;
    QAction *saveAsAct;
    QAction *exitAct;

    QAction *undoAct;
    QAction *redoAct;
    QAction *cutAct;
    QAction *copyAct;
    QAction *pasteAct;
    QAction *commentAct;
    QAction *uncommentAct;
    QAction *insertTemplateAct;

    QAction *assembleAct;
    QAction *assembleAndLoadAct;

    // Sim68K Run Actions
    QAction *runAct;
    QAction *runToCursorAct;
    QAction *autoTraceAct;
    QAction *stepOverAct;
    QAction *traceIntoAct;
    QAction *pauseAct;
    QAction *rewindAct;
    QAction *reloadAct;
    QAction *resetAct;
    QAction *loadS68Act;
    QAction *toggleBpAct;
    QAction *clearBpsAct;

    // Search Actions
    QAction *findAct;
    QAction *goToPcAct;

    // View Actions
    QAction *viewOutputAct;
    QAction *viewHwAct;
    QAction *viewMemoryAct;
    QAction *viewVariablesAct;
    QAction *viewStackAct;
    QAction *viewEasyBinAct;
    QAction *viewHelpAct;

    // Help & Updates Actions
    QAction *aboutAct;
    QAction *helpDocAct;
    QAction *helpHwAct;
    QAction *helpPdfAct;
    QAction *checkUpdatesAct;
    QAction *autoUpdateCheckAct;
    QAction *repoSetupAct;
};

#endif // MAINWINDOW_H
