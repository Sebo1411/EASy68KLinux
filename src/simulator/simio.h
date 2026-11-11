#ifndef SIMIO_H
#define SIMIO_H

#include <string>
#include <iostream>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "def.h"

class SimIO {
public:
    virtual ~SimIO() = default;

    virtual void textOut(const std::string& str) { (void)str; }
    virtual void textOutCR(const std::string& str) { (void)str; }
    virtual void charOut(char ch) { (void)ch; }
    virtual void charIn(char *ch) { if (ch) *ch = 0; }
    virtual void textIn(char *buf, long *len, long *num) { (void)buf; (void)len; (void)num; }
    virtual void doCRLF() {}
    virtual void clear() {}
    virtual void gotorc(int r, int c) { (void)r; (void)c; }
    virtual void getrc(short *d1) { if (d1) { d1[0] = 0; d1[1] = 0; } }
    virtual void getCharAt(unsigned short r, unsigned short c, char *d1) { (void)r; (void)c; if (d1) *d1 = ' '; }
    virtual void setFontProperties(int c, int s) { (void)c; (void)s; }
    virtual void setWindowSize(unsigned short width, unsigned short height) { (void)width; (void)height; }
    virtual void getWindowSize(unsigned short &width, unsigned short &height) { width = 640; height = 480; }
    virtual void drawPixel(int x, int y) { (void)x; (void)y; }
    virtual int  getPixel(int x, int y) { (void)x; (void)y; return 0; }
    virtual void line(int x1, int y1, int x2, int y2) { (void)x1; (void)y1; (void)x2; (void)y2; }
    virtual void lineTo(int x, int y) { (void)x; (void)y; }
    virtual void moveTo(int x, int y) { (void)x; (void)y; }
    virtual void getXY(short *x, short *y) { if (x) *x = 0; if (y) *y = 0; }
    virtual void setLineColor(int c) { (void)c; }
    virtual void setFillColor(int c) { (void)c; }
    virtual void rectangle(int x1, int y1, int x2, int y2) { (void)x1; (void)y1; (void)x2; (void)y2; }
    virtual void ellipse(int x1, int y1, int x2, int y2) { (void)x1; (void)y1; (void)x2; (void)y2; }
    virtual void floodFill(int x1, int y1) { (void)x1; (void)y1; }
    virtual void unfilledRectangle(int x1, int y1, int x2, int y2) { (void)x1; (void)y1; (void)x2; (void)y2; }
    virtual void unfilledEllipse(int x1, int y1, int x2, int y2) { (void)x1; (void)y1; (void)x2; (void)y2; }
    virtual void setDrawingMode(int m) { (void)m; }
    virtual void setPenWidth(int w) { (void)w; }
    virtual void getKeyState(long *d1) { if (d1) *d1 = 0; }
    virtual void displayFileDialog(long *mode, int A1, int A2, int A3, short *result) {
        (void)mode; (void)A1; (void)A2; (void)A3;
        if (result) *result = -1;
    }
    virtual void drawText(const std::string& str, int x, int y) { (void)str; (void)x; (void)y; }
    virtual void scrollRect(unsigned short r, unsigned short c, unsigned short w, unsigned short h, unsigned short dir) {
        (void)r; (void)c; (void)w; (void)h; (void)dir;
    }
    virtual void playSound(char *fileName, short *result) { (void)fileName; if (result) *result = -1; }
    virtual void loadSound(char *fileName, int waveIndex) { (void)fileName; (void)waveIndex; }
    virtual void playSoundMem(int waveIndex, short *result) { (void)waveIndex; if (result) *result = -1; }
    virtual void controlSound(int control, int waveIndex, short *result) { (void)control; (void)waveIndex; if (result) *result = -1; }
    virtual void playSoundDX(char *fileName, short *result) { (void)fileName; if (result) *result = -1; }
    virtual void loadSoundDX(char *fileName, int waveIndex, short *result) { (void)fileName; (void)waveIndex; if (result) *result = -1; }
    virtual void playSoundMemDX(int waveIndex, short *result) { (void)waveIndex; if (result) *result = -1; }
    virtual void controlSoundDX(int control, int waveIndex, short *result) { (void)control; (void)waveIndex; if (result) *result = -1; }
    virtual void setupWindow() {}
    virtual void FormPaint(void*) {}
    virtual void closeNetConnection(int closeIP, int *result) { (void)closeIP; if (result) *result = -1; }
    virtual void createNetClient(int settings, char *server, int *result) { (void)settings; (void)server; if (result) *result = -1; }
    virtual void createNetServer(int settings, int *result) { (void)settings; if (result) *result = -1; }
    virtual void getLocalIP(char *localIP, int *result) { (void)localIP; if (result) *result = -1; }
    virtual void receiveNet(int settings, char *buffer, int *count, char *senderIP, int *result) {
        (void)settings; (void)buffer; (void)count; (void)senderIP; if (result) *result = -1;
    }
    virtual void receivePortNet(long *D0, long *D1, char *buffer, char *senderIP) {
        (void)D0; (void)D1; (void)buffer; (void)senderIP;
    }
    virtual void sendNet(int settings, char *data, char *remoteIP, int *count, int *result) {
        (void)settings; (void)data; (void)remoteIP; (void)count; if (result) *result = -1;
    }
    virtual void sendPortNet(long *D0, long *D1, char *data, char *remoteIP) {
        (void)D0; (void)D1; (void)data; (void)remoteIP;
    }
    virtual void initComm(int cid, char *portName, short *result) { (void)cid; (void)portName; if (result) *result = -1; }
    virtual void readComm(int cid, unsigned char *n, char *str, short *result) {
        (void)cid; (void)n; (void)str; if (result) *result = -1;
    }
    virtual void sendComm(int cid, unsigned char *n, char *str, short *result) {
        (void)cid; (void)n; (void)str; if (result) *result = -1;
    }
    virtual void setCommParams(int cid, int settings, short *result) { (void)cid; (void)settings; if (result) *result = -1; }
    virtual void ResetSounds() {}

