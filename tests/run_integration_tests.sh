#!/usr/bin/env bash
# ==============================================================================
# ijkLang (ijkL) - Integration Test Suite for Multi-Unit Modular System
# Validates Requirements 1 to 17
# Compatible with Linux, macOS and Unix-like environments (POSIX/Bash)
# ==============================================================================

set -u

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CONFIG="${1:-Debug}"

# Cores ANSI
COLOR_RESET="\033[0m"
COLOR_GREEN="\033[32m"
COLOR_RED="\033[31m"
COLOR_CYAN="\033[36m"
COLOR_GRAY="\033[90m"

# Localização do binário do compilador (ijkl / ijkl.exe)
if [ -f "$PROJECT_ROOT/bin/ijkl" ]; then
    COMPILER_BIN="$PROJECT_ROOT/bin/ijkl"
elif [ -f "$PROJECT_ROOT/bin/$CONFIG/ijkl" ]; then
    COMPILER_BIN="$PROJECT_ROOT/bin/$CONFIG/ijkl"
elif [ -f "$PROJECT_ROOT/bin/$CONFIG/ijkl.exe" ]; then
    COMPILER_BIN="$PROJECT_ROOT/bin/$CONFIG/ijkl.exe"
elif [ -f "$PROJECT_ROOT/bin/Release/ijkl" ]; then
    COMPILER_BIN="$PROJECT_ROOT/bin/Release/ijkl"
elif [ -f "$PROJECT_ROOT/bin/Release/ijkl.exe" ]; then
    COMPILER_BIN="$PROJECT_ROOT/bin/Release/ijkl.exe"
elif [ -f "$PROJECT_ROOT/bin/Debug/ijkl.exe" ]; then
    COMPILER_BIN="$PROJECT_ROOT/bin/Debug/ijkl.exe"
else
    echo "Error: Compiler binary (ijkl) not found. Please build the project first." >&2
    exit 1
fi

# Localização da Máquina Virtual (ijkl-m / ijkl-m.exe)
if [ -f "$PROJECT_ROOT/bin/ijkl-m" ]; then
    VM_BIN="$PROJECT_ROOT/bin/ijkl-m"
elif [ -f "$PROJECT_ROOT/bin/$CONFIG/ijkl-m" ]; then
    VM_BIN="$PROJECT_ROOT/bin/$CONFIG/ijkl-m"
elif [ -f "$PROJECT_ROOT/bin/$CONFIG/ijkl-m.exe" ]; then
    VM_BIN="$PROJECT_ROOT/bin/$CONFIG/ijkl-m.exe"
elif [ -f "$PROJECT_ROOT/bin/Release/ijkl-m" ]; then
    VM_BIN="$PROJECT_ROOT/bin/Release/ijkl-m"
elif [ -f "$PROJECT_ROOT/bin/Release/ijkl-m.exe" ]; then
    VM_BIN="$PROJECT_ROOT/bin/Release/ijkl-m.exe"
elif [ -f "$PROJECT_ROOT/bin/Debug/ijkl-m.exe" ]; then
    VM_BIN="$PROJECT_ROOT/bin/Debug/ijkl-m.exe"
else
    echo "Error: VM binary (ijkl-m) not found. Please build the project first." >&2
    exit 1
fi

# Localização do arquivo INI de configuração
if [ -f "$PROJECT_ROOT/bin/ijkl.ini" ]; then
    INI_FILE="$PROJECT_ROOT/bin/ijkl.ini"
elif [ -f "$PROJECT_ROOT/bin/$CONFIG/ijkl.ini" ]; then
    INI_FILE="$PROJECT_ROOT/bin/$CONFIG/ijkl.ini"
elif [ -f "$PROJECT_ROOT/ijkl.ini" ]; then
    INI_FILE="$PROJECT_ROOT/ijkl.ini"
else
    echo "Error: Configuration file (ijkl.ini) not found." >&2
    exit 1
fi

printf "${COLOR_CYAN}======================================================================${COLOR_RESET}\n"
printf "${COLOR_CYAN}ijkLang (ijkL) - Multi-Unit Modular Architecture Integration Tests${COLOR_RESET}\n"
printf "${COLOR_GRAY}Compiler: %s${COLOR_RESET}\n" "$COMPILER_BIN"
printf "${COLOR_GRAY}VM:       %s${COLOR_RESET}\n" "$VM_BIN"
printf "${COLOR_GRAY}INI:      %s${COLOR_RESET}\n" "$INI_FILE"
printf "${COLOR_CYAN}======================================================================${COLOR_RESET}\n"

PASSED_COUNT=0
FAILED_COUNT=0
TOTAL_COUNT=0

assert_test() {
    local test_name="$1"
    local success="$2"
    local detail="${3:-}"

    TOTAL_COUNT=$((TOTAL_COUNT + 1))
    if [ "$success" -eq 1 ]; then
        PASSED_COUNT=$((PASSED_COUNT + 1))
        printf "  ${COLOR_GREEN}[PASS] %s${COLOR_RESET}\n" "$test_name"
    else
        FAILED_COUNT=$((FAILED_COUNT + 1))
        printf "  ${COLOR_RED}[FAIL] %s${COLOR_RESET}\n" "$test_name"
        if [ -n "$detail" ]; then
            printf "         ${COLOR_RED}%s${COLOR_RESET}\n" "$detail"
        fi
    fi
}

BUILD_DIR="$PROJECT_ROOT/build/integration"
mkdir -p "$BUILD_DIR"

# ------------------------------------------------------------------------------
# Test 1: Variant 1 (A.B.C.ijk)
# ------------------------------------------------------------------------------
T1_SRC="$PROJECT_ROOT/tests/integration/test_variant1/Principal.ijk"
T1_CI="$BUILD_DIR/test_variant1.ci"
T1_OUT=$("$COMPILER_BIN" -ini "$INI_FILE" -o "$T1_CI" "$T1_SRC" 2>&1)
T1_CODE=$?
if [ $T1_CODE -eq 0 ] && [ -f "$T1_CI" ]; then
    "$VM_BIN" "$T1_CI" >/dev/null 2>&1
    VM_RET=$?
    if [ $VM_RET -eq 20 ]; then
        assert_test "Variant 1 (A.B.C.ijk) - Compilation and VM Execution (return = 20)" 1
    else
        assert_test "Variant 1 (A.B.C.ijk) - Execution" 0 "Expected return 20, got $VM_RET"
    fi
else
    assert_test "Variant 1 (A.B.C.ijk) - Compilation" 0 "$T1_OUT"
fi

# ------------------------------------------------------------------------------
# Test 2: Variant 2 (A.B/C.ijk)
# ------------------------------------------------------------------------------
T2_SRC="$PROJECT_ROOT/tests/integration/test_variant2/Principal.ijk"
T2_CI="$BUILD_DIR/test_variant2.ci"
T2_OUT=$("$COMPILER_BIN" -ini "$INI_FILE" -o "$T2_CI" "$T2_SRC" 2>&1)
T2_CODE=$?
if [ $T2_CODE -eq 0 ] && [ -f "$T2_CI" ]; then
    "$VM_BIN" "$T2_CI" >/dev/null 2>&1
    VM_RET=$?
    if [ $VM_RET -eq 18 ]; then
        assert_test "Variant 2 (A.B/C.ijk) - Compilation and VM Execution (return = 18)" 1
    else
        assert_test "Variant 2 (A.B/C.ijk) - Execution" 0 "Expected return 18, got $VM_RET"
    fi
