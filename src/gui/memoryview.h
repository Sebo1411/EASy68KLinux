#ifndef MEMORYVIEW_H
#define MEMORYVIEW_H

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QMap>
#include <QCompleter>
#include <cstdint>

enum class MemEditSubstate : uint8_t {
    HexHighNybble = 0,
    HexLowNybble = 1,
    AsciiByte = 2
};

struct MemoryCursorFSM {
    int row = 0;          // 0 .. visibleRows - 1
    int byteIndex = 0;    // 0 .. 15
    MemEditSubstate substate = MemEditSubstate::HexHighNybble;

    constexpr int toScreenColumn() const noexcept {
        return (substate == MemEditSubstate::AsciiByte)
            ? (58 + byteIndex)
            : (10 + byteIndex * 3 + static_cast<int>(substate));
    }

    static constexpr MemoryCursorFSM fromScreenCoordinates(int r, int col, int maxRows = 32) noexcept {
        MemoryCursorFSM fsm;
        int clampedRow = r < 0 ? 0 : (r >= maxRows ? maxRows - 1 : r);
        fsm.row = clampedRow;

        if (col >= 58) {
            int b = col - 58;
            if (b < 0) b = 0;
            if (b > 15) b = 15;
            fsm.byteIndex = b;
            fsm.substate = MemEditSubstate::AsciiByte;
        } else {
            int offset = col - 10;
            if (offset < 0) offset = 0;
            if (offset > 47) offset = 47;
            int b = offset / 3;
            if (b > 15) b = 15;
            fsm.byteIndex = b;
            int rem = offset % 3;
            if (rem == 0) {
                fsm.substate = MemEditSubstate::HexHighNybble;
            } else {
                fsm.substate = MemEditSubstate::HexLowNybble;
            }
        }
        return fsm;
    }

    constexpr void moveLeft(int maxRows, uint32_t& baseAddress) noexcept {
        (void)maxRows;
        if (substate == MemEditSubstate::AsciiByte) {
            if (byteIndex > 0) {
                byteIndex--;
            } else {
                if (row > 0) {
                    row--;
                    byteIndex = 15;
                } else if (baseAddress >= 16) {
                    baseAddress -= 16;
                    byteIndex = 15;
                } else {
                    substate = MemEditSubstate::HexLowNybble;
                    byteIndex = 15;
                }
            }
        } else if (substate == MemEditSubstate::HexLowNybble) {
            substate = MemEditSubstate::HexHighNybble;
        } else { // HexHighNybble
            if (byteIndex > 0) {
                byteIndex--;
                substate = MemEditSubstate::HexLowNybble;
            } else {
                if (row > 0) {
                    row--;
                    byteIndex = 15;
                    substate = MemEditSubstate::HexLowNybble;
                } else if (baseAddress >= 16) {
                    baseAddress -= 16;
                    byteIndex = 15;
                    substate = MemEditSubstate::HexLowNybble;
                }
            }
        }
    }

    constexpr void moveRight(int maxRows, uint32_t& baseAddress, uint32_t memorySize = 0x01000000) noexcept {
        if (substate == MemEditSubstate::AsciiByte) {
            if (byteIndex < 15) {
                byteIndex++;
            } else {
                byteIndex = 0;
                if (row + 1 < maxRows) {
                    row++;
                } else if (baseAddress + 16 < memorySize) {
                    baseAddress += 16;
                }
            }
        } else if (substate == MemEditSubstate::HexHighNybble) {
            substate = MemEditSubstate::HexLowNybble;
        } else { // HexLowNybble
            if (byteIndex < 15) {
                byteIndex++;
                substate = MemEditSubstate::HexHighNybble;
            } else {
                byteIndex = 0;
                substate = MemEditSubstate::HexHighNybble;
                if (row + 1 < maxRows) {
                    row++;
                } else if (baseAddress + 16 < memorySize) {
                    baseAddress += 16;
                }
            }
        }
    }

    constexpr void moveUp(uint32_t& baseAddress) noexcept {
        if (row > 0) {
            row--;
        } else if (baseAddress >= 16) {
            baseAddress -= 16;
        }
    }

    constexpr void moveDown(int maxRows, uint32_t& baseAddress, uint32_t memorySize = 0x01000000) noexcept {
        if (row + 1 < maxRows) {
            row++;
        } else if (baseAddress + 16 < memorySize) {
            baseAddress += 16;
        }
    }

    constexpr void toggleRegion() noexcept {
        if (substate == MemEditSubstate::AsciiByte) {
            substate = MemEditSubstate::HexHighNybble;
        } else {
            substate = MemEditSubstate::AsciiByte;
        }
    }
};

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
    int getCursorRow() const { return m_cursorFsm.row; }
    int getCursorCol() const { return m_cursorFsm.toScreenColumn(); }
    MemoryCursorFSM cursorFsm() const noexcept { return m_cursorFsm; }

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

    MemoryCursorFSM m_cursorFsm;
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
    void setProgramStartAddress(uint32_t startAddr);
    uint32_t getBaseAddress() const;
    QString getAddressInputText() const { return addrInput ? addrInput->text() : QString(); }
    void updateView();

    // Symbol table navigation & address resolution
    void setSymbolTable(const QMap<QString, uint32_t> &symbols);
    void clearSymbolTable();
    bool resolveAddressExpression(const QString &expr, uint32_t &outAddr) const;
    const QMap<QString, uint32_t>& getSymbolTable() const { return m_symbols; }

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
    QMap<QString, uint32_t> m_symbols;
    QCompleter *m_completer = nullptr;
};

#endif // MEMORYVIEW_H
