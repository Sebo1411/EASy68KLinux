#include "stackview.h"
#include "simulatormanager.h"
#include "../simulator/def.h"
#include "../simulator/proto.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QWheelEvent>
#include <QFontMetrics>
#include <algorithm>

StackDisplayArea::StackDisplayArea(QWidget *parent)
    : QWidget(parent)
{
    QFont monoFont("Monospace", 9);
    monoFont.setStyleHint(QFont::Monospace);
    setFont(monoFont);

    QFontMetrics fm(monoFont);
    rowHeight = std::max(16, fm.lineSpacing());
    charWidth = std::max(7, fm.horizontalAdvance('0'));
}

void StackDisplayArea::setMemoryPointer(const char *mem, uint32_t memSize)
{
    memoryPtr = mem;
    memorySize = memSize;
    update();
}

void StackDisplayArea::updateStack(uint32_t currentA7, uint32_t selectedReg)
{
    a7StackAddr = currentA7 & 0x00FFFFFF;
    selectedRegAddr = selectedReg & 0x00FFFFFF;
    update();
}

void StackDisplayArea::scrollByBytes(int delta)
{
    midAddr = (midAddr + delta) & 0x00FFFFFF;
    midAddr &= ~1; // Keep word-aligned
    update();
}

void StackDisplayArea::setMidAddress(uint32_t addr)
{
    midAddr = addr & 0x00FFFFFF;
    midAddr &= ~1;
    update();
}

void StackDisplayArea::wheelEvent(QWheelEvent *event)
{
    if (event->angleDelta().y() > 0) {
        scrollByBytes(-4);
    } else if (event->angleDelta().y() < 0) {
        scrollByBytes(4);
    }
}

void StackDisplayArea::paintEvent(QPaintEvent * /* event */)
{
    QPainter painter(this);
    painter.setFont(font());
    QFontMetrics fm = painter.fontMetrics();

    // VS Code Dark+ background (#1E1E1E)
    painter.fillRect(rect(), QColor(0x1E, 0x1E, 0x1E));

    if (!memoryPtr || memorySize == 0) {
        painter.setPen(QColor(0x85, 0x85, 0x85));
        painter.drawText(rect(), Qt::AlignCenter, tr("Simulation memory not allocated."));
        return;
    }

    int nRows = std::max(1, height() / rowHeight);
    int midRow = nRows / 2;
    uint32_t dispAddr = (midAddr - (midRow * 4)) & 0x00FFFFFF;

    for (int r = 0; r < nRows; ++r) {
        int y = r * rowHeight;
        uint32_t rowAddr = (dispAddr + (r * 4)) & 0x00FFFFFF;

        if (r % 2 == 1) {
            painter.fillRect(0, y, width(), rowHeight, QColor(0x22, 0x22, 0x23));
        }

        // Draw Address Prefix (#569CD6 VS Code Blue)
        QString addrText = QString("%1: ").arg(rowAddr, 8, 16, QLatin1Char('0')).toUpper();
        painter.setPen(QColor(0x56, 0x9C, 0xD6));
        painter.drawText(6, y + fm.ascent() + (rowHeight - fm.height()) / 2, addrText);

        int curX = 6 + fm.horizontalAdvance(addrText);

        // Draw 4 bytes
        for (int i = 0; i < 4; ++i) {
            uint32_t byteAddr = (rowAddr + i) & 0x00FFFFFF;
            uint8_t byteVal = 0;
            if (byteAddr < memorySize) {
                byteVal = static_cast<uint8_t>(memoryPtr[byteAddr]);
            }

            QString byteStr = QString("%1 ").arg(byteVal, 2, 16, QLatin1Char('0')).toUpper();
            int byteWidth = fm.horizontalAdvance(byteStr);

            // Highlight: selected reg (VS Code Blue #094771), system stack A7 (Dark Gold #5C4813)
            QRect byteRect(curX, y, byteWidth - 2, rowHeight);
            if (byteAddr == selectedRegAddr) {
                painter.fillRect(byteRect, QColor(0x09, 0x47, 0x71));
                painter.setPen(QColor(0x37, 0x94, 0xFF));
                painter.drawRect(byteRect.adjusted(0, 0, -1, -1));
                painter.setPen(Qt::white);
            } else if (byteAddr == a7StackAddr) {
                painter.fillRect(byteRect, QColor(0x5C, 0x48, 0x13));
                painter.setPen(QColor(0xFF, 0xCC, 0x00));
                painter.drawRect(byteRect.adjusted(0, 0, -1, -1));
                painter.setPen(Qt::white);
            } else if (byteVal != 0 && byteVal != 0xFF) {
                painter.setPen(QColor(0x4E, 0xC9, 0xB0));
            } else {
                painter.setPen(QColor(0xD4, 0xD4, 0xD4));
            }

            painter.drawText(curX, y + fm.ascent() + (rowHeight - fm.height()) / 2, byteStr);

            curX += byteWidth;
        }
    }
}

