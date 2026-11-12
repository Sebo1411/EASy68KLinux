#ifndef SRECORDPARSER_H
#define SRECORDPARSER_H

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>

// 256-entry branchless lookup table for ASCII hex characters
// -1 for non-hex characters
// 0..15 for '0'-'9', 'A'-'F', 'a'-'f'
inline constexpr int8_t kHexDigitLut[256] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0x00-0x0F
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0x10-0x1F
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0x20-0x2F
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, -1, -1, -1, -1, -1, -1, // 0x30-0x3F ('0'-'9')
    -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0x40-0x4F ('A'-'F')
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0x50-0x5F
    -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0x60-0x6F ('a'-'f')
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0x70-0x7F
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0x80-0x8F
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0x90-0x9F
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0xA0-0xAF
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0xB0-0xBF
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0xC0-0xCF
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0xD0-0xDF
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0xE0-0xEF
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1  // 0xF0-0xFF
};

inline constexpr int8_t hexDigitVal(char c) noexcept {
    return kHexDigitLut[static_cast<uint8_t>(c)];
}

inline constexpr bool parseHexByte(char hi, char lo, uint8_t &val) noexcept {
    int8_t h = hexDigitVal(hi);
    int8_t l = hexDigitVal(lo);
    if (h < 0 || l < 0) return false;
    val = static_cast<uint8_t>((static_cast<uint8_t>(h) << 4) | static_cast<uint8_t>(l));
    return true;
}

enum class SRecordLineState : uint8_t {
    StartS = 0,
    RecordType,
    ByteCount,
    Address,
    PayloadAndChecksum,
    Complete,
    Invalid
};

struct SRecordLineFSM {
    SRecordLineState state = SRecordLineState::StartS;
    char recordType = 0;
    int byteCount = 0;
    uint32_t address = 0;
    int addrByteLen = 0;
    int dataByteLen = 0;
    uint8_t runningChecksum = 0;
    std::vector<uint8_t> payload;
    uint8_t readChecksum = 0;
    bool checksumValid = false;

    void reset() {
        state = SRecordLineState::StartS;
        recordType = 0;
        byteCount = 0;
        address = 0;
        addrByteLen = 0;
        dataByteLen = 0;
        runningChecksum = 0;
        payload.clear();
        readChecksum = 0;
        checksumValid = false;
    }

    bool parseLine(const char* line) {
        reset();
        if (!line) {
            state = SRecordLineState::Invalid;
            return false;
        }

        const char* p = line;
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
            p++;
        }
        if (*p == '\0') {
            return true;
        }

        // 1. StartS
        if (*p != 'S') {
            state = SRecordLineState::Invalid;
            return false;
        }
        p++;
        state = SRecordLineState::RecordType;

        // 2. RecordType
        recordType = *p++;
        switch (recordType) {
        case '0':
        case '1':
        case '5':
        case '9':
            addrByteLen = 2;
            break;
        case '2':
        case '8':
            addrByteLen = 3;
            break;
        case '3':
        case '7':
            addrByteLen = 4;
            break;
        default:
            state = SRecordLineState::Invalid;
            return false;
        }
        state = SRecordLineState::ByteCount;

        // 3. ByteCount
        if (*p == '\0' || *(p + 1) == '\0') {
            state = SRecordLineState::Invalid;
            return false;
        }
        uint8_t bcVal = 0;
        if (!parseHexByte(*p, *(p + 1), bcVal)) {
            state = SRecordLineState::Invalid;
            return false;
        }
        p += 2;
        byteCount = bcVal;
        if (byteCount < addrByteLen + 1) {
            state = SRecordLineState::Invalid;
            return false;
        }
        runningChecksum = bcVal;
        dataByteLen = byteCount - addrByteLen - 1;
        state = SRecordLineState::Address;

        // 4. Address
        address = 0;
        for (int i = 0; i < addrByteLen; ++i) {
            if (*p == '\0' || *(p + 1) == '\0') {
                state = SRecordLineState::Invalid;
                return false;
            }
            uint8_t aByte = 0;
            if (!parseHexByte(*p, *(p + 1), aByte)) {
                state = SRecordLineState::Invalid;
                return false;
            }
            p += 2;
            address = (address << 8) | aByte;
            runningChecksum += aByte;
        }
        state = SRecordLineState::PayloadAndChecksum;

        // 5. Payload
        payload.reserve(dataByteLen);
        for (int i = 0; i < dataByteLen; ++i) {
            if (*p == '\0' || *(p + 1) == '\0') {
                state = SRecordLineState::Invalid;
                return false;
            }
            uint8_t dByte = 0;
            if (!parseHexByte(*p, *(p + 1), dByte)) {
                state = SRecordLineState::Invalid;
                return false;
            }
            p += 2;
            payload.push_back(dByte);
            runningChecksum += dByte;
        }

        // 6. Checksum
        if (*p == '\0' || *(p + 1) == '\0') {
            state = SRecordLineState::Invalid;
            return false;
        }
        if (!parseHexByte(*p, *(p + 1), readChecksum)) {
            state = SRecordLineState::Invalid;
            return false;
        }
        p += 2;

        checksumValid = (((runningChecksum + readChecksum) & 0xFF) == 0xFF);
        state = SRecordLineState::Complete;
        return true;
    }
};

#endif // SRECORDPARSER_H
