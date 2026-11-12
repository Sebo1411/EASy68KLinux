#include "hardwarewidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QFileDialog>
#include <QMessageBox>
#include <QFont>
#include <QPoint>
#include <deque>
#include "../simulator/extern.h"
#include "../simulator/proto.h"

extern int seg7loc, LEDloc, switchLoc, pbLoc;
extern bool ROMMap, ReadMap, ProtectedMap, InvalidMap;
extern int ROMStart, ROMEnd, ReadStart, ReadEnd;
extern int ProtectedStart, ProtectedEnd, InvalidStart, InvalidEnd;

namespace {

inline QRgb applyRasterMode(int mode, QRgb bgRgb, QRgb drawRgb)
{
    uint32_t bg = bgRgb & 0x00FFFFFFU;
    uint32_t draw = drawRgb & 0x00FFFFFFU;
    uint32_t res = draw;
    switch (mode) {
    case 0:  res = 0x000000U; break;
    case 1:  res = 0xFFFFFFU; break;
    case 2:  res = bg; break;
    case 3:  res = (~bg) & 0xFFFFFFU; break;
    case 4:  res = draw; break;
    case 5:  res = (~draw) & 0xFFFFFFU; break;
    case 6:  res = ((~bg) | draw) & 0xFFFFFFU; break;
    case 7:  res = ((~bg) & draw) & 0xFFFFFFU; break;
    case 8:  res = (bg | (~draw)) & 0xFFFFFFU; break;
    case 9:  res = (bg & (~draw)) & 0xFFFFFFU; break;
    case 10: res = (bg | draw) & 0xFFFFFFU; break;
    case 11: res = (~(bg | draw)) & 0xFFFFFFU; break;
    case 12: res = (bg & draw) & 0xFFFFFFU; break;
    case 13: res = (~(bg & draw)) & 0xFFFFFFU; break;
    case 14: res = (bg ^ draw) & 0xFFFFFFU; break;
    case 15: res = (~(bg ^ draw)) & 0xFFFFFFU; break;
    default: res = draw; break;
    }
    return 0xFF000000U | res;
}

void triggerSimIrqLevel(byte level)
{
    if (level >= 1 && level <= 7) {
        irq |= (1 << (level - 1));
        if (stopInstruction) {
            irqHandler();
        }
    }
}

void updateGlobalMouseSnapshot(const QMouseEvent *e, bool isPress, bool isRelease, bool isDbl)
{
    int x = static_cast<int>(e->position().x());
    int y = static_cast<int>(e->position().y());
    Qt::MouseButtons btns = e->buttons();
    Qt::KeyboardModifiers mods = e->modifiers();

    bool left = (btns & Qt::LeftButton) != 0 || (isPress && e->button() == Qt::LeftButton);
    bool right = (btns & Qt::RightButton) != 0 || (isPress && e->button() == Qt::RightButton);
    bool mid = (btns & Qt::MiddleButton) != 0 || (isPress && e->button() == Qt::MiddleButton);
    bool sh = (mods & Qt::ShiftModifier) != 0;
    bool al = (mods & Qt::AltModifier) != 0;
    bool ct = (mods & Qt::ControlModifier) != 0;

    mouseX = x;
    mouseY = y;
    mouseLeft = left;
    mouseRight = right;
    mouseMiddle = mid;
    mouseDouble = isDbl;
    keyShift = sh;
    keyAlt = al;
    keyCtrl = ct;

    if (isPress) {
        mouseXDown = x;
        mouseYDown = y;
        mouseLeftDown = (e->button() == Qt::LeftButton) || left;
        mouseRightDown = (e->button() == Qt::RightButton) || right;
        mouseMiddleDown = (e->button() == Qt::MiddleButton) || mid;
        mouseDoubleDown = isDbl;
        keyShiftDown = sh;
        keyAltDown = al;
        keyCtrlDown = ct;
    }
    if (isRelease) {
        mouseXUp = x;
        mouseYUp = y;
        mouseLeftUp = (e->button() == Qt::LeftButton);
        mouseRightUp = (e->button() == Qt::RightButton);
        mouseMiddleUp = (e->button() == Qt::MiddleButton);
        mouseDoubleUp = false;
        keyShiftUp = sh;
        keyAltUp = al;
        keyCtrlUp = ct;
    }
}

} // namespace

static inline QColor decodeColor(int c) {
    // 68000 TRAP #15 color format is $00BBGGRR
    int r = c & 0xFF;
    int g = (c >> 8) & 0xFF;
    int b = (c >> 16) & 0xFF;
    return QColor(r, g, b);
}

static inline int encodeColor(const QColor &col) {
    return (col.red() & 0xFF) | ((col.green() & 0xFF) << 8) | ((col.blue() & 0xFF) << 16);
}

// -----------------------------------------------------------------------------
// SevenSegmentDisplayWidget
// -----------------------------------------------------------------------------
HardwareWidget::SevenSegmentDisplayWidget::SevenSegmentDisplayWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(520, 96);
    setFixedHeight(96);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    for (int i = 0; i < 8; ++i) {
        digits[i] = 0;
    }
}

void HardwareWidget::SevenSegmentDisplayWidget::setDigitValue(int digitIndex, uint8_t val)
{
    if (digitIndex >= 0 && digitIndex < 8) {
        digits[digitIndex] = val;
        update();
    }
}