// -----------------------------------------------------------------------------
// StackView
// -----------------------------------------------------------------------------

StackView::StackView(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void StackView::setupUi()
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(6);

    displayAreaWidget = new StackDisplayArea(this);
    layout->addWidget(displayAreaWidget, 1);

    auto *sidePanel = new QWidget(this);
    sidePanel->setMaximumWidth(120);
    auto *sideLayout = new QVBoxLayout(sidePanel);
    sideLayout->setContentsMargins(4, 4, 4, 4);
    sideLayout->setSpacing(6);

    sideLayout->addWidget(new QLabel(tr("<b>View:</b>"), sidePanel));

    whichStackCombo = new QComboBox(sidePanel);
    whichStackCombo->addItem("A7 (Current)");
    whichStackCombo->addItem("SS");
    whichStackCombo->addItem("US");
    for (int i = 0; i < 7; ++i) {
        whichStackCombo->addItem(QString("A%1").arg(i));
    }
    sideLayout->addWidget(whichStackCombo);

    sideLayout->addSpacing(10);

    upBtn = new QPushButton(tr("▲ Scroll Up"), sidePanel);
    sideLayout->addWidget(upBtn);

    downBtn = new QPushButton(tr("▼ Scroll Down"), sidePanel);
    sideLayout->addWidget(downBtn);

    sideLayout->addStretch();
    layout->addWidget(sidePanel, 0);

    connect(whichStackCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &StackView::onRegisterSelectionChanged);
    connect(upBtn, &QPushButton::clicked, this, &StackView::onUpClicked);
    connect(downBtn, &QPushButton::clicked, this, &StackView::onDownClicked);
}

void StackView::setSimulatorManager(SimulatorManager *sm)
{
    simManager = sm;
    if (simManager) {
        setMemoryPointer(simManager->getMemory(), simManager->getMemorySize());
    }
}

void StackView::setMemoryPointer(const char *mem, uint32_t memSize)
{
    displayAreaWidget->setMemoryPointer(mem, memSize);
}

uint32_t StackView::getSelectedRegisterAddress() const
{
    if (!simManager) return 0;
    const long *A = simManager->getA();
    if (!A) return 0;

    int idx = whichStackCombo->currentIndex();
    if (idx == 0) {
        // A7 Current
        int act = a_reg(7);
        return static_cast<uint32_t>(A[act]);
    } else if (idx == 1) {
        // SS
        return static_cast<uint32_t>(A[8]);
    } else if (idx == 2) {
        // US
        return static_cast<uint32_t>(A[7]);
    } else {
        // A0..A6
        int aIdx = idx - 3;
        if (aIdx >= 0 && aIdx < 7) {
            return static_cast<uint32_t>(A[aIdx]);
        }
    }
    return 0;
}

void StackView::updateView()
{
    if (!simManager) return;
    const long *A = simManager->getA();
    if (!A) return;

    int activeA7Idx = a_reg(7);
    uint32_t currentA7 = static_cast<uint32_t>(A[activeA7Idx]);
    uint32_t selectedAddr = getSelectedRegisterAddress();

    displayAreaWidget->updateStack(currentA7, selectedAddr);
}

void StackView::onRegisterSelectionChanged(int /* index */)
{
    uint32_t addr = getSelectedRegisterAddress();
    displayAreaWidget->setMidAddress(addr);
    updateView();
}

void StackView::onUpClicked()
{
    displayAreaWidget->scrollByBytes(-4);
}

void StackView::onDownClicked()
{
    displayAreaWidget->scrollByBytes(4);
}
