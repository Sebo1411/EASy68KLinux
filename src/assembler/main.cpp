#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include "asm.h"

extern bool listFlag;
extern bool objFlag;
extern bool xrefFlag;
extern bool CEXflag;
extern bool BITflag;
extern bool CREflag;
extern bool MEXflag;
extern bool SEXflag;
extern int errorCount;
extern int warningCount;

static void printUsage(const char* progName) {
    std::cout << "EASy68K Assembler v5.16.1 for Linux\n"
              << "Usage: " << progName << " [options] <source.x68>\n\n"
              << "Options:\n"
              << "  -o <file>    Output S-record object file (default: <source_base>.s68)\n"
              << "  -l, --list   Generate listing file (default: <source_base>.l68)\n"
              << "  --listing <file> Specify listing filename\n"
              << "  -x           Generate cross-reference table\n"
              << "  -b           Enable 68020+ bitfield instructions\n"
              << "  -c           Expand constants in listing\n"
              << "  -h, --help   Show this help message\n"
              << "  -v, --version Show version\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string inputFile;
    std::string outputFile;
    std::string listFile;
    bool genListing = false;

    // Default settings
    objFlag = true;
    listFlag = false;
    xrefFlag = false;
    CEXflag = true;
    BITflag = false;
    CREflag = false;
    MEXflag = false;
    SEXflag = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-v" || arg == "--version") {
            std::cout << "EASy68K Assembler v5.16.1 (Linux Native)\n";
            return 0;
        } else if (arg == "-b") {
            BITflag = true;
        } else if (arg == "-c") {
            CEXflag = true;
        } else if (arg == "-x") {
            xrefFlag = true;
        } else if (arg == "-o") {
            if (i + 1 < argc) {
                outputFile = argv[++i];
            } else {
                std::cerr << "Error: -o requires a filename\n";
                return 1;
            }
        } else if (arg == "-l" || arg == "--list") {
            genListing = true;
        } else if (arg == "--listing") {
            genListing = true;
            if (i + 1 < argc) {
                listFile = argv[++i];
            } else {
                std::cerr << "Error: --listing requires a filename\n";
                return 1;
            }
        } else if (arg[0] == '-') {
            std::cerr << "Unknown option: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        } else {
            if (inputFile.empty()) {
                inputFile = arg;
            } else {
                std::cerr << "Error: multiple input files specified: " << inputFile << " and " << arg << "\n";
                return 1;
            }
        }
    }

    if (inputFile.empty()) {
        std::cerr << "Error: no input file specified\n";
        return 1;
    }

    // Determine base filename
    std::string base = inputFile;
    size_t lastDot = base.find_last_of('.');
    size_t lastSlash = base.find_last_of("/\\");
    if (lastDot != std::string::npos && (lastSlash == std::string::npos || lastDot > lastSlash)) {
        base = base.substr(0, lastDot);
    }

    if (outputFile.empty()) {
        outputFile = base + ".s68";
    }

    if (genListing) {
        listFlag = true;
        if (listFile.empty()) {
            listFile = base + ".l68";
        }
    } else {
        listFlag = false;
        listFile = "";
    }

    std::cout << "Assembling: " << inputFile << "\n";
    int errors = assembleFile(inputFile.c_str(), listFile.c_str(), outputFile.c_str());

    if (errors > 0) {
        std::cerr << "Assembly failed with " << errors << " error(s) and " << warningCount << " warning(s).\n";
        return 1;
    }

    std::cout << "Assembly completed successfully. Output: " << outputFile;
    if (listFlag) {
        std::cout << ", Listing: " << listFile;
    }
    std::cout << "\n";

    return 0;
}
