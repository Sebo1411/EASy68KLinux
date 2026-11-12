#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTabWidget>
#include <QPlainTextEdit>
#include <QListWidget>
#include <QString>
#include <QStringList>
#include <QProcess>
#include <QVector>

class CodeEditor;
class SyntaxHighlighter;
class FindReplaceBar;
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
class QCloseEvent;
class QLabel;
class QMenu;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    static QIcon createAppWindowIcon();
    static QIcon createModernToolbarIcon(const QString &name);

    CodeEditor* getEditor() const { return editor; }
    QTabWidget* getEditorTabs() const { return editorTabs; }
    FindReplaceBar* getFindReplaceBar() const { return findReplaceBar; }
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
    int getAutoTraceIntervalMs() const { return m_autoTraceIntervalMs; }

public slots:
    void newFile();
    void newTab();
    bool closeEditorTab(int index = -1);
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
    void onReplaceText();
    void onFindNext();
    void onFindPrev();
    void onCommentLines();
    void onUncommentLines();
    void onInsertTemplate();
    void onAdvancedBreakpoints();
    void onClearRecentFiles();
    void openRecentFile();

    void about();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    struct EditorTabState {
        CodeEditor *editor = nullptr;
        SyntaxHighlighter *highlighter = nullptr;
        QString filePath;
        QString s68Path;
        QString listingPath;
    };

    void setupUi();
    void createActions();
    void createMenus();
    void createToolBars();
    void showAssemblerStatusDialog(int errors, int warnings, const QStringList &details = {});
    bool maybeSave();
    bool maybeSaveTab(int index);
    bool maybeSaveAll();
    bool saveEditorTab(int index);
    bool saveEditorTabAs(int index);
    void loadFile(const QString &fileName);
    void setCurrentFile(const QString &fileName);
    void loadListing(const QString &listingPath);
    int createNewEditorTab(const QString &fileName = QString(), const QString &content = QString());
    int findTabByFilePath(const QString &fileName) const;
    void syncActiveEditorTab(int index);
    void updateTabTitle(int index);
    void updateCursorPositionLabel();
    void adjustEditorFontSize(int deltaOrReset);
    void addRecentFile(const QString &fileName);
    void updateRecentFilesMenu();
    void refreshSimRightTab(QWidget *tab);
    void loadSettings();
    void saveSettings();

    // Widgets
    QTabWidget *mainTabs = nullptr;
    QTabWidget *editorTabs = nullptr;
    QVector<EditorTabState> m_editorTabs;
    CodeEditor *editor = nullptr;
    SyntaxHighlighter *highlighter = nullptr;
    FindReplaceBar *findReplaceBar = nullptr;
    QListWidget *buildOutputList = nullptr;
    QLabel *cursorPosLabel = nullptr;

    RegistersView *registersView = nullptr;
    ListingWidget *listingWidget = nullptr;
    QPlainTextEdit *executionLogView = nullptr;
    QTabWidget *simRightTabs = nullptr;
    MemoryView *memoryView = nullptr;
    VariablesView *variablesView = nullptr;
    StackView *stackView = nullptr;
    HardwareWidget *hardwareWidget = nullptr;
    TerminalWidget *terminalWidget = nullptr;
    EasyBinWidget *easyBinWidget = nullptr;
    HelpWidget *helpWidget = nullptr;

    SimulatorManager *simManager = nullptr;
    Updater *updater = nullptr;
    QProcess *assemblerProcess = nullptr;

    QString currentFilePath;
    QString currentS68Path;
    QString currentListingPath;
    QStringList listingLines;
    QStringList m_recentFiles;
    static constexpr int MaxRecentFiles = 10;
    int m_assemblingTabIndex = -1;
    bool pendingAutoLoad = false;

    // Options & refresh state
    bool m_bitfieldEnabled = false;
    bool m_xrefEnabled = false;
    bool m_exceptionsEnabled = false;
    bool m_simhaltDefault = true;
    int m_autoTraceIntervalMs = 50;
    int m_editorFontSize = 10;
    bool m_dirtyMemory = false;
    bool m_dirtyVariables = false;
    bool m_dirtyStack = false;
    bool m_dirtyHardware = false;

    // Recent files menu & actions
    QMenu *recentFilesMenu = nullptr;
    QAction *recentFileActs[MaxRecentFiles] = {};
    QAction *clearRecentAct = nullptr;

    // Actions
    QAction *newAct = nullptr;
    QAction *newTabAct = nullptr;
    QAction *closeTabAct = nullptr;
    QAction *openAct = nullptr;
    QAction *saveAct = nullptr;
    QAction *saveAsAct = nullptr;
    QAction *exitAct = nullptr;

    QAction *undoAct = nullptr;
    QAction *redoAct = nullptr;
    QAction *cutAct = nullptr;
    QAction *copyAct = nullptr;
    QAction *pasteAct = nullptr;
    QAction *commentAct = nullptr;
    QAction *uncommentAct = nullptr;
    QAction *insertTemplateAct = nullptr;

    QAction *assembleAct = nullptr;
    QAction *assembleAndLoadAct = nullptr;

    // Sim68K Run Actions
    QAction *runAct = nullptr;
    QAction *runToCursorAct = nullptr;
    QAction *autoTraceAct = nullptr;
    QAction *stepOverAct = nullptr;
    QAction *traceIntoAct = nullptr;
    QAction *pauseAct = nullptr;
    QAction *rewindAct = nullptr;
    QAction *reloadAct = nullptr;
    QAction *resetAct = nullptr;
    QAction *loadS68Act = nullptr;
    QAction *toggleBpAct = nullptr;
    QAction *clearBpsAct = nullptr;
    QAction *advancedBpsAct = nullptr;

    // Search Actions
    QAction *findAct = nullptr;
    QAction *replaceAct = nullptr;
    QAction *findNextAct = nullptr;
    QAction *findPrevAct = nullptr;
    QAction *goToPcAct = nullptr;

    // View Actions
    QAction *viewOutputAct = nullptr;
    QAction *viewHwAct = nullptr;
    QAction *viewMemoryAct = nullptr;
    QAction *viewVariablesAct = nullptr;
    QAction *viewStackAct = nullptr;
    QAction *viewEasyBinAct = nullptr;
    QAction *viewHelpAct = nullptr;

    // Options Actions
    QAction *exceptionsOptAct = nullptr;
    QAction *bitfieldOptAct = nullptr;
    QAction *xrefOptAct = nullptr;
    QAction *simhaltOptAct = nullptr;
    QAction *autoTraceIntervalAct = nullptr;
    QAction *fontIncreaseAct = nullptr;
    QAction *fontDecreaseAct = nullptr;
    QAction *fontResetAct = nullptr;

    // Help & Updates Actions
    QAction *aboutAct = nullptr;
    QAction *helpDocAct = nullptr;
    QAction *helpHwAct = nullptr;
    QAction *helpPdfAct = nullptr;
    QAction *checkUpdatesAct = nullptr;
    QAction *autoUpdateCheckAct = nullptr;
    QAction *repoSetupAct = nullptr;
};

#endif // MAINWINDOW_H