else
    assert_test "Variant 2 (A.B/C.ijk) - Compilation" 0 "$T2_OUT"
fi

# ------------------------------------------------------------------------------
# Test 3: Variant 3 (A/B/C.ijk)
# ------------------------------------------------------------------------------
T3_SRC="$PROJECT_ROOT/tests/integration/test_variant3/Principal.ijk"
T3_CI="$BUILD_DIR/test_variant3.ci"
T3_OUT=$("$COMPILER_BIN" -ini "$INI_FILE" -o "$T3_CI" "$T3_SRC" 2>&1)
T3_CODE=$?
if [ $T3_CODE -eq 0 ] && [ -f "$T3_CI" ]; then
    "$VM_BIN" "$T3_CI" >/dev/null 2>&1
    VM_RET=$?
    if [ $VM_RET -eq 24 ]; then
        assert_test "Variant 3 (A/B/C.ijk) - Compilation and VM Execution (return = 24)" 1
    else
        assert_test "Variant 3 (A/B/C.ijk) - Execution" 0 "Expected return 24, got $VM_RET"
    fi
else
    assert_test "Variant 3 (A/B/C.ijk) - Compilation" 0 "$T3_OUT"
fi

# ------------------------------------------------------------------------------
# Test 4: Ambiguity Detection
# ------------------------------------------------------------------------------
T4_SRC="$PROJECT_ROOT/tests/integration/test_ambiguity/Principal.ijk"
T4_CI="$BUILD_DIR/test_ambiguity.ci"
T4_OUT=$("$COMPILER_BIN" -ini "$INI_FILE" -o "$T4_CI" "$T4_SRC" 2>&1)
T4_CODE=$?
if [ $T4_CODE -ne 0 ] && echo "$T4_OUT" | grep -qi "Ambiguous"; then
    assert_test "Ambiguity Detection on Multiple Path Matches (Req 8)" 1
else
    assert_test "Ambiguity Detection on Multiple Path Matches (Req 8)" 0 "Compiler should reject ambiguous duplicate files. ExitCode=$T4_CODE"
fi

# ------------------------------------------------------------------------------
# Test 5: Rejection of keywords not in active language
# ------------------------------------------------------------------------------
T5_SRC="$PROJECT_ROOT/tests/integration/test_foreign_lang/Principal.ijk"
T5_CI="$BUILD_DIR/test_foreign_lang.ci"
T5_OUT=$("$COMPILER_BIN" -ini "$INI_FILE" -o "$T5_CI" "$T5_SRC" 2>&1)
T5_CODE=$?
if [ $T5_CODE -ne 0 ] && (echo "$T5_OUT" | grep -qi "Syntax error" || echo "$T5_OUT" | grep -qi "Error"); then
    assert_test "Strict Syntax Error on Unrecognized Keyword (No Language Inference)" 1
else
    assert_test "Strict Syntax Error on Unrecognized Keyword (No Language Inference)" 0 "Compiler should reject unrecognized 'if' in pt-BR mode. ExitCode=$T5_CODE"
fi

# ------------------------------------------------------------------------------
# Test 6: Circular dependency
# ------------------------------------------------------------------------------
T6_SRC="$PROJECT_ROOT/tests/integration/test_circular/Principal.ijk"
T6_CI="$BUILD_DIR/test_circular.ci"
T6_OUT=$("$COMPILER_BIN" -ini "$INI_FILE" -o "$T6_CI" "$T6_SRC" 2>&1)
T6_CODE=$?
if [ $T6_CODE -eq 0 ] && [ -f "$T6_CI" ]; then
    "$VM_BIN" "$T6_CI" >/dev/null 2>&1
    VM_RET=$?
    if [ $VM_RET -eq 42 ]; then
        assert_test "Circular Dependency without Infinite Loops (Req 9) (return = 42)" 1
    else
        assert_test "Circular Dependency without Infinite Loops (Req 9)" 0 "Expected return 42, got $VM_RET"
    fi
else
    assert_test "Circular Dependency without Infinite Loops (Req 9)" 0 "$T6_OUT"
fi

# ------------------------------------------------------------------------------
# Test 7: Symbol collision on short-name resolution
# ------------------------------------------------------------------------------
T7_SRC="$PROJECT_ROOT/tests/integration/test_collision/Principal.ijk"
T7_CI="$BUILD_DIR/test_collision.ci"
T7_OUT=$("$COMPILER_BIN" -ini "$INI_FILE" -o "$T7_CI" "$T7_SRC" 2>&1)
T7_CODE=$?
if [ $T7_CODE -ne 0 ] && (echo "$T7_OUT" | grep -qi "Ambiguous identifier" || echo "$T7_OUT" | grep -qi "multiple imported modules"); then
    assert_test "Short-Name Symbol Collision Error (Req 10)" 1
else
    assert_test "Short-Name Symbol Collision Error (Req 10)" 0 "Compiler should report ambiguous collision. ExitCode=$T7_CODE"
fi

# ------------------------------------------------------------------------------
# Test 8: Full CLI Arguments and Exit Code via r0
# ------------------------------------------------------------------------------
T8_SRC="$PROJECT_ROOT/tests/integration/test_cli_args/Principal.ijk"
T8_CI="$BUILD_DIR/test_cli_args.ci"
T8_OUT=$("$COMPILER_BIN" -ini "$INI_FILE" -o "$T8_CI" "$T8_SRC" 2>&1)
T8_CODE=$?
if [ $T8_CODE -eq 0 ] && [ -f "$T8_CI" ]; then
    "$VM_BIN" "$T8_CI" primeiroSegundo argumentoDois >/dev/null 2>&1
    VM_RET=$?
    if [ $VM_RET -eq 42 ]; then
        assert_test "VM Bootstrap with CLI Arguments and Return via r0 (Req 14) (exit_code = 42)" 1
    else
        assert_test "VM Bootstrap with CLI Arguments and Return via r0 (Req 14)" 0 "Expected return 42, got $VM_RET"
    fi
else
    assert_test "VM Bootstrap with CLI Arguments and Return via r0 (Req 14)" 0 "$T8_OUT"
fi

# ------------------------------------------------------------------------------
# Test 9: Modular Visibility - Public Interface (Rules 0, 1, 2 Valid)
# ------------------------------------------------------------------------------
T9_SRC="$PROJECT_ROOT/tests/integration/test_visibility/PrincipalValido.ijk"
T9_CI="$BUILD_DIR/test_visibility_valido.ci"
T9_OUT=$("$COMPILER_BIN" -ini "$INI_FILE" -o "$T9_CI" "$T9_SRC" 2>&1)
T9_CODE=$?
if [ $T9_CODE -eq 0 ] && [ -f "$T9_CI" ]; then
    "$VM_BIN" "$T9_CI" >/dev/null 2>&1
    VM_RET=$?
    if [ $VM_RET -eq 37 ]; then
        assert_test "Modular Visibility: Public Constants, Types & Public Routines (exit_code = 37)" 1
    else
        assert_test "Modular Visibility: Public Constants, Types & Public Routines" 0 "Expected return 37, got $VM_RET"
    fi
