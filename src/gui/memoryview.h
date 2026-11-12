#ifndef MEMORYVIEW_H
#define MEMORYVIEW_H

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <cstdint>

class MemoryGridArea : public QWidget
{
    Q_OBJECT

public:
    explicit MemoryGridArea(QWidget *parent = nullptr);

    void setMemoryPointer(char *mem, uint32_t memSize);
    void setBaseAddress(uint32_t addr);
    uint32_t getBaseAddress() const { return currentAddress; }
    void refresh();

    // Direct programmatic edit for testing & automation
    bool handleKeyEdit(int key, QChar ch);
    void setCursorPosition(int r, int c);
    int getCursorRow() const { return cursorRow; }
    int getCursorCol() const { return cursorCol; }

signals:
    void memoryModified(uint32_t addr);
    void baseAddressChanged(uint32_t addr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void clampAndSnapCursor();
    int getVisibleRowCount() const;

    char *memoryPtr = nullptr;
    uint32_t memorySize = 0;
    uint32_t currentAddress = 0x00001000;

    int cursorRow = 0;
    int cursorCol = 10; // Start at first hex nybble
    bool cursorVisible = false;

    int rowHeight = 18;
    int charWidth = 9;

    static const int HEX_LEFT = 10;
    static const int HEX_RIGHT = 56;
    static const int ASC_LEFT = 58;
    static const int ASC_RIGHT = 73;
};

class MemoryView : public QWidget
{
    Q_OBJECT

public:
    explicit MemoryView(QWidget *parent = nullptr);

    void setMemoryPointer(char *mem, uint32_t memSize);
    void setBaseAddress(uint32_t addr);
    uint32_t getBaseAddress() const;
    void updateView();

    // Memory operations matching Sim68K Memory1.cpp
    bool copyMemory(uint32_t fromAddr, uint32_t toAddr, uint32_t byteCount);
    bool fillMemory(uint32_t fromAddr, uint32_t toAddr, uint32_t pattern, int patternSize);
    bool saveMemoryRange(uint32_t fromAddr, uint32_t toAddr, const QString &filePath);

    MemoryGridArea* gridArea() const { return grid; }

signals:
    void memoryModified(uint32_t addr);

public slots:
    void onGotoAddress();
    void onRowUp();
    void onRowDown();
    void onPageUp();
    void onPageDown();
    void onCopyClicked();
    void onFillClicked();
    void onSaveClicked();

private:
    void setupUi();

    char *memoryPtr = nullptr;
    uint32_t memorySize = 0;

    QLineEdit *addrInput;
    QLineEdit *fromInput;
    QLineEdit *toInput;
    QLineEdit *bytesInput;

    QPushButton *copyBtn;
    QPushButton *fillBtn;
    QPushButton *saveBtn;
    QPushButton *rowUpBtn;
    QPushButton *rowDownBtn;
    QPushButton *pageUpBtn;
    QPushButton *pageDownBtn;

    MemoryGridArea *grid;
};

#endif // MEMORYVIEW_H
