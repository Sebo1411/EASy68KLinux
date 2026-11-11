#ifndef SIMMESSAGE_H
#define SIMMESSAGE_H

#include <string>
#include <functional>

typedef std::function<void(const std::string&)> SimMessageCallback;

void setSimMessageCallback(SimMessageCallback cb);
void simMessage(const std::string& msg);
void simMessage(const char* fmt, ...);

#endif // SIMMESSAGE_H