else
    assert_test "Modular Visibility: Public Constants, Types & Public Routines" 0 "$T9_OUT"
fi

# ------------------------------------------------------------------------------
# Test 10: Modular Visibility - Accessing Private Routine Fails
# ------------------------------------------------------------------------------
T10_SRC="$PROJECT_ROOT/tests/integration/test_visibility/PrincipalRotinaPrivada.ijk"
T10_CI="$BUILD_DIR/test_visibility_priv_routine.ci"
T10_OUT=$("$COMPILER_BIN" -ini "$INI_FILE" -o "$T10_CI" "$T10_SRC" 2>&1)
T10_CODE=$?
if [ $T10_CODE -ne 0 ] && echo "$T10_OUT" | grep -qi "is private and cannot be accessed"; then
    assert_test "Modular Visibility: Accessing Private Routine Fails with Semantic Error" 1
else
    assert_test "Modular Visibility: Accessing Private Routine Fails with Semantic Error" 0 "Compiler should reject accessing private routine. ExitCode=$T10_CODE"
fi

# ------------------------------------------------------------------------------
# Test 11: Modular Visibility - Accessing Private Module Variable Fails
# ------------------------------------------------------------------------------
T11_SRC="$PROJECT_ROOT/tests/integration/test_visibility/PrincipalVariavelPrivada.ijk"
T11_CI="$BUILD_DIR/test_visibility_priv_var.ci"
T11_OUT=$("$COMPILER_BIN" -ini "$INI_FILE" -o "$T11_CI" "$T11_SRC" 2>&1)
T11_CODE=$?
if [ $T11_CODE -ne 0 ] && echo "$T11_OUT" | grep -qi "is private to unit"; then
    assert_test "Modular Visibility: Accessing Private Module Variable Fails with Semantic Error" 1
else
    assert_test "Modular Visibility: Accessing Private Module Variable Fails with Semantic Error" 0 "Compiler should reject accessing private variable. ExitCode=$T11_CODE"
fi

# ------------------------------------------------------------------------------
# Test 12: Modular Visibility - Unit Routine Without Visibility Modifier Fails at Parser
# ------------------------------------------------------------------------------
T12_SRC="$PROJECT_ROOT/tests/integration/test_visibility/UnidadeSemModificador.ijk"
T12_CI="$BUILD_DIR/test_visibility_missing_modifier.ci"
T12_OUT=$("$COMPILER_BIN" -ini "$INI_FILE" -o "$T12_CI" "$T12_SRC" 2>&1)
T12_CODE=$?
if [ $T12_CODE -ne 0 ] && echo "$T12_OUT" | grep -qi "Expected 'publico' or 'privado'"; then
    assert_test "Modular Visibility: Unit Routine Without Visibility Modifier Fails at Parser" 1
else
    assert_test "Modular Visibility: Unit Routine Without Visibility Modifier Fails at Parser" 0 "Compiler should require publico or privado in units. ExitCode=$T12_CODE"
fi

# ------------------------------------------------------------------------------
# Test 13: Modular Visibility - Program Routine Without Visibility Modifier Fails at Parser
# ------------------------------------------------------------------------------
T13_SRC="$PROJECT_ROOT/tests/integration/test_visibility/ProgramaSemModificador.ijk"
T13_CI="$BUILD_DIR/test_visibility_prog_missing_modifier.ci"
T13_OUT=$("$COMPILER_BIN" -ini "$INI_FILE" -o "$T13_CI" "$T13_SRC" 2>&1)
T13_CODE=$?
if [ $T13_CODE -ne 0 ] && echo "$T13_OUT" | grep -qi "Expected 'publico' or 'privado'"; then
    assert_test "Modular Visibility: Program Routine Without Visibility Modifier Fails at Parser" 1
else
    assert_test "Modular Visibility: Program Routine Without Visibility Modifier Fails at Parser" 0 "Compiler should require publico or privado in programs. ExitCode=$T13_CODE"
fi

# ------------------------------------------------------------------------------
# Test 14: Entry Function Declared as 'publico' Fails with Semantic Error
# ------------------------------------------------------------------------------
T14_SRC="$PROJECT_ROOT/tests/integration/test_visibility/PrincipalPublico.ijk"
T14_CI="$BUILD_DIR/test_visibility_principal_publico.ci"
T14_OUT=$("$COMPILER_BIN" -ini "$INI_FILE" -o "$T14_CI" "$T14_SRC" 2>&1)
T14_CODE=$?
if [ $T14_CODE -ne 0 ] && echo "$T14_OUT" | grep -qi "must be declared as 'privado'"; then
    assert_test "Modular Visibility: Entry Function Declared as 'publico' Fails with Semantic Error" 1
else
    assert_test "Modular Visibility: Entry Function Declared as 'publico' Fails with Semantic Error" 0 "Compiler must reject publico entry point. ExitCode=$T14_CODE"
fi

# ------------------------------------------------------------------------------
# Test 15: Importing Program Unit Fails with Error
# ------------------------------------------------------------------------------
T15_SRC="$PROJECT_ROOT/tests/integration/test_visibility/ImportarPrograma.ijk"
T15_CI="$BUILD_DIR/test_visibility_import_program.ci"
T15_OUT=$("$COMPILER_BIN" -ini "$INI_FILE" -o "$T15_CI" "$T15_SRC" 2>&1)
T15_CODE=$?
if [ $T15_CODE -ne 0 ] && echo "$T15_OUT" | grep -qi "Cannot import program unit"; then
    assert_test "Modular Architecture: Importing Program Unit Fails with Error" 1
else
    assert_test "Modular Architecture: Importing Program Unit Fails with Error" 0 "Compiler must reject importing program units. ExitCode=$T15_CODE"
fi

# ------------------------------------------------------------------------------
# Test 16: Entry Function in Module Unit Fails with Semantic Error
# ------------------------------------------------------------------------------
T16_SRC="$PROJECT_ROOT/tests/integration/test_visibility/UnidadeComPrincipal.ijk"
T16_CI="$BUILD_DIR/test_visibility_unit_with_main.ci"
T16_OUT=$("$COMPILER_BIN" -ini "$INI_FILE" -o "$T16_CI" "$T16_SRC" 2>&1)
T16_CODE=$?
if [ $T16_CODE -ne 0 ] && echo "$T16_OUT" | grep -qi "cannot be declared inside a module unit"; then
    assert_test "Modular Architecture: Entry Function in Module Unit Fails with Semantic Error" 1
else
    assert_test "Modular Architecture: Entry Function in Module Unit Fails with Semantic Error" 0 "Compiler must reject entry point in module unit. ExitCode=$T16_CODE"
