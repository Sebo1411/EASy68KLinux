#include "simmessage.h"
#include <iostream>
#include <cstdarg>
#include <vector>

static SimMessageCallback g_msgCallback = nullptr;

void setSimMessageCallback(SimMessageCallback cb) {
    g_msgCallback = cb;
}

void simMessage(const std::string& msg) {
    if (g_msgCallback) {
        g_msgCallback(msg);
    } else {
        std::cerr << "[SIM] " << msg << std::endl;
    }
}

void simMessage(const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    simMessage(std::string(buf));
}
