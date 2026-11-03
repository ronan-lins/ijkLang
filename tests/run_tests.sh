#!/usr/bin/env bash
# ==============================================================================
# ijkLang (ijkL) - Test Runner Suite for Lexer, Parser, AST, Codegen and VM
# Executes validation on all examples in examples/en-US and examples/pt-BR
# Compatible with Linux, macOS and Unix-like environments (POSIX/Bash)
# ==============================================================================

set -u

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CONFIG="${1:-Debug}"

# Cores ANSI para saída no terminal
COLOR_RESET="\033[0m"
COLOR_GREEN="\033[32m"
COLOR_RED="\033[31m"
COLOR_CYAN="\033[36m"
COLOR_YELLOW="\033[33m"
COLOR_MAGENTA="\033[35m"
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

# Localização do binário da Máquina Virtual (ijkl-m / ijkl-m.exe)
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

printf "${COLOR_CYAN}======================================================================${COLOR_RESET}\n"
printf "${COLOR_CYAN}ijkLang (ijkL) - Comprehensive Compiler & VM Test Suite${COLOR_RESET}\n"
printf "${COLOR_GRAY}Compiler: %s${COLOR_RESET}\n" "$COMPILER_BIN"
printf "${COLOR_GRAY}VM:       %s${COLOR_RESET}\n" "$VM_BIN"
printf "${COLOR_CYAN}======================================================================${COLOR_RESET}\n"

PASSED_COUNT=0
FAILED_COUNT=0
TOTAL_COUNT=0

run_compiler_stage_test() {
    local file_path="$1"
    local ini_file="$2"
    local lang_label="$3"
    local stage_flag="$4"
    local stage_name="$5"
    local file_name
    file_name="$(basename "$file_path")"
    TOTAL_COUNT=$((TOTAL_COUNT + 1))

    local stdout_err
    stdout_err=$("$COMPILER_BIN" -ini "$ini_file" "$stage_flag" "$file_path" 2>&1)
    local exit_code=$?

    local has_error=0
    if [ $exit_code -ne 0 ]; then
        has_error=1
    elif echo "$stdout_err" | grep -q "TOKEN_ERROR"; then
        has_error=1
    elif echo "$stdout_err" | grep -q "\[Error\]"; then
        has_error=1
    elif echo "$stdout_err" | grep -q "Syntax error"; then
        has_error=1
    fi

    if [ $has_error -eq 1 ]; then
        FAILED_COUNT=$((FAILED_COUNT + 1))
        printf "  ${COLOR_RED}[FAIL] (%s) [%s] %s${COLOR_RESET}\n" "$lang_label" "$stage_name" "$file_name"
        if [ -n "$stdout_err" ]; then
            printf "         ${COLOR_RED}%s${COLOR_RESET}\n" "$stdout_err"
        fi
    else
        PASSED_COUNT=$((PASSED_COUNT + 1))
        printf "  ${COLOR_GREEN}[PASS] (%s) [%s] %s${COLOR_RESET}\n" "$lang_label" "$stage_name" "$file_name"
    fi
}

run_vm_execution_test() {
    local file_path="$1"
    local ini_file="$2"
    local lang_label="$3"
    local file_name
    file_name="$(basename "$file_path")"
    local base_name="${file_name%.*}"
    TOTAL_COUNT=$((TOTAL_COUNT + 1))

    mkdir -p "$PROJECT_ROOT/build"
    local temp_ci="$PROJECT_ROOT/build/temp_${lang_label}_${base_name}.ci"

    local compile_out
    compile_out=$("$COMPILER_BIN" -ini "$ini_file" -o "$temp_ci" "$file_path" 2>&1)
    local compile_code=$?

    if [ $compile_code -ne 0 ] || [ ! -f "$temp_ci" ]; then
        FAILED_COUNT=$((FAILED_COUNT + 1))
        printf "  ${COLOR_RED}[FAIL] (%s) [VM-Exec/Compile] %s${COLOR_RESET}\n" "$lang_label" "$file_name"
        if [ -n "$compile_out" ]; then
            printf "         ${COLOR_RED}%s${COLOR_RESET}\n" "$compile_out"
        fi
        return
    fi

    local vm_out
    vm_out=$("$VM_BIN" --stats "$temp_ci" 2>&1)
    local vm_code=$?

    rm -f "$temp_ci"

    local has_error=0
    if [ $vm_code -ne 0 ]; then
        has_error=1
    elif echo "$vm_out" | grep -q "\[VM Error\]"; then
        has_error=1
    fi

    if [ $has_error -eq 1 ]; then
        FAILED_COUNT=$((FAILED_COUNT + 1))
        printf "  ${COLOR_RED}[FAIL] (%s) [VM-Exec] %s (ExitCode: %d)${COLOR_RESET}\n" "$lang_label" "$file_name" "$vm_code"
        if [ -n "$vm_out" ]; then
            printf "         ${COLOR_RED}%s${COLOR_RESET}\n" "$vm_out"
        fi
    else
        PASSED_COUNT=$((PASSED_COUNT + 1))
        printf "  ${COLOR_GREEN}[PASS] (%s) [VM-Exec] %s${COLOR_RESET}\n" "$lang_label" "$file_name"
    fi
}