fi

# ------------------------------------------------------------------------------
# Test 17: Routine in Program Unit Declared as 'publico' Fails with Semantic Error
# ------------------------------------------------------------------------------
T17_SRC="$PROJECT_ROOT/tests/integration/test_visibility/ProgramaRotinaPublica.ijk"
T17_CI="$BUILD_DIR/test_visibility_prog_public_routine.ci"
T17_OUT=$("$COMPILER_BIN" -ini "$INI_FILE" -o "$T17_CI" "$T17_SRC" 2>&1)
T17_CODE=$?
if [ $T17_CODE -ne 0 ] && echo "$T17_OUT" | grep -qi "cannot be declared as 'publico'"; then
    assert_test "Modular Visibility: Routine in Program Unit Declared as 'publico' Fails with Semantic Error" 1
else
    assert_test "Modular Visibility: Routine in Program Unit Declared as 'publico' Fails with Semantic Error" 0 "Compiler must reject public routines in program units. ExitCode=$T17_CODE"
fi

# ------------------------------------------------------------------------------
# Test 18: Loose Statements Directly in Namespace Body Fail with Syntax Error
# ------------------------------------------------------------------------------
T18_SRC="$PROJECT_ROOT/tests/integration/test_lifecycle/CorpoComComando.ijk"
T18_CI="$BUILD_DIR/test_lifecycle_corpo_comando.ci"
T18_OUT=$("$COMPILER_BIN" -ini "$INI_FILE" -o "$T18_CI" "$T18_SRC" 2>&1)
T18_CODE=$?
if [ $T18_CODE -ne 0 ] && echo "$T18_OUT" | grep -qi "Executable statements are not allowed directly in namespace body"; then
    assert_test "Pure Declarations: Loose Executable Statements in Namespace Body Fail at Parser" 1
else
    assert_test "Pure Declarations: Loose Executable Statements in Namespace Body Fail at Parser" 0 "Compiler must reject loose statements. ExitCode=$T18_CODE"
fi

# ------------------------------------------------------------------------------
# Test 19: Lifecycle Routine Declared as Public Fails with Semantic Error
# ------------------------------------------------------------------------------
T19_SRC="$PROJECT_ROOT/tests/integration/test_lifecycle/CicloVidaPublico.ijk"
T19_CI="$BUILD_DIR/test_lifecycle_publico.ci"
T19_OUT=$("$COMPILER_BIN" -ini "$INI_FILE" -o "$T19_CI" "$T19_SRC" 2>&1)
T19_CODE=$?
if [ $T19_CODE -ne 0 ] && echo "$T19_OUT" | grep -qi "must be declared as 'privado'"; then
    assert_test "Lifecycle Routines: Lifecycle Procedure Declared as 'publico' Fails with Semantic Error" 1
else
    assert_test "Lifecycle Routines: Lifecycle Procedure Declared as 'publico' Fails with Semantic Error" 0 "Compiler must reject public lifecycle routine. ExitCode=$T19_CODE"
fi

# ------------------------------------------------------------------------------
# Test 20: Lifecycle Routine with Return Value Fails with Semantic Error
# ------------------------------------------------------------------------------
T20_SRC="$PROJECT_ROOT/tests/integration/test_lifecycle/CicloVidaComRetorno.ijk"
T20_CI="$BUILD_DIR/test_lifecycle_retorno.ci"
T20_OUT=$("$COMPILER_BIN" -ini "$INI_FILE" -o "$T20_CI" "$T20_SRC" 2>&1)
T20_CODE=$?
if [ $T20_CODE -ne 0 ] && echo "$T20_OUT" | grep -qi "must be a procedure"; then
    assert_test "Lifecycle Routines: Lifecycle Function with Return Value Fails with Semantic Error" 1
else
    assert_test "Lifecycle Routines: Lifecycle Function with Return Value Fails with Semantic Error" 0 "Compiler must reject lifecycle routine with return value. ExitCode=$T20_CODE"
fi

# ------------------------------------------------------------------------------
# Test 21: Lifecycle Routine with Parameters Fails with Semantic Error
# ------------------------------------------------------------------------------
T21_SRC="$PROJECT_ROOT/tests/integration/test_lifecycle/CicloVidaComParametros.ijk"
T21_CI="$BUILD_DIR/test_lifecycle_params.ci"
T21_OUT=$("$COMPILER_BIN" -ini "$INI_FILE" -o "$T21_CI" "$T21_SRC" 2>&1)
T21_CODE=$?
if [ $T21_CODE -ne 0 ] && echo "$T21_OUT" | grep -qi "cannot take any parameters"; then
    assert_test "Lifecycle Routines: Lifecycle Routine with Parameters Fails with Semantic Error" 1
else
    assert_test "Lifecycle Routines: Lifecycle Routine with Parameters Fails with Semantic Error" 0 "Compiler must reject lifecycle routine with parameters. ExitCode=$T21_CODE"
fi

# ------------------------------------------------------------------------------
# Test 22: Module and Program Lifecycle Routines Execution in VM
# ------------------------------------------------------------------------------
T22_SRC="$PROJECT_ROOT/tests/integration/test_lifecycle/ProgramaCiclo.ijk"
T22_CI="$BUILD_DIR/test_lifecycle_exec.ci"
T22_OUT=$("$COMPILER_BIN" -ini "$INI_FILE" -o "$T22_CI" "$T22_SRC" 2>&1)
T22_CODE=$?
if [ $T22_CODE -eq 0 ] && [ -f "$T22_CI" ]; then
    "$VM_BIN" "$T22_CI" >/dev/null 2>&1
    VM_RET=$?
    if [ $VM_RET -eq 45 ]; then
        assert_test "Lifecycle Routines: Initialization and Teardown Execution in VM (exit_code = 45)" 1
    else
        assert_test "Lifecycle Routines: Initialization and Teardown Execution in VM" 0 "Expected return 45, got $VM_RET"
    fi
else
    assert_test "Lifecycle Routines: Initialization and Teardown Execution in VM" 0 "$T22_OUT"
fi

# ------------------------------------------------------------------------------
# Test 23: Default Output File (.ci) Inferred When -o is Omitted
# ------------------------------------------------------------------------------
T23_SRC="$BUILD_DIR/test_default_output.ijk"
T23_EXPECTED_CI="$BUILD_DIR/test_default_output.ci"
cat << 'EOF' > "$T23_SRC"
programa TesteDefaultOutput {
    privado principal(): inteiro {
        retorne 50;
    }
}
EOF
rm -f "$T23_EXPECTED_CI"
T23_OUT=$("$COMPILER_BIN" -ini "$INI_FILE" "$T23_SRC" 2>&1)
T23_CODE=$?
if [ $T23_CODE -eq 0 ] && [ -f "$T23_EXPECTED_CI" ]; then
    "$VM_BIN" "$T23_EXPECTED_CI" >/dev/null 2>&1
    VM_RET=$?
    if [ $VM_RET -eq 50 ]; then
        assert_test "Default Output: Inferred <source>.ci When -o is Omitted (exit_code = 50)" 1
    else
        assert_test "Default Output: Inferred <source>.ci When -o is Omitted" 0 "Expected return 50, got $VM_RET"
    fi
    rm -f "$T23_SRC" "$T23_EXPECTED_CI"