void HardwareWidget::SevenSegmentDisplayWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Dark sleek hardware bezel background and frame
    p.fillRect(rect(), QColor(0x14, 0x14, 0x18));
    p.setPen(QColor(0x2D, 0x2D, 0x35));
    p.drawRect(rect().adjusted(0, 0, -1, -1));

    const int totalDigits = 8;
    const qreal step = width() / static_cast<qreal>(totalDigits);
    const qreal digitH = 54.0;
    const qreal digitW = qMin(step * 0.55, 34.0);
    const qreal startY = 26.0;
    const qreal t = 4.8;
    const qreal gap = 1.4;

    auto makeHorizontalHex = [](qreal x0, qreal x1, qreal y0, qreal thick) {
        QPainterPath path;
        qreal yMid = y0 + thick / 2.0;
        qreal yBot = y0 + thick;
        path.moveTo(x0, yMid);
        path.lineTo(x0 + thick / 2.0, y0);
        path.lineTo(x1 - thick / 2.0, y0);
        path.lineTo(x1, yMid);
        path.lineTo(x1 - thick / 2.0, yBot);
        path.lineTo(x0 + thick / 2.0, yBot);
        path.closeSubpath();
        return path;
    };

    auto makeVerticalHex = [](qreal x0, qreal y0, qreal y1, qreal thick) {
        QPainterPath path;
        qreal xMid = x0 + thick / 2.0;
        qreal xRight = x0 + thick;
        path.moveTo(xMid, y0);
        path.lineTo(xRight, y0 + thick / 2.0);
        path.lineTo(xRight, y1 - thick / 2.0);
        path.lineTo(xMid, y1);
        path.lineTo(x0, y1 - thick / 2.0);
        path.lineTo(x0, y0 + thick / 2.0);
        path.closeSubpath();
        return path;
    };

    QFont monoFont("Monospace", 8);
    monoFont.setStyleHint(QFont::Monospace);
    p.setFont(monoFont);

    QColor litFill(0xFF, 0x2A, 0x2A);
    QPen litPen(QColor(255, 60, 60, 80), 1.0);
    QColor unlitFill(0x2A, 0x14, 0x16);
    QPen unlitPen(Qt::NoPen);

    for (int i = 0; i < totalDigits; ++i) {
        qreal cellX = i * step;

        // Draw hardware address label ($E00000..$E0000E) centered at top
        p.setPen(QColor(0x85, 0x85, 0x85));
        QRectF labelRect(cellX, 4, step, 16);
        p.drawText(labelRect, Qt::AlignCenter, QString("$E0000%1").arg(i * 2, 1, 16).toUpper());

        // Center digit box inside its 1/8th cell with space for DP
        qreal x = cellX + (step - digitW) / 2.0 - 4.0;
        qreal y = startY;

        uint8_t mask = digits[i];

        qreal yA = y;
        qreal yD = y + digitH - t;
        qreal yG = y + digitH / 2.0 - t / 2.0;
        qreal xLeft = x;
        qreal xRight = x + digitW - t;

        QPainterPath segA = makeHorizontalHex(xLeft + t / 2.0 + gap, xRight + t / 2.0 - gap, yA, t);
        QPainterPath segB = makeVerticalHex(xRight, yA + t / 2.0 + gap, yG + t / 2.0 - gap, t);
        QPainterPath segC = makeVerticalHex(xRight, yG + t / 2.0 + gap, yD + t / 2.0 - gap, t);
        QPainterPath segD = makeHorizontalHex(xLeft + t / 2.0 + gap, xRight + t / 2.0 - gap, yD, t);
        QPainterPath segE = makeVerticalHex(xLeft, yG + t / 2.0 + gap, yD + t / 2.0 - gap, t);
        QPainterPath segF = makeVerticalHex(xLeft, yA + t / 2.0 + gap, yG + t / 2.0 - gap, t);
        QPainterPath segG = makeHorizontalHex(xLeft + t / 2.0 + gap, xRight + t / 2.0 - gap, yG, t);

        const QPainterPath *paths[7] = { &segA, &segB, &segC, &segD, &segE, &segF, &segG };
        for (int b = 0; b < 7; ++b) {
            bool on = (mask & (1 << b)) != 0;
            p.setPen(on ? litPen : unlitPen);
            p.setBrush(on ? litFill : unlitFill);
            p.drawPath(*paths[b]);
        }

        // Decimal point (DP, bit 7)
        bool dpOn = (mask & (1 << 7)) != 0;
        p.setPen(dpOn ? litPen : unlitPen);
        p.setBrush(dpOn ? litFill : unlitFill);
        qreal dpX = x + digitW + 5.0;
        qreal dpY = y + digitH - 3.5;
        p.drawEllipse(QPointF(dpX, dpY), 2.8, 2.8);
    }
}

// -----------------------------------------------------------------------------
// LedBankWidget
// -----------------------------------------------------------------------------
HardwareWidget::LedBankWidget::LedBankWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(520, 58);
    setFixedHeight(58);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void HardwareWidget::LedBankWidget::setLeds(uint8_t val)
{
    ledsValue = val;
    update();
}

void HardwareWidget::LedBankWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    p.fillRect(rect(), QColor(0x18, 0x18, 0x1C));
    p.setPen(QColor(0x2D, 0x2D, 0x35));
    p.drawRect(rect().adjusted(0, 0, -1, -1));

    const qreal step = width() / 8.0;
    const qreal radius = 10.0;
    const qreal cy = 20.0;

    QFont font("Monospace", 8, QFont::Bold);
    font.setStyleHint(QFont::Monospace);
    p.setFont(font);

    for (int i = 0; i < 8; ++i) {
        int bit = 7 - i; // Left is bit 7, Right is bit 0
        bool isOn = (ledsValue & (1 << bit)) != 0;

        qreal cx = step * i + step / 2.0;

        // Metallic outer bezel ring
        p.setPen(QColor(0x4A, 0x4A, 0x55));
        p.setBrush(QColor(0x3C, 0x3C, 0x44));
        p.drawEllipse(QPointF(cx, cy), radius + 2.5, radius + 2.5);

        // Dark recessed socket
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x12, 0x12, 0x16));
        p.drawEllipse(QPointF(cx, cy), radius + 0.5, radius + 0.5);

        // Radial gradient LED lens
        if (isOn) {
            QRadialGradient grad(cx - 3, cy - 3, radius);
            grad.setColorAt(0.0, QColor(0xFF, 0x80, 0x80));
            grad.setColorAt(0.5, QColor(0xFF, 0x20, 0x20));
            grad.setColorAt(1.0, QColor(0xB0, 0x00, 0x00));
            p.setBrush(grad);
            p.setPen(QColor(0xFF, 0x40, 0x40, 100));
        } else {
            QRadialGradient grad(cx - 2, cy - 2, radius);
            grad.setColorAt(0.0, QColor(0x45, 0x15, 0x15));
            grad.setColorAt(1.0, QColor(0x20, 0x08, 0x08));
            p.setBrush(grad);
            p.setPen(QColor(0x1A, 0x05, 0x05));
        }
        p.drawEllipse(QPointF(cx, cy), radius, radius);

        // Visible bit label with ample bottom margin
        p.setPen(isOn ? QColor(0xCC, 0xCC, 0xCC) : QColor(0x85, 0x85, 0x85));
        p.drawText(QRectF(cx - 28, 35, 56, 18), Qt::AlignCenter, QString("LED %1").arg(bit));
    }
}

// -----------------------------------------------------------------------------
// CanvasView
// -----------------------------------------------------------------------------
HardwareWidget::CanvasView::CanvasView(HardwareWidget *owner, QWidget *parent)
    : QWidget(parent), ownerWidget(owner)
{
    setFixedSize(640, 480);
    setAutoFillBackground(true);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

void HardwareWidget::CanvasView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    QMutexLocker locker(&ownerWidget->graphicsMutex);
    p.drawImage(0, 0, ownerWidget->canvas);
}

