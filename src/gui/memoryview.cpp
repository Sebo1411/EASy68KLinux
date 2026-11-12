#include "memoryview.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QFontMetrics>
#include <QMessageBox>
#include <QFileDialog>
#include <QFile>
#include <algorithm>
#include <cstring>

// -----------------------------------------------------------------------------
// MemoryGridArea
// -----------------------------------------------------------------------------

MemoryGridArea::MemoryGridArea(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    QFont monoFont("Monospace", 9);
    monoFont.setStyleHint(QFont::Monospace);
    setFont(monoFont);

    QFontMetrics fm(monoFont);
    rowHeight = std::max(16, fm.lineSpacing());
    charWidth = std::max(7, fm.horizontalAdvance('0'));
}

void MemoryGridArea::setMemoryPointer(char *mem, uint32_t memSize)
{
    memoryPtr = mem;
    memorySize = memSize;
    update();
}

void MemoryGridArea::setBaseAddress(uint32_t addr)
{
    currentAddress = addr & ~0x0F;
    emit baseAddressChanged(currentAddress);
    update();
}

void MemoryGridArea::refresh()
{
    update();
}

int MemoryGridArea::getVisibleRowCount() const
{
    // Subtract 1 row for the header
    int rows = (height() / rowHeight) - 1;
    return std::max(1, rows);
}

void MemoryGridArea::clampAndSnapCursor()
{
    int numRows = getVisibleRowCount();
    cursorRow = std::clamp(cursorRow, 0, numRows - 1);

    if (cursorCol < HEX_LEFT) {
        cursorCol = HEX_LEFT;
    } else if (cursorCol <= HEX_RIGHT) {
        // In hex area: columns are (HEX_LEFT + b*3) and (HEX_LEFT + b*3 + 1), space at (HEX_LEFT + b*3 + 2)
        int offset = cursorCol - HEX_LEFT;
        if (offset % 3 == 2) {
            cursorCol++;
            if (cursorCol > HEX_RIGHT) {
                cursorCol = ASC_LEFT;
            }
        }
    } else if (cursorCol < ASC_LEFT) {
        cursorCol = ASC_LEFT;
    } else if (cursorCol > ASC_RIGHT) {
        cursorCol = ASC_RIGHT;
    }
}

void MemoryGridArea::setCursorPosition(int r, int c)
{
    cursorRow = r;
    cursorCol = c;
    cursorVisible = true;
    clampAndSnapCursor();
    update();
}

void MemoryGridArea::mousePressEvent(QMouseEvent *event)
{
    setFocus();
    cursorVisible = true;
    int r = (event->pos().y() / rowHeight) - 1;
    QFontMetrics fm(font());
    QString sampleRow(75, QLatin1Char('0'));
    int xRel = std::max(0, event->pos().x() - 4);
    int c = 0;
    for (int col = 0; col <= ASC_RIGHT; ++col) {
        int colStart = fm.horizontalAdvance(sampleRow.left(col));
        int colEnd = fm.horizontalAdvance(sampleRow.left(col + 1));
        if (xRel < colEnd) {
            c = col;
            break;
        }
        c = col;
        (void)colStart;
    }

    cursorRow = r;
    cursorCol = c;
    clampAndSnapCursor();
    update();
}

void MemoryGridArea::wheelEvent(QWheelEvent *event)
{
    if (event->angleDelta().y() > 0) {
        if (currentAddress >= 16) {
            setBaseAddress(currentAddress - 16);
        }
    } else if (event->angleDelta().y() < 0) {
        if (currentAddress + 16 < memorySize) {
            setBaseAddress(currentAddress + 16);
        }
    }
}

void MemoryGridArea::resizeEvent(QResizeEvent * /* event */)
{
    clampAndSnapCursor();
    update();
}

