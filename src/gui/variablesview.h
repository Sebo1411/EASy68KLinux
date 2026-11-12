#ifndef VARIABLESVIEW_H
#define VARIABLESVIEW_H

#include <QWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QMap>
#include <QList>
#include <cstdint>

class SimulatorManager;
class MemoryView;

struct VariableEntry {
    QString name;
    uint32_t address = 0;
    int byteSize = 2; // 1 = BYTE (.B), 2 = WORD (.W), 4 = LONG (.L)
    int elementCount = 1;
    bool isCustomWatch = false;
    QString customExpr;
};

class VariablesView : public QWidget
{
    Q_OBJECT

public:
    explicit VariablesView(QWidget *parent = nullptr);
    ~VariablesView() override = default;

    void setSimulatorManager(SimulatorManager *sm);
    void setMemoryView(MemoryView *mv) { m_memoryView = mv; }

    // Automatic discovery from .L68 listing
    void setDiscoveredVariables(const QList<VariableEntry> &vars);
    const QList<VariableEntry>& getDiscoveredVariables() const { return m_variables; }

    // Persistent Initial Values API
    void setPersistentInitialValue(const QString &varName, const QString &valStr);
    QString getPersistentInitialValue(const QString &varName) const;
    void applyPersistentInitialValues();

    // Live refresh
    void updateVariables();
    void clear();

    // Custom watch API
    void addWatchExpression(const QString &expr, int byteSize = 2);
    void removeWatch(int index);

signals:
    void memoryModified(uint32_t addr);

private slots:
    void onAddWatchClicked();
    void onRemoveWatchClicked();
    void onApplyInitialsClicked();
    void onTableCellChanged(int row, int col);
    void onTableItemDoubleClicked(QTableWidgetItem *item);

private:
    void setupUi();
    void rebuildTable();
    uint32_t readMemoryValue(uint32_t addr, int sizeBytes) const;
    void writeMemoryValue(uint32_t addr, int sizeBytes, uint32_t val);
    bool parseValueString(const QString &str, int sizeBytes, uint32_t &outVal) const;

    SimulatorManager *m_simManager = nullptr;
    MemoryView *m_memoryView = nullptr;

    QTableWidget *m_table = nullptr;
    QLineEdit *m_watchInput = nullptr;
    QComboBox *m_sizeCombo = nullptr;
    QPushButton *m_addBtn = nullptr;
    QPushButton *m_removeBtn = nullptr;
    QCheckBox *m_autoRestoreCheck = nullptr;
    QPushButton *m_applyInitialsBtn = nullptr;

    QList<VariableEntry> m_variables;
    QMap<QString, QString> m_persistentInitialValues; // varName.toUpper() -> user input string (e.g. "$1234", "100")
    QMap<uint32_t, uint32_t> m_lastValues; // address -> last read value (for change diff highlighting)
    bool m_updatingTable = false;
};

#endif // VARIABLESVIEW_H