void HardwareWidget::CanvasView::mousePressEvent(QMouseEvent *event)
{
    setFocus(Qt::MouseFocusReason);
    updateGlobalMouseSnapshot(event, true, false, false);
    triggerSimIrqLevel(mouseDownIRQ);
    event->accept();
}

void HardwareWidget::CanvasView::mouseReleaseEvent(QMouseEvent *event)
{
    updateGlobalMouseSnapshot(event, false, true, false);
    triggerSimIrqLevel(mouseUpIRQ);
    event->accept();
}

void HardwareWidget::CanvasView::mouseMoveEvent(QMouseEvent *event)
{
    updateGlobalMouseSnapshot(event, false, false, false);
    triggerSimIrqLevel(mouseMoveIRQ);
    event->accept();
}

void HardwareWidget::CanvasView::mouseDoubleClickEvent(QMouseEvent *event)
{
    setFocus(Qt::MouseFocusReason);
    updateGlobalMouseSnapshot(event, true, false, true);
    triggerSimIrqLevel(mouseDownIRQ);
    event->accept();
}

void HardwareWidget::CanvasView::keyPressEvent(QKeyEvent *event)
{
    emit ownerWidget->canvasKeyEvent(event, true);
    event->accept();
}

void HardwareWidget::CanvasView::keyReleaseEvent(QKeyEvent *event)
{
    emit ownerWidget->canvasKeyEvent(event, false);
    event->accept();
}

// -----------------------------------------------------------------------------
// HardwareWidget
// -----------------------------------------------------------------------------
HardwareWidget::HardwareWidget(QWidget *parent)
    : QWidget(parent),
      canvas(640, 480, QImage::Format_ARGB32_Premultiplied),
      backBuffer(640, 480, QImage::Format_ARGB32_Premultiplied)
{
    canvas.fill(Qt::black);
    backBuffer.fill(Qt::black);
    setupUi();
    resetHardware();
}