bool MemoryGridArea::handleKeyEdit(int key, QChar ch)
{
    if (!memoryPtr || memorySize == 0) return false;

    int numRows = getVisibleRowCount();

    // Arrow navigation
    if (key == Qt::Key_Up) {
        cursorRow--;
        if (cursorRow < 0) {
            cursorRow = 0;
            if (currentAddress >= 16) {
                setBaseAddress(currentAddress - 16);
            }
        }
        clampAndSnapCursor();
        update();
        return true;
    }
    if (key == Qt::Key_Down) {
        cursorRow++;
        if (cursorRow >= numRows) {
            cursorRow = numRows - 1;
            if (currentAddress + 16 < memorySize) {
                setBaseAddress(currentAddress + 16);
            }
        }
        clampAndSnapCursor();
        update();
        return true;
    }
    if (key == Qt::Key_Left || key == Qt::Key_Backspace) {
        if (cursorCol >= ASC_LEFT) {
            cursorCol--;
            if (cursorCol < ASC_LEFT) {
                if (cursorRow > 0) {
                    cursorRow--;
                    cursorCol = ASC_RIGHT;
                } else if (currentAddress >= 16) {
                    setBaseAddress(currentAddress - 16);
                    cursorCol = ASC_RIGHT;
                } else {
                    cursorCol = HEX_RIGHT;
                }
            }
        } else {
            cursorCol--;
            if (cursorCol >= HEX_LEFT && ((cursorCol - HEX_LEFT) % 3) == 2) {
                cursorCol--; // Skip space leftward to land on previous byte's low nybble
            }
            if (cursorCol < HEX_LEFT) {
                if (cursorRow > 0) {
                    cursorRow--;
                    cursorCol = HEX_RIGHT;
                } else if (currentAddress >= 16) {
                    setBaseAddress(currentAddress - 16);
                    cursorCol = HEX_RIGHT;
                } else {
                    cursorCol = HEX_LEFT;
                }
            }
        }
        clampAndSnapCursor();
        update();
        return true;
    }
    if (key == Qt::Key_Right) {
        if (cursorCol <= HEX_RIGHT) {
            cursorCol++;
            if (cursorCol <= HEX_RIGHT && ((cursorCol - HEX_LEFT) % 3) == 2) {
                cursorCol++; // Skip space rightward to land on next byte's high nybble
            }
            if (cursorCol > HEX_RIGHT) {
                cursorCol = HEX_LEFT;
                cursorRow++;
                if (cursorRow >= numRows) {
                    cursorRow = numRows - 1;
                    if (currentAddress + 16 < memorySize) {
                        setBaseAddress(currentAddress + 16);
                    }
                }
            }
        } else {
            cursorCol++;
            if (cursorCol > ASC_RIGHT) {
                cursorCol = ASC_LEFT;
                cursorRow++;
                if (cursorRow >= numRows) {
                    cursorRow = numRows - 1;
                    if (currentAddress + 16 < memorySize) {
                        setBaseAddress(currentAddress + 16);
                    }
                }
            }
        }
        clampAndSnapCursor();
        update();
        return true;
    }
    if (key == Qt::Key_Tab) {
        // Toggle between Hex and ASCII for the same byte
        if (cursorCol <= HEX_RIGHT) {
            int byteIdx = (cursorCol - HEX_LEFT) / 3;
            cursorCol = ASC_LEFT + byteIdx;
        } else {
            int byteIdx = cursorCol - ASC_LEFT;
            cursorCol = HEX_LEFT + byteIdx * 3;
        }
        clampAndSnapCursor();
        update();
        return true;
    }
    if (key == Qt::Key_PageUp) {
        if (currentAddress >= 256) {
            setBaseAddress(currentAddress - 256);
        } else {
            setBaseAddress(0);
        }
        return true;
    }
    if (key == Qt::Key_PageDown) {
        if (currentAddress + 256 < memorySize) {
            setBaseAddress(currentAddress + 256);
        }
        return true;
    }

    // Hex editing
    if (cursorCol <= HEX_RIGHT) {
        char c = ch.toUpper().toLatin1();
        uint8_t nybble = 0;
        if (c >= '0' && c <= '9') {
            nybble = static_cast<uint8_t>(c - '0');
        } else if (c >= 'A' && c <= 'F') {
            nybble = static_cast<uint8_t>(c - 'A' + 10);
        } else {
            return false;
        }

        int byteIndex = (cursorCol - HEX_LEFT) / 3;
        bool isLowNybble = ((cursorCol - HEX_LEFT) % 3) == 1;
        uint32_t addr = currentAddress + cursorRow * 16 + byteIndex;

        if (addr < memorySize) {
            uint8_t origByte = static_cast<uint8_t>(memoryPtr[addr]);
            if (isLowNybble) {
                origByte = (origByte & 0xF0) | nybble;
            } else {
                origByte = (origByte & 0x0F) | (nybble << 4);
            }
            memoryPtr[addr] = static_cast<char>(origByte);
            emit memoryModified(addr);

            // Advance cursor
            cursorCol++;
            if ((cursorCol - HEX_LEFT) % 3 == 2) {
                cursorCol++;
            }
            if (cursorCol > HEX_RIGHT) {
                cursorCol = HEX_LEFT;
                cursorRow++;
                if (cursorRow >= numRows) {
                    cursorRow = numRows - 1;
                    if (currentAddress + 16 < memorySize) {
                        setBaseAddress(currentAddress + 16);
                    }
                }
            }
            clampAndSnapCursor();
            update();
            return true;
        }
    }
    // ASCII editing
    else if (cursorCol >= ASC_LEFT && cursorCol <= ASC_RIGHT) {
        if (ch.isPrint()) {
            int byteIndex = cursorCol - ASC_LEFT;
            uint32_t addr = currentAddress + cursorRow * 16 + byteIndex;
            if (addr < memorySize) {
                memoryPtr[addr] = static_cast<char>(ch.toLatin1());
                emit memoryModified(addr);

                cursorCol++;
                if (cursorCol > ASC_RIGHT) {
                    cursorCol = ASC_LEFT;
                    cursorRow++;
                    if (cursorRow >= numRows) {
                        cursorRow = numRows - 1;
                        if (currentAddress + 16 < memorySize) {
                            setBaseAddress(currentAddress + 16);
                        }
                    }
                }
                clampAndSnapCursor();
                update();
                return true;
            }
        }
    }

    return false;
}