    bool fullScreen = false;
    void* BackBuffer = nullptr;
    void* Font = nullptr;
};

// Standard CLI Implementation for headless & terminal usage
class CliSimIO : public SimIO {
public:
    void textOut(const std::string& str) override {
        std::cout << str << std::flush;
    }

    void textOutCR(const std::string& str) override {
        std::cout << str << std::endl;
    }

    void charOut(char ch) override {
        std::cout.put(ch);
        std::cout.flush();
    }

    void charIn(char *ch) override {
        if (!ch) return;
        int c = std::getchar();
        *ch = (c == EOF) ? 0 : static_cast<char>(c);
    }

    void textIn(char *buf, long *len, long *num) override {
        if (!buf) return;
        long requested = len ? *len : 255;
        if (requested <= 0) {
            // A zero capacity used to make maxLen-1 underflow and write
            // buf[-1].  Treat it as "nothing to read".
            if (len) *len = 0;
            if (num) *num = 0;
            return;
        }
        size_t maxLen = static_cast<size_t>(requested);
        std::string line;
        if (std::getline(std::cin, line)) {
            if (line.length() >= maxLen) {
                line.resize(maxLen - 1);
            }
            std::memcpy(buf, line.c_str(), line.length() + 1);
            if (len) *len = static_cast<long>(line.length());
            if (num) {
                try {
                    *num = std::stol(line, nullptr, 0);
                } catch (...) {
                    *num = 0;
                }
            }
        } else {
            buf[0] = '\0';
            if (len) *len = 0;
            if (num) *num = 0;
        }
    }

    void doCRLF() override {
        std::cout << std::endl;
    }

    void clear() override {
        // ANSI clear screen
        std::cout << "\033[2J\033[H" << std::flush;
    }
};

extern SimIO *simIO;
void setSimIO(SimIO *io);

#endif // SIMIO_H