void HardwareWidget::setupUi()
{
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    auto *outerScrollArea = new QScrollArea(this);
    outerScrollArea->setWidgetResizable(true);
    outerScrollArea->setFrameShape(QFrame::NoFrame);
    outerScrollArea->setStyleSheet("QScrollArea { background-color: #1E1E1E; border: none; }");

    auto *contentWidget = new QWidget(outerScrollArea);
    contentWidget->setObjectName("hardwareContentWidget");
    auto *mainLayout = new QVBoxLayout(contentWidget);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(10);

    // 1. Simulated Peripherals Box
    auto *periphBox = new QGroupBox(tr("Simulated Peripherals (Hardware I/O)"), contentWidget);
    auto *periphLayout = new QVBoxLayout(periphBox);
    periphLayout->setSpacing(8);

    // 7-Segment Display
    auto *segLabel = new QLabel(tr("7-Segment Display (8 Digits @ $E00000..$E0000E):"), periphBox);
    segLabel->setStyleSheet("font-weight: bold; color: #CCCCCC;");
    periphLayout->addWidget(segLabel);
    sevenSegWidget = new SevenSegmentDisplayWidget(periphBox);
    periphLayout->addWidget(sevenSegWidget);

    // LED Indicators
    auto *ledLabel = new QLabel(tr("LED Indicators (8 LEDs @ $E00010):"), periphBox);
    ledLabel->setStyleSheet("font-weight: bold; color: #CCCCCC;");
    periphLayout->addWidget(ledLabel);
    ledBankWidget = new LedBankWidget(periphBox);
    periphLayout->addWidget(ledBankWidget);

    // Switches and Buttons Grid
    auto *controlsGrid = new QGridLayout();
    controlsGrid->setSpacing(6);

    auto *swGroupLabel = new QLabel(tr("Toggle Switches ($E00012):"), periphBox);
    swGroupLabel->setStyleSheet("font-weight: bold; color: #CCCCCC;");
    controlsGrid->addWidget(swGroupLabel, 0, 0, 1, 8);

    QString switchStyle = QStringLiteral(
        "QCheckBox {"
        "    background-color: #252526;"
        "    color: #CCCCCC;"
        "    border: 1px solid #3C3C3C;"
        "    border-radius: 4px;"
        "    padding: 4px 8px;"
        "    font-family: Monospace;"
        "    font-weight: bold;"
        "}"
        "QCheckBox:hover {"
        "    background-color: #2D2D30;"
        "    border-color: #555555;"
        "}"
        "QCheckBox:checked {"
        "    background-color: #143622;"
        "    border: 1px solid #4EC9B0;"
        "    color: #4EC9B0;"
        "}"
        "QCheckBox::indicator {"
        "    width: 14px;"
        "    height: 14px;"
        "    border-radius: 2px;"
        "    border: 1px solid #5A5A66;"
        "    background: #1E1E24;"
        "}"
        "QCheckBox::indicator:checked {"
        "    background-color: #16A34A;"
        "    border: 1px solid #4EC9B0;"
        "}"
    );

    for (int i = 0; i < 8; ++i) {
        int bit = 7 - i;
        switches[bit] = new QCheckBox(QString("SW%1").arg(bit), periphBox);
        switches[bit]->setMinimumHeight(30);
        switches[bit]->setCursor(Qt::PointingHandCursor);
        switches[bit]->setStyleSheet(switchStyle);
        controlsGrid->addWidget(switches[bit], 1, i);
        connect(switches[bit], &QCheckBox::toggled, this, [this, bit](bool checked) {
            onSwitchToggled(bit, checked);
        });
    }

    auto *pbGroupLabel = new QLabel(tr("Pushbuttons (Normally High, $E00014):"), periphBox);
    pbGroupLabel->setStyleSheet("font-weight: bold; color: #CCCCCC;");
    controlsGrid->addWidget(pbGroupLabel, 2, 0, 1, 8);

    QString pbStyle = QStringLiteral(
        "QPushButton {"
        "    background-color: #2D2D30;"
        "    color: #E0E0E0;"
        "    border: 1px solid #45454D;"
        "    border-bottom: 3px solid #1E1E1E;"
        "    border-radius: 4px;"
        "    font-family: Monospace;"
        "    font-weight: bold;"
        "    padding: 4px 8px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #38383D;"
        "    border-color: #007ACC;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #094771;"
        "    color: #FFFFFF;"
        "    border: 1px solid #3794FF;"
        "    border-bottom: 1px solid #3794FF;"
        "    padding-top: 6px;"
        "    padding-bottom: 2px;"
        "}"
    );

    for (int i = 0; i < 8; ++i) {
        int bit = 7 - i;
        pushButtons[bit] = new QPushButton(QString("PB%1").arg(bit), periphBox);
        pushButtons[bit]->setMinimumHeight(30);
        pushButtons[bit]->setMinimumWidth(52);
        pushButtons[bit]->setCursor(Qt::PointingHandCursor);
        pushButtons[bit]->setStyleSheet(pbStyle);
        controlsGrid->addWidget(pushButtons[bit], 3, i);

        connect(pushButtons[bit], &QPushButton::pressed, this, [this, bit]() {
            onPushbuttonPressed(bit);
        });
        connect(pushButtons[bit], &QPushButton::released, this, [this, bit]() {
            onPushbuttonReleased(bit);
        });
    }

    periphLayout->addLayout(controlsGrid);
    mainLayout->addWidget(periphBox);

    // 2. Interrupt & Memory Map Horizontal Row
    auto *middleLayout = new QHBoxLayout();

    // Interrupt Box (matching Sim68K Hardware.png)
    auto *irqBox = new QGroupBox(tr("Interrupts & Reset"), contentWidget);
    auto *irqMainLayout = new QVBoxLayout(irqBox);
    irqMainLayout->setSpacing(6);

    auto *irqBtnRow = new QHBoxLayout();
    irqBtnRow->setSpacing(4);

    QString irqStyle = QStringLiteral(
        "QPushButton {"
        "    background-color: #881337;"
        "    color: #FFFFFF;"
        "    border: 1px solid #9F1239;"
        "    border-bottom: 2px solid #4C0519;"
        "    border-radius: 3px;"
        "    font-weight: bold;"
        "    padding: 3px 6px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #9F1239;"
        "    border-color: #BE123C;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #BE123C;"
        "    border-bottom: 1px solid #BE123C;"
        "    padding-top: 4px;"
        "    padding-bottom: 2px;"
        "}"
    );

    for (int i = 0; i < 7; ++i) {
        int level = i + 1;
        irqButtons[i] = new QPushButton(QString("IRQ %1").arg(level), irqBox);
        irqButtons[i]->setMinimumHeight(28);
        irqButtons[i]->setStyleSheet(irqStyle);
        irqBtnRow->addWidget(irqButtons[i]);
        connect(irqButtons[i], &QPushButton::clicked, this, [this, level]() {
            triggerInterrupt(level);
        });
    }
    hwResetBtn = new QPushButton(tr("Reset"), irqBox);
    hwResetBtn->setMinimumHeight(28);
    hwResetBtn->setStyleSheet(irqStyle);
    hwResetBtn->setToolTip(tr("Reset 68000 Processor & Peripherals"));
    irqBtnRow->addWidget(hwResetBtn);
    connect(hwResetBtn, &QPushButton::clicked, this, [this]() {
        resetHardware();
        emit hardwareResetRequested();
    });
    irqMainLayout->addLayout(irqBtnRow);

    QString hexEditStyle = QStringLiteral(
        "QLineEdit {"
        "    background-color: #1E1E1E;"
        "    color: #9CDCFE;"
        "    border: 1px solid #3C3C3C;"
        "    border-radius: 3px;"
        "    padding: 2px 4px;"
        "    font-family: Monospace;"
        "    font-weight: bold;"
        "}"
        "QLineEdit:focus {"
        "    border-color: #007ACC;"
        "}"
    );

    auto *autoIrqRow = new QHBoxLayout();
    autoIrqRow->setSpacing(6);
    autoIrqCheck = new QCheckBox(tr("Auto Interrupt:"), irqBox);
    autoIrqCombo = new QComboBox(irqBox);
    for (int level = 1; level <= 7; ++level) {
        autoIrqCombo->addItem(QString("IRQ %1").arg(level), level);
    }
    autoIrqIntervalEdit = new QLineEdit("1000", irqBox);
    autoIrqIntervalEdit->setMinimumWidth(74);
    autoIrqIntervalEdit->setMinimumHeight(26);
    autoIrqIntervalEdit->setAlignment(Qt::AlignCenter);
    autoIrqIntervalEdit->setStyleSheet(hexEditStyle);

    autoIrqRow->addWidget(autoIrqCheck);
    autoIrqRow->addWidget(autoIrqCombo);
    autoIrqRow->addWidget(new QLabel(tr("Every (ms):"), irqBox));
    autoIrqRow->addWidget(autoIrqIntervalEdit);
    autoIrqRow->addStretch();
    irqMainLayout->addLayout(autoIrqRow);

    autoIrqTimer = new QTimer(this);
    connect(autoIrqTimer, &QTimer::timeout, this, [this]() {
        int level = autoIrqCombo->currentData().toInt();
        triggerInterrupt(level);
    });
    connect(autoIrqCheck, &QCheckBox::toggled, this, &HardwareWidget::onAutoIrqChanged);
    connect(autoIrqCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &HardwareWidget::onAutoIrqChanged);
    connect(autoIrqIntervalEdit, &QLineEdit::editingFinished, this, &HardwareWidget::onAutoIrqChanged);

    middleLayout->addWidget(irqBox, 1);

    // Memory Map Box
    auto *mapBox = new QGroupBox(tr("Memory Map"), contentWidget);
    auto *mapGrid = new QGridLayout(mapBox);
    mapGrid->setSpacing(4);
    QFont monoFont("Monospace", 8);

    auto setupMapEdit = [&](QLineEdit *le) {
        le->setFont(monoFont);
        le->setMinimumWidth(74);
        le->setMinimumHeight(26);
        le->setAlignment(Qt::AlignCenter);
        le->setStyleSheet(hexEditStyle);
    };

    romMapCheck = new QCheckBox(tr("ROM:"), mapBox);
    romStartEdit = new QLineEdit("000000", mapBox);
    setupMapEdit(romStartEdit);
    romEndEdit = new QLineEdit("001FFF", mapBox);
    setupMapEdit(romEndEdit);
    mapGrid->addWidget(romMapCheck, 0, 0);
    mapGrid->addWidget(romStartEdit, 0, 1);
    mapGrid->addWidget(new QLabel("-", mapBox), 0, 2);
    mapGrid->addWidget(romEndEdit, 0, 3);

    readMapCheck = new QCheckBox(tr("Read Only:"), mapBox);
    readStartEdit = new QLineEdit("002000", mapBox);
    setupMapEdit(readStartEdit);
    readEndEdit = new QLineEdit("003FFF", mapBox);
    setupMapEdit(readEndEdit);
    mapGrid->addWidget(readMapCheck, 0, 4);
    mapGrid->addWidget(readStartEdit, 0, 5);
    mapGrid->addWidget(new QLabel("-", mapBox), 0, 6);
    mapGrid->addWidget(readEndEdit, 0, 7);

    protectedMapCheck = new QCheckBox(tr("Protected:"), mapBox);
    protectedStartEdit = new QLineEdit("004000", mapBox);
    setupMapEdit(protectedStartEdit);
    protectedEndEdit = new QLineEdit("005FFF", mapBox);
    setupMapEdit(protectedEndEdit);
    mapGrid->addWidget(protectedMapCheck, 1, 0);
    mapGrid->addWidget(protectedStartEdit, 1, 1);
    mapGrid->addWidget(new QLabel("-", mapBox), 1, 2);
    mapGrid->addWidget(protectedEndEdit, 1, 3);

    invalidMapCheck = new QCheckBox(tr("Invalid:"), mapBox);
    invalidStartEdit = new QLineEdit("006000", mapBox);
    setupMapEdit(invalidStartEdit);
    invalidEndEdit = new QLineEdit("007FFF", mapBox);
    setupMapEdit(invalidEndEdit);
    mapGrid->addWidget(invalidMapCheck, 1, 4);
    mapGrid->addWidget(invalidStartEdit, 1, 5);
    mapGrid->addWidget(new QLabel("-", mapBox), 1, 6);
    mapGrid->addWidget(invalidEndEdit, 1, 7);

    auto connectMap = [this](QWidget *w) {
        if (auto *cb = qobject_cast<QCheckBox*>(w)) {
            connect(cb, &QCheckBox::toggled, this, &HardwareWidget::onMemoryMapChanged);
        } else if (auto *le = qobject_cast<QLineEdit*>(w)) {
            connect(le, &QLineEdit::editingFinished, this, &HardwareWidget::onMemoryMapChanged);
        }
    };
    connectMap(romMapCheck); connectMap(romStartEdit); connectMap(romEndEdit);
    connectMap(readMapCheck); connectMap(readStartEdit); connectMap(readEndEdit);
    connectMap(protectedMapCheck); connectMap(protectedStartEdit); connectMap(protectedEndEdit);
    connectMap(invalidMapCheck); connectMap(invalidStartEdit); connectMap(invalidEndEdit);

    middleLayout->addWidget(mapBox, 2);
    mainLayout->addLayout(middleLayout);

    // 3. 2D Graphics Canvas Box
    auto *gfxBox = new QGroupBox(tr("2D Graphics Canvas (TRAP #15 Tasks 80..96)"), contentWidget);
    auto *gfxLayout = new QVBoxLayout(gfxBox);

    auto *gfxToolbar = new QHBoxLayout();
    canvasSizeLabel = new QLabel(tr("Resolution: 640 x 480"), gfxBox);
    auto *clearBtn = new QPushButton(tr("Clear Canvas"), gfxBox);
    auto *saveBtn = new QPushButton(tr("Save Image..."), gfxBox);

    connect(clearBtn, &QPushButton::clicked, this, &HardwareWidget::onClearCanvasClicked);
    connect(saveBtn, &QPushButton::clicked, this, &HardwareWidget::onSaveCanvasClicked);

    gfxToolbar->addWidget(canvasSizeLabel);
    gfxToolbar->addStretch();
    gfxToolbar->addWidget(clearBtn);
    gfxToolbar->addWidget(saveBtn);
    gfxLayout->addLayout(gfxToolbar);

    auto *scrollArea = new QScrollArea(gfxBox);
    canvasView = new CanvasView(this, scrollArea);
    scrollArea->setWidget(canvasView);
    scrollArea->setWidgetResizable(false);
    scrollArea->setMinimumHeight(240);
    gfxLayout->addWidget(scrollArea);

    mainLayout->addWidget(gfxBox);

    outerScrollArea->setWidget(contentWidget);
    outerLayout->addWidget(outerScrollArea);
}

