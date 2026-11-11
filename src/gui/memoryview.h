#ifndef MEMORYVIEW_H
#define MEMORYVIEW_H

#include <QWidget>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <cstdint>

class MemoryView : public QWidget
{
    Q_OBJECT

public:
    explicit MemoryView(QWidget *parent = nullptr);

    void setMemoryPointer(const char *mem, uint32_t memSize);
    void updateView();
    void setBaseAddress(uint32_t addr);

private slots:
    void onGotoAddress();
    void onPrevPage();
    void onNextPage();

private:
    const char *memoryPtr = nullptr;
    uint32_t memorySize = 0;
    uint32_t currentAddress = 0x00001000;
    const uint32_t bytesPerPage = 512; // 32 rows of 16 bytes

    QLineEdit *addrInput;
    QPushButton *gotoBtn;
    QPushButton *prevBtn;
    QPushButton *nextBtn;
    QPlainTextEdit *hexDisplay;

    void setupUi();
    void renderMemory();
};

#endif // MEMORYVIEW_H
