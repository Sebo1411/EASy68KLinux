#include "fileio.h"
#include "../simulator/srecparser.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <memory>

static const int MAXBUF = 515;
static const int SRECDATA = 32;

void finishSRecord(char *sRec, int bytes)
{
    unsigned char checksum = 0;
    char byteC[3];

    // Point to byte count location (offset 2)
    snprintf(byteC, sizeof(byteC), "%02X", bytes);
    sRec[2] = byteC[0];
    sRec[3] = byteC[1];

    char* ptr = sRec + 2;
    for (int i = 0; i < bytes; ++i) {
        uint8_t byteVal = 0;
        parseHexByte(ptr[0], ptr[1], byteVal);
        ptr += 2;
        checksum += byteVal;
    }
    snprintf(ptr, 4, "%02X\n", static_cast<unsigned char>(~checksum & 0xFF));
}

bool loadSrec(const std::string& name, unsigned char* mem, size_t memSize, SrecInfo& info, std::string& errorMsg)
{
    FILE* fp = fopen(name.c_str(), "rt");
    if (!fp) {
        errorMsg = "Cannot open S-record file: " + name;
        return false;
    }

    char lbuf[MAXBUF + 1];
    int line = 0;
    bool sRecError = false;
    uint32_t lowAddr = 0xFFFFFFFF;
    uint32_t highAddr = 0;

    info.startAddr = 0xFFFFFFFF;
    info.endAddr = 0;
    info.entryAddr = 0;
    info.length = 0;
    info.headerS0.clear();

    SRecordLineFSM fsm;

    while (fgets(lbuf, MAXBUF, fp) != nullptr) {
        line++;
        if (!fsm.parseLine(lbuf)) {
            errorMsg = "Invalid S-Record header on line " + std::to_string(line);
            sRecError = true;
            break;
        }

        if (fsm.state != SRecordLineState::Complete) {
            continue;
        }

        if (fsm.recordType == '5') {
            continue;
        }

        if (fsm.recordType == '7' || fsm.recordType == '8' || fsm.recordType == '9') {
            info.entryAddr = fsm.address;
            break;
        }

        if (fsm.recordType == '0') {
            for (uint8_t b : fsm.payload) {
                info.headerS0 += (b >= 32 && b <= 126) ? static_cast<char>(b) : '.';
            }
        } else if (fsm.recordType == '1' || fsm.recordType == '2' || fsm.recordType == '3') {
            uint32_t loc = fsm.address;
            for (uint8_t b : fsm.payload) {
                if (loc >= memSize) {
                    errorMsg = "Address exceeds available memory size on line " + std::to_string(line);
                    sRecError = true;
                    break;
                }
                if (loc < lowAddr) lowAddr = loc;
                if (loc > highAddr) highAddr = loc;
                if (mem) {
                    mem[loc] = b;
                }
                loc++;
            }
            if (sRecError) break;
        }
    }

    fclose(fp);

    if (sRecError)
        return false;

    if (lowAddr <= highAddr) {
        info.startAddr = lowAddr;
        info.endAddr = highAddr;
        info.length = highAddr - lowAddr + 1;
    } else {
        info.startAddr = 0;
        info.endAddr = 0;
        info.length = 0;
    }

    return true;
}