void HardwareWidget::triggerInterrupt(int level)
{
    if (level >= 1 && level <= 7) {
        irq |= (1 << (level - 1));
        if (stopInstruction) {
            irqHandler();
        }
    }
}

void HardwareWidget::setAutoInterrupt(int level, int intervalMs, bool enabled)
{
    if (level >= 1 && level <= 7 && autoIrqCombo) {
        autoIrqCombo->setCurrentIndex(level - 1);
    }
    if (intervalMs > 0 && autoIrqIntervalEdit) {
        autoIrqIntervalEdit->setText(QString::number(intervalMs));
    }
    if (autoIrqCheck) {
        autoIrqCheck->setChecked(enabled);
    }
    onAutoIrqChanged();
}

bool HardwareWidget::isAutoInterruptEnabled() const
{
    return autoIrqTimer && autoIrqTimer->isActive();
}

void HardwareWidget::onAutoIrqChanged()
{
    if (!autoIrqTimer || !autoIrqCheck || !autoIrqIntervalEdit) return;
    if (autoIrqCheck->isChecked()) {
        bool ok = false;
        int ms = autoIrqIntervalEdit->text().trimmed().toInt(&ok);
        if (!ok || ms < 5) ms = 50;
        autoIrqTimer->setInterval(ms);
        if (!autoIrqTimer->isActive()) {
            autoIrqTimer->start();
        }
    } else {
        autoIrqTimer->stop();
    }
}

void HardwareWidget::onMemoryMapChanged()
{
    bool ok = false;
    ROMMap = romMapCheck->isChecked();
    ROMStart = romStartEdit->text().trimmed().toInt(&ok, 16);
    ROMEnd = romEndEdit->text().trimmed().toInt(&ok, 16);

    ReadMap = readMapCheck->isChecked();
    ReadStart = readStartEdit->text().trimmed().toInt(&ok, 16);
    ReadEnd = readEndEdit->text().trimmed().toInt(&ok, 16);

    ProtectedMap = protectedMapCheck->isChecked();
    ProtectedStart = protectedStartEdit->text().trimmed().toInt(&ok, 16);
    ProtectedEnd = protectedEndEdit->text().trimmed().toInt(&ok, 16);

    InvalidMap = invalidMapCheck->isChecked();
    InvalidStart = invalidStartEdit->text().trimmed().toInt(&ok, 16);
    InvalidEnd = invalidEndEdit->text().trimmed().toInt(&ok, 16);
}

