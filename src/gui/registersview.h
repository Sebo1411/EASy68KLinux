#ifndef REGISTERSVIEW_H
#define REGISTERSVIEW_H

#include <QWidget>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QMouseEvent>
#include <cstdint>

class SimulatorManager;

class RegisterLineEdit : public QLineEdit
{
    Q_OBJECT

public:
    explicit RegisterLineEdit(QWidget *parent = nullptr);

signals:
    void doubleClicked();

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;
};

class RegistersView : public QWidget
{
    Q_OBJECT

public:
    explicit RegistersView(QWidget *parent = nullptr);

    void setSimulatorManager(SimulatorManager *sm);
    void updateRegisters(const long *D, const long *A, long pc, short sr, uint64_t cycles);

    // Getters for testing
    uint32_t getD(int idx) const;
    uint32_t getA(int idx) const;
    uint32_t getPC() const;
    uint16_t getSR() const;
    uint64_t getCycles() const;

signals:
    void addressDoubleClicked(uint32_t addr);
    void pcDoubleClicked(uint32_t pc);
    void registerChanged(const QString &regName, uint32_t val);

private slots:
    void onDRegisterEdited(int idx);
    void onARegisterEdited(int idx);
    void onUSRegisterEdited();
    void onSSRegisterEdited();
    void onPCRegisterEdited();
    void onSRRegisterEdited();
    void onClearCyclesClicked();

private:
    void setupUi();

    SimulatorManager *simManager = nullptr;

    RegisterLineEdit *dRegs[8];
    RegisterLineEdit *aRegs[8];
    RegisterLineEdit *pcEdit;
    RegisterLineEdit *srEdit;
    RegisterLineEdit *usEdit;
    RegisterLineEdit *ssEdit;
    QLineEdit *cyclesEdit;
    QPushButton *clearCyclesBtn;

    bool updatingUi = false;
};

#endif // REGISTERSVIEW_H
