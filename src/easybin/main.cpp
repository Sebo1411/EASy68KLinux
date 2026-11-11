#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <cstdlib>
#include "def.h"
#include "fileio.h"

using namespace easybin;

static void printUsage(const char* prog)
{
    std::cout << TITLE << "\n"
              << "Motorola S-Record <-> Binary Conversion Utility\n\n"
              << "Usage:\n"
              << "  " << prog << " -i <input.s68>                               Inspect S-Record file\n"
              << "  " << prog << " -s2b <input.s68> <output.bin> [options]       Convert S-Record to raw binary\n"
              << "  " << prog << " -b2s <input.bin> <output.s68> [options]       Convert raw binary to S-Record\n\n"
              << "Options for -s2b:\n"
              << "  --split <1|2|4>            Split output into 1, 2, or 4 interleaved byte files (default: 1)\n"
              << "  --offset <hex_addr>        Base address offset (default: S-Record start address)\n"
              << "  --length <hex_len>         Length of binary data to export\n\n"
              << "Options for -b2s:\n"
              << "  --addr <hex_addr>          Load address in memory for binary (default: 0x1000)\n"
              << "  --entry <hex_addr>         Execution entry point address (default: same as load address)\n"
              << "  --split <1|2|4>            De-interleave binary with split factor\n\n"
              << "  -h, --help                 Display this help message\n";
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string mode = argv[1];
    if (mode == "-h" || mode == "--help") {
        printUsage(argv[0]);
        return 0;
    }

    std::vector<unsigned char> mem(MEMSIZE, 0);

    if (mode == "-i") {
        if (argc < 3) {
            std::cerr << "Error: Missing input file.\n";
            return 1;
        }
        std::string s68File = argv[2];
        SrecInfo info;
        std::string err;
        if (!loadSrec(s68File, nullptr, MEMSIZE, info, err)) {
            std::cerr << "Error: " << err << "\n";
            return 1;
        }
        std::cout << "File: " << s68File << "\n";
        if (!info.headerS0.empty()) {
            std::cout << "Header (S0):   " << info.headerS0 << "\n";
        }
        std::cout << std::hex << std::uppercase << std::setfill('0');
        std::cout << "Start Address: 0x" << std::setw(8) << info.startAddr << "\n";
        std::cout << "End Address:   0x" << std::setw(8) << info.endAddr << "\n";
        std::cout << "Entry Point:   0x" << std::setw(8) << info.entryAddr << "\n";
        std::cout << std::dec;
        std::cout << "Data Length:   " << info.length << " bytes ("
                  << std::hex << "0x" << info.length << std::dec << ")\n";
        return 0;
    }

    if (mode == "-s2b") {
        if (argc < 4) {
            std::cerr << "Error: Missing input or output file.\n";
            printUsage(argv[0]);
            return 1;
        }
        std::string s68File = argv[2];
        std::string binFile = argv[3];
        int split = 1;
        uint32_t customOffset = 0xFFFFFFFF;
        uint32_t customLength = 0;

        for (int i = 4; i < argc; ++i) {
            std::string opt = argv[i];
            if (opt == "--split" && i + 1 < argc) {
                split = std::atoi(argv[++i]);
            } else if (opt == "--offset" && i + 1 < argc) {
                customOffset = std::strtoul(argv[++i], nullptr, 16);
            } else if (opt == "--length" && i + 1 < argc) {
                customLength = std::strtoul(argv[++i], nullptr, 16);
            }
        }

        SrecInfo info;
        std::string err;
        if (!loadSrec(s68File, mem.data(), MEMSIZE, info, err)) {
            std::cerr << "Error: " << err << "\n";
            return 1;
        }

        uint32_t fromAddr = (customOffset != 0xFFFFFFFF) ? customOffset : info.startAddr;
        uint32_t len = (customLength != 0) ? customLength : info.length;

        if (!saveBinary(binFile, mem.data(), fromAddr, len, MEMSIZE, split, err)) {
            std::cerr << "Error saving binary: " << err << "\n";
            return 1;
        }

        std::cout << "Successfully converted " << s68File << " -> " << binFile
                  << " (" << len << " bytes from 0x" << std::hex << fromAddr << std::dec
                  << ", split=" << split << ")\n";
        return 0;
    }

    if (mode == "-b2s") {
        if (argc < 4) {
            std::cerr << "Error: Missing input or output file.\n";
            printUsage(argv[0]);
            return 1;
        }
        std::string binFile = argv[2];
        std::string s68File = argv[3];
        uint32_t loadAddr = 0x1000;
        uint32_t entryAddr = 0xFFFFFFFF;
        int split = 1;

        for (int i = 4; i < argc; ++i) {
            std::string opt = argv[i];
            if (opt == "--addr" && i + 1 < argc) {
                loadAddr = std::strtoul(argv[++i], nullptr, 16);
            } else if (opt == "--entry" && i + 1 < argc) {
                entryAddr = std::strtoul(argv[++i], nullptr, 16);
            } else if (opt == "--split" && i + 1 < argc) {
                split = std::atoi(argv[++i]);
            }
        }
        if (entryAddr == 0xFFFFFFFF) {
            entryAddr = loadAddr;
        }

        size_t bytesLoaded = 0;
        std::string err;
        if (!loadBinary(binFile, mem.data(), MEMSIZE, loadAddr, split, bytesLoaded, err)) {
            std::cerr << "Error loading binary: " << err << "\n";
            return 1;
        }

        if (bytesLoaded == 0) {
            std::cerr << "Error: Zero bytes loaded from binary.\n";
            return 1;
        }

        uint32_t toAddr = loadAddr + static_cast<uint32_t>(bytesLoaded) - 1;
        if (!saveSRecord(s68File, mem.data(), loadAddr, toAddr, entryAddr, MEMSIZE, err)) {
            std::cerr << "Error saving S-Record: " << err << "\n";
            return 1;
        }

        std::cout << "Successfully converted " << binFile << " -> " << s68File
                  << " (" << bytesLoaded << " bytes at 0x" << std::hex << loadAddr
                  << ", entry 0x" << entryAddr << std::dec << ")\n";
        return 0;
    }

    std::cerr << "Unknown mode: " << mode << "\n";
    printUsage(argv[0]);
    return 1;
}