EN_INI="$PROJECT_ROOT/tests/ijkl_en.ini"
PT_INI="$PROJECT_ROOT/tests/ijkl_pt.ini"
ADDITIONAL_EN="$PROJECT_ROOT/tests/test_english.ijk"
ADDITIONAL_PT="$PROJECT_ROOT/tests/test_lexer.ijk"

# ------------------------------------------------------------------------------
# FASE 1: Análise Léxica (Lexer --tokens)
# ------------------------------------------------------------------------------
printf "\n${COLOR_MAGENTA}>>> [STAGE 1/5] Lexical Analysis (--tokens):${COLOR_RESET}\n"

printf "\n  ${COLOR_YELLOW}>> examples/en-US:${COLOR_RESET}\n"
for file in "$PROJECT_ROOT"/examples/en-US/*.ijk; do
    [ -f "$file" ] && run_compiler_stage_test "$file" "$EN_INI" "en-US" "--tokens" "Lexer"
done

printf "\n  ${COLOR_YELLOW}>> examples/pt-BR:${COLOR_RESET}\n"
for file in "$PROJECT_ROOT"/examples/pt-BR/*.ijk; do
    [ -f "$file" ] && run_compiler_stage_test "$file" "$PT_INI" "pt-BR" "--tokens" "Lexer"
done

if [ -f "$ADDITIONAL_EN" ]; then
    run_compiler_stage_test "$ADDITIONAL_EN" "$EN_INI" "en-US" "--tokens" "Lexer"
fi
if [ -f "$ADDITIONAL_PT" ]; then
    run_compiler_stage_test "$ADDITIONAL_PT" "$PT_INI" "pt-BR" "--tokens" "Lexer"
fi

# ------------------------------------------------------------------------------
# FASE 2: Análise Sintática e Construção da AST (Parser --ast)
# ------------------------------------------------------------------------------
printf "\n${COLOR_MAGENTA}>>> [STAGE 2/5] Syntactic Analysis & AST Construction (--ast):${COLOR_RESET}\n"

printf "\n  ${COLOR_YELLOW}>> examples/en-US:${COLOR_RESET}\n"
for file in "$PROJECT_ROOT"/examples/en-US/*.ijk; do
    [ -f "$file" ] && run_compiler_stage_test "$file" "$EN_INI" "en-US" "--ast" "Parser/AST"
done

printf "\n  ${COLOR_YELLOW}>> examples/pt-BR:${COLOR_RESET}\n"
for file in "$PROJECT_ROOT"/examples/pt-BR/*.ijk; do
    [ -f "$file" ] && run_compiler_stage_test "$file" "$PT_INI" "pt-BR" "--ast" "Parser/AST"
done

if [ -f "$ADDITIONAL_EN" ]; then
    run_compiler_stage_test "$ADDITIONAL_EN" "$EN_INI" "en-US" "--ast" "Parser/AST"
fi
if [ -f "$ADDITIONAL_PT" ]; then
    run_compiler_stage_test "$ADDITIONAL_PT" "$PT_INI" "pt-BR" "--ast" "Parser/AST"
fi

# ------------------------------------------------------------------------------
# FASE 3: Tabela de Símbolos e Escopos (Symtab --symtab)
# ------------------------------------------------------------------------------
printf "\n${COLOR_MAGENTA}>>> [STAGE 3/5] Symbol Table & Scopes Analysis (--symtab):${COLOR_RESET}\n"

printf "\n  ${COLOR_YELLOW}>> examples/en-US:${COLOR_RESET}\n"
for file in "$PROJECT_ROOT"/examples/en-US/*.ijk; do
    [ -f "$file" ] && run_compiler_stage_test "$file" "$EN_INI" "en-US" "--symtab" "Symtab"
done

printf "\n  ${COLOR_YELLOW}>> examples/pt-BR:${COLOR_RESET}\n"
for file in "$PROJECT_ROOT"/examples/pt-BR/*.ijk; do
    [ -f "$file" ] && run_compiler_stage_test "$file" "$PT_INI" "pt-BR" "--symtab" "Symtab"
done

if [ -f "$ADDITIONAL_EN" ]; then
    run_compiler_stage_test "$ADDITIONAL_EN" "$EN_INI" "en-US" "--symtab" "Symtab"
fi
if [ -f "$ADDITIONAL_PT" ]; then
    run_compiler_stage_test "$ADDITIONAL_PT" "$PT_INI" "pt-BR" "--symtab" "Symtab"
fi

# ------------------------------------------------------------------------------
# FASE 4: Análise Semântica e Geração de Código Intermediário (--codegen)
# ------------------------------------------------------------------------------
printf "\n${COLOR_MAGENTA}>>> [STAGE 4/5] Semantic Validation & CI Code Generation (--codegen):${COLOR_RESET}\n"

printf "\n  ${COLOR_YELLOW}>> examples/en-US:${COLOR_RESET}\n"
for file in "$PROJECT_ROOT"/examples/en-US/*.ijk; do
    [ -f "$file" ] && run_compiler_stage_test "$file" "$EN_INI" "en-US" "--codegen" "Codegen"
done

printf "\n  ${COLOR_YELLOW}>> examples/pt-BR:${COLOR_RESET}\n"
for file in "$PROJECT_ROOT"/examples/pt-BR/*.ijk; do
    [ -f "$file" ] && run_compiler_stage_test "$file" "$PT_INI" "pt-BR" "--codegen" "Codegen"
done

if [ -f "$ADDITIONAL_EN" ]; then
    run_compiler_stage_test "$ADDITIONAL_EN" "$EN_INI" "en-US" "--codegen" "Codegen"
fi
if [ -f "$ADDITIONAL_PT" ]; then
    run_compiler_stage_test "$ADDITIONAL_PT" "$PT_INI" "pt-BR" "--codegen" "Codegen"
fi

# ------------------------------------------------------------------------------
# FASE 5: Execução na Máquina Virtual (ijkl-m)
# ------------------------------------------------------------------------------
printf "\n${COLOR_MAGENTA}>>> [STAGE 5/5] Virtual Machine Execution (ijkl-m):${COLOR_RESET}\n"

printf "\n  ${COLOR_YELLOW}>> examples/en-US:${COLOR_RESET}\n"
for file in "$PROJECT_ROOT"/examples/en-US/*.ijk; do
    [ -f "$file" ] && run_vm_execution_test "$file" "$EN_INI" "en-US"
done

printf "\n  ${COLOR_YELLOW}>> examples/pt-BR:${COLOR_RESET}\n"
for file in "$PROJECT_ROOT"/examples/pt-BR/*.ijk; do
    [ -f "$file" ] && run_vm_execution_test "$file" "$PT_INI" "pt-BR"
done

# Vetores de referência e paridade do gerador pseudoaleatório.
TOTAL_COUNT=$((TOTAL_COUNT + 1))
if python3 "$PROJECT_ROOT/tests/run_random_tests.py" --compiler "$COMPILER_BIN" --vm "$VM_BIN"; then
    PASSED_COUNT=$((PASSED_COUNT + 1))
else
    FAILED_COUNT=$((FAILED_COUNT + 1))
fi

printf "\n${COLOR_CYAN}======================================================================${COLOR_RESET}\n"
if [ $FAILED_COUNT -eq 0 ]; then
    printf "${COLOR_GREEN}Test Results Summary: %d Passed, %d Failed (Total: %d)${COLOR_RESET}\n" "$PASSED_COUNT" "$FAILED_COUNT" "$TOTAL_COUNT"
else
    printf "${COLOR_RED}Test Results Summary: %d Passed, %d Failed (Total: %d)${COLOR_RESET}\n" "$PASSED_COUNT" "$FAILED_COUNT" "$TOTAL_COUNT"
fi
printf "${COLOR_CYAN}======================================================================${COLOR_RESET}\n"

if [ $FAILED_COUNT -gt 0 ]; then
    exit 1
else
    exit 0
fi
