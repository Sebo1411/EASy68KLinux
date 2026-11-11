# EASy68K Native Linux

Native Linux port of **EASy68K**: Editor, Assembler, Simulator, and S-Record / Binary utility for Motorola 68000 microprocessors.

Ported and modernized to C++17 for GCC 15+ on Linux (Kubuntu / Ubuntu) with zero WINE or Windows emulation dependencies.

---

## Deliverables

1. **`easy68k-asm`**: Standalone native 68000 cross-assembler.
2. **`easy68k-sim`**: Standalone native headless/CLI 68000 simulator with TRAP #15 support, cycle counting, and instruction tracing.
3. **`easybin`**: Standalone native Motorola S-Record (`.s68`) <-> Raw Binary (`.bin`) conversion utility.
4. **`easy68k-gui`**: Native Qt6 graphical IDE featuring:
   - Syntax-highlighted 68000 assembly editor with line numbers and integrated error navigation.
   - Interactive registers & memory inspector.
   - Interactive TRAP #15 terminal console.
   - **Simulated Hardware Panel**: 8-digit 7-segment displays (`$E00000..$E0000E`), 8 LED indicators (`$E00010`), 8 interactive toggle switches (`$E00012`), and 8 pushbuttons (`$E00014`).
   - **2D Graphics Canvas**: 640x480 interactive drawing canvas supporting TRAP #15 Tasks 80–96 (pixel, line, rectangle, ellipse, text).
   - **Integrated EASyBIN Converter**: GUI for S-Record inspection and `.s68` <-> `.bin` multi-format splitting and conversion.
   - **Integrated Help & Reference Browser**: Full offline HTML documentation and Motorola 68000 Quick Reference PDF browser (`F1`).

---

## Building

Requires GCC 15+ (or any C++17 compiler), CMake (>= 3.16), and Qt6 Widgets:

```bash
mkdir -p build && cd build
cmake ..
cmake --build . -j$(nproc)
```

The build is configured with strict compiler flags (`-std=c++17 -Wall -Wextra -Werror`).

---

## Running the Applications

### 1. Assembler (`easy68k-asm`)
```bash
./build/easy68k-asm [options] <source.x68>

# Example:
./build/easy68k-asm examples/hello.x68 -o examples/hello.s68 --listing examples/hello.l68
```

### 2. Simulator (`easy68k-sim`)
```bash
./build/easy68k-sim [options] <s68_file>

# Options:
#   -t, --trace           Enable instruction-level tracing
#   -m, --max-cycles <N>  Maximum cycle count limit
#   -q, --quiet           Quiet mode (suppress register dump)

# Example:
./build/easy68k-sim examples/hello.s68
```

### 3. Binary Utility (`easybin`)
```bash
# Inspect an S-Record file:
./build/easybin -i examples/hello.s68

# Convert S-Record to raw binary:
./build/easybin -s2b examples/hello.s68 examples/hello.bin

# Convert raw binary to S-Record:
./build/easybin -b2s examples/hello.bin examples/hello.s68 --addr 0x1000 --entry 0x1000
```

### 4. Graphical IDE (`easy68k-gui`)
```bash
./build/easy68k-gui
```

---

## Test Suite

Run the automated test regression suite:

```bash
cd build
ctest --output-on-failure
```
Or directly via the runner script:
```bash
./tests/run_tests.sh
```

---

## Packaging

Generate native packages using CPack:

```bash
cd build
cpack -G TGZ    # Generates easy68k-linux-5.16.1-Linux.tar.gz
cpack -G DEB    # Generates easy68k-linux-5.16.1-Linux.deb
cpack -G RPM    # Generates easy68k-linux-5.16.1-Linux.rpm
```

GitHub Actions automatically builds and tests on every commit, generating `.deb`, `.rpm`, `.tar.gz`, and standalone `.AppImage` packages.

---

## Original Credits
- Original Assembler code: Paul McKee, modified by Charles Kelly.
- Original Simulator code: Paul McKee, modified by Charles Kelly and Eric Nelson.
- EASyBIN utility: Charles Kelly.
- Distributed under GNU General Public License.


