#include "disassembler.h"
#include <cstdio>
#include <vector>

namespace {

static inline std::string hex2(uint8_t v) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%02X", v);
    return buf;
}

static inline std::string hex4(uint16_t v) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%04X", v);
    return buf;
}

static inline std::string hex8(uint32_t v) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%08X", v);
    return buf;
}

static const char *kCCNames[16] = {
    "T", "F", "HI", "LS", "CC", "CS", "NE", "EQ",
    "VC", "VS", "PL", "MI", "GE", "LT", "GT", "LE"
};

std::string formatRegisterList(uint16_t mask, bool reverse) {
    std::string res;
    for (int i = 0; i < 16; ++i) {
        if (mask & (1 << i)) {
            std::string r;
            if (reverse) {
                if (i < 8) r = "A" + std::to_string(7 - i);
                else r = "D" + std::to_string(15 - i);
            } else {
                if (i < 8) r = "D" + std::to_string(i);
                else r = "A" + std::to_string(i - 8);
            }
            if (!res.empty()) res += "/";
            res += r;
        }
    }
    return res.empty() ? "None" : res;
}

std::string formatEA(int mode, int reg, int sizeBytes, const uint8_t *mem, uint32_t memSize, uint32_t &extPc, bool &ok, bool isControl = false) {
    if (!ok) return "(invalid)";

    auto fetchWord = [&](uint16_t &w) -> bool {
        if (extPc + 1 >= memSize) { ok = false; return false; }
        w = (static_cast<uint16_t>(mem[extPc]) << 8) | static_cast<uint16_t>(mem[extPc + 1]);
        extPc += 2;
        return true;
    };

    auto fetchLong = [&](uint32_t &l) -> bool {
        if (extPc + 3 >= memSize) { ok = false; return false; }
        l = (static_cast<uint32_t>(mem[extPc]) << 24) |
            (static_cast<uint32_t>(mem[extPc + 1]) << 16) |
            (static_cast<uint32_t>(mem[extPc + 2]) << 8) |
            static_cast<uint32_t>(mem[extPc + 3]);
        extPc += 4;
        return true;
    };

    switch (mode) {
    case 0:
        return "D" + std::to_string(reg);
    case 1:
        return "A" + std::to_string(reg);
    case 2:
        return "(A" + std::to_string(reg) + ")";
    case 3:
        return "(A" + std::to_string(reg) + ")+";
    case 4:
        return "-(A" + std::to_string(reg) + ")";
    case 5: {
        uint16_t w = 0;
        if (!fetchWord(w)) return "(invalid)";
        int16_t disp = static_cast<int16_t>(w);
        char buf[32];
        if (disp >= 0) snprintf(buf, sizeof(buf), "$%X(A%d)", static_cast<uint16_t>(disp), reg);
        else snprintf(buf, sizeof(buf), "-$%X(A%d)", static_cast<uint16_t>(-disp), reg);
        return buf;
    }
    case 6: {
        uint16_t ext = 0;
        if (!fetchWord(ext)) return "(invalid)";
        char xregType = (ext & 0x8000) ? 'A' : 'D';
        int xregNum = (ext >> 12) & 7;
        char xsize = (ext & 0x0800) ? 'L' : 'W';
        int8_t disp8 = static_cast<int8_t>(ext & 0xFF);
        char buf[32];
        if (disp8 >= 0) {
            snprintf(buf, sizeof(buf), "$%X(A%d,%c%d.%c)", static_cast<uint8_t>(disp8), reg, xregType, xregNum, xsize);
        } else {
            snprintf(buf, sizeof(buf), "-$%X(A%d,%c%d.%c)", static_cast<uint8_t>(-disp8), reg, xregType, xregNum, xsize);
        }
        return buf;
    }
    case 7:
        switch (reg) {
        case 0: { // Absolute Short
            uint16_t w = 0;
            if (!fetchWord(w)) return "(invalid)";
            if (isControl) {
                return "($" + hex4(w) + ").W";
            }
            return "$" + hex4(w);
        }
        case 1: { // Absolute Long
            uint32_t l = 0;
            if (!fetchLong(l)) return "(invalid)";
            if (isControl) {
                return "($" + hex8(l) + ").L";
            }
            return "$" + hex8(l);
        }
        case 2: { // PC with displacement
            uint16_t w = 0;
            if (!fetchWord(w)) return "(invalid)";
            int16_t disp = static_cast<int16_t>(w);
            char buf[32];
            if (disp >= 0) snprintf(buf, sizeof(buf), "$%X(PC)", static_cast<uint16_t>(disp));
            else snprintf(buf, sizeof(buf), "-$%X(PC)", static_cast<uint16_t>(-disp));
            return buf;
        }
        case 3: { // PC with index
            uint16_t ext = 0;
            if (!fetchWord(ext)) return "(invalid)";
            char xregType = (ext & 0x8000) ? 'A' : 'D';
            int xregNum = (ext >> 12) & 7;
            char xsize = (ext & 0x0800) ? 'L' : 'W';
            int8_t disp8 = static_cast<int8_t>(ext & 0xFF);
            char buf[32];
            if (disp8 >= 0) {
                snprintf(buf, sizeof(buf), "$%X(PC,%c%d.%c)", static_cast<uint8_t>(disp8), xregType, xregNum, xsize);
            } else {
                snprintf(buf, sizeof(buf), "-$%X(PC,%c%d.%c)", static_cast<uint8_t>(-disp8), xregType, xregNum, xsize);
            }
            return buf;
        }
        case 4: { // Immediate
            if (sizeBytes == 1) {
                uint16_t w = 0;
                if (!fetchWord(w)) return "(invalid)";
                return "#$" + hex2(static_cast<uint8_t>(w & 0xFF));
            } else if (sizeBytes == 4) {
                uint32_t l = 0;
                if (!fetchLong(l)) return "(invalid)";
                return "#$" + hex8(l);
            } else {
                uint16_t w = 0;
                if (!fetchWord(w)) return "(invalid)";
                return "#$" + hex4(w);
            }
        }
        default:
            ok = false;
            return "(invalid)";
        }
    default:
        ok = false;
        return "(invalid)";
    }
}

} // namespace