bool saveSRecord(const std::string& name, const unsigned char* mem, uint32_t fromAddr, uint32_t toAddr, uint32_t entryAddr, size_t memSize, std::string& errorMsg)
{
    if (fromAddr > toAddr) {
        errorMsg = "Invalid memory range: start address greater than end address";
        return false;
    }
    if (fromAddr >= memSize || toAddr >= memSize) {
        errorMsg = "Memory range exceeds available memory size";
        return false;
    }

    std::ofstream oFile(name);
    if (!oFile) {
        errorMsg = "Cannot create output S-record file: " + name;
        return false;
    }

    // Write S0 Header
    char sRecord[128];
    std::strcpy(sRecord, "S0210000535245434F5244202020313143524541544544204259204541535942494E");
    finishSRecord(sRecord, 0x21);
    oFile << sRecord;

    uint32_t sRecAddr = fromAddr;
    uint32_t totalBytes = toAddr - fromAddr + 1;
    uint32_t dataCount = 0;

    while (dataCount < totalBytes) {
        int byteCount = 0;
        if ((sRecAddr & 0xFFFF) == sRecAddr) {
            snprintf(sRecord, sizeof(sRecord), "S1  %04X", sRecAddr);
            byteCount = 2;
        } else if ((sRecAddr & 0xFFFFFF) == sRecAddr) {
            snprintf(sRecord, sizeof(sRecord), "S2  %06X", sRecAddr);
            byteCount = 3;
        } else {
            snprintf(sRecord, sizeof(sRecord), "S3  %08X", sRecAddr);
            byteCount = 4;
        }

        char* sRecPtr = sRecord + 4 + byteCount * 2;
        int sRecData = 0;

        while (sRecData < SRECDATA && dataCount < totalBytes) {
            snprintf(sRecPtr, 3, "%02X", mem[sRecAddr++]);
            sRecPtr += 2;
            sRecData++;
            dataCount++;
        }

        finishSRecord(sRecord, sRecData + byteCount + 1);
        oFile << sRecord;
    }

    // Write termination record S7/S8/S9
    if ((entryAddr & 0xFFFF) == entryAddr) {
        snprintf(sRecord, sizeof(sRecord), "S9  %04X", entryAddr);
        finishSRecord(sRecord, 3);
    } else if ((entryAddr & 0xFFFFFF) == entryAddr) {
        snprintf(sRecord, sizeof(sRecord), "S8  %06X", entryAddr);
        finishSRecord(sRecord, 4);
    } else {
        snprintf(sRecord, sizeof(sRecord), "S7  %08X", entryAddr);
        finishSRecord(sRecord, 5);
    }
    oFile << sRecord;

    return true;
}

bool loadBinary(const std::string& name, unsigned char* mem, size_t memSize, uint32_t loadAddr, int split, size_t& bytesLoaded, std::string& errorMsg)
{
    std::ifstream iFile(name, std::ios::binary);
    if (!iFile) {
        errorMsg = "Cannot open binary file: " + name;
        return false;
    }

    if (split <= 0) split = 1;

    if (loadAddr >= memSize) {
        errorMsg = "Load address exceeds available memory size";
        return false;
    }

    bytesLoaded = 0;
    uint32_t addr = loadAddr;

    char ch;
    while (addr < memSize && iFile.get(ch)) {
        mem[addr] = static_cast<unsigned char>(ch);
        // Record the span covered, including holes for split images, so the
        // caller can compute the last written address.  Use 64-bit math to
        // avoid uint32 overflow at the top of memory.
        const uint64_t next = static_cast<uint64_t>(addr) + static_cast<uint64_t>(split);
        if (next >= memSize) {
            bytesLoaded = static_cast<size_t>(memSize - loadAddr);
            addr = static_cast<uint32_t>(memSize);
            break;
        }
        addr = static_cast<uint32_t>(next);
        bytesLoaded = static_cast<size_t>(addr - loadAddr);
    }

    return true;
}

bool saveBinary(const std::string& baseName, const unsigned char* mem, uint32_t fromAddr, uint32_t length, size_t memSize, int split, std::string& errorMsg)
{
    if (fromAddr > memSize || length > memSize - fromAddr) {
        errorMsg = "Memory range exceeds available memory size";
        return false;
    }
    if (split <= 0) split = 1;

    if (split == 1) {
        std::ofstream oFile(baseName, std::ios::binary);
        if (!oFile) {
            errorMsg = "Cannot create binary file: " + baseName;
            return false;
        }
        oFile.write(reinterpret_cast<const char*>(&mem[fromAddr]), length);
        return true;
    }

    // Split mode 2 or 4
    std::string base = baseName;
    std::string ext;
    size_t dotPos = baseName.rfind('.');
    if (dotPos != std::string::npos) {
        ext = baseName.substr(dotPos);
        base = baseName.substr(0, dotPos);
    }

    std::vector<std::unique_ptr<std::ofstream>> files;
    for (int s = 0; s < split; ++s) {
        std::string partName = base + "_" + std::to_string(s) + ext;
        auto ofs = std::make_unique<std::ofstream>(partName, std::ios::binary);
        if (!*ofs) {
            errorMsg = "Cannot create part file: " + partName;
            return false;
        }
        files.push_back(std::move(ofs));
    }

    for (uint32_t i = 0; i < length; ++i) {
        int streamIndex = i % split;
        files[streamIndex]->put(static_cast<char>(mem[fromAddr + i]));
    }

    return true;
}
