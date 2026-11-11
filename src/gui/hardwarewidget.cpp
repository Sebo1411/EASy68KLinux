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
#include "../simulator/extern.h"

extern int seg7loc, LEDloc, switchLoc, pbLoc;

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
    setMinimumSize(420, 80);
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

    // Bezel background
    p.fillRect(rect(), QColor(20, 20, 24));
    p.setPen(QColor(60, 60, 70));
    p.drawRect(rect().adjusted(0, 0, -1, -1));

    const int totalDigits = 8;
    const qreal digitWidth = (width() - 20) / static_cast<qreal>(totalDigits);
    const qreal digitHeight = height() - 16;
    const qreal startY = 8;

    for (int i = 0; i < totalDigits; ++i) {
        qreal x = 10 + i * digitWidth;
        qreal w = digitWidth - 8;
        qreal h = digitHeight;

        uint8_t mask = digits[i];

        QColor onColor(255, 30, 30);
        QColor offColor(45, 12, 12);

        // Segment dimensions
        qreal segThick = qMax(3.0, w * 0.16);

        // Define segment rects
        // a: top
        QRectF segA(x + segThick, startY, w - 2 * segThick - 4, segThick);
        // b: top-right
        QRectF segB(x + w - segThick - 4, startY + segThick, segThick, (h / 2) - segThick);
        // c: bottom-right
        QRectF segC(x + w - segThick - 4, startY + (h / 2), segThick, (h / 2) - segThick);
        // d: bottom
        QRectF segD(x + segThick, startY + h - segThick, w - 2 * segThick - 4, segThick);
        // e: bottom-left
        QRectF segE(x, startY + (h / 2), segThick, (h / 2) - segThick);
        // f: top-left
        QRectF segF(x, startY + segThick, segThick, (h / 2) - segThick);
        // g: middle
        QRectF segG(x + segThick, startY + (h / 2) - (segThick / 2), w - 2 * segThick - 4, segThick);
        // dp: dot
        QRectF segDP(x + w - 3, startY + h - segThick, segThick, segThick);

        p.setPen(Qt::NoPen);

        p.setBrush((mask & (1 << 0)) ? onColor : offColor);
        p.drawRoundedRect(segA, 1.5, 1.5);

        p.setBrush((mask & (1 << 1)) ? onColor : offColor);
        p.drawRoundedRect(segB, 1.5, 1.5);

        p.setBrush((mask & (1 << 2)) ? onColor : offColor);
        p.drawRoundedRect(segC, 1.5, 1.5);

        p.setBrush((mask & (1 << 3)) ? onColor : offColor);
        p.drawRoundedRect(segD, 1.5, 1.5);

        p.setBrush((mask & (1 << 4)) ? onColor : offColor);
        p.drawRoundedRect(segE, 1.5, 1.5);

        p.setBrush((mask & (1 << 5)) ? onColor : offColor);
        p.drawRoundedRect(segF, 1.5, 1.5);

        p.setBrush((mask & (1 << 6)) ? onColor : offColor);
        p.drawRoundedRect(segG, 1.5, 1.5);

        p.setBrush((mask & (1 << 7)) ? onColor : offColor);
        p.drawEllipse(segDP);
    }
}

