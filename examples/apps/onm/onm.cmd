@echo off
rem ==============================================================================
rem Script: onm.cmd
rem Application: onm ? Object Notation Management
rem Description: Launcher and compiler for onm (interactive & FPGA modes)
rem ==============================================================================

setlocal EnableDelayedExpansion

set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%..\..\..\"

pushd "%PROJECT_ROOT%"
set "PROJECT_ROOT=%CD%\"
popd

rem ------------------------------------------------------------------------------
rem Localizacao dos binarios da VM [ijkl-m] e do Compilador [ijkl]
rem ------------------------------------------------------------------------------
set "VM_BIN="
if exist "%PROJECT_ROOT%bin\Debug\ijkl-m.exe" (
    set "VM_BIN=%PROJECT_ROOT%bin\Debug\ijkl-m.exe"
) else if exist "%PROJECT_ROOT%bin\Release\ijkl-m.exe" (
    set "VM_BIN=%PROJECT_ROOT%bin\Release\ijkl-m.exe"
) else if exist "%PROJECT_ROOT%bin\ijkl-m.exe" (
    set "VM_BIN=%PROJECT_ROOT%bin\ijkl-m.exe"
) else (
    where ijkl-m.exe >nul 2>&1
    if !ERRORLEVEL! equ 0 (
        for /f "delims=" %%I in ('where ijkl-m.exe') do (
            if "!VM_BIN!"=="" set "VM_BIN=%%I"
        )
    )
)

set "COMPILER_BIN="
if exist "%PROJECT_ROOT%bin\Debug\ijkl.exe" (
    set "COMPILER_BIN=%PROJECT_ROOT%bin\Debug\ijkl.exe"
) else if exist "%PROJECT_ROOT%bin\Release\ijkl.exe" (
    set "COMPILER_BIN=%PROJECT_ROOT%bin\Release\ijkl.exe"
) else if exist "%PROJECT_ROOT%bin\ijkl.exe" (
    set "COMPILER_BIN=%PROJECT_ROOT%bin\ijkl.exe"
) else (
    where ijkl.exe >nul 2>&1
    if !ERRORLEVEL! equ 0 (
        for /f "delims=" %%I in ('where ijkl.exe') do (
            if "!COMPILER_BIN!"=="" set "COMPILER_BIN=%%I"
        )
    )
)

set "INI_FILE="
if exist "%PROJECT_ROOT%tests\ijkl_en.ini" (
    set "INI_FILE=%PROJECT_ROOT%tests\ijkl_en.ini"
) else if exist "%PROJECT_ROOT%bin\Debug\ijkl.ini" (
    set "INI_FILE=%PROJECT_ROOT%bin\Debug\ijkl.ini"
) else if exist "%PROJECT_ROOT%ijkl.ini" (
    set "INI_FILE=%PROJECT_ROOT%ijkl.ini"
)

rem ------------------------------------------------------------------------------
rem Verificacao do arquivo .ci
rem ------------------------------------------------------------------------------
if exist "%SCRIPT_DIR%onm.ci" (
    if "!VM_BIN!"=="" (
        echo [ERRO] Maquina Virtual ijkl-m.exe nao encontrada no sistema.
        exit /b 1
    )
    pushd "%SCRIPT_DIR%"
    "!VM_BIN!" onm.ci %*
    set "EXIT_CODE=!ERRORLEVEL!"
    popd
    exit /b !EXIT_CODE!
)

rem ------------------------------------------------------------------------------
rem Caso onm.ci nao exista: fluxo interativo de compilacao
rem ------------------------------------------------------------------------------
echo [AVISO] O arquivo de codigo intermediario 'onm.ci' nao foi encontrado.
echo.
echo ==============================================================================
echo  COMPILACAO DO PROJETO ONM
echo ==============================================================================
echo  [1] Modo Padrao - Emite todas as instrucoes no arquivo .ci
echo      - Emite todo o codigo intermediario e rotinas da biblioteca em um unico
echo        arquivo .ci auto-contido.
echo      - [IMPORTANTE] Para utilizacao com o microcontrolador VHDL em FPGA,
echo        e OBRIGATORIO utilizar o Modo Padrao.
echo.
echo  [2] Modo Nativo - Opcao --native-builtins
echo      - Omite as instrucoes de funcoes builtin emitidas no .ci.
echo      - A execucao chamara diretamente as implementacoes internas em C da VM.
echo.
echo  [N] Cancelar operacao e sair
echo ==============================================================================
set "COMPILE_CHOICE="
set /p "COMPILE_CHOICE=Selecione a opcao [1, 2 ou N] [1]: "
if "!COMPILE_CHOICE!"=="" set "COMPILE_CHOICE=1"

if /i "!COMPILE_CHOICE:~0,1!"=="N" (
    echo Operacao cancelada pelo usuario.
    exit /b 0
)

if "!COMPILER_BIN!"=="" (
    echo [ERRO] Compilador ijkl.exe nao encontrado no sistema.
    exit /b 1
)

echo.
if "!COMPILE_CHOICE:~0,1!"=="2" (
    echo Compilando onm.ijk no Modo Nativo [--native-builtins]...
    if not "!INI_FILE!"=="" (
        "!COMPILER_BIN!" -ini "!INI_FILE!" --native-builtins -o "%SCRIPT_DIR%onm.ci" "%SCRIPT_DIR%onm.ijk"
    ) else (
        "!COMPILER_BIN!" --native-builtins -o "%SCRIPT_DIR%onm.ci" "%SCRIPT_DIR%onm.ijk"
    )
) else (
    echo Compilando onm.ijk no Modo Padrao [completo para VM / FPGA]...
    if not "!INI_FILE!"=="" (
        "!COMPILER_BIN!" -ini "!INI_FILE!" -o "%SCRIPT_DIR%onm.ci" "%SCRIPT_DIR%onm.ijk"
    ) else (
        "!COMPILER_BIN!" -o "%SCRIPT_DIR%onm.ci" "%SCRIPT_DIR%onm.ijk"
    )
)

if !ERRORLEVEL! neq 0 (
    echo.
    echo [ERRO] Falha na compilacao de 'onm.ijk'.
    exit /b !ERRORLEVEL!
)

echo.
echo [SUCESSO] Arquivo 'onm.ci' gerado com exito.
echo Iniciando execucao na Maquina Virtual ijkl-m...
echo.

if "!VM_BIN!"=="" (
    echo [ERRO] Maquina Virtual ijkl-m.exe nao encontrada para execucao.
    exit /b 1
)

pushd "%SCRIPT_DIR%"
"!VM_BIN!" onm.ci %*
set "EXIT_CODE=!ERRORLEVEL!"
popd
exit /b !EXIT_CODE!
