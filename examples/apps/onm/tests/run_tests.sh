#!/usr/bin/env bash
# ==============================================================================
# Script: run_tests.sh
# Purpose: Automated unit tests and parity verification for onm in Linux/macOS
# Standard: POSIX bash, Clean Code (AGENTS.md)
# ==============================================================================

set -euo pipefail

CONFIG="${1:-Debug}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../../../" && pwd)"

COMPILER="$PROJECT_ROOT/bin/$CONFIG/ijkl"
VM="$PROJECT_ROOT/bin/$CONFIG/ijkl-m"
INI_FILE="$PROJECT_ROOT/tests/ijkl_en.ini"
if [ ! -f "$INI_FILE" ]; then
    INI_FILE="$PROJECT_ROOT/bin/$CONFIG/ijkl.ini"
fi

if [ -f "$COMPILER.exe" ]; then
    COMPILER="$COMPILER.exe"
fi
if [ -f "$VM.exe" ]; then
    VM="$VM.exe"
fi

echo "======================================================================"
echo " onm — Object Notation Management: Automated Test Suite (bash)"
echo " Compiler: $COMPILER"
echo " VM:       $VM"
echo " INI:      $INI_FILE"
echo "======================================================================"

if [ ! -f "$COMPILER" ]; then
    echo "[ERROR] Compiler executable not found: $COMPILER"
    exit 1
fi
if [ ! -f "$VM" ]; then
    echo "[ERROR] VM executable not found: $VM"
    exit 1
fi

TOTAL_PASSED=0
TOTAL_FAILED=0

# --- Phase 1: Unit Tests ---
echo ""
echo "--- Phase 1: Unit Tests ---"

for test_file in "$SCRIPT_DIR"/test_*.ijk; do
    [ -f "$test_file" ] || continue
    test_name="$(basename "$test_file" .ijk)"
    ci_file="$SCRIPT_DIR/$test_name.ci"

    if "$COMPILER" -ini "$INI_FILE" -o "$ci_file" "$test_file" >/dev/null 2>&1; then
        if vm_output="$("$VM" "$ci_file" 2>&1)" && echo "$vm_output" | grep -q "\[PASS\]"; then
            echo "  [PASS] $test_name (exit = 0)"
            TOTAL_PASSED=$((TOTAL_PASSED + 1))
        else
            echo "  [FAIL] $test_name execution failed"
            TOTAL_FAILED=$((TOTAL_FAILED + 1))
        fi
    else
        echo "  [FAIL] $test_name compilation failed"
        TOTAL_FAILED=$((TOTAL_FAILED + 1))
    fi
done

# --- Phase 2: Compilation of onm.ci and onm_native.ci ---
echo ""
echo "--- Phase 2: Building Application CI Artifacts ---"

ONM_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ONM_SOURCE="$ONM_DIR/onm.ijk"
ONM_CI="$ONM_DIR/onm.ci"
ONM_NATIVE_CI="$ONM_DIR/onm_native.ci"

if "$COMPILER" -ini "$INI_FILE" -o "$ONM_CI" "$ONM_SOURCE" >/dev/null 2>&1; then
    ci_size=$(wc -c < "$ONM_CI")
    echo "  [PASS] onm.ci compiled successfully ($ci_size bytes)"
    TOTAL_PASSED=$((TOTAL_PASSED + 1))
else
    echo "  [FAIL] Failed to compile onm.ci"
    TOTAL_FAILED=$((TOTAL_FAILED + 1))
fi

if "$COMPILER" -ini "$INI_FILE" --native-builtins -o "$ONM_NATIVE_CI" "$ONM_SOURCE" >/dev/null 2>&1; then
    nat_size=$(wc -c < "$ONM_NATIVE_CI")
    echo "  [PASS] onm_native.ci compiled successfully ($nat_size bytes)"
    TOTAL_PASSED=$((TOTAL_PASSED + 1))
else
    echo "  [FAIL] Failed to compile onm_native.ci"
    TOTAL_FAILED=$((TOTAL_FAILED + 1))
fi

# --- Phase 3: Runtime Parity Verification ---
echo ""
echo "--- Phase 3: Runtime Parity Verification ---"

SCENARIO_FILE="$SCRIPT_DIR/scenario_full.txt"
TMP_DIR1="$(mktemp -d -t onm_test1_XXXXXX)"
TMP_DIR2="$(mktemp -d -t onm_test2_XXXXXX)"

cleanup() {
    rm -rf "$TMP_DIR1" "$TMP_DIR2"
}
trap cleanup EXIT

cp "$ONM_CI" "$TMP_DIR1/onm.ci"
cp "$ONM_NATIVE_CI" "$TMP_DIR2/onm_native.ci"

(cd "$TMP_DIR1" && "$VM" onm.ci < "$SCENARIO_FILE" > out.txt 2> err.txt) || true
(cd "$TMP_DIR2" && "$VM" onm_native.ci < "$SCENARIO_FILE" > out.txt 2> err.txt) || true

if diff -q "$TMP_DIR1/out.txt" "$TMP_DIR2/out.txt" >/dev/null 2>&1; then
    echo "  [PASS] Console stdout is 100% IDENTICAL between onm.ci and onm_native.ci"
    TOTAL_PASSED=$((TOTAL_PASSED + 1))
else
    echo "  [FAIL] Console stdout differs between onm.ci and onm_native.ci"
    TOTAL_FAILED=$((TOTAL_FAILED + 1))
fi

if diff -q "$TMP_DIR1/onm.data" "$TMP_DIR2/onm.data" >/dev/null 2>&1; then
    echo "  [PASS] Resulting onm.data is 100% IDENTICAL between onm.ci and onm_native.ci"
    TOTAL_PASSED=$((TOTAL_PASSED + 1))
else
    echo "  [FAIL] Resulting onm.data differs between onm.ci and onm_native.ci"
    TOTAL_FAILED=$((TOTAL_FAILED + 1))
fi

echo ""
echo "======================================================================"
echo " Results: $TOTAL_PASSED Passed, $TOTAL_FAILED Failed"
echo "======================================================================"

if [ "$TOTAL_FAILED" -gt 0 ]; then
    exit 1
fi
exit 0
