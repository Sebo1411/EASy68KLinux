#include "memoryview.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFont>
#include <QRegularExpression>

MemoryView::MemoryView(QWidget *parent) : QWidget(parent)
{
    setupUi();
}

void MemoryView::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    auto *topBar = new QHBoxLayout();
    topBar->addWidget(new QLabel(tr("Address ($):"), this));

    addrInput = new QLineEdit("00001000", this);
    addrInput->setMaximumWidth(120);
    QFont monoFont("Monospace", 9);
    monoFont.setStyleHint(QFont::Monospace);
    addrInput->setFont(monoFont);
    topBar->addWidget(addrInput);

    gotoBtn = new QPushButton(tr("Go"), this);
    topBar->addWidget(gotoBtn);

    prevBtn = new QPushButton(tr("<< Prev Page"), this);
    topBar->addWidget(prevBtn);

    nextBtn = new QPushButton(tr("Next Page >>"), this);
    topBar->addWidget(nextBtn);

    topBar->addStretch();
    layout->addLayout(topBar);

    hexDisplay = new QPlainTextEdit(this);
    hexDisplay->setReadOnly(true);
    hexDisplay->setFont(monoFont);
    hexDisplay->setLineWrapMode(QPlainTextEdit::NoWrap);
    layout->addWidget(hexDisplay);

    connect(gotoBtn, &QPushButton::clicked, this, &MemoryView::onGotoAddress);
    connect(addrInput, &QLineEdit::returnPressed, this, &MemoryView::onGotoAddress);
    connect(prevBtn, &QPushButton::clicked, this, &MemoryView::onPrevPage);
    connect(nextBtn, &QPushButton::clicked, this, &MemoryView::onNextPage);
}

void MemoryView::setMemoryPointer(const char *mem, uint32_t memSize)
{
    memoryPtr = mem;
    memorySize = memSize;
    renderMemory();
}

void MemoryView::setBaseAddress(uint32_t addr)
{
    currentAddress = addr & ~0x0F; // Align to 16 bytes
    addrInput->setText(QString("%1").arg(currentAddress, 8, 16, QLatin1Char('0')).toUpper());
    renderMemory();
}

void MemoryView::updateView()
{
    renderMemory();
}

void MemoryView::onGotoAddress()
{
    QString text = addrInput->text().trimmed();
    if (text.startsWith('$') || text.startsWith("0x", Qt::CaseInsensitive)) {
        text = text.mid(text.startsWith('$') ? 1 : 2);
    }
    bool ok = false;
    uint32_t addr = text.toUInt(&ok, 16);
    if (ok) {
        setBaseAddress(addr);
    }
}

void MemoryView::onPrevPage()
{
    if (currentAddress >= bytesPerPage) {
        setBaseAddress(currentAddress - bytesPerPage);
    } else {
        setBaseAddress(0);
    }
}

void MemoryView::onNextPage()
{
    if (currentAddress + bytesPerPage < memorySize) {
        setBaseAddress(currentAddress + bytesPerPage);
    }
}

void MemoryView::renderMemory()
{
    if (!memoryPtr || memorySize == 0) {
        hexDisplay->setPlainText(tr("Simulation memory not allocated."));
        return;
    }

    QString out;
    out.reserve(bytesPerPage * 5);

    uint32_t addr = currentAddress & ~0x0F;
    uint32_t rows = bytesPerPage / 16;

    for (uint32_t r = 0; r < rows; ++r) {
        uint32_t rowAddr = addr + r * 16;
        if (rowAddr >= memorySize) break;

        out += QString("%1: ").arg(rowAddr, 8, 16, QLatin1Char('0')).toUpper();

        // 16 hex bytes with a middle divider
        QString asciiPart;
        for (int i = 0; i < 16; ++i) {
            uint32_t byteAddr = rowAddr + i;
            if (byteAddr < memorySize) {
                unsigned char val = static_cast<unsigned char>(memoryPtr[byteAddr]);
                out += QString("%1 ").arg(val, 2, 16, QLatin1Char('0')).toUpper();
                if (val >= 32 && val <= 126) {
                    asciiPart.append(QChar(val));
                } else {
                    asciiPart.append('.');
                }
            } else {
                out += "   ";
                asciiPart.append(' ');
            }
            if (i == 7) out += " ";
        }

        out += " |" + asciiPart + "|\n";
    }

    hexDisplay->setPlainText(out);
}
