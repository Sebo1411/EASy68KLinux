#ifndef TERMINALWIDGET_H
#define TERMINALWIDGET_H

#include <QWidget>
#include <QPlainTextEdit>
#include <QMutex>
#include <QWaitCondition>
#include <QTextCharFormat>
#include <array>
#include <deque>
#include <string>
#include <vector>
#include "../simulator/simio.h"

class HardwareWidget;
#ifdef EASY68K_HAS_MULTIMEDIA
class QSoundEffect;
#endif

class TerminalWidget : public QPlainTextEdit, public SimIO
{
    Q_OBJECT

public:
    explicit TerminalWidget(QWidget *parent = nullptr);
    ~TerminalWidget() override;

    void setHardwareWidget(HardwareWidget *hw);
    HardwareWidget* getHardwareWidget() const { return hardwareWidget; }

    void handleHostKeyEvent(QKeyEvent *e, bool pressed);

    // SimIO text & cursor interface implementation
    void textOut(const std::string& str) override;
    void textOutCR(const std::string& str) override;
    void charOut(char ch) override;
    void charIn(char *ch) override;
    void textIn(char *buf, long *len, long *num) override;
    void doCRLF() override;
    void clear() override;
    void gotorc(int r, int c) override;
    void getrc(short *d1) override;
    void getCharAt(unsigned short r, unsigned short c, char *d1) override;
    void setFontProperties(int c, int s) override;
    void scrollRect(unsigned short r, unsigned short c, unsigned short h, unsigned short w, unsigned short dir) override;
    void getKeyState(long *d1) override;
    void displayFileDialog(long *mode, int A1, int A2, int A3, short *result) override;

    // Sound methods (TRAP #15 Tasks 70-77)
    void playSound(char *fileName, short *result) override;
    void loadSound(char *fileName, int waveIndex) override;
    void playSoundMem(int waveIndex, short *result) override;
    void controlSound(int control, int waveIndex, short *result) override;
    void playSoundDX(char *fileName, short *result) override;
    void loadSoundDX(char *fileName, int waveIndex, short *result) override;
    void playSoundMemDX(int waveIndex, short *result) override;
    void controlSoundDX(int control, int waveIndex, short *result) override;
    void ResetSounds() override;

    // Graphics methods forwarded to HardwareWidget
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
    void floodFill(int x1, int y1) override;
    void unfilledRectangle(int x1, int y1, int x2, int y2) override;
    void unfilledEllipse(int x1, int y1, int x2, int y2) override;
    void setDrawingMode(int m) override;
    void FormPaint(void *p) override;
    void drawText(const std::string& str, int x, int y) override;
    void getWindowSize(unsigned short &w, unsigned short &h) override;
    void setWindowSize(unsigned short w, unsigned short h) override;

signals:
    void appendTextSignal(const QString &text);
    void clearSignal();
    void inputWaitChanged(bool waiting);

protected:
    void keyPressEvent(QKeyEvent *e) override;
    void keyReleaseEvent(QKeyEvent *e) override;

private slots:
    void onAppendText(const QString &text);
    void onClear();

private:
    static uint8_t qtKeyToEasy68kCode(int key);
    void insertTextAtCursor(const QString &text);
    void removeLastCharOnScreen();

    QMutex inputMutex;
    QWaitCondition inputCond;
    std::deque<char> keyBuffer;
    HardwareWidget *hardwareWidget = nullptr;

    std::array<bool, 256> keyStates{};
    uint16_t lastKeyDownCode = 0;
    uint16_t lastKeyUpCode = 0;
    QTextCharFormat activeTextFormat;

    std::array<QString, 256> stdSoundPaths;
    std::array<QString, 256> dxSoundPaths;
#ifdef EASY68K_HAS_MULTIMEDIA
    QSoundEffect *stdSoundPlayer = nullptr;
    std::array<QSoundEffect*, 256> dxSoundPlayers{};
    std::vector<QSoundEffect*> oneshotDxPlayers;
#endif
};

#endif // TERMINALWIDGET_H
