#ifndef TERMINALWIDGET_H
#define TERMINALWIDGET_H

#include <QWidget>
#include <QPlainTextEdit>
#include <QMutex>
#include <QWaitCondition>
#include <deque>
#include <string>
#include "../simulator/simio.h"

class HardwareWidget;

class TerminalWidget : public QPlainTextEdit, public SimIO
{
    Q_OBJECT

public:
    explicit TerminalWidget(QWidget *parent = nullptr);
    ~TerminalWidget() override = default;

    void setHardwareWidget(HardwareWidget *hw) { hardwareWidget = hw; }
    HardwareWidget* getHardwareWidget() const { return hardwareWidget; }

    // SimIO interface implementation
    void textOut(const std::string& str) override;
    void textOutCR(const std::string& str) override;
    void charOut(char ch) override;
    void charIn(char *ch) override;
    void textIn(char *buf, long *len, long *num) override;
    void doCRLF() override;
    void clear() override;

    // Graphics methods
    void setLineColor(int c) override;
    void setFillColor(int c) override;
    void setPenWidth(int w) override;
    void drawPixel(int x, int y) override;
    int  getPixel(int x, int y) override;
    void line(int x1, int y1, int x2, int y2) override;
    void lineTo(int x, int y) override;
    void moveTo(int x, int y) override;
    void getXY(short *x, short *y) override;
    void rectangle(int x1, int y1, int x2, int y2) override;
    void ellipse(int x1, int y1, int x2, int y2) override;
    void unfilledRectangle(int x1, int y1, int x2, int y2) override;
    void unfilledEllipse(int x1, int y1, int x2, int y2) override;
    void drawText(const std::string& str, int x, int y) override;
    void getWindowSize(unsigned short &w, unsigned short &h) override;
    void setWindowSize(unsigned short w, unsigned short h) override;

signals:
    void appendTextSignal(const QString &text);
    void clearSignal();

protected:
    void keyPressEvent(QKeyEvent *e) override;

private slots:
    void onAppendText(const QString &text);
    void onClear();

private:
    QMutex inputMutex;
    QWaitCondition inputCond;
    std::deque<char> keyBuffer;
    HardwareWidget *hardwareWidget = nullptr;
};

#endif // TERMINALWIDGET_H