// -----------------------------------------------------------------------------
// LedBankWidget
// -----------------------------------------------------------------------------
HardwareWidget::LedBankWidget::LedBankWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(420, 40);
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

    p.fillRect(rect(), QColor(25, 25, 30));
    p.setPen(QColor(60, 60, 70));
    p.drawRect(rect().adjusted(0, 0, -1, -1));

    const qreal step = width() / 8.0;
    const qreal radius = qMin(12.0, (height() - 14) / 2.0);

    QFont font = p.font();
    font.setPointSize(8);
    font.setBold(true);
    p.setFont(font);

    for (int i = 0; i < 8; ++i) {
        int bit = 7 - i; // Left is bit 7, Right is bit 0
        bool isOn = (ledsValue & (1 << bit)) != 0;

        qreal cx = step * i + step / 2.0;
        qreal cy = 18.0;

        // Draw LED ring
        p.setPen(QColor(80, 80, 90));
        p.setBrush(QColor(15, 15, 20));
        p.drawEllipse(QPointF(cx, cy), radius + 2.0, radius + 2.0);

        // Draw LED center
        if (isOn) {
            QRadialGradient grad(cx - 2, cy - 2, radius);
            grad.setColorAt(0.0, QColor(255, 120, 120));
            grad.setColorAt(0.7, QColor(255, 20, 20));
            grad.setColorAt(1.0, QColor(160, 0, 0));
            p.setBrush(grad);
            p.setPen(Qt::NoPen);
        } else {
            p.setBrush(QColor(40, 10, 10));
            p.setPen(QColor(25, 5, 5));
        }
        p.drawEllipse(QPointF(cx, cy), radius, radius);

        // Label
        p.setPen(QColor(180, 180, 190));
        p.drawText(QRectF(cx - 15, cy + radius + 1, 30, 16), Qt::AlignCenter, QString::number(bit));
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
}

void HardwareWidget::CanvasView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    QMutexLocker locker(&ownerWidget->graphicsMutex);
    p.drawImage(0, 0, ownerWidget->canvas);
}

// -----------------------------------------------------------------------------
// HardwareWidget
// -----------------------------------------------------------------------------
HardwareWidget::HardwareWidget(QWidget *parent)
    : QWidget(parent), canvas(640, 480, QImage::Format_ARGB32_Premultiplied)
{
    canvas.fill(Qt::black);
    setupUi();
    resetHardware();
}

void HardwareWidget::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(8);

    // 1. Simulated Peripherals Box
    auto *periphBox = new QGroupBox(tr("Simulated Peripherals (Hardware I/O)"), this);
    auto *periphLayout = new QVBoxLayout(periphBox);
    periphLayout->setSpacing(6);

    // 7-Segment Display
    auto *segLabel = new QLabel(tr("7-Segment Display (8 Digits @ $E00000..$E0000E):"), periphBox);
    segLabel->setStyleSheet("font-weight: bold;");
    periphLayout->addWidget(segLabel);
    sevenSegWidget = new SevenSegmentDisplayWidget(periphBox);
    periphLayout->addWidget(sevenSegWidget);

    // LED Indicators
    auto *ledLabel = new QLabel(tr("LED Indicators (8 LEDs @ $E00010):"), periphBox);
    ledLabel->setStyleSheet("font-weight: bold;");
    periphLayout->addWidget(ledLabel);
    ledBankWidget = new LedBankWidget(periphBox);
    periphLayout->addWidget(ledBankWidget);

    // Switches and Buttons Grid
    auto *controlsGrid = new QGridLayout();
    controlsGrid->setSpacing(6);

    auto *swGroupLabel = new QLabel(tr("Toggle Switches ($E00012):"), periphBox);
    swGroupLabel->setStyleSheet("font-weight: bold;");
    controlsGrid->addWidget(swGroupLabel, 0, 0, 1, 8);

    for (int i = 0; i < 8; ++i) {
        int bit = 7 - i;
        switches[bit] = new QCheckBox(QString("SW%1").arg(bit), periphBox);
        controlsGrid->addWidget(switches[bit], 1, i);
        connect(switches[bit], &QCheckBox::toggled, this, [this, bit](bool checked) {
            onSwitchToggled(bit, checked);
        });
    }

    auto *pbGroupLabel = new QLabel(tr("Pushbuttons (Normally High, $E00014):"), periphBox);
    pbGroupLabel->setStyleSheet("font-weight: bold;");
    controlsGrid->addWidget(pbGroupLabel, 2, 0, 1, 8);

    for (int i = 0; i < 8; ++i) {
        int bit = 7 - i;
        pushButtons[bit] = new QPushButton(QString("PB%1").arg(bit), periphBox);
        pushButtons[bit]->setMinimumWidth(36);
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

    // 2. 2D Graphics Canvas Box
    auto *gfxBox = new QGroupBox(tr("2D Graphics Canvas (TRAP #15 Tasks 80..96)"), this);
    auto *gfxLayout = new QVBoxLayout(gfxBox);

    auto *gfxToolbar = new QHBoxLayout();
    auto *sizeLabel = new QLabel(tr("Resolution: 640 x 480"), gfxBox);
    auto *clearBtn = new QPushButton(tr("Clear Canvas"), gfxBox);
    auto *saveBtn = new QPushButton(tr("Save Image..."), gfxBox);

    connect(clearBtn, &QPushButton::clicked, this, &HardwareWidget::onClearCanvasClicked);
    connect(saveBtn, &QPushButton::clicked, this, &HardwareWidget::onSaveCanvasClicked);

    gfxToolbar->addWidget(sizeLabel);
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
    if (x >= 0 && x < canvas.width() && y >= 0 && y < canvas.height()) {
        canvas.setPixelColor(x, y, currentLineColor);
    }
    canvasView->update();
}

