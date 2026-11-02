#!/usr/bin/env bash
# ==============================================================================
# Script: onm.sh
# Application: onm — Object Notation Management
# Description: Launcher and compiler for onm (interactive & FPGA modes)
# ==============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

# ------------------------------------------------------------------------------
# Localizacao dos binarios da VM (ijkl-m) e do Compilador (ijkl)
# ------------------------------------------------------------------------------
VM_BIN=""
if [ -f "$PROJECT_ROOT/bin/Debug/ijkl-m" ]; then
    VM_BIN="$PROJECT_ROOT/bin/Debug/ijkl-m"
elif [ -f "$PROJECT_ROOT/bin/Debug/ijkl-m.exe" ]; then
    VM_BIN="$PROJECT_ROOT/bin/Debug/ijkl-m.exe"
elif [ -f "$PROJECT_ROOT/bin/Release/ijkl-m" ]; then
    VM_BIN="$PROJECT_ROOT/bin/Release/ijkl-m"
elif [ -f "$PROJECT_ROOT/bin/Release/ijkl-m.exe" ]; then
    VM_BIN="$PROJECT_ROOT/bin/Release/ijkl-m.exe"
elif [ -f "$PROJECT_ROOT/bin/ijkl-m" ]; then
    VM_BIN="$PROJECT_ROOT/bin/ijkl-m"
elif [ -f "$PROJECT_ROOT/bin/ijkl-m.exe" ]; then
    VM_BIN="$PROJECT_ROOT/bin/ijkl-m.exe"
elif command -v ijkl-m >/dev/null 2>&1; then
    VM_BIN="ijkl-m"
fi

COMPILER_BIN=""
if [ -f "$PROJECT_ROOT/bin/Debug/ijkl" ]; then
    COMPILER_BIN="$PROJECT_ROOT/bin/Debug/ijkl"
elif [ -f "$PROJECT_ROOT/bin/Debug/ijkl.exe" ]; then
    COMPILER_BIN="$PROJECT_ROOT/bin/Debug/ijkl.exe"
elif [ -f "$PROJECT_ROOT/bin/Release/ijkl" ]; then
    COMPILER_BIN="$PROJECT_ROOT/bin/Release/ijkl"
elif [ -f "$PROJECT_ROOT/bin/Release/ijkl.exe" ]; then
    COMPILER_BIN="$PROJECT_ROOT/bin/Release/ijkl.exe"
elif [ -f "$PROJECT_ROOT/bin/ijkl" ]; then
    COMPILER_BIN="$PROJECT_ROOT/bin/ijkl"
elif [ -f "$PROJECT_ROOT/bin/ijkl.exe" ]; then
    COMPILER_BIN="$PROJECT_ROOT/bin/ijkl.exe"
elif command -v ijkl >/dev/null 2>&1; then
    COMPILER_BIN="ijkl"
fi

INI_FILE=""
if [ -f "$PROJECT_ROOT/tests/ijkl_en.ini" ]; then
    INI_FILE="$PROJECT_ROOT/tests/ijkl_en.ini"
elif [ -f "$PROJECT_ROOT/bin/Debug/ijkl.ini" ]; then
    INI_FILE="$PROJECT_ROOT/bin/Debug/ijkl.ini"
elif [ -f "$PROJECT_ROOT/ijkl.ini" ]; then
    INI_FILE="$PROJECT_ROOT/ijkl.ini"
fi

# ------------------------------------------------------------------------------
# Verificacao do arquivo .ci
# ------------------------------------------------------------------------------
if [ -f "$SCRIPT_DIR/onm.ci" ]; then
    if [ -z "$VM_BIN" ]; then
        echo "[ERRO] Maquina Virtual (ijkl-m) nao encontrada no sistema." >&2
        exit 1
    fi
    cd "$SCRIPT_DIR"
    exec "$VM_BIN" onm.ci "$@"
fi

# ------------------------------------------------------------------------------
# Caso onm.ci nao exista: fluxo interativo de compilacao
# ------------------------------------------------------------------------------
echo "[AVISO] O arquivo de codigo intermediario 'onm.ci' nao foi encontrado."
echo ""
echo "=============================================================================="
echo " COMPILACAO DO PROJETO ONM"
echo "=============================================================================="
echo " [1] Modo Padrao (Emite todas as instrucoes no arquivo .ci)"
echo "     - Emite todo o codigo intermediario e rotinas da biblioteca em um unico"
echo "       arquivo .ci auto-contido."
echo "     - [IMPORTANTE] Para utilizacao com o microcontrolador VHDL em FPGA,"
echo "       e OBRIGATORIO utilizar o Modo Padrao."
echo ""
echo " [2] Modo Nativo (--native-builtins)"
echo "     - Omite as instrucoes de funcoes builtin emitidas no .ci."
echo "     - A execucao chamara diretamente as implementacoes internas em C da VM."
echo ""
echo " [N] Cancelar operacao e sair"
echo "=============================================================================="
read -r -p "Selecione a opcao (1, 2 ou N) [1]: " COMPILE_CHOICE
COMPILE_CHOICE="${COMPILE_CHOICE:-1}"

case "$COMPILE_CHOICE" in
    1|[sS]|[yY]|[sS][iI][mM]|[yY][eE][sS])
        COMPILE_CHOICE="1"
        ;;
    2)
        COMPILE_CHOICE="2"
        ;;
    *)
        echo "Operacao cancelada pelo usuario."
        exit 0
        ;;
esac

if [ -z "$COMPILER_BIN" ]; then
    echo "[ERRO] Compilador (ijkl) nao encontrado no sistema." >&2
    exit 1
fi

echo ""
if [ "$COMPILE_CHOICE" = "2" ]; then
    echo "Compilando onm.ijk no Modo Nativo (--native-builtins)..."
    if [ -n "$INI_FILE" ]; then
        "$COMPILER_BIN" -ini "$INI_FILE" --native-builtins -o "$SCRIPT_DIR/onm.ci" "$SCRIPT_DIR/onm.ijk"
    else
        "$COMPILER_BIN" --native-builtins -o "$SCRIPT_DIR/onm.ci" "$SCRIPT_DIR/onm.ijk"
    fi
else
    echo "Compilando onm.ijk no Modo Padrao (completo para VM / FPGA)..."
    if [ -n "$INI_FILE" ]; then
        "$COMPILER_BIN" -ini "$INI_FILE" -o "$SCRIPT_DIR/onm.ci" "$SCRIPT_DIR/onm.ijk"
    else
        "$COMPILER_BIN" -o "$SCRIPT_DIR/onm.ci" "$SCRIPT_DIR/onm.ijk"
    fi
fi

echo ""
echo "[SUCESSO] Arquivo 'onm.ci' gerado com exito."
echo "Iniciando execucao na Maquina Virtual (ijkl-m)..."
echo ""

if [ -z "$VM_BIN" ]; then
    echo "[ERRO] Maquina Virtual (ijkl-m) nao encontrada para execucao." >&2
    exit 1
fi

cd "$SCRIPT_DIR"
exec "$VM_BIN" onm.ci "$@"