std::string disassemble68k(const uint8_t *memory, uint32_t memSize, uint32_t pc, int *bytesConsumed)
{
    if (!memory || pc + 1 >= memSize) {
        if (bytesConsumed) *bytesConsumed = 0;
        return "???";
    }

    uint16_t op = (static_cast<uint16_t>(memory[pc]) << 8) | static_cast<uint16_t>(memory[pc + 1]);
    uint32_t extPc = pc + 2;
    bool ok = true;

    auto fetchWord = [&](uint16_t &w) -> bool {
        if (extPc + 1 >= memSize) { ok = false; return false; }
        w = (static_cast<uint16_t>(memory[extPc]) << 8) | static_cast<uint16_t>(memory[extPc + 1]);
        extPc += 2;
        return true;
    };

    auto fetchLong = [&](uint32_t &l) -> bool {
        if (extPc + 3 >= memSize) { ok = false; return false; }
        l = (static_cast<uint32_t>(memory[extPc]) << 24) |
            (static_cast<uint32_t>(memory[extPc + 1]) << 16) |
            (static_cast<uint32_t>(memory[extPc + 2]) << 8) |
            static_cast<uint32_t>(memory[extPc + 3]);
        extPc += 4;
        return true;
    };

    std::string result;
    uint16_t line = (op >> 12) & 0xF;

    switch (line) {
    case 0x0: {
        if (op == 0x003C) {
            uint16_t imm = 0;
            if (fetchWord(imm)) result = "ORI #$" + hex2(static_cast<uint8_t>(imm & 0xFF)) + ", CCR";
        } else if (op == 0x007C) {
            uint16_t imm = 0;
            if (fetchWord(imm)) result = "ORI #$" + hex4(imm) + ", SR";
        } else if (op == 0x023C) {
            uint16_t imm = 0;
            if (fetchWord(imm)) result = "ANDI #$" + hex2(static_cast<uint8_t>(imm & 0xFF)) + ", CCR";
        } else if (op == 0x027C) {
            uint16_t imm = 0;
            if (fetchWord(imm)) result = "ANDI #$" + hex4(imm) + ", SR";
        } else if (op == 0x0A3C) {
            uint16_t imm = 0;
            if (fetchWord(imm)) result = "EORI #$" + hex2(static_cast<uint8_t>(imm & 0xFF)) + ", CCR";
        } else if (op == 0x0A7C) {
            uint16_t imm = 0;
            if (fetchWord(imm)) result = "EORI #$" + hex4(imm) + ", SR";
        } else if ((op & 0x0138) == 0x0108) {
            // MOVEP
            int dx = (op >> 9) & 7;
            int dir = (op >> 7) & 1;
            int sz = (op >> 6) & 1;
            int ay = op & 7;
            uint16_t disp = 0;
            fetchWord(disp);
            int16_t sdisp = static_cast<int16_t>(disp);
            char dispBuf[32];
            if (sdisp >= 0) snprintf(dispBuf, sizeof(dispBuf), "$%X(A%d)", static_cast<uint16_t>(sdisp), ay);
            else snprintf(dispBuf, sizeof(dispBuf), "-$%X(A%d)", static_cast<uint16_t>(-sdisp), ay);
            std::string szStr = sz ? ".L" : ".W";
            if (dir == 0) result = "MOVEP" + szStr + " " + dispBuf + ", D" + std::to_string(dx);
            else result = "MOVEP" + szStr + " D" + std::to_string(dx) + ", " + dispBuf;
        } else if ((op & 0x0100) == 0 && ((op >> 9) & 7) != 4) {
            // Immediate ops: ORI, ANDI, SUBI, ADDI, EORI, CMPI
            static const char *immNames[8] = { "ORI", "ANDI", "SUBI", "ADDI", "", "EORI", "CMPI", "" };
            int opIdx = (op >> 9) & 7;
            int szCode = (op >> 6) & 3;
            if (szCode < 3 && immNames[opIdx][0] != '\0') {
                int szBytes = (szCode == 0 ? 1 : (szCode == 1 ? 2 : 4));
                std::string szStr = (szBytes == 1 ? ".B" : (szBytes == 2 ? ".W" : ".L"));
                std::string immStr;
                if (szBytes == 1) {
                    uint16_t w = 0;
                    fetchWord(w);
                    immStr = "#$" + hex2(static_cast<uint8_t>(w & 0xFF));
                } else if (szBytes == 2) {
                    uint16_t w = 0;
                    fetchWord(w);
                    immStr = "#$" + hex4(w);
                } else {
                    uint32_t l = 0;
                    fetchLong(l);
                    immStr = "#$" + hex8(l);
                }
                std::string ea = formatEA((op >> 3) & 7, op & 7, szBytes, memory, memSize, extPc, ok);
                result = std::string(immNames[opIdx]) + szStr + " " + immStr + ", " + ea;
            }
        } else if ((op & 0x0100) != 0) {
            // Dynamic bit ops: BTST, BCHG, BCLR, BSET
            static const char *bitOps[4] = { "BTST", "BCHG", "BCLR", "BSET" };
            int opIdx = (op >> 6) & 3;
            int dn = (op >> 9) & 7;
            int eaMode = (op >> 3) & 7;
            int eaReg = op & 7;
            int szBytes = (eaMode == 0 ? 4 : 1);
            std::string ea = formatEA(eaMode, eaReg, szBytes, memory, memSize, extPc, ok);
            result = std::string(bitOps[opIdx]) + " D" + std::to_string(dn) + ", " + ea;
        } else if ((op & 0x0F00) == 0x0800) {
            // Static bit ops
            static const char *bitOps[4] = { "BTST", "BCHG", "BCLR", "BSET" };
            int opIdx = (op >> 6) & 3;
            uint16_t bitVal = 0;
            fetchWord(bitVal);
            int eaMode = (op >> 3) & 7;
            int eaReg = op & 7;
            int szBytes = (eaMode == 0 ? 4 : 1);
            std::string ea = formatEA(eaMode, eaReg, szBytes, memory, memSize, extPc, ok);
            result = std::string(bitOps[opIdx]) + " #" + std::to_string(bitVal & 0xFF) + ", " + ea;
        }
        break;
    }
    case 0x1:
    case 0x2:
    case 0x3: {
        // MOVE / MOVEA
        int szBytes = (line == 0x1 ? 1 : (line == 0x3 ? 2 : 4));
        std::string szStr = (szBytes == 1 ? ".B" : (szBytes == 2 ? ".W" : ".L"));
        int srcMode = (op >> 3) & 7;
        int srcReg = op & 7;
        int dstMode = (op >> 6) & 7;
        int dstReg = (op >> 9) & 7;

        std::string srcEA = formatEA(srcMode, srcReg, szBytes, memory, memSize, extPc, ok);
        std::string dstEA = formatEA(dstMode, dstReg, szBytes, memory, memSize, extPc, ok);

        if (dstMode == 1) {
            result = "MOVEA" + szStr + " " + srcEA + ", " + dstEA;
        } else {
            result = "MOVE" + szStr + " " + srcEA + ", " + dstEA;
        }
        break;
    }
    case 0x4: {
        if (op == 0x4E70) result = "RESET";
        else if (op == 0x4E71) result = "NOP";
        else if (op == 0x4E72) {
            uint16_t imm = 0;
            fetchWord(imm);
            result = "STOP #$" + hex4(imm);
        } else if (op == 0x4E73) result = "RTE";
        else if (op == 0x4E75) result = "RTS";
        else if (op == 0x4E76) result = "TRAPV";
        else if (op == 0x4E77) result = "RTR";
        else if (op == 0x4AFC) result = "ILLEGAL";
        else if ((op & 0xFFF0) == 0x4E40) {
            result = "TRAP #" + std::to_string(op & 0xF);
        } else if ((op & 0xFFF8) == 0x4E50) {
            uint16_t disp = 0;
            fetchWord(disp);
            result = "LINK A" + std::to_string(op & 7) + ", #" + std::to_string(static_cast<int16_t>(disp));
        } else if ((op & 0xFFF8) == 0x4E58) {
            result = "UNLK A" + std::to_string(op & 7);
        } else if ((op & 0xFFF8) == 0x4E60) {
            result = "MOVE A" + std::to_string(op & 7) + ", USP";
        } else if ((op & 0xFFF8) == 0x4E68) {
            result = "MOVE USP, A" + std::to_string(op & 7);
        } else if ((op & 0xFFF8) == 0x4840) {
            result = "SWAP D" + std::to_string(op & 7);
        } else if ((op & 0xFFF8) == 0x4880) {
            result = "EXT.W D" + std::to_string(op & 7);
        } else if ((op & 0xFFF8) == 0x48C0) {
            result = "EXT.L D" + std::to_string(op & 7);
        } else if ((op & 0xFFC0) == 0x40C0) {
            std::string ea = formatEA((op >> 3) & 7, op & 7, 2, memory, memSize, extPc, ok);
            result = "MOVE SR, " + ea;
        } else if ((op & 0xFFC0) == 0x44C0) {
            std::string ea = formatEA((op >> 3) & 7, op & 7, 2, memory, memSize, extPc, ok);
            result = "MOVE " + ea + ", CCR";
        } else if ((op & 0xFFC0) == 0x46C0) {
            std::string ea = formatEA((op >> 3) & 7, op & 7, 2, memory, memSize, extPc, ok);
            result = "MOVE " + ea + ", SR";
        } else if ((op & 0xFFC0) == 0x4800) {
            std::string ea = formatEA((op >> 3) & 7, op & 7, 1, memory, memSize, extPc, ok);
            result = "NBCD " + ea;
        } else if ((op & 0xFFC0) == 0x4840) {
            std::string ea = formatEA((op >> 3) & 7, op & 7, 4, memory, memSize, extPc, ok, true);
            result = "PEA " + ea;
        } else if ((op & 0xFFC0) == 0x4AC0) {
            std::string ea = formatEA((op >> 3) & 7, op & 7, 1, memory, memSize, extPc, ok);
            result = "TAS " + ea;
        } else if ((op & 0xFFC0) == 0x4E80) {
            std::string ea = formatEA((op >> 3) & 7, op & 7, 0, memory, memSize, extPc, ok, true);
            result = "JSR " + ea;
        } else if ((op & 0xFFC0) == 0x4EC0) {
            std::string ea = formatEA((op >> 3) & 7, op & 7, 0, memory, memSize, extPc, ok, true);
            result = "JMP " + ea;
        } else if ((op & 0xFB80) == 0x4880) {
            // MOVEM
            int dir = (op >> 10) & 1;
            int sz = (op >> 6) & 1;
            int mode = (op >> 3) & 7;
            int reg = op & 7;
            uint16_t mask = 0;
            fetchWord(mask);
            bool rev = (dir == 0 && mode == 4);
            std::string regList = formatRegisterList(mask, rev);
            std::string ea = formatEA(mode, reg, sz ? 4 : 2, memory, memSize, extPc, ok, true);
            std::string szStr = sz ? ".L" : ".W";
            if (dir == 0) result = "MOVEM" + szStr + " " + regList + ", " + ea;
            else result = "MOVEM" + szStr + " " + ea + ", " + regList;
        } else if ((op & 0xF1C0) == 0x41C0) {
            int an = (op >> 9) & 7;
            std::string ea = formatEA((op >> 3) & 7, op & 7, 4, memory, memSize, extPc, ok, true);
            result = "LEA " + ea + ", A" + std::to_string(an);
        } else if ((op & 0xF1C0) == 0x4180) {
            int dn = (op >> 9) & 7;
            std::string ea = formatEA((op >> 3) & 7, op & 7, 2, memory, memSize, extPc, ok);
            result = "CHK.W " + ea + ", D" + std::to_string(dn);
        } else if ((op & 0xFF00) == 0x4200 || (op & 0xFF00) == 0x4400 ||
                   (op & 0xFF00) == 0x4000 || (op & 0xFF00) == 0x4600 ||
                   (op & 0xFF00) == 0x4A00) {
            int uIdx = ((op >> 9) & 7);
            const char *name = (uIdx == 0 ? "NEGX" :
                                (uIdx == 1 ? "CLR" :
                                (uIdx == 2 ? "NEG" :
                                (uIdx == 3 ? "NOT" : "TST"))));
            int szCode = (op >> 6) & 3;
            if (szCode < 3) {
                int szBytes = (szCode == 0 ? 1 : (szCode == 1 ? 2 : 4));
                std::string szStr = (szBytes == 1 ? ".B" : (szBytes == 2 ? ".W" : ".L"));
                std::string ea = formatEA((op >> 3) & 7, op & 7, szBytes, memory, memSize, extPc, ok);
                result = std::string(name) + szStr + " " + ea;
            }
        }
        break;
    }
    case 0x5: {
        int szCode = (op >> 6) & 3;
        if (szCode == 3) {
            if (((op >> 3) & 7) == 1) {
                // DBcc
                int cc = (op >> 8) & 0xF;
                int dn = op & 7;
                uint16_t disp = 0;
                fetchWord(disp);
                uint32_t target = extPc - 2 + static_cast<int16_t>(disp);
                char buf[32];
                snprintf(buf, sizeof(buf), "DB%s D%d, $%X", kCCNames[cc], dn, target);
                result = buf;
            } else {
                // Scc
                int cc = (op >> 8) & 0xF;
                std::string ea = formatEA((op >> 3) & 7, op & 7, 1, memory, memSize, extPc, ok);
                result = std::string("S") + kCCNames[cc] + " " + ea;
            }
        } else {
            // ADDQ / SUBQ
            int isSub = (op >> 8) & 1;
            int data = (op >> 9) & 7;
            if (data == 0) data = 8;
            int szBytes = (szCode == 0 ? 1 : (szCode == 1 ? 2 : 4));
            std::string szStr = (szBytes == 1 ? ".B" : (szBytes == 2 ? ".W" : ".L"));
            std::string ea = formatEA((op >> 3) & 7, op & 7, szBytes, memory, memSize, extPc, ok);
            result = (isSub ? "SUBQ" : "ADDQ") + szStr + " #" + std::to_string(data) + ", " + ea;
        }
        break;
    }
    case 0x6: {
        // Branches
        int cc = (op >> 8) & 0xF;
        int8_t disp8 = static_cast<int8_t>(op & 0xFF);
        uint32_t target = 0;
        if (disp8 != 0) {
            target = pc + 2 + disp8;
        } else {
            uint16_t disp16 = 0;
            fetchWord(disp16);
            target = pc + 2 + static_cast<int16_t>(disp16);
        }
        std::string name;
        if (cc == 0) name = "BRA";
        else if (cc == 1) name = "BSR";
        else name = std::string("B") + kCCNames[cc];
        char buf[32];
        snprintf(buf, sizeof(buf), "%s $%X", name.c_str(), target);
        result = buf;
        break;
    }
    case 0x7: {
        if ((op & 0x0100) == 0) {
            int dn = (op >> 9) & 7;
            int8_t data = static_cast<int8_t>(op & 0xFF);
            result = "MOVEQ #" + std::to_string(data) + ", D" + std::to_string(dn);
        }
        break;
    }
    case 0x8: {
        if ((op & 0xF1F0) == 0x8100) {
            int rx = (op >> 9) & 7;
            int ry = op & 7;
            if ((op & 8) == 0) result = "SBCD D" + std::to_string(ry) + ", D" + std::to_string(rx);
            else result = "SBCD -(A" + std::to_string(ry) + "), -(A" + std::to_string(rx) + ")";
        } else if ((op & 0xF1C0) == 0x80C0) {
            std::string ea = formatEA((op >> 3) & 7, op & 7, 2, memory, memSize, extPc, ok);
            result = "DIVU.W " + ea + ", D" + std::to_string((op >> 9) & 7);
        } else if ((op & 0xF1C0) == 0x81C0) {
            std::string ea = formatEA((op >> 3) & 7, op & 7, 2, memory, memSize, extPc, ok);
            result = "DIVS.W " + ea + ", D" + std::to_string((op >> 9) & 7);
        } else {
            int opmode = (op >> 6) & 7;
            int dn = (op >> 9) & 7;
            int szBytes = (opmode & 3) == 0 ? 1 : ((opmode & 3) == 1 ? 2 : 4);
            std::string szStr = (szBytes == 1 ? ".B" : (szBytes == 2 ? ".W" : ".L"));
            std::string ea = formatEA((op >> 3) & 7, op & 7, szBytes, memory, memSize, extPc, ok);
            if ((opmode & 4) == 0) result = "OR" + szStr + " " + ea + ", D" + std::to_string(dn);
            else result = "OR" + szStr + " D" + std::to_string(dn) + ", " + ea;
        }
        break;
    }
    case 0x9: {
        if ((op & 0xF0C0) == 0x90C0) {
            int sz = ((op >> 8) & 1) ? 4 : 2;
            std::string szStr = (sz == 4 ? ".L" : ".W");
            std::string ea = formatEA((op >> 3) & 7, op & 7, sz, memory, memSize, extPc, ok);
            result = "SUBA" + szStr + " " + ea + ", A" + std::to_string((op >> 9) & 7);
        } else if ((op & 0xF130) == 0x9100) {
            int szCode = (op >> 6) & 3;
            int szBytes = (szCode == 0 ? 1 : (szCode == 1 ? 2 : 4));
            std::string szStr = (szBytes == 1 ? ".B" : (szBytes == 2 ? ".W" : ".L"));
            int rx = (op >> 9) & 7;
            int ry = op & 7;
            if ((op & 8) == 0) result = "SUBX" + szStr + " D" + std::to_string(ry) + ", D" + std::to_string(rx);
            else result = "SUBX" + szStr + " -(A" + std::to_string(ry) + "), -(A" + std::to_string(rx) + ")";
        } else {
            int opmode = (op >> 6) & 7;
            int dn = (op >> 9) & 7;
            int szBytes = (opmode & 3) == 0 ? 1 : ((opmode & 3) == 1 ? 2 : 4);
            std::string szStr = (szBytes == 1 ? ".B" : (szBytes == 2 ? ".W" : ".L"));
            std::string ea = formatEA((op >> 3) & 7, op & 7, szBytes, memory, memSize, extPc, ok);
            if ((opmode & 4) == 0) result = "SUB" + szStr + " " + ea + ", D" + std::to_string(dn);
            else result = "SUB" + szStr + " D" + std::to_string(dn) + ", " + ea;
        }
        break;
    }
    case 0xB: {
        if ((op & 0xF0C0) == 0xB0C0) {
            int sz = ((op >> 8) & 1) ? 4 : 2;
            std::string szStr = (sz == 4 ? ".L" : ".W");
            std::string ea = formatEA((op >> 3) & 7, op & 7, sz, memory, memSize, extPc, ok);
            result = "CMPA" + szStr + " " + ea + ", A" + std::to_string((op >> 9) & 7);
        } else if ((op & 0xF138) == 0xB108) {
            int szCode = (op >> 6) & 3;
            int szBytes = (szCode == 0 ? 1 : (szCode == 1 ? 2 : 4));
            std::string szStr = (szBytes == 1 ? ".B" : (szBytes == 2 ? ".W" : ".L"));
            int ax = (op >> 9) & 7;
            int ay = op & 7;
            result = "CMPM" + szStr + " (A" + std::to_string(ay) + ")+, (A" + std::to_string(ax) + ")+";
        } else if (((op >> 6) & 7) >= 4) {
            int opmode = (op >> 6) & 7;
            int dn = (op >> 9) & 7;
            int szBytes = (opmode == 4 ? 1 : (opmode == 5 ? 2 : 4));
            std::string szStr = (szBytes == 1 ? ".B" : (szBytes == 2 ? ".W" : ".L"));
            std::string ea = formatEA((op >> 3) & 7, op & 7, szBytes, memory, memSize, extPc, ok);
            result = "EOR" + szStr + " D" + std::to_string(dn) + ", " + ea;
        } else {
            int opmode = (op >> 6) & 7;
            int dn = (op >> 9) & 7;
            int szBytes = (opmode == 0 ? 1 : (opmode == 1 ? 2 : 4));
            std::string szStr = (szBytes == 1 ? ".B" : (szBytes == 2 ? ".W" : ".L"));
            std::string ea = formatEA((op >> 3) & 7, op & 7, szBytes, memory, memSize, extPc, ok);
            result = "CMP" + szStr + " " + ea + ", D" + std::to_string(dn);
        }
        break;
    }
    case 0xC: {
        if ((op & 0xF1F0) == 0xC100) {
            int rx = (op >> 9) & 7;
            int ry = op & 7;
            if ((op & 8) == 0) result = "ABCD D" + std::to_string(ry) + ", D" + std::to_string(rx);
            else result = "ABCD -(A" + std::to_string(ry) + "), -(A" + std::to_string(rx) + ")";
        } else if ((op & 0xF130) == 0xC100) {
            int opmode = (op >> 3) & 0x1F;
            int rx = (op >> 9) & 7;
            int ry = op & 7;
            if (opmode == 8) result = "EXG D" + std::to_string(rx) + ", D" + std::to_string(ry);
            else if (opmode == 9) result = "EXG A" + std::to_string(rx) + ", A" + std::to_string(ry);
            else if (opmode == 17) result = "EXG D" + std::to_string(rx) + ", A" + std::to_string(ry);
        } else if ((op & 0xF1C0) == 0xC0C0) {
            std::string ea = formatEA((op >> 3) & 7, op & 7, 2, memory, memSize, extPc, ok);
            result = "MULU.W " + ea + ", D" + std::to_string((op >> 9) & 7);
        } else if ((op & 0xF1C0) == 0xC1C0) {
            std::string ea = formatEA((op >> 3) & 7, op & 7, 2, memory, memSize, extPc, ok);
            result = "MULS.W " + ea + ", D" + std::to_string((op >> 9) & 7);
        } else {
            int opmode = (op >> 6) & 7;
            int dn = (op >> 9) & 7;
            int szBytes = (opmode & 3) == 0 ? 1 : ((opmode & 3) == 1 ? 2 : 4);
            std::string szStr = (szBytes == 1 ? ".B" : (szBytes == 2 ? ".W" : ".L"));
            std::string ea = formatEA((op >> 3) & 7, op & 7, szBytes, memory, memSize, extPc, ok);
            if ((opmode & 4) == 0) result = "AND" + szStr + " " + ea + ", D" + std::to_string(dn);
            else result = "AND" + szStr + " D" + std::to_string(dn) + ", " + ea;
        }
        break;
    }
    case 0xD: {
        if ((op & 0xF0C0) == 0xD0C0) {
            int sz = ((op >> 8) & 1) ? 4 : 2;
            std::string szStr = (sz == 4 ? ".L" : ".W");
            std::string ea = formatEA((op >> 3) & 7, op & 7, sz, memory, memSize, extPc, ok);
            result = "ADDA" + szStr + " " + ea + ", A" + std::to_string((op >> 9) & 7);
        } else if ((op & 0xF130) == 0xD100) {
            int szCode = (op >> 6) & 3;
            int szBytes = (szCode == 0 ? 1 : (szCode == 1 ? 2 : 4));
            std::string szStr = (szBytes == 1 ? ".B" : (szBytes == 2 ? ".W" : ".L"));
            int rx = (op >> 9) & 7;
            int ry = op & 7;
            if ((op & 8) == 0) result = "ADDX" + szStr + " D" + std::to_string(ry) + ", D" + std::to_string(rx);
            else result = "ADDX" + szStr + " -(A" + std::to_string(ry) + "), -(A" + std::to_string(rx) + ")";
        } else {
            int opmode = (op >> 6) & 7;
            int dn = (op >> 9) & 7;
            int szBytes = (opmode & 3) == 0 ? 1 : ((opmode & 3) == 1 ? 2 : 4);
            std::string szStr = (szBytes == 1 ? ".B" : (szBytes == 2 ? ".W" : ".L"));
            std::string ea = formatEA((op >> 3) & 7, op & 7, szBytes, memory, memSize, extPc, ok);
            if ((opmode & 4) == 0) result = "ADD" + szStr + " " + ea + ", D" + std::to_string(dn);
            else result = "ADD" + szStr + " D" + std::to_string(dn) + ", " + ea;
        }
        break;
    }
    case 0xE: {
        static const char *shiftNames[4] = { "AS", "LS", "ROX", "RO" };
        if (((op >> 6) & 3) == 3) {
            // Memory shift
            int dir = (op >> 8) & 1;
            int type = (op >> 9) & 3;
            std::string name = std::string(shiftNames[type]) + (dir ? "L.W" : "R.W");
            std::string ea = formatEA((op >> 3) & 7, op & 7, 2, memory, memSize, extPc, ok);
            result = name + " " + ea;
        } else {
            // Register shift
            int szCode = (op >> 6) & 3;
            int szBytes = (szCode == 0 ? 1 : (szCode == 1 ? 2 : 4));
            std::string szStr = (szBytes == 1 ? ".B" : (szBytes == 2 ? ".W" : ".L"));
            int dir = (op >> 8) & 1;
            int type = (op >> 3) & 3;
            std::string name = std::string(shiftNames[type]) + (dir ? "L" : "R") + szStr;
            int dy = op & 7;
            if ((op & 0x0020) == 0) {
                int count = (op >> 9) & 7;
                if (count == 0) count = 8;
                result = name + " #" + std::to_string(count) + ", D" + std::to_string(dy);
            } else {
                int dx = (op >> 9) & 7;
                result = name + " D" + std::to_string(dx) + ", D" + std::to_string(dy);
            }
        }
        break;
    }
    case 0xF: {
        if (op == 0xFFFF) {
            uint16_t nextW = 0;
            if (extPc + 1 < memSize) {
                nextW = (static_cast<uint16_t>(memory[extPc]) << 8) | static_cast<uint16_t>(memory[extPc + 1]);
                if (nextW == 0xFFFF) {
                    extPc += 2;
                    result = "SIMHALT";
                }
            }
        }
        break;
    }
    default:
        break;
    }

    if (!ok || result.empty()) {
        extPc = pc + 2;
        result = "DC.W $" + hex4(op);
    }

    if (bytesConsumed) {
        *bytesConsumed = static_cast<int>(extPc - pc);
    }
    return result;
}
