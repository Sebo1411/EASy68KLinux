#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include "def.h"
#include "extern.h"
#include "proto.h"
#include "simio.h"
#include "simmessage.h"

static void printUsage(const char* prog)
{
    std::cout << "EASy68K Native Linux Simulator (CLI)\n"
              << "Usage: " << prog << " [options] <s68_file>\n\n"
              << "Options:\n"
              << "  -t, --trace              Enable instruction-level tracing\n"
              << "  -m, --max-cycles <N>     Maximum cycle count before aborting (0 = unlimited)\n"
              << "  -q, --quiet              Suppress banner and final register dump\n"
              << "  -h, --help               Display this help message\n";
}

int main(int argc, char* argv[])
{
    std::string s68File;
    bool traceMode = false;
    bool quietMode = false;
    uint64_t maxCycles = 0;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-t" || arg == "--trace") {
            traceMode = true;
        } else if (arg == "-q" || arg == "--quiet") {
            quietMode = true;
        } else if ((arg == "-m" || arg == "--max-cycles") && i + 1 < argc) {
            maxCycles = std::strtoull(argv[++i], nullptr, 0);
        } else if (arg[0] == '-') {
            std::cerr << "Unknown option: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        } else {
            s68File = arg;
        }
    }

    if (s68File.empty()) {
        std::cerr << "Error: No S-Record file specified.\n";
        printUsage(argv[0]);
        return 1;
    }

    // Allocate 16MB 68000 address space
    memory = new char[MEMSIZE]();
    if (!memory) {
        std::cerr << "Error: Failed to allocate 16MB simulation memory.\n";
        return 1;
    }

    CliSimIO cliIO;
    setSimIO(&cliIO);

    setSimMessageCallback([quietMode](const std::string& msg) {
        if (!quietMode) {
            std::cout << "[SIM] " << msg << "\n";
        }
    });

    initSim();

    if (loadSrec(const_cast<char*>(s68File.c_str())) != SUCCESS) {
        std::cerr << "Error: Failed to load S-Record file: " << s68File << "\n";
        delete[] memory;
        return 1;
    }

    if (!quietMode) {
        std::cout << "EASy68K Linux Simulator: Loaded " << s68File << "\n";
        std::cout << "Initial PC: 0x" << std::hex << std::uppercase << PC
                  << ", SSP: 0x" << A[8] << ", USP: 0x" << A[7] << std::dec << "\n";
        std::cout << "--------------------------------------------------\n";
    }

    runMode = true;
    while (!halt && (runMode || traceMode)) {
        if (traceMode) {
            trace = true;
        }
        runprog();
        if (maxCycles > 0 && cycles >= maxCycles) {
            std::cerr << "\n[SIM] Maximum cycles limit reached (" << maxCycles << "). Aborting.\n";
            break;
        }
    }

    if (!quietMode) {
        std::cout << "\n--------------------------------------------------\n";
        std::cout << "Execution Halted. Total Cycles: " << cycles << "\n";
        std::cout << std::hex << std::uppercase << std::setfill('0');
        std::cout << "Registers:\n";
        for (int i = 0; i < 8; ++i) {
            std::cout << "D" << i << "=" << std::setw(8) << static_cast<uint32_t>(D[i]) << " ";
            if (i == 3 || i == 7) std::cout << "\n";
        }
        for (int i = 0; i < 8; ++i) {
            std::cout << "A" << i << "=" << std::setw(8) << static_cast<uint32_t>(A[i]) << " ";
            if (i == 3 || i == 7) std::cout << "\n";
        }
        std::cout << "PC=" << std::setw(8) << static_cast<uint32_t>(PC) << "  SR=" << std::setw(4) << static_cast<uint16_t>(SR)
                  << "  USP=" << std::setw(8) << static_cast<uint32_t>(A[7]) << "  SSP=" << std::setw(8) << static_cast<uint32_t>(A[8]) << "\n";
    }

    delete[] memory;
    memory = nullptr;
    return 0;
}
