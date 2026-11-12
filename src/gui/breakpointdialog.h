#ifndef BREAKPOINTDIALOG_H
#define BREAKPOINTDIALOG_H

#include <QDialog>
#include <QString>
#include <QVector>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTabWidget;
class QTableWidget;
class SimulatorManager;

class BreakpointDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BreakpointDialog(SimulatorManager *simManager = nullptr, QWidget *parent = nullptr);

    void reloadFromSimulator();

    // Parse textual condition expressions such as "R0 AND A1" or "(P0 || P1) && A0"
    // Returns true on success and populates infix/postfix token vectors for BPointExpr.
    static bool parseExpression(const QString &text,
                                QVector<int> &infixOut,
                                QVector<int> &postfixOut,
                                QString *errorOut = nullptr);

    // Helper for 1-click creation of a standalone BPointExpr referencing bpIndex (0..99).
    // Returns the allocated expression slot (0..MAX_BP_EXPR-1), or -1 if full.
    static int ensureSimpleExpressionForBPoint(int bpIndex, int hitCount = 1);

    static int findFreeBPointSlot(int startIdx, int endIdx);

signals:
    void breakpointsUpdated();

private slots:
    void onAddRegBreakpoint();
    void onRemoveRegBreakpoint();
    void onAddAddrWatchpoint();
    void onRemoveAddrWatchpoint();
    void onAddExpression();
    void onRemoveExpression();
    void onClearAll();

private:
    void setupUi();
    void refreshTables();
    void notifyChanged();

    static QString regTypeIdName(long typeId);
    static QString operatorName(int op);
    static QString sizeName(long sz);
    static QString rwName(int rw);
    static bool parseHexValue(const QString &text, long &outVal);

    SimulatorManager *m_simManager = nullptr;
    QTabWidget *m_tabs = nullptr;

    // Tab 1: PC/Reg breakpoints (indices 0..49)
    QTableWidget *m_regTable = nullptr;
    QComboBox *m_regTargetCombo = nullptr;
    QComboBox *m_regOpCombo = nullptr;
    QLineEdit *m_regValEdit = nullptr;
    QComboBox *m_regSizeCombo = nullptr;
    QSpinBox *m_regCountSpin = nullptr;
    QCheckBox *m_regAutoExprCheck = nullptr;

    // Tab 2: Memory watchpoints (indices 50..99)
    QTableWidget *m_addrTable = nullptr;
    QLineEdit *m_addrEdit = nullptr;
    QComboBox *m_addrRwCombo = nullptr;
    QComboBox *m_addrOpCombo = nullptr;
    QLineEdit *m_addrValEdit = nullptr;
    QComboBox *m_addrSizeCombo = nullptr;
    QSpinBox *m_addrCountSpin = nullptr;
    QCheckBox *m_addrAutoExprCheck = nullptr;

    // Tab 3: Expressions (bpExpressions[0..MAX_BP_EXPR-1])
    QTableWidget *m_exprTable = nullptr;
    QLineEdit *m_exprEdit = nullptr;
    QSpinBox *m_exprCountSpin = nullptr;
    QLabel *m_exprHelpLabel = nullptr;
};

#endif // BREAKPOINTDIALOG_H