void MemoryGridArea::keyPressEvent(QKeyEvent *event)
{
    if (handleKeyEdit(event->key(), event->text().isEmpty() ? QChar() : event->text().at(0))) {
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void MemoryGridArea::paintEvent(QPaintEvent * /* event */)
{
    QPainter painter(this);
    painter.setFont(font());
    QFontMetrics fm = painter.fontMetrics();

    // VS Code Dark+ background (#1E1E1E)
    painter.fillRect(rect(), QColor(0x1E, 0x1E, 0x1E));

    // Row 0: Fixed Header (#252526)
    QString header = "Address   00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F  0123456789ABCDEF";
    painter.fillRect(0, 0, width(), rowHeight, QColor(0x25, 0x25, 0x26));
    painter.setPen(QColor(0x9C, 0xDC, 0xFE));
    painter.drawText(4, fm.ascent() + (rowHeight - fm.height()) / 2, header);
    painter.setPen(QColor(0x3C, 0x3C, 0x3C));
    painter.drawLine(0, rowHeight - 1, width(), rowHeight - 1);

    if (!memoryPtr || memorySize == 0) {
        painter.setPen(QColor(0x85, 0x85, 0x85));
        painter.drawText(rect(), Qt::AlignCenter, tr("Simulation memory not allocated."));
        return;
    }

    int numRows = getVisibleRowCount();

    for (int r = 0; r < numRows; ++r) {
        uint32_t rowAddr = currentAddress + r * 16;
        if (rowAddr >= memorySize) break;

        int y = (r + 1) * rowHeight;
        int baselineY = y + fm.ascent() + (rowHeight - fm.height()) / 2;

        // Subtle alternating row tint
        if (r % 2 == 1) {
            painter.fillRect(0, y, width(), rowHeight, QColor(0x22, 0x22, 0x23));
        }

        // Build formatted string for metrics
        QString rowText = QString("%1: ").arg(rowAddr, 8, 16, QLatin1Char('0')).toUpper();

        QString asciiPart;
        asciiPart.reserve(16);

        for (int b = 0; b < 16; ++b) {
            uint32_t byteAddr = rowAddr + b;
            if (byteAddr < memorySize) {
                uint8_t val = static_cast<uint8_t>(memoryPtr[byteAddr]);
                rowText += QString("%1 ").arg(val, 2, 16, QLatin1Char('0')).toUpper();
                if (val >= 32 && val <= 126) {
                    asciiPart.append(QChar(val));
                } else {
                    asciiPart.append('.');
                }
            } else {
                rowText += "   ";
                asciiPart.append(' ');
            }
        }
        rowText += " ";
        rowText += asciiPart;

        // Draw cursor cell highlight if on this row
        if (cursorVisible && r == cursorRow && cursorCol >= 0 && cursorCol < rowText.length()) {
            int cellX = 4 + fm.horizontalAdvance(rowText.left(cursorCol));
            int cellW = std::max(charWidth, fm.horizontalAdvance(rowText.mid(cursorCol, 1)));
            QRect cellRect(cellX, y, cellW, rowHeight);
            painter.fillRect(cellRect, hasFocus() ? QColor(0x09, 0x47, 0x71) : QColor(0x2A, 0x2D, 0x2E));
            painter.setPen(hasFocus() ? QColor(0x37, 0x94, 0xFF) : QColor(0x6E, 0x76, 0x81));
            painter.drawRect(cellRect.adjusted(0, 0, -1, -1));
        }

        // 1. Draw Address column (#569CD6 VS Code Blue)
        QString addrPrefix = rowText.left(10);
        painter.setPen(QColor(0x56, 0x9C, 0xD6));
        painter.drawText(4, baselineY, addrPrefix);

        // 2. Draw Hex bytes (non-zero bytes highlighted in #4EC9B0 teal, zero bytes in #D4D4D4)
        for (int b = 0; b < 16; ++b) {
            uint32_t byteAddr = rowAddr + b;
            if (byteAddr >= memorySize) break;
            uint8_t val = static_cast<uint8_t>(memoryPtr[byteAddr]);
            int colStart = HEX_LEFT + b * 3;
            int xByte = 4 + fm.horizontalAdvance(rowText.left(colStart));
            QString byteHex = rowText.mid(colStart, 2);

            bool isCursorOnByte = (cursorVisible && r == cursorRow &&
                                   (cursorCol == colStart || cursorCol == colStart + 1));
            if (isCursorOnByte) {
                painter.setPen(Qt::white);
            } else if (val != 0) {
                painter.setPen(QColor(0x4E, 0xC9, 0xB0));
            } else {
                painter.setPen(QColor(0x85, 0x85, 0x85));
            }
            painter.drawText(xByte, baselineY, byteHex);
        }

        // 3. Draw ASCII characters (#CE9178 for printable, #5A5A5A for dots)
        for (int b = 0; b < 16; ++b) {
            uint32_t byteAddr = rowAddr + b;
            if (byteAddr >= memorySize) break;
            uint8_t val = static_cast<uint8_t>(memoryPtr[byteAddr]);
            int colPos = ASC_LEFT + b;
            int xAsc = 4 + fm.horizontalAdvance(rowText.left(colPos));
            QString chStr = rowText.mid(colPos, 1);

            bool isCursorOnAsc = (cursorVisible && r == cursorRow && cursorCol == colPos);
            if (isCursorOnAsc) {
                painter.setPen(Qt::white);
            } else if (val >= 32 && val <= 126) {
                painter.setPen(QColor(0xCE, 0x91, 0x78));
            } else {
                painter.setPen(QColor(0x5A, 0x5A, 0x5A));
            }
            painter.drawText(xAsc, baselineY, chStr);
        }
    }
}

// -----------------------------------------------------------------------------
// MemoryView
// -----------------------------------------------------------------------------

MemoryView::MemoryView(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void MemoryView::setupUi()
{
    QFont monoFont("Monospace", 9);
    monoFont.setStyleHint(QFont::Monospace);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // Row 1: Address Navigation Bar (wide enough for full 8-digit hex address!)
    auto *navLayout = new QHBoxLayout();
    navLayout->setSpacing(6);

    navLayout->addWidget(new QLabel(tr("$ Address:"), this));
    addrInput = new QLineEdit("00001000", this);
    addrInput->setFont(monoFont);
    addrInput->setMaxLength(8);
    addrInput->setMinimumWidth(100);
    addrInput->setMaximumWidth(120);
    addrInput->setAlignment(Qt::AlignCenter);
    navLayout->addWidget(addrInput);

    auto *goBtn = new QPushButton(tr("Go"), this);
    navLayout->addWidget(goBtn);

    navLayout->addSpacing(8);

    rowUpBtn = new QPushButton(tr("Row ▲"), this);
    navLayout->addWidget(rowUpBtn);
    rowDownBtn = new QPushButton(tr("Row ▼"), this);
    navLayout->addWidget(rowDownBtn);

    pageUpBtn = new QPushButton(tr("Page ▲"), this);
    navLayout->addWidget(pageUpBtn);
    pageDownBtn = new QPushButton(tr("Page ▼"), this);
    navLayout->addWidget(pageDownBtn);

    navLayout->addStretch();
    mainLayout->addLayout(navLayout);

    // Row 2: Range Operations Bar (From / To / Bytes / Copy / Fill / Save)
    auto *rangeLayout = new QHBoxLayout();
    rangeLayout->setSpacing(6);

    rangeLayout->addWidget(new QLabel(tr("From: $"), this));
    fromInput = new QLineEdit("00001000", this);
    fromInput->setFont(monoFont);
    fromInput->setMaxLength(8);
    fromInput->setMinimumWidth(90);
    fromInput->setMaximumWidth(110);
    fromInput->setAlignment(Qt::AlignCenter);
    rangeLayout->addWidget(fromInput);

    rangeLayout->addWidget(new QLabel(tr("To: $"), this));
    toInput = new QLineEdit("0000101F", this);
    toInput->setFont(monoFont);
    toInput->setMaxLength(8);
    toInput->setMinimumWidth(90);
    toInput->setMaximumWidth(110);
    toInput->setAlignment(Qt::AlignCenter);
    rangeLayout->addWidget(toInput);

    rangeLayout->addWidget(new QLabel(tr("Bytes: $"), this));
    bytesInput = new QLineEdit("00", this);
    bytesInput->setFont(monoFont);
    bytesInput->setMaxLength(8);
    bytesInput->setMinimumWidth(80);
    bytesInput->setMaximumWidth(100);
    bytesInput->setAlignment(Qt::AlignCenter);
    rangeLayout->addWidget(bytesInput);

    copyBtn = new QPushButton(tr("Copy"), this);
    rangeLayout->addWidget(copyBtn);

    fillBtn = new QPushButton(tr("Fill"), this);
    rangeLayout->addWidget(fillBtn);

    saveBtn = new QPushButton(tr("Save"), this);
    rangeLayout->addWidget(saveBtn);

    rangeLayout->addStretch();
    mainLayout->addLayout(rangeLayout);

    // Central interactive memory grid
    grid = new MemoryGridArea(this);
    mainLayout->addWidget(grid);

    // Connections
    connect(goBtn, &QPushButton::clicked, this, &MemoryView::onGotoAddress);
    connect(addrInput, &QLineEdit::returnPressed, this, &MemoryView::onGotoAddress);
    connect(rowUpBtn, &QPushButton::clicked, this, &MemoryView::onRowUp);
    connect(rowDownBtn, &QPushButton::clicked, this, &MemoryView::onRowDown);
    connect(pageUpBtn, &QPushButton::clicked, this, &MemoryView::onPageUp);
    connect(pageDownBtn, &QPushButton::clicked, this, &MemoryView::onPageDown);
    connect(copyBtn, &QPushButton::clicked, this, &MemoryView::onCopyClicked);
    connect(fillBtn, &QPushButton::clicked, this, &MemoryView::onFillClicked);
    connect(saveBtn, &QPushButton::clicked, this, &MemoryView::onSaveClicked);

    connect(grid, &MemoryGridArea::memoryModified, this, &MemoryView::memoryModified);
    connect(grid, &MemoryGridArea::baseAddressChanged, this, [this](uint32_t addr) {
        addrInput->setText(QString("%1").arg(addr, 8, 16, QLatin1Char('0')).toUpper());
    });
}

void MemoryView::setMemoryPointer(char *mem, uint32_t memSize)
{
    memoryPtr = mem;
    memorySize = memSize;
    grid->setMemoryPointer(mem, memSize);
}

void MemoryView::setBaseAddress(uint32_t addr)
{
    grid->setBaseAddress(addr);
    uint32_t aligned = grid->getBaseAddress();
    addrInput->setText(QString("%1").arg(aligned, 8, 16, QLatin1Char('0')).toUpper());
}

void MemoryView::setProgramStartAddress(uint32_t startAddr)
{
    setBaseAddress(startAddr);
    uint32_t aligned = grid->getBaseAddress();
    fromInput->setText(QString("%1").arg(aligned, 8, 16, QLatin1Char('0')).toUpper());
    toInput->setText(QString("%1").arg(aligned + 0x1F, 8, 16, QLatin1Char('0')).toUpper());
}

uint32_t MemoryView::getBaseAddress() const
{
    return grid->getBaseAddress();
}

void MemoryView::updateView()
{
    grid->refresh();
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

void MemoryView::onRowUp()
{
    uint32_t addr = getBaseAddress();
    if (addr >= 16) {
        setBaseAddress(addr - 16);
    }
}

void MemoryView::onRowDown()
{
    uint32_t addr = getBaseAddress();
    if (addr + 16 < memorySize) {
        setBaseAddress(addr + 16);
    }
}

void MemoryView::onPageUp()
{
    uint32_t addr = getBaseAddress();
    if (addr >= 256) {
        setBaseAddress(addr - 256);
    } else {
        setBaseAddress(0);
    }
}

void MemoryView::onPageDown()
{
    uint32_t addr = getBaseAddress();
    if (addr + 256 < memorySize) {
        setBaseAddress(addr + 256);
    }
}

bool MemoryView::copyMemory(uint32_t fromAddr, uint32_t toAddr, uint32_t byteCount)
{
    if (!memoryPtr || byteCount == 0 || fromAddr == toAddr) return false;
    if (fromAddr + byteCount > memorySize || toAddr + byteCount > memorySize) return false;

    // Handle overlapping copies cleanly matching Sim68K Memory1.cpp
    std::memmove(&memoryPtr[toAddr], &memoryPtr[fromAddr], byteCount);

    emit memoryModified(toAddr);
    grid->refresh();
    return true;
}

bool MemoryView::fillMemory(uint32_t fromAddr, uint32_t toAddr, uint32_t pattern, int patternSize)
{
    if (!memoryPtr || fromAddr > toAddr || toAddr >= memorySize) return false;
    if (patternSize <= 0 || patternSize > 4) patternSize = 1;

    uint8_t bytes[4];
    bytes[0] = static_cast<uint8_t>((pattern >> 24) & 0xFF);
    bytes[1] = static_cast<uint8_t>((pattern >> 16) & 0xFF);
    bytes[2] = static_cast<uint8_t>((pattern >> 8) & 0xFF);
    bytes[3] = static_cast<uint8_t>(pattern & 0xFF);

    int startByte = 4 - patternSize;

    for (uint32_t addr = fromAddr; addr <= toAddr; ++addr) {
        int idx = startByte + static_cast<int>((addr - fromAddr) % static_cast<uint32_t>(patternSize));
        memoryPtr[addr] = static_cast<char>(bytes[idx]);
    }

    emit memoryModified(fromAddr);
    grid->refresh();
    return true;
}

void MemoryView::onCopyClicked()
{
    bool okFrom = false, okTo = false, okBytes = false;
    uint32_t from = fromInput->text().trimmed().toUInt(&okFrom, 16);
    uint32_t to = addrInput->text().trimmed().toUInt(&okTo, 16);
    uint32_t count = bytesInput->text().trimmed().toUInt(&okBytes, 16);

    if (!okFrom || !okTo || !okBytes || count == 0) {
        QMessageBox::warning(this, tr("Memory Copy"), tr("Invalid copy parameters."));
        return;
    }

    if (!copyMemory(from, to, count)) {
        QMessageBox::warning(this, tr("Memory Copy"), tr("Failed to copy memory range."));
    }
}

void MemoryView::onFillClicked()
{
    bool okFrom = false, okTo = false, okBytes = false;
    uint32_t from = fromInput->text().trimmed().toUInt(&okFrom, 16);
    uint32_t to = toInput->text().trimmed().toUInt(&okTo, 16);
    QString pStr = bytesInput->text().trimmed();
    uint32_t pattern = pStr.toUInt(&okBytes, 16);

    if (!okFrom || !okTo || !okBytes || from > to) {
        QMessageBox::warning(this, tr("Memory Fill"), tr("Invalid fill parameters."));
        return;
    }

    int pSize = std::max(1, static_cast<int>((pStr.length() + 1) / 2));
    if (!fillMemory(from, to, pattern, pSize)) {
        QMessageBox::warning(this, tr("Memory Fill"), tr("Failed to fill memory range."));
    }
}

bool MemoryView::saveMemoryRange(uint32_t fromAddr, uint32_t toAddr, const QString &filePath)
{
    if (!memoryPtr || fromAddr > toAddr || toAddr >= memorySize || filePath.isEmpty()) {
        return false;
    }
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    uint32_t len = (toAddr - fromAddr) + 1;
    qint64 written = file.write(&memoryPtr[fromAddr], len);
    file.close();
    return written == static_cast<qint64>(len);
}

void MemoryView::onSaveClicked()
{
    bool okFrom = false, okTo = false;
    uint32_t from = fromInput->text().trimmed().toUInt(&okFrom, 16);
    uint32_t to = toInput->text().trimmed().toUInt(&okTo, 16);
    if (!okFrom || !okTo || from > to || to >= memorySize) {
        QMessageBox::warning(this, tr("Save Memory"), tr("Invalid From / To address range."));
        return;
    }
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save Memory Range"),
                                                    "memory.bin",
                                                    tr("Binary Files (*.bin);;All Files (*)"));
    if (!fileName.isEmpty()) {
        if (!saveMemoryRange(from, to, fileName)) {
            QMessageBox::warning(this, tr("Save Memory"), tr("Failed to write binary memory file."));
        }
    }
}