void HardwareWidget::resetHardware()
{
    if (memory) {
        // Initialize pushbuttons to 0xFF (all released)
        memory[pbLoc & ADDRMASK] = static_cast<char>(0xFF);
        // Clear switches, LEDs, and 7-segment displays
        memory[switchLoc & ADDRMASK] = 0;
        memory[LEDloc & ADDRMASK] = 0;
        for (int i = 0; i < 8; ++i) {
            memory[(seg7loc + 2 * i) & ADDRMASK] = 0;
        }
    }

    for (int i = 0; i < 8; ++i) {
        if (switches[i]) {
            QSignalBlocker b(switches[i]);
            switches[i]->setChecked(false);
        }
    }

    updateHardware();
    clearCanvas();
}

void HardwareWidget::updateHardware()
{
    if (!memory) return;

    // 7-segment display (even addresses seg7loc + 2*i)
    for (int i = 0; i < 8; ++i) {
        uint8_t val = static_cast<uint8_t>(memory[(seg7loc + 2 * i) & ADDRMASK]);
        sevenSegWidget->setDigitValue(i, val);
    }

    // LEDs ($E00010)
    uint8_t ledVal = static_cast<uint8_t>(memory[LEDloc & ADDRMASK]);
    ledBankWidget->setLeds(ledVal);

    // Sync switches with memory in case code modified switchLoc
    uint8_t swVal = static_cast<uint8_t>(memory[switchLoc & ADDRMASK]);
    for (int i = 0; i < 8; ++i) {
        bool expected = (swVal & (1 << i)) != 0;
        if (switches[i] && switches[i]->isChecked() != expected) {
            QSignalBlocker b(switches[i]);
            switches[i]->setChecked(expected);
        }
    }

    canvasView->update();
}

void HardwareWidget::onSwitchToggled(int index, bool checked)
{
    if (!memory) return;
    uint8_t swVal = static_cast<uint8_t>(memory[switchLoc & ADDRMASK]);
    if (checked) {
        swVal |= (1 << index);
    } else {
        swVal &= ~(1 << index);
    }
    memory[switchLoc & ADDRMASK] = static_cast<char>(swVal);
}

void HardwareWidget::onPushbuttonPressed(int index)
{
    if (!memory) return;
    // Active low: pressed = 0
    uint8_t pbVal = static_cast<uint8_t>(memory[pbLoc & ADDRMASK]);
    pbVal &= ~(1 << index);
    memory[pbLoc & ADDRMASK] = static_cast<char>(pbVal);
}

void HardwareWidget::onPushbuttonReleased(int index)
{
    if (!memory) return;
    // Active low: released = 1
    uint8_t pbVal = static_cast<uint8_t>(memory[pbLoc & ADDRMASK]);
    pbVal |= (1 << index);
    memory[pbLoc & ADDRMASK] = static_cast<char>(pbVal);
}

void HardwareWidget::onClearCanvasClicked()
{
    clearCanvas();
}

void HardwareWidget::onSaveCanvasClicked()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save Canvas Image"),
                                                    "easy68k_canvas.png",
                                                    tr("PNG Images (*.png);;All Files (*)"));
    if (!fileName.isEmpty()) {
        QMutexLocker locker(&graphicsMutex);
        if (!canvas.save(fileName)) {
            QMessageBox::warning(this, tr("Save Error"), tr("Failed to save image to %1").arg(fileName));
        }
    }
}

// -----------------------------------------------------------------------------
// Graphics API
// -----------------------------------------------------------------------------
QImage& HardwareWidget::activeCanvas()
{
    return doubleBuffered ? backBuffer : canvas;
}

void HardwareWidget::notifyCanvasChanged()
{
    if (!doubleBuffered && canvasView) {
        canvasView->update();
    }
}

void HardwareWidget::commitGraphicsDraw(const QImage *overlay, const QColor & /* drawCol */)
{
    if (!overlay) return;
    QImage &dst = activeCanvas();
    const int w = std::min(dst.width(), overlay->width());
    const int h = std::min(dst.height(), overlay->height());
    for (int y = 0; y < h; ++y) {
        const QRgb *srcLine = reinterpret_cast<const QRgb*>(overlay->constScanLine(y));
        QRgb *dstLine = reinterpret_cast<QRgb*>(dst.scanLine(y));
        for (int x = 0; x < w; ++x) {
            if (qAlpha(srcLine[x]) > 0) {
                dstLine[x] = applyRasterMode(currentDrawingMode, dstLine[x], srcLine[x]);
            }
        }
    }
}

void HardwareWidget::setLineColor(int c)
{
    QMutexLocker locker(&graphicsMutex);
    currentLineColor = decodeColor(c);
}

void HardwareWidget::setFillColor(int c)
{
    QMutexLocker locker(&graphicsMutex);
    currentFillColor = decodeColor(c);
}

void HardwareWidget::setPenWidth(int w)
{
    QMutexLocker locker(&graphicsMutex);
    currentPenWidth = qMax(1, w);
}

void HardwareWidget::drawPixel(int x, int y)
{
    QMutexLocker locker(&graphicsMutex);
    QImage &dst = activeCanvas();
    if (x >= 0 && x < dst.width() && y >= 0 && y < dst.height()) {
        // Task 82 draws pixel in pen color; drawing mode is ignored per graphics.htm
        dst.setPixelColor(x, y, currentLineColor);
    }
    notifyCanvasChanged();
}

int HardwareWidget::getPixel(int x, int y)
{
    QMutexLocker locker(&graphicsMutex);
    const QImage &src = activeCanvas();
    if (x >= 0 && x < src.width() && y >= 0 && y < src.height()) {
        return encodeColor(src.pixelColor(x, y));
    }
    return 0;
}

void HardwareWidget::line(int x1, int y1, int x2, int y2)
{
    QMutexLocker locker(&graphicsMutex);
    currentX = static_cast<short>(x2);
    currentY = static_cast<short>(y2);
    if (currentDrawingMode == 2) return;

    QColor penCol = currentLineColor;
    if (currentDrawingMode == 0) penCol = Qt::black;
    else if (currentDrawingMode == 1) penCol = Qt::white;

    QImage &dst = activeCanvas();
    if (currentDrawingMode == 0 || currentDrawingMode == 1 || currentDrawingMode == 4) {
        QPainter p(&dst);
        p.setPen(QPen(penCol, currentPenWidth));
        p.drawLine(x1, y1, x2, y2);
    } else {
        QImage overlay(dst.size(), QImage::Format_ARGB32);
        overlay.fill(Qt::transparent);
        QPainter p(&overlay);
        p.setPen(QPen(currentLineColor, currentPenWidth));
        p.drawLine(x1, y1, x2, y2);
        commitGraphicsDraw(&overlay, currentLineColor);
    }
    notifyCanvasChanged();
}

