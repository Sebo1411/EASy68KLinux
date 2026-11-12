#ifndef HARDWAREWIDGET_H
#define HARDWAREWIDGET_H

#include <QWidget>
#include <QImage>
#include <QMutex>
#include <QColor>
#include <QPushButton>
#include <QCheckBox>
#include <QLineEdit>
#include <QComboBox>
#include <QTimer>
#include <QLabel>
#include <string>

class HardwareWidget : public QWidget
{
    Q_OBJECT

public:
    explicit HardwareWidget(QWidget *parent = nullptr);
    ~HardwareWidget() override = default;

    // Hardware panel update from simulator memory
    void updateHardware();
    void resetHardware();

    // Graphics Canvas TRAP #15 implementations
    void setLineColor(int c);
    void setFillColor(int c);
    void setPenWidth(int w);
    void drawPixel(int x, int y);
    int  getPixel(int x, int y);
    void line(int x1, int y1, int x2, int y2);
    void lineTo(int x, int y);
    void moveTo(int x, int y);
    void getXY(short *x, short *y);
    void rectangle(int x1, int y1, int x2, int y2);
    void ellipse(int x1, int y1, int x2, int y2);
    void unfilledRectangle(int x1, int y1, int x2, int y2);
    void unfilledEllipse(int x1, int y1, int x2, int y2);
    void drawText(const std::string& str, int x, int y);
    void clearCanvas();
    void getWindowSize(unsigned short &w, unsigned short &h);
    void setWindowSize(unsigned short w, unsigned short h);

    // Getters for testing and inspection
    uint8_t getSevenSegmentValue(int digitIndex) const;
    QWidget* getSevenSegmentWidget() const { return reinterpret_cast<QWidget*>(sevenSegWidget); }
    uint8_t getLEDValue() const;
    QWidget* getLedBankWidget() const { return reinterpret_cast<QWidget*>(ledBankWidget); }
    uint8_t getSwitchValue() const;
    uint8_t getPushbuttonValue() const;
    QColor  getCanvasPixel(int x, int y) const;
    const QImage& getCanvasImage() const { return canvas; }

    // Interrupt handling
    void triggerInterrupt(int level);
    void setAutoInterrupt(int level, int intervalMs, bool enabled);
    bool isAutoInterruptEnabled() const;

signals:
    void hardwareResetRequested();

public slots:
    void onClearCanvasClicked();
    void onSaveCanvasClicked();
    void onMemoryMapChanged();
    void onAutoIrqChanged();

private slots:
    void onSwitchToggled(int index, bool checked);
    void onPushbuttonPressed(int index);
    void onPushbuttonReleased(int index);

private:
    void setupUi();

    // 7-segment digit custom painter
    class SevenSegmentDisplayWidget : public QWidget {
    public:
        explicit SevenSegmentDisplayWidget(QWidget *parent = nullptr);
        void setDigitValue(int digitIndex, uint8_t val);
        uint8_t getDigitValue(int digitIndex) const { return digits[digitIndex]; }
    protected:
        void paintEvent(QPaintEvent *event) override;
    private:
        uint8_t digits[8];
    };

    // LED Bank custom painter
    class LedBankWidget : public QWidget {
    public:
        explicit LedBankWidget(QWidget *parent = nullptr);
        void setLeds(uint8_t val);
        uint8_t getLeds() const { return ledsValue; }
    protected:
        void paintEvent(QPaintEvent *event) override;
    private:
        uint8_t ledsValue = 0;
    };

    // Canvas drawing widget
    class CanvasView : public QWidget {
    public:
        explicit CanvasView(HardwareWidget *owner, QWidget *parent = nullptr);
    protected:
        void paintEvent(QPaintEvent *event) override;
    private:
        HardwareWidget *ownerWidget;
    };

    SevenSegmentDisplayWidget *sevenSegWidget;
    LedBankWidget *ledBankWidget;
    QCheckBox *switches[8];
    QPushButton *pushButtons[8];
    CanvasView *canvasView;

    // Interrupt & Memory Map widgets
    QPushButton *irqButtons[7];
    QComboBox *autoIrqCombo = nullptr;
    QLineEdit *autoIrqIntervalEdit = nullptr;
    QCheckBox *autoIrqCheck = nullptr;
    QTimer *autoIrqTimer = nullptr;
    QPushButton *hwResetBtn = nullptr;
    QCheckBox *romMapCheck;
    QLineEdit *romStartEdit;
    QLineEdit *romEndEdit;

    QCheckBox *readMapCheck;
    QLineEdit *readStartEdit;
    QLineEdit *readEndEdit;

    QCheckBox *protectedMapCheck;
    QLineEdit *protectedStartEdit;
    QLineEdit *protectedEndEdit;

    QCheckBox *invalidMapCheck;
    QLineEdit *invalidStartEdit;
    QLineEdit *invalidEndEdit;

    // Graphics state protected by mutex
    mutable QMutex graphicsMutex;
    QImage canvas;
    QColor currentLineColor = Qt::white;
    QColor currentFillColor = Qt::black;
    int currentPenWidth = 1;
    short currentX = 0;
    short currentY = 0;
};

#endif // HARDWAREWIDGET_H
