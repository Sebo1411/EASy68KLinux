#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${1:-$ROOT_DIR/build}"

ASM="$BUILD_DIR/easy68k-asm"
SIM="$BUILD_DIR/easy68k-sim"
EASYBIN="$BUILD_DIR/easybin"

if [[ ! -x "$ASM" || ! -x "$SIM" || ! -x "$EASYBIN" ]]; then
    echo "Error: Binaries not found in $BUILD_DIR. Please build first."
    exit 1
fi

echo "============================================================"
echo " EASy68K Linux Native Test Suite"
echo " GCC $(gcc -dumpversion) on $(uname -s) $(uname -m)"
echo "============================================================"

PASSED_COUNT=0
TOTAL_COUNT=0

for test_file in "$SCRIPT_DIR"/test_*.x68; do
    test_name="$(basename "$test_file" .x68)"
    echo -n "Running $test_name... "
    TOTAL_COUNT=$((TOTAL_COUNT + 1))

    list_file="$SCRIPT_DIR/${test_name}.l68"
    s68_file="$SCRIPT_DIR/${test_name}.s68"

    # 1. Assemble
    if ! "$ASM" "$test_file" -o "$s68_file" --listing "$list_file" > /dev/null 2>&1; then
        echo "FAILED (Assembly error)"
        exit 1
    fi

    # 2. Simulate
    sim_out="$("$SIM" -q "$s68_file" 2>&1)"
    if echo "$sim_out" | grep -q "FAILED"; then
        echo "FAILED (Simulation assertion failed)"
        echo "$sim_out"
        exit 1
    fi

    if ! echo "$sim_out" | grep -q "PASSED"; then
        echo "FAILED (Expected 'PASSED' in simulator output)"
        echo "$sim_out"
        exit 1
    fi

    # Optional per-test expectations: one literal substring per line in
    # tests/<name>.expected.  Used for output that PASSED alone cannot assert.
    expected_file="$SCRIPT_DIR/${test_name}.expected"
    if [[ -f "$expected_file" ]]; then
        while IFS= read -r expected || [[ -n "$expected" ]]; do
            [[ -z "$expected" ]] && continue
            if ! grep -Fq -- "$expected" <<< "$sim_out"; then
                echo "FAILED (Missing expected output: $expected)"
                echo "$sim_out"
                exit 1
            fi
        done < "$expected_file"
    fi

    # 3. Test EASyBIN round-trip conversion on this test file
    bin_file="$SCRIPT_DIR/${test_name}.bin"
    reconstructed_s68="$SCRIPT_DIR/${test_name}_recon.s68"

    "$EASYBIN" -s2b "$s68_file" "$bin_file" > /dev/null 2>&1
    "$EASYBIN" -b2s "$bin_file" "$reconstructed_s68" --addr 0x1000 --entry 0x1000 > /dev/null 2>&1

    recon_out="$("$SIM" -q "$reconstructed_s68" 2>&1)"
    if ! echo "$recon_out" | grep -q "PASSED"; then
        echo "FAILED (EASyBIN roundtrip S68 did not pass)"
        exit 1
    fi

    # Clean up generated artifacts
    rm -f "$list_file" "$s68_file" "$bin_file" "$reconstructed_s68"

    echo "PASSED"
    PASSED_COUNT=$((PASSED_COUNT + 1))
done

# Also test examples/hello.x68, examples/hardware_demo.x68, examples/graphics_demo.x68
echo -n "Running examples/hello.x68... "
TOTAL_COUNT=$((TOTAL_COUNT + 1))
"$ASM" "$ROOT_DIR/examples/hello.x68" -o "$SCRIPT_DIR/hello_ex.s68" --listing "$SCRIPT_DIR/hello_ex.l68" > /dev/null
hello_out="$("$SIM" -q "$SCRIPT_DIR/hello_ex.s68")"
rm -f "$SCRIPT_DIR/hello_ex.s68" "$SCRIPT_DIR/hello_ex.l68"
if echo "$hello_out" | grep -q "Hello, Linux EASy68K!"; then
    echo "PASSED"
    PASSED_COUNT=$((PASSED_COUNT + 1))
else
    echo "FAILED"
    exit 1
fi

echo -n "Running examples/hardware_demo.x68... "
TOTAL_COUNT=$((TOTAL_COUNT + 1))
"$ASM" "$ROOT_DIR/examples/hardware_demo.x68" -o "$SCRIPT_DIR/hw_ex.s68" > /dev/null
hw_out="$("$SIM" -q "$SCRIPT_DIR/hw_ex.s68")"
rm -f "$SCRIPT_DIR/hw_ex.s68"
if echo "$hw_out" | grep -q "Hardware Demo initialized"; then
    echo "PASSED"
    PASSED_COUNT=$((PASSED_COUNT + 1))
else
    echo "FAILED"
    exit 1
fi

echo -n "Running examples/graphics_demo.x68... "
TOTAL_COUNT=$((TOTAL_COUNT + 1))
"$ASM" "$ROOT_DIR/examples/graphics_demo.x68" -o "$SCRIPT_DIR/gfx_ex.s68" > /dev/null
gfx_out="$("$SIM" -q "$SCRIPT_DIR/gfx_ex.s68")"
rm -f "$SCRIPT_DIR/gfx_ex.s68"
if echo "$gfx_out" | grep -q "Graphics Demo drawn successfully"; then
    echo "PASSED"
    PASSED_COUNT=$((PASSED_COUNT + 1))
else
    echo "FAILED"
    exit 1
fi

# S-Record parser regression: a DOS/Windows (CRLF) file must load the same
# as the LF version.  The parser used to include '\r' in the byte count and
# mis-read every record.
echo -n "Running CRLF S-Record... "
TOTAL_COUNT=$((TOTAL_COUNT + 1))
crlf_file="$SCRIPT_DIR/hello_crlf.s68"
"$ASM" "$ROOT_DIR/examples/hello.x68" -o "$crlf_file" > /dev/null 2>&1
sed -i 's/$/\r/' "$crlf_file"
crlf_out="$("$SIM" -q "$crlf_file" 2>&1)"
if echo "$crlf_out" | grep -q "Hello, Linux EASy68K!"; then
    echo "PASSED"
    PASSED_COUNT=$((PASSED_COUNT + 1))
else
    echo "FAILED"
    echo "$crlf_out"
    rm -f "$crlf_file"
    exit 1
fi
rm -f "$crlf_file"

echo "============================================================"
echo " Test Suite Summary: $PASSED_COUNT / $TOTAL_COUNT passed successfully!"
echo "============================================================"