void HardwareWidget::lineTo(int x, int y)
{
    QMutexLocker locker(&graphicsMutex);
    int x1 = currentX;
    int y1 = currentY;
    currentX = static_cast<short>(x);
    currentY = static_cast<short>(y);
    if (currentDrawingMode == 2) return;

    QColor penCol = currentLineColor;
    if (currentDrawingMode == 0) penCol = Qt::black;
    else if (currentDrawingMode == 1) penCol = Qt::white;

    QImage &dst = activeCanvas();
    if (currentDrawingMode == 0 || currentDrawingMode == 1 || currentDrawingMode == 4) {
        QPainter p(&dst);
        p.setPen(QPen(penCol, currentPenWidth));
        p.drawLine(x1, y1, x, y);
    } else {
        QImage overlay(dst.size(), QImage::Format_ARGB32);
        overlay.fill(Qt::transparent);
        QPainter p(&overlay);
        p.setPen(QPen(currentLineColor, currentPenWidth));
        p.drawLine(x1, y1, x, y);
        commitGraphicsDraw(&overlay, currentLineColor);
    }
    notifyCanvasChanged();
}

void HardwareWidget::moveTo(int x, int y)
{
    QMutexLocker locker(&graphicsMutex);
    currentX = static_cast<short>(x);
    currentY = static_cast<short>(y);
}

void HardwareWidget::getXY(short *x, short *y)
{
    QMutexLocker locker(&graphicsMutex);
    if (x) *x = currentX;
    if (y) *y = currentY;
}

void HardwareWidget::rectangle(int x1, int y1, int x2, int y2)
{
    QMutexLocker locker(&graphicsMutex);
    if (currentDrawingMode == 2) return;

    QColor penCol = currentLineColor;
    QColor fillCol = currentFillColor;
    if (currentDrawingMode == 0) { penCol = Qt::black; fillCol = Qt::black; }
    else if (currentDrawingMode == 1) { penCol = Qt::white; fillCol = Qt::white; }

    QImage &dst = activeCanvas();
    QRect r = QRect(QPoint(x1, y1), QPoint(x2, y2)).normalized();
    if (currentDrawingMode == 0 || currentDrawingMode == 1 || currentDrawingMode == 4) {
        QPainter p(&dst);
        p.setPen(QPen(penCol, currentPenWidth));
        p.setBrush(fillCol);
        p.drawRect(r);
    } else {
        QImage overlay(dst.size(), QImage::Format_ARGB32);
        overlay.fill(Qt::transparent);
        QPainter p(&overlay);
        p.setPen(QPen(currentLineColor, currentPenWidth));
        p.setBrush(currentFillColor);
        p.drawRect(r);
        commitGraphicsDraw(&overlay, currentLineColor);
    }
    notifyCanvasChanged();
}

void HardwareWidget::ellipse(int x1, int y1, int x2, int y2)
{
    QMutexLocker locker(&graphicsMutex);
    if (currentDrawingMode == 2) return;

    QColor penCol = currentLineColor;
    QColor fillCol = currentFillColor;
    if (currentDrawingMode == 0) { penCol = Qt::black; fillCol = Qt::black; }
    else if (currentDrawingMode == 1) { penCol = Qt::white; fillCol = Qt::white; }

    QImage &dst = activeCanvas();
    QRect r = QRect(QPoint(x1, y1), QPoint(x2, y2)).normalized();
    if (currentDrawingMode == 0 || currentDrawingMode == 1 || currentDrawingMode == 4) {
        QPainter p(&dst);
        p.setPen(QPen(penCol, currentPenWidth));
        p.setBrush(fillCol);
        p.drawEllipse(r);
    } else {
        QImage overlay(dst.size(), QImage::Format_ARGB32);
        overlay.fill(Qt::transparent);
        QPainter p(&overlay);
        p.setPen(QPen(currentLineColor, currentPenWidth));
        p.setBrush(currentFillColor);
        p.drawEllipse(r);
        commitGraphicsDraw(&overlay, currentLineColor);
    }
    notifyCanvasChanged();
}

void HardwareWidget::floodFill(int x1, int y1)
{
    QMutexLocker locker(&graphicsMutex);
    if (currentDrawingMode == 2) return;
    QImage &dst = activeCanvas();
    const int w = dst.width();
    const int h = dst.height();
    if (x1 < 0 || x1 >= w || y1 < 0 || y1 >= h) return;

    QColor fillCol = currentFillColor;
    if (currentDrawingMode == 0) fillCol = Qt::black;
    else if (currentDrawingMode == 1) fillCol = Qt::white;

    QRgb targetRgb = dst.pixel(x1, y1);
    QRgb fillRgb = applyRasterMode(currentDrawingMode, targetRgb, fillCol.rgb());
    if ((targetRgb & 0x00FFFFFFU) == (fillRgb & 0x00FFFFFFU)) {
        return;
    }

    std::deque<QPoint> q;
    q.emplace_back(x1, y1);
    while (!q.empty()) {
        QPoint pt = q.front();
        q.pop_front();
        int px = pt.x();
        int py = pt.y();
        if (px < 0 || px >= w || py < 0 || py >= h) continue;
        if ((dst.pixel(px, py) & 0x00FFFFFFU) != (targetRgb & 0x00FFFFFFU)) continue;

        int lx = px;
        while (lx > 0 && (dst.pixel(lx - 1, py) & 0x00FFFFFFU) == (targetRgb & 0x00FFFFFFU)) {
            --lx;
        }
        int rx = px;
        while (rx + 1 < w && (dst.pixel(rx + 1, py) & 0x00FFFFFFU) == (targetRgb & 0x00FFFFFFU)) {
            ++rx;
        }

        QRgb *row = reinterpret_cast<QRgb*>(dst.scanLine(py));
        for (int x = lx; x <= rx; ++x) {
            row[x] = fillRgb;
            if (py > 0 && (dst.pixel(x, py - 1) & 0x00FFFFFFU) == (targetRgb & 0x00FFFFFFU)) {
                q.emplace_back(x, py - 1);
            }
            if (py + 1 < h && (dst.pixel(x, py + 1) & 0x00FFFFFFU) == (targetRgb & 0x00FFFFFFU)) {
                q.emplace_back(x, py + 1);
            }
        }
    }
    notifyCanvasChanged();
}