else
    assert_test "Default Output: Inferred <source>.ci When -o is Omitted" 0 "Failed to generate $T23_EXPECTED_CI. Output: $T23_OUT"
fi

# ------------------------------------------------------------------------------
# Test 24: Extension Inference (.ijk in compiler, .ci in VM)
# ------------------------------------------------------------------------------
T24_BASE="$BUILD_DIR/test_ext_inference"
T24_SRC="$T24_BASE.ijk"
T24_EXPECTED_CI="$T24_BASE.ci"
cat << 'EOF' > "$T24_SRC"
programa TesteExtInference {
    privado principal(): inteiro {
        retorne 77;
    }
}
EOF
rm -f "$T24_EXPECTED_CI"
T24_OUT=$("$COMPILER_BIN" -ini "$INI_FILE" "$T24_BASE" 2>&1)
T24_CODE=$?
if [ $T24_CODE -eq 0 ] && [ -f "$T24_EXPECTED_CI" ]; then
    "$VM_BIN" "$T24_BASE" >/dev/null 2>&1
    VM_RET=$?
    if [ $VM_RET -eq 77 ]; then
        assert_test "Extension Inference: Compiler infers .ijk and VM infers .ci (exit_code = 77)" 1
    else
        assert_test "Extension Inference: Compiler infers .ijk and VM infers .ci" 0 "Expected return 77, got $VM_RET"
    fi
    rm -f "$T24_SRC" "$T24_EXPECTED_CI"
else
    assert_test "Extension Inference: Compiler infers .ijk and VM infers .ci" 0 "Failed to compile $T24_BASE without extension. Output: $T24_OUT"
fi

# ------------------------------------------------------------------------------
# Test 25: Routine Overloading & Name Mangling (VM execution exit_code = 42)
# ------------------------------------------------------------------------------
T25_SRC="$PROJECT_ROOT/tests/test_overloading.ijk"
T25_CI="$BUILD_DIR/test_overloading.ci"
T25_OUT=$("$COMPILER_BIN" -ini "$INI_FILE" -o "$T25_CI" "$T25_SRC" 2>&1)
T25_CODE=$?
if [ $T25_CODE -eq 0 ] && [ -f "$T25_CI" ]; then
    "$VM_BIN" "$T25_CI" >/dev/null 2>&1
    VM_RET=$?
    if [ $VM_RET -eq 42 ]; then
        assert_test "Routine Overloading: Functions, Procedures, Widening & VM Execution (exit_code = 42)" 1
    else
        assert_test "Routine Overloading: Functions, Procedures, Widening & VM Execution" 0 "Expected return 42, got $VM_RET"
    fi
else
    assert_test "Routine Overloading: Compilation" 0 "Failed to compile $T25_SRC. Output: $T25_OUT"
fi

# ------------------------------------------------------------------------------
# Test 26: Overloading Semantic Error on Duplicate Parameter Types
# ------------------------------------------------------------------------------
T26_SRC="$BUILD_DIR/test_err_dup.ijk"
cat << 'EOF' > "$T26_SRC"
programa TesteDup {
    privado somar(inteiro a): inteiro { retorne a; }
    privado somar(inteiro b): inteiro { retorne b; }
    privado principal(): inteiro { retorne 0; }
}
EOF
T26_OUT=$("$COMPILER_BIN" -ini "$INI_FILE" "$T26_SRC" 2>&1)
T26_CODE=$?
if [ $T26_CODE -ne 0 ] && echo "$T26_OUT" | grep -qi "Redeclaration of routine 'somar' with identical parameter types"; then
    assert_test "Overloading: Duplicate Parameter Types Fails with Semantic Error" 1
else
    assert_test "Overloading: Duplicate Parameter Types Fails with Semantic Error" 0 "Expected error on duplicate parameter types. Output: $T26_OUT"
fi
rm -f "$T26_SRC"

# ------------------------------------------------------------------------------
# Test 27: Overloading Semantic Error on Ambiguous Call
# ------------------------------------------------------------------------------
T27_SRC="$BUILD_DIR/test_err_ambig.ijk"
cat << 'EOF' > "$T27_SRC"
programa TesteAmbig {
    privado proc(real a, inteiro b): inteiro { retorne 1; }
    privado proc(inteiro a, real b): inteiro { retorne 2; }
    privado principal(): inteiro { retorne proc(1, 2); }
}
EOF
T27_OUT=$("$COMPILER_BIN" -ini "$INI_FILE" "$T27_SRC" 2>&1)
T27_CODE=$?
if [ $T27_CODE -ne 0 ] && echo "$T27_OUT" | grep -qi "Call to overloaded routine 'proc' is ambiguous"; then
    assert_test "Overloading: Ambiguous Overload Call Fails with Semantic Error" 1
else
    assert_test "Overloading: Ambiguous Overload Call Fails with Semantic Error" 0 "Expected ambiguous call error. Output: $T27_OUT"
fi
rm -f "$T27_SRC"

# ------------------------------------------------------------------------------
# Test 28: Overloading Semantic Error on Collision with Variable
# ------------------------------------------------------------------------------
T28_SRC="$BUILD_DIR/test_err_collision.ijk"
cat << 'EOF' > "$T28_SRC"
programa TesteColisao {
    inteiro item = 10;
    privado item(inteiro a): inteiro { retorne a; }
    privado principal(): inteiro { retorne 0; }
}
EOF
T28_OUT=$("$COMPILER_BIN" -ini "$INI_FILE" "$T28_SRC" 2>&1)
T28_CODE=$?
if [ $T28_CODE -ne 0 ] && echo "$T28_OUT" | grep -qi "Redeclaration of identifier 'item' in the same scope"; then
    assert_test "Overloading: Collision Between Routine and Variable Fails with Semantic Error" 1
else
    assert_test "Overloading: Collision Between Routine and Variable Fails with Semantic Error" 0 "Expected collision error. Output: $T28_OUT"
fi
rm -f "$T28_SRC"

# ------------------------------------------------------------------------------
# Test 29: Overloading Semantic Error on Overloading Entry Point
# ------------------------------------------------------------------------------
T29_SRC="$BUILD_DIR/test_err_main.ijk"
cat << 'EOF' > "$T29_SRC"
programa TesteOverloadMain {
    privado principal(inteiro a): inteiro { retorne a; }
    privado principal(): inteiro { retorne 0; }
}
EOF
T29_OUT=$("$COMPILER_BIN" -ini "$INI_FILE" "$T29_SRC" 2>&1)
T29_CODE=$?
if [ $T29_CODE -ne 0 ] && echo "$T29_OUT" | grep -qi "Routine 'principal' cannot be overloaded"; then
    assert_test "Overloading: Overloading Entry Point 'principal' Fails with Semantic Error" 1
else
    assert_test "Overloading: Overloading Entry Point 'principal' Fails with Semantic Error" 0 "Expected entry point overload error. Output: $T29_OUT"
