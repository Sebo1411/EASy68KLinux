#ifndef DISASSEMBLER_H
#define DISASSEMBLER_H

#include <cstdint>
#include <string>

std::string disassemble68k(const uint8_t *memory, uint32_t memSize, uint32_t pc, int *bytesConsumed = nullptr);

#endif // DISASSEMBLER_H