void HardwareWidget::unfilledRectangle(int x1, int y1, int x2, int y2)
{
    QMutexLocker locker(&graphicsMutex);
    if (currentDrawingMode == 2) return;

    QColor penCol = currentLineColor;
    if (currentDrawingMode == 0) penCol = Qt::black;
    else if (currentDrawingMode == 1) penCol = Qt::white;

    QImage &dst = activeCanvas();
    QRect r = QRect(QPoint(x1, y1), QPoint(x2, y2)).normalized();
    if (currentDrawingMode == 0 || currentDrawingMode == 1 || currentDrawingMode == 4) {
        QPainter p(&dst);
        p.setPen(QPen(penCol, currentPenWidth));
        p.setBrush(Qt::NoBrush);
        p.drawRect(r);
    } else {
        QImage overlay(dst.size(), QImage::Format_ARGB32);
        overlay.fill(Qt::transparent);
        QPainter p(&overlay);
        p.setPen(QPen(currentLineColor, currentPenWidth));
        p.setBrush(Qt::NoBrush);
        p.drawRect(r);
        commitGraphicsDraw(&overlay, currentLineColor);
    }
    notifyCanvasChanged();
}

void HardwareWidget::unfilledEllipse(int x1, int y1, int x2, int y2)
{
    QMutexLocker locker(&graphicsMutex);
    if (currentDrawingMode == 2) return;

    QColor penCol = currentLineColor;
    if (currentDrawingMode == 0) penCol = Qt::black;
    else if (currentDrawingMode == 1) penCol = Qt::white;

    QImage &dst = activeCanvas();
    QRect r = QRect(QPoint(x1, y1), QPoint(x2, y2)).normalized();
    if (currentDrawingMode == 0 || currentDrawingMode == 1 || currentDrawingMode == 4) {
        QPainter p(&dst);
        p.setPen(QPen(penCol, currentPenWidth));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(r);
    } else {
        QImage overlay(dst.size(), QImage::Format_ARGB32);
        overlay.fill(Qt::transparent);
        QPainter p(&overlay);
        p.setPen(QPen(currentLineColor, currentPenWidth));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(r);
        commitGraphicsDraw(&overlay, currentLineColor);
    }
    notifyCanvasChanged();
}

void HardwareWidget::setDrawingMode(int mode)
{
    QMutexLocker locker(&graphicsMutex);
    if (mode >= 0 && mode <= 15) {
        currentDrawingMode = mode;
    } else if (mode == 16) {
        if (doubleBuffered) {
            canvas = backBuffer;
            doubleBuffered = false;
            canvasView->update();
        }
    } else if (mode == 17) {
        if (!doubleBuffered) {
            backBuffer = canvas;
            doubleBuffered = true;
        }
    }
}

void HardwareWidget::repaintCanvas()
{
    QMutexLocker locker(&graphicsMutex);
    if (doubleBuffered) {
        canvas = backBuffer;
    }
    canvasView->update();
}

void HardwareWidget::drawText(const std::string& str, int x, int y)
{
    QMutexLocker locker(&graphicsMutex);
    if (currentDrawingMode == 2) return;

    QColor penCol = currentLineColor;
    if (currentDrawingMode == 0) penCol = Qt::black;
    else if (currentDrawingMode == 1) penCol = Qt::white;

    QImage &dst = activeCanvas();
    QFont font("Monospace", 10);
    QFontMetrics fm(font);
    int baselineY = y + fm.ascent();

    if (currentDrawingMode == 0 || currentDrawingMode == 1 || currentDrawingMode == 4) {
        QPainter p(&dst);
        p.setPen(penCol);
        p.setFont(font);
        p.drawText(x, baselineY, QString::fromStdString(str));
    } else {
        QImage overlay(dst.size(), QImage::Format_ARGB32);
        overlay.fill(Qt::transparent);
        QPainter p(&overlay);
        p.setPen(currentLineColor);
        p.setFont(font);
        p.drawText(x, baselineY, QString::fromStdString(str));
        commitGraphicsDraw(&overlay, currentLineColor);
    }
    notifyCanvasChanged();
}

void HardwareWidget::clearCanvas()
{
    QMutexLocker locker(&graphicsMutex);
    canvas.fill(Qt::black);
    backBuffer.fill(Qt::black);
    currentX = 0;
    currentY = 0;
    canvasView->update();
}

void HardwareWidget::getWindowSize(unsigned short &w, unsigned short &h)
{
    QMutexLocker locker(&graphicsMutex);
    w = static_cast<unsigned short>(canvas.width());
    h = static_cast<unsigned short>(canvas.height());
}

void HardwareWidget::setWindowSize(unsigned short w, unsigned short h)
{
    QMutexLocker locker(&graphicsMutex);
    unsigned short targetW = (w == 0 && h == 0) ? 640 : w;
    unsigned short targetH = (w == 0 && h == 0) ? 480 : h;
    if (targetW > 0 && targetH > 0) {
        if (targetW != canvas.width() || targetH != canvas.height()) {
            canvas = QImage(targetW, targetH, QImage::Format_ARGB32_Premultiplied);
            canvas.fill(Qt::black);
            backBuffer = QImage(targetW, targetH, QImage::Format_ARGB32_Premultiplied);
            backBuffer.fill(Qt::black);
            canvasView->setFixedSize(targetW, targetH);
            if (canvasSizeLabel) {
                canvasSizeLabel->setText(tr("Resolution: %1 x %2").arg(targetW).arg(targetH));
            }
            canvasView->update();
        }
    }
}

uint8_t HardwareWidget::getSevenSegmentValue(int digitIndex) const
{
    return sevenSegWidget->getDigitValue(digitIndex);
}

uint8_t HardwareWidget::getLEDValue() const
{
    return ledBankWidget->getLeds();
}

uint8_t HardwareWidget::getSwitchValue() const
{
    if (!memory) return 0;
    return static_cast<uint8_t>(memory[switchLoc & ADDRMASK]);
}

uint8_t HardwareWidget::getPushbuttonValue() const
{
    if (!memory) return 0xFF;
    return static_cast<uint8_t>(memory[pbLoc & ADDRMASK]);
}

QColor HardwareWidget::getCanvasPixel(int x, int y) const
{
    QMutexLocker locker(&graphicsMutex);
    if (x >= 0 && x < canvas.width() && y >= 0 && y < canvas.height()) {
        return canvas.pixelColor(x, y);
    }
    return Qt::black;
}