fi
rm -f "$T29_SRC"

# ------------------------------------------------------------------------------
# Test 30: Portuguese Framework ijkL.Conversao (Module & Member Aliases, VM exit_code = 52)
# ------------------------------------------------------------------------------
T30_SRC="$BUILD_DIR/test_conversao_pt.ijk"
T30_CI="$BUILD_DIR/test_conversao_pt.ci"
cat << 'EOF' > "$T30_SRC"
importe ijkL.Conversao;

programa TesteConversaoPT {
    privado principal(): inteiro {
        inteiro a = paraInteiro("42");
        inteiro b = ijkL.Conversao.paraInteiro("10");
        retorne a + b;
    }
}
EOF
T30_OUT=$("$COMPILER_BIN" -ini "$INI_FILE" -o "$T30_CI" "$T30_SRC" 2>&1)
T30_CODE=$?
if [ $T30_CODE -eq 0 ] && [ -f "$T30_CI" ]; then
    "$VM_BIN" "$T30_CI" >/dev/null 2>&1
    VM_RET=$?
    if [ $VM_RET -eq 52 ]; then
        assert_test "Framework ijkL.Conversao: Portuguese Module & Member Aliases (exit_code = 52)" 1
    else
        assert_test "Framework ijkL.Conversao: Portuguese Module & Member Aliases" 0 "Expected return 52, got $VM_RET"
    fi
else
    assert_test "Framework ijkL.Conversao: Compilation" 0 "Failed to compile $T30_SRC. Output: $T30_OUT"
fi
rm -f "$T30_SRC"

# ------------------------------------------------------------------------------
# Test 31: English Framework ijkL.Convert (Direct Canonical, -l en-US, VM exit_code = 43)
# ------------------------------------------------------------------------------
T31_SRC="$BUILD_DIR/test_convert_en.ijk"
T31_CI="$BUILD_DIR/test_convert_en.ci"
cat << 'EOF' > "$T31_SRC"
import ijkL.Convert;

program TestConvertEN {
    private main(): int {
        int a = toInt("40");
        int b = ijkL.Convert.toInt("3");
        return a + b;
    }
}
EOF
T31_OUT=$("$COMPILER_BIN" -ini "$INI_FILE" -l en-US -o "$T31_CI" "$T31_SRC" 2>&1)
T31_CODE=$?
if [ $T31_CODE -eq 0 ] && [ -f "$T31_CI" ]; then
    "$VM_BIN" "$T31_CI" >/dev/null 2>&1
    VM_RET=$?
    if [ $VM_RET -eq 43 ]; then
        assert_test "Framework ijkL.Convert: English Canonical Resolution & Overloading (exit_code = 43)" 1
    else
        assert_test "Framework ijkL.Convert: English Canonical Resolution & Overloading" 0 "Expected return 43, got $VM_RET"
    fi
else
    assert_test "Framework ijkL.Convert: Compilation" 0 "Failed to compile $T31_SRC. Output: $T31_OUT"
fi
rm -f "$T31_SRC"

# ------------------------------------------------------------------------------
# Test 32: Portuguese Framework ijkL.Texto (length, compare, concat, substring, VM exit_code = 11)
# ------------------------------------------------------------------------------
T32_SRC="$BUILD_DIR/test_texto_pt.ijk"
T32_CI="$BUILD_DIR/test_texto_pt.ci"
cat << 'EOF' > "$T32_SRC"
importe ijkL.Texto;

programa TesteTextoPT {
    privado principal(): inteiro {
        texto s = "abcdef";
        inteiro tam = ijkL.Texto.tamanho(s);
        inteiro cmp = ijkL.Texto.comparar("abc", "abc");
        texto cat = ijkL.Texto.concatenar("hello", "world");
        inteiro cat_tam = tamanho(cat);
        texto sub = subtexto(cat, 0, 5);
        inteiro sub_tam = tamanho(sub);
        retorne tam + cmp + cat_tam - sub_tam;
    }
}
EOF
T32_OUT=$("$COMPILER_BIN" -ini "$INI_FILE" -o "$T32_CI" "$T32_SRC" 2>&1)
T32_CODE=$?
if [ $T32_CODE -eq 0 ] && [ -f "$T32_CI" ]; then
    "$VM_BIN" "$T32_CI" >/dev/null 2>&1
    VM_RET=$?
    if [ $VM_RET -eq 11 ]; then
        assert_test "Framework ijkL.Texto: String & Text Processing Operations (exit_code = 11)" 1
    else
        assert_test "Framework ijkL.Texto: String & Text Processing Operations" 0 "Expected return 11, got $VM_RET"
    fi
else
    assert_test "Framework ijkL.Texto: Compilation" 0 "Failed to compile $T32_SRC. Output: $T32_OUT"
fi
rm -f "$T32_SRC"

# ------------------------------------------------------------------------------
# Test 33: Portuguese Framework ijkL.Colecoes (Dynamic List, Add, Get, Size, Destroy, VM exit_code = 53)
# ------------------------------------------------------------------------------
T33_SRC="$BUILD_DIR/test_colecoes_pt.ijk"
T33_CI="$BUILD_DIR/test_colecoes_pt.ci"
cat << 'EOF' > "$T33_SRC"
importe ijkL.Colecoes;

programa TesteColecoesPT {
    tipo Item = registro {
        inteiro valor;
    };

    privado principal(): inteiro {
        List lista = criarLista(2);
        ponteiro Item i1 = reserve($Item);
        @i1.valor = 10;
        ponteiro Item i2 = reserve($Item);
        @i2.valor = 25;
        ponteiro Item i3 = reserve($Item);
        @i3.valor = 15;

        adicionar(lista, i1);
        adicionar(lista, i2);
        adicionar(lista, i3);

        inteiro sz = tamanhoLista(lista);
        ponteiro Item r1 = obter(lista, 0);
        ponteiro Item r2 = obter(lista, 1);
        ponteiro Item r3 = obter(lista, 2);

        inteiro total = @r1.valor + @r2.valor + @r3.valor + sz;

        libere(i1);
        libere(i2);
        libere(i3);
        destruirLista(lista);
        retorne total;
    }
}
EOF
T33_OUT=$("$COMPILER_BIN" -ini "$INI_FILE" -o "$T33_CI" "$T33_SRC" 2>&1)
T33_CODE=$?
if [ $T33_CODE -eq 0 ] && [ -f "$T33_CI" ]; then
    "$VM_BIN" "$T33_CI" >/dev/null 2>&1
    VM_RET=$?
    if [ $VM_RET -eq 53 ]; then
        assert_test "Framework ijkL.Colecoes: Dynamic List Allocation, Growth & Element Access (exit_code = 53)" 1
    else
        assert_test "Framework ijkL.Colecoes: Dynamic List Allocation, Growth & Element Access" 0 "Expected return 53, got $VM_RET"
    fi
else
    assert_test "Framework ijkL.Colecoes: Compilation" 0 "Failed to compile $T33_SRC. Output: $T33_OUT"
fi

