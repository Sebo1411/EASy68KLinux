#ifndef EASYBIN_FILEIO_H
#define EASYBIN_FILEIO_H

#include <string>
#include <cstdint>
#include <cstddef>

struct SrecInfo {
    uint32_t startAddr = 0xFFFFFFFF;
    uint32_t endAddr = 0;
    uint32_t entryAddr = 0;
    uint32_t length = 0;
    std::string headerS0;
};

bool loadSrec(const std::string& name, unsigned char* mem, size_t memSize, SrecInfo& info, std::string& errorMsg);
bool saveSRecord(const std::string& name, const unsigned char* mem, uint32_t fromAddr, uint32_t toAddr, uint32_t entryAddr, size_t memSize, std::string& errorMsg);
bool loadBinary(const std::string& name, unsigned char* mem, size_t memSize, uint32_t loadAddr, int split, size_t& bytesLoaded, std::string& errorMsg);
bool saveBinary(const std::string& baseName, const unsigned char* mem, uint32_t fromAddr, uint32_t length, size_t memSize, int split, std::string& errorMsg);
void finishSRecord(char* sRec, int bytes);

#endif // EASYBIN_FILEIO_H