int HardwareWidget::getPixel(int x, int y)
{
    QMutexLocker locker(&graphicsMutex);
    if (x >= 0 && x < canvas.width() && y >= 0 && y < canvas.height()) {
        return encodeColor(canvas.pixelColor(x, y));
    }
    return 0;
}

void HardwareWidget::line(int x1, int y1, int x2, int y2)
{
    QMutexLocker locker(&graphicsMutex);
    QPainter p(&canvas);
    p.setPen(QPen(currentLineColor, currentPenWidth));
    p.drawLine(x1, y1, x2, y2);
    currentX = static_cast<short>(x2);
    currentY = static_cast<short>(y2);
    canvasView->update();
}

void HardwareWidget::lineTo(int x, int y)
{
    QMutexLocker locker(&graphicsMutex);
    QPainter p(&canvas);
    p.setPen(QPen(currentLineColor, currentPenWidth));
    p.drawLine(currentX, currentY, x, y);
    currentX = static_cast<short>(x);
    currentY = static_cast<short>(y);
    canvasView->update();
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
    QPainter p(&canvas);
    p.setPen(QPen(currentLineColor, currentPenWidth));
    p.setBrush(currentFillColor);
    p.drawRect(QRect(QPoint(x1, y1), QPoint(x2, y2)));
    canvasView->update();
}

void HardwareWidget::ellipse(int x1, int y1, int x2, int y2)
{
    QMutexLocker locker(&graphicsMutex);
    QPainter p(&canvas);
    p.setPen(QPen(currentLineColor, currentPenWidth));
    p.setBrush(currentFillColor);
    p.drawEllipse(QRect(QPoint(x1, y1), QPoint(x2, y2)));
    canvasView->update();
}

void HardwareWidget::unfilledRectangle(int x1, int y1, int x2, int y2)
{
    QMutexLocker locker(&graphicsMutex);
    QPainter p(&canvas);
    p.setPen(QPen(currentLineColor, currentPenWidth));
    p.setBrush(Qt::NoBrush);
    p.drawRect(QRect(QPoint(x1, y1), QPoint(x2, y2)));
    canvasView->update();
}

void HardwareWidget::unfilledEllipse(int x1, int y1, int x2, int y2)
{
    QMutexLocker locker(&graphicsMutex);
    QPainter p(&canvas);
    p.setPen(QPen(currentLineColor, currentPenWidth));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QRect(QPoint(x1, y1), QPoint(x2, y2)));
    canvasView->update();
}

void HardwareWidget::drawText(const std::string& str, int x, int y)
{
    QMutexLocker locker(&graphicsMutex);
    QPainter p(&canvas);
    p.setPen(currentLineColor);
    QFont font("Monospace", 10);
    p.setFont(font);
    p.drawText(x, y, QString::fromStdString(str));
    canvasView->update();
}

void HardwareWidget::clearCanvas()
{
    QMutexLocker locker(&graphicsMutex);
    canvas.fill(Qt::black);
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
    if (w > 0 && h > 0) {
        canvas = canvas.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        canvasView->setFixedSize(w, h);
        canvasView->update();
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