# ------------------------------------------------------------------------------
# Test 34: VM Native Builtins Framework Parity (--native-builtins, exit_code = 53)
# ------------------------------------------------------------------------------
T34_CI="$BUILD_DIR/test_colecoes_pt_native.ci"
T34_OUT=$("$COMPILER_BIN" -ini "$INI_FILE" --native-builtins -o "$T34_CI" "$T33_SRC" 2>&1)
T34_CODE=$?
if [ $T34_CODE -eq 0 ] && [ -f "$T34_CI" ]; then
    "$VM_BIN" "$T34_CI" >/dev/null 2>&1
    VM_RET=$?
    if [ $VM_RET -eq 53 ]; then
        assert_test "Framework Parity: Compilation with --native-builtins and VM Execution (exit_code = 53)" 1
    else
        assert_test "Framework Parity: Compilation with --native-builtins and VM Execution" 0 "Expected return 53, got $VM_RET"
    fi
else
    assert_test "Framework Parity: Compilation with --native-builtins" 0 "Failed to compile with --native-builtins. Output: $T34_OUT"
rm -f "$T33_SRC"

# ------------------------------------------------------------------------------
# Test 35: Inline Assembly Block (asm { ... }) Execution & Parity (exit_code = 42)
# ------------------------------------------------------------------------------
T35_SRC="$BUILD_DIR/test_asm_block.ijk"
T35_CI="$BUILD_DIR/test_asm_block.ci"
cat << 'EOF' > "$T35_SRC"
programa TesteAsmBlock {
    privado dobrar(inteiro n): inteiro {
        inteiro d = 0;
        asm {
            r0 := [ra, 0];
            r0 := r0 + r0;
            [ra, 8] := r0;
        }
        retorne d;
    }

    privado principal(): inteiro {
        inteiro res = dobrar(21);
        retorne res;
    }
}
EOF
T35_OUT=$("$COMPILER_BIN" -ini "$INI_FILE" -o "$T35_CI" "$T35_SRC" 2>&1)
T35_CODE=$?
if [ $T35_CODE -eq 0 ] && [ -f "$T35_CI" ]; then
    "$VM_BIN" "$T35_CI" >/dev/null 2>&1
    VM_RET=$?
    if [ $VM_RET -eq 42 ]; then
        assert_test "Inline Assembly Block (asm { ... }): Code Generation & VM Execution (exit_code = 42)" 1
    else
        assert_test "Inline Assembly Block (asm { ... }): Code Generation & VM Execution" 0 "Expected return 42, got $VM_RET"
    fi
else
    assert_test "Inline Assembly Block (asm { ... }): Compilation" 0 "Failed to compile $T35_SRC. Output: $T35_OUT"
fi
rm -f "$T35_SRC"

# ------------------------------------------------------------------------------
# Test 36: Framework ijkL.Memory: DMA Block Copy, Fill & Compare (exit_code = 42)
# ------------------------------------------------------------------------------
T36_SRC="$BUILD_DIR/test_memory_dma_integration.ijk"
T36_CI="$BUILD_DIR/test_memory_dma_integration.ci"
cat << 'EOF' > "$T36_SRC"
importe ijkL.Memoria;

programa TesteMemoriaDMA {
    privado principal(): inteiro {
        ponteiro buf1 = reserve(32);
        ponteiro buf2 = reserve(32);

        ijkL.Memoria.preencher(buf1, 65, 32);
        ijkL.Memoria.preencher(buf2, 65, 32);

        inteiro cmp1 = ijkL.Memoria.comparar(buf1, buf2, 32);
        se (cmp1 != 0) {
            retorne 1;
        }

        buf2[2] = 66;
        inteiro cmp2 = ijkL.Memoria.comparar(buf1, buf2, 32);
        se (cmp2 == 0) {
            retorne 2;
        }

        ijkL.Memoria.copiar(buf1, buf2, 32);
        inteiro cmp3 = ijkL.Memoria.comparar(buf1, buf2, 32);
        se (cmp3 != 0) {
            retorne 3;
        }

        Integer item = buf1[2];
        se (item.value != 66) {
            retorne 4;
        }

        libere(buf1);
        libere(buf2);
        retorne 42;
    }
}
EOF
T36_OUT=$("$COMPILER_BIN" -ini "$INI_FILE" -o "$T36_CI" "$T36_SRC" 2>&1)
T36_CODE=$?
if [ $T36_CODE -eq 0 ] && [ -f "$T36_CI" ]; then
    "$VM_BIN" "$T36_CI" >/dev/null 2>&1
    VM_RET=$?
    if [ $VM_RET -eq 42 ]; then
        assert_test "Framework ijkL.Memory: DMA Block Copy, Fill & Compare (exit_code = 42)" 1
    else
        assert_test "Framework ijkL.Memory: DMA Block Copy, Fill & Compare" 0 "Expected return 42, got $VM_RET"
    fi
else
    assert_test "Framework ijkL.Memory: Compilation" 0 "Failed to compile $T36_SRC. Output: $T36_OUT"
fi
rm -f "$T36_SRC"

# ------------------------------------------------------------------------------
# Test 37: Framework ijkL.IO (File, Folder, Path) (exit_code = 42)
# ------------------------------------------------------------------------------
T37_SRC="$BUILD_DIR/test_io_integration.ijk"
T37_CI="$BUILD_DIR/test_io_integration.ci"
cat << 'EOF' > "$T37_SRC"
importe ijkL.ES.Arquivo;
importe ijkL.ES.Pasta;
importe ijkL.IO.Path;
importe ijkL.Texto;

programa TesteIntegracaoFS {
    privado principal(): inteiro {
        texto dir = "build/teste_integracao_fs";
        texto arq = "dados.txt";
        texto caminho = combinar(dir, arq);

        se (ijkL.ES.Arquivo.existe(caminho)) {
            ijkL.ES.Arquivo.excluir(caminho);
        }

        se (!ijkL.ES.Pasta.existe(dir)) {
            logico c_ok = ijkL.ES.Pasta.criar(dir);
            se (!c_ok) {
                retorne 1;
            }
        }

        logico w_ok = ijkL.ES.Arquivo.escreverTodoTexto(caminho, "Conteudo Didatico ijkLang");
        se (!w_ok) {
            retorne 2;
        }

        se (!ijkL.ES.Arquivo.existe(caminho)) {
            retorne 3;
        }

        texto lido = ijkL.ES.Arquivo.lerTodoTexto(caminho);
        se (!ijkL.Texto.igual(lido, "Conteudo Didatico ijkLang")) {
            retorne 4;
        }

        logico a_ok = ijkL.ES.Arquivo.anexarTexto(caminho, " - Anexo");
        se (!a_ok) {
            retorne 5;
        }

        texto lido2 = ijkL.ES.Arquivo.lerTodoTexto(caminho);
        se (!ijkL.Texto.igual(lido2, "Conteudo Didatico ijkLang - Anexo")) {
            retorne 6;
        }

        logico d_ok = ijkL.ES.Arquivo.excluir(caminho);
        se (!d_ok || ijkL.ES.Arquivo.existe(caminho)) {
            retorne 7;
        }

        ijkL.ES.Pasta.excluir(dir);
        retorne 42;
    }
}
EOF
T37_OUT=$("$COMPILER_BIN" -ini "$INI_FILE" -o "$T37_CI" "$T37_SRC" 2>&1)
T37_CODE=$?
if [ $T37_CODE -eq 0 ] && [ -f "$T37_CI" ]; then
    "$VM_BIN" "$T37_CI" >/dev/null 2>&1
    VM_RET=$?
    if [ $VM_RET -eq 42 ]; then
        assert_test "Framework ijkL.IO (File, Folder, Path): File System Operations (exit_code = 42)" 1
    else
        assert_test "Framework ijkL.IO (File, Folder, Path): File System Operations" 0 "Expected return 42, got $VM_RET"
    fi
