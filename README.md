# EASy68K Native Linux

[![CI Build & Test](https://github.com/Sebo1411/EASy68KLinux/actions/workflows/ci.yml/badge.svg)](https://github.com/Sebo1411/EASy68KLinux/actions/workflows/ci.yml)
[![Release Packaging & Publishing](https://github.com/Sebo1411/EASy68KLinux/actions/workflows/release.yml/badge.svg)](https://github.com/Sebo1411/EASy68KLinux/actions/workflows/release.yml)

Native 64-bit Linux port of **EASy68K**: Editor, Assembler, Simulator, and S-Record / Binary utility for Motorola 68000 microprocessors.

Ported and modernized to C++17 and Qt6 for Linux (`x86_64`) with zero WINE or Windows emulation dependencies.

---

## Deliverables

1. **`easy68k-asm`**: Standalone native 68000 structured macro cross-assembler.
2. **`easy68k-sim`**: Standalone native headless/CLI 68000 simulator with `TRAP #15` support, cycle counting, and instruction tracing.
3. **`easybin`**: Standalone native Motorola S-Record (`.s68`) <-> Raw Binary (`.bin`) conversion utility.
4. **`easy68k-gui`**: Native Qt6 graphical IDE featuring:
   - Syntax-highlighted 68000 assembly editor (`Edit68K`) with line numbers, comment/uncomment tools, 68000 template generator, and integrated error navigation.
   - Classic `Sim68K` simulator layout with non-snapping `.L68` listing view, clickable gutter breakpoint indicators, and separate execution trace log.
   - Horizontal editable registers panel (`D0..D7`, `A0..A7`, 16-bit binary `SR`, `US`, `SS`, `PC`, `Cycles`) and dedicated 4-byte `Stack` inspector.
   - Interactive click-to-edit `Memory` hex nybble & ASCII inspector with `Copy`, `Fill`, and `Save` range operations.
   - Interactive `TRAP #15` terminal console.
   - **Simulated Hardware Panel**: 8-digit 7-segment displays (`$E00000..$E0000E`), 8 LED indicators (`$E00010`), 8 interactive toggle switches (`$E00012`), 8 pushbuttons (`$E00014`), manual & auto-interval `IRQ 1..7` interrupt generator, and `Memory Map` (`ROM`, `Read Only`, `Protected`, `Invalid`) enforcement.
   - **2D Graphics Canvas**: 640x480 interactive drawing canvas supporting `TRAP #15` Tasks 80–96 (pixel, line, rectangle, ellipse, flood fill, text, mouse, and pen/brush colors).
   - **Integrated EASyBIN Converter**: GUI for S-Record inspection and `.s68` <-> `.bin` multi-format splitting and conversion.
   - **Integrated Help & Reference Browser**: Full offline HTML documentation and Motorola 68000 Quick Reference PDF browser (`F1`).
   - **Integrated Auto-Updater**: Built-in GitHub release checker (`Help -> Check for Updates...`) with SHA-256 verification and automatic startup check (`Help -> Check for Updates on Startup`).

---

## Installation & Automatic Updates

EASy68K Linux supports **three automatic update channels**:

### 1. APT Repository (Debian / Ubuntu / Kubuntu / Linux Mint)
Install and enable automatic updates via `apt upgrade`:
```bash
curl -fsSL https://sebo1411.github.io/EASy68KLinux/setup-apt.sh | sudo bash
```

### 2. DNF / RPM Repository (Fedora / RHEL / openSUSE)
Install and enable automatic updates via `dnf upgrade` or `zypper up`:
```bash
curl -fsSL https://sebo1411.github.io/EASy68KLinux/setup-rpm.sh | sudo bash
```

### 3. Portable AppImage + `AppImageUpdate` (`zsync`) & In-App GUI Updater
Download `easy68k-linux-5.17.0-x86_64.AppImage` from [GitHub Releases](https://github.com/Sebo1411/EASy68KLinux/releases/latest):
```bash
chmod +x easy68k-linux-5.17.0-x86_64.AppImage
./easy68k-linux-5.17.0-x86_64.AppImage
```
- **In-App GUI Auto-Updater**: Open **Help -> Check for Updates...** inside `easy68k-gui` to verify `SHA256SUMS.txt`, download the newest release, atomically replace the `.AppImage` (or install `.deb`/`.rpm` via `pkexec`), and restart.
- **`AppImageUpdate` (`zsync`)**: Every `.AppImage` embeds `gh-releases-zsync|Sebo1411|EASy68KLinux|latest|easy68k-linux-*x86_64.AppImage.zsync` metadata for binary delta updates with *AppImageUpdate*, *Gear Lever*, or *AppImageLauncher*.

---

## Building from Source

Requires a C++17 compiler (`g++` or `clang++`), CMake (>= 3.16), and Qt6 (`Core`, `Gui`, `Widgets`, `Network`, `Test`, and `Multimedia` optional for TRAP #15 sound):

```bash
mkdir -p build && cd build
cmake ..
cmake --build . -j$(nproc)
```

The build enforces strict compiler flags (`-std=c++17 -Wall -Wextra -Werror`).

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

Run the automated native and GUI regression suites:

```bash
cd build
QT_QPA_PLATFORM=offscreen ctest --output-on-failure -V
```
Or directly via the runner script:
```bash
./tests/run_tests.sh
```

---

## Packaging

Generate native distribution packages using CPack and the helper scripts:

```bash
cd build
cpack -G "TGZ;DEB;RPM"

# Build standalone AppImage + .zsync metadata:
cd ..
./scripts/build_appimage.sh build dist 5.17.0 Sebo1411 EASy68KLinux

# Generate static APT & RPM repository metadata:
./scripts/generate_repos.sh dist gh-pages-root
```

---

## Original Credits
- Original Assembler code: Paul McKee, modified by Charles Kelly.
- Original Simulator code: Paul McKee, modified by Charles Kelly and Eric Nelson.
- EASyBIN utility: Charles Kelly.
- Distributed under GNU General Public License.
