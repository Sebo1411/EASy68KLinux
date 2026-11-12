#ifndef STACKVIEW_H
#define STACKVIEW_H

#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <cstdint>

class SimulatorManager;

class StackDisplayArea : public QWidget
{
    Q_OBJECT

public:
    explicit StackDisplayArea(QWidget *parent = nullptr);

    void setMemoryPointer(const char *mem, uint32_t memSize);
    void updateStack(uint32_t currentA7, uint32_t selectedRegAddr);
    void scrollByBytes(int delta);
    void setMidAddress(uint32_t addr);
    uint32_t getMidAddress() const { return midAddr; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    const char *memoryPtr = nullptr;
    uint32_t memorySize = 0;
    uint32_t midAddr = 0x01000000;
    uint32_t a7StackAddr = 0x01000000;
    uint32_t selectedRegAddr = 0x01000000;

    int rowHeight = 18;
    int charWidth = 8;
};

class StackView : public QWidget
{
    Q_OBJECT

public:
    explicit StackView(QWidget *parent = nullptr);

    void setSimulatorManager(SimulatorManager *sm);
    void setMemoryPointer(const char *mem, uint32_t memSize);
    void updateView();

    StackDisplayArea* displayArea() const { return displayAreaWidget; }

private slots:
    void onRegisterSelectionChanged(int index);
    void onUpClicked();
    void onDownClicked();

private:
    void setupUi();
    uint32_t getSelectedRegisterAddress() const;

    SimulatorManager *simManager = nullptr;

    QComboBox *whichStackCombo;
    QPushButton *upBtn;
    QPushButton *downBtn;
    StackDisplayArea *displayAreaWidget;
};

#endif // STACKVIEW_H