else
    assert_test "Framework ijkL.IO (File, Folder, Path): Compilation" 0 "Failed to compile $T37_SRC. Output: $T37_OUT"
fi
rm -f "$T37_SRC"

# ------------------------------------------------------------------------------
# Test 38: Standard Framework Extensions: Math, Text & Collections (exit_code = 42)
# ------------------------------------------------------------------------------
T38_SRC="$BUILD_DIR/test_framework_extensions_integration.ijk"
T38_CI="$BUILD_DIR/test_framework_extensions_integration.ci"
cat << 'EOF' > "$T38_SRC"
importe ijkL.Matematica;
importe ijkL.Texto;
importe ijkL.Colecoes;

programa TesteFrameworkExtensoes {
    privado principal(): inteiro {
        // --- 1. Matematica ---
        inteiro a = ijkL.Matematica.absoluto(-50);
        inteiro m1 = ijkL.Matematica.minimo(10, 25);
        inteiro m2 = ijkL.Matematica.maximo(10, 25);
        inteiro c1 = ijkL.Matematica.limitar(5, 10, 20);
        inteiro p1 = ijkL.Matematica.potencia(2, 6);
        real r1 = ijkL.Matematica.raiz(25.0);

        se (a != 50 || m1 != 10 || m2 != 25 || c1 != 10 || p1 != 64 || r1 != 5.0) {
            retorne 1;
        }

        // --- 2. Texto ---
        texto t1 = "  ijkLang Didatico  ";
        texto t_aparado = ijkL.Texto.aparar(t1);
        se (!ijkL.Texto.igual(t_aparado, "ijkLang Didatico")) {
            retorne 2;
        }
        se (!ijkL.Texto.comecaCom(t_aparado, "ijkLang") || !ijkL.Texto.terminaCom(t_aparado, "Didatico")) {
            retorne 3;
        }
        se (!ijkL.Texto.contem(t_aparado, "Lang") || ijkL.Texto.indiceDe(t_aparado, "Didatico") != 8) {
            retorne 4;
        }

        // --- 3. Colecoes: Lista, Pilha, Fila, Mapa ---
        List lista = ijkL.Colecoes.criarLista(2);
        ijkL.Colecoes.adicionar(lista, "X");
        ijkL.Colecoes.adicionar(lista, "Y");
        ijkL.Colecoes.adicionar(lista, "Z");
        se (lista.count != 3) {
            retorne 5;
        }
        ijkL.Colecoes.removerEm(lista, 1);
        texto elem1 = ijkL.Colecoes.obter(lista, 1);
        se (!ijkL.Texto.igual(elem1, "Z") || lista.count != 2) {
            retorne 6;
        }
        ijkL.Colecoes.destruirLista(lista);

        Stack pilha = ijkL.Colecoes.criarPilha(2);
        ijkL.Colecoes.empilhar(pilha, "P1");
        ijkL.Colecoes.empilhar(pilha, "P2");
        texto topo_p = ijkL.Colecoes.topo(pilha);
        texto desemp = ijkL.Colecoes.desempilhar(pilha);
        se (!ijkL.Texto.igual(topo_p, "P2") || !ijkL.Texto.igual(desemp, "P2") || pilha.count != 1) {
            retorne 7;
        }
        ijkL.Colecoes.destruirPilha(pilha);

        Queue fila = ijkL.Colecoes.criarFila(2);
        ijkL.Colecoes.enfileirar(fila, "F1");
        ijkL.Colecoes.enfileirar(fila, "F2");
        texto topo_f = ijkL.Colecoes.topo(fila);
        texto desenf = ijkL.Colecoes.desenfileirar(fila);
        se (!ijkL.Texto.igual(topo_f, "F1") || !ijkL.Texto.igual(desenf, "F1") || fila.count != 1) {
            retorne 8;
        }
        ijkL.Colecoes.destruirFila(fila);

        Map mapa = ijkL.Colecoes.criarMapa(4);
        ijkL.Colecoes.inserir(mapa, "k1", "V1");
        ijkL.Colecoes.inserir(mapa, "k2", "V2");
        se (!ijkL.Colecoes.contemChave(mapa, "k1") || !ijkL.Colecoes.contemChave(mapa, "k2")) {
            retorne 9;
        }
        texto v1 = ijkL.Colecoes.obter(mapa, "k1");
        se (!ijkL.Texto.igual(v1, "V1")) {
            retorne 10;
        }
        ijkL.Colecoes.remover(mapa, "k1");
        se (ijkL.Colecoes.contemChave(mapa, "k1") || mapa.count != 1) {
            retorne 11;
        }
        ijkL.Colecoes.destruirMapa(mapa);

        retorne 42;
    }
}
EOF
T38_OUT=$("$COMPILER_BIN" -ini "$INI_FILE" -o "$T38_CI" "$T38_SRC" 2>&1)
T38_CODE=$?
if [ $T38_CODE -eq 0 ] && [ -f "$T38_CI" ]; then
    "$VM_BIN" "$T38_CI" >/dev/null 2>&1
    VM_RET=$?
    if [ $VM_RET -eq 42 ]; then
        assert_test "Framework Extensions: Math, Text & Collections (exit_code = 42)" 1
    else
        assert_test "Framework Extensions: Math, Text & Collections" 0 "Expected return 42, got $VM_RET"
    fi
else
    assert_test "Framework Extensions: Compilation" 0 "Failed to compile $T38_SRC. Output: $T38_OUT"
fi
rm -f "$T38_SRC"



printf "${COLOR_CYAN}======================================================================${COLOR_RESET}\n"
if [ $FAILED_COUNT -eq 0 ]; then
    printf "${COLOR_GREEN}Integration Results: %d Passed, %d Failed, %d Total${COLOR_RESET}\n" "$PASSED_COUNT" "$FAILED_COUNT" "$TOTAL_COUNT"
else
    printf "${COLOR_RED}Integration Results: %d Passed, %d Failed, %d Total${COLOR_RESET}\n" "$PASSED_COUNT" "$FAILED_COUNT" "$TOTAL_COUNT"
fi
printf "${COLOR_CYAN}======================================================================${COLOR_RESET}\n"

if [ $FAILED_COUNT -gt 0 ]; then
    exit 1
else
    exit 0
fi
