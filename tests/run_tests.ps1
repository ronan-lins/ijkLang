# ==============================================================================
# ijkLang (ijkL) - Test Runner Suite for Lexer, Parser and AST Generation
# Executes validation on all examples in examples/en-US and examples/pt-BR
# ==============================================================================

param(
    [string]$Configuration = "Debug"
)

$ErrorActionPreference = "Stop"

$ProjectRoot = (Get-Item $PSScriptRoot).Parent.FullName
$BinaryPath = Join-Path $ProjectRoot "bin\$Configuration\ijkl.exe"

if (-not (Test-Path $BinaryPath)) {
    # Fallback to Release if Debug not present or vice versa
    $FallbackConfig = if ($Configuration -eq "Debug") { "Release" } else { "Debug" }
    $BinaryPath = Join-Path $ProjectRoot "bin\$FallbackConfig\ijkl.exe"
}

if (-not (Test-Path $BinaryPath)) {
    Write-Error "Compiler binary not found at $BinaryPath. Please build the project first."
}

$VmBinaryPath = Join-Path $ProjectRoot "bin\$Configuration\ijkl-m.exe"
if (-not (Test-Path $VmBinaryPath)) {
    $FallbackConfig = if ($Configuration -eq "Debug") { "Release" } else { "Debug" }
    $VmBinaryPath = Join-Path $ProjectRoot "bin\$FallbackConfig\ijkl-m.exe"
}

if (-not (Test-Path $VmBinaryPath)) {
    Write-Error "VM binary not found at $VmBinaryPath. Please build the project first."
}

Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host "ijkLang (ijkL) - Comprehensive Compiler & VM Test Suite" -ForegroundColor Cyan
Write-Host "Compiler: $BinaryPath" -ForegroundColor Gray
Write-Host "VM:       $VmBinaryPath" -ForegroundColor Gray
Write-Host "======================================================================" -ForegroundColor Cyan

$PassedCount = 0
$FailedCount = 0
$TotalCount = 0

function Run-CompilerStageTest {
    param(
        [string]$FilePath,
        [string]$IniFile,
        [string]$LanguageLabel,
        [string]$StageFlag,
        [string]$StageName
    )

    $global:TotalCount++
    $FileName = Split-Path $FilePath -Leaf

    $ProcessInfo = New-Object System.Diagnostics.ProcessStartInfo
    $ProcessInfo.FileName = $BinaryPath
    $ProcessInfo.Arguments = "-ini `"$IniFile`" $StageFlag `"$FilePath`""
    $ProcessInfo.WorkingDirectory = $ProjectRoot
    $ProcessInfo.RedirectStandardOutput = $true
    $ProcessInfo.RedirectStandardError = $true
    $ProcessInfo.UseShellExecute = $false

    $Process = [System.Diagnostics.Process]::Start($ProcessInfo)
    $Stdout = $Process.StandardOutput.ReadToEnd()
    $Stderr = $Process.StandardError.ReadToEnd()
    $Process.WaitForExit()

    $HasError = ($Process.ExitCode -ne 0) -or ($Stdout -match "TOKEN_ERROR") -or ($Stderr -match "\[Error\]") -or ($Stderr -match "Syntax error")

    if ($HasError) {
        $global:FailedCount++
        Write-Host "  [FAIL] ($LanguageLabel) [$StageName] $FileName" -ForegroundColor Red
        if ($Stderr) {
            Write-Host "         $Stderr" -ForegroundColor DarkRed
        }
    } else {
        $global:PassedCount++
        Write-Host "  [PASS] ($LanguageLabel) [$StageName] $FileName" -ForegroundColor Green
    }
}

function Run-VmExecutionTest {
    param(
        [string]$FilePath,
        [string]$IniFile,
        [string]$LanguageLabel
    )

    $global:TotalCount++
    $FileName = Split-Path $FilePath -Leaf
    $FileBaseName = [System.IO.Path]::GetFileNameWithoutExtension($FilePath)
    $TempCiDir = Join-Path $ProjectRoot "build"
    if (-not (Test-Path $TempCiDir)) {
        New-Item -ItemType Directory -Path $TempCiDir -Force | Out-Null
    }
    $TempCi = Join-Path $TempCiDir "temp_${LanguageLabel}_${FileBaseName}.ci"

    # Passo 1: Compilar para Codigo Intermediario (.ci)
    $CompileProcessInfo = New-Object System.Diagnostics.ProcessStartInfo
    $CompileProcessInfo.FileName = $BinaryPath
    $CompileProcessInfo.Arguments = "-ini `"$IniFile`" -o `"$TempCi`" `"$FilePath`""
    $CompileProcessInfo.WorkingDirectory = $ProjectRoot
    $CompileProcessInfo.RedirectStandardOutput = $true
    $CompileProcessInfo.RedirectStandardError = $true
    $CompileProcessInfo.UseShellExecute = $false

    $CompileProcess = [System.Diagnostics.Process]::Start($CompileProcessInfo)
    $CompileStdout = $CompileProcess.StandardOutput.ReadToEnd()
    $CompileStderr = $CompileProcess.StandardError.ReadToEnd()
    $CompileProcess.WaitForExit()

    if ($CompileProcess.ExitCode -ne 0 -or -not (Test-Path $TempCi)) {
        $global:FailedCount++
        Write-Host "  [FAIL] ($LanguageLabel) [VM-Exec/Compile] $FileName" -ForegroundColor Red
        if ($CompileStderr) {
            Write-Host "         $CompileStderr" -ForegroundColor DarkRed
        }
        return
    }

    # Passo 2: Executar o Codigo Intermediario na Maquina Virtual (ijkl-m.exe)
    $VmProcessInfo = New-Object System.Diagnostics.ProcessStartInfo
    $VmProcessInfo.FileName = $VmBinaryPath
    $VmProcessInfo.Arguments = "--stats `"$TempCi`""
    $VmProcessInfo.WorkingDirectory = $ProjectRoot
    $VmProcessInfo.RedirectStandardOutput = $true
    $VmProcessInfo.RedirectStandardError = $true
    $VmProcessInfo.UseShellExecute = $false

    $VmProcess = [System.Diagnostics.Process]::Start($VmProcessInfo)
    $VmStdout = $VmProcess.StandardOutput.ReadToEnd()
    $VmStderr = $VmProcess.StandardError.ReadToEnd()
    $VmProcess.WaitForExit()

    # Limpeza do arquivo .ci temporario
    if (Test-Path $TempCi) {
        Remove-Item -Path $TempCi -Force -ErrorAction SilentlyContinue
    }

    $HasError = ($VmProcess.ExitCode -ne 0) -or ($VmStderr -match "\[VM Error\]") -or ($VmStdout -match "\[VM Error\]")

    if ($HasError) {
        $global:FailedCount++
        Write-Host "  [FAIL] ($LanguageLabel) [VM-Exec] $FileName (ExitCode: $($VmProcess.ExitCode))" -ForegroundColor Red
        if ($VmStderr) {
            Write-Host "         $VmStderr" -ForegroundColor DarkRed
        }
        if ($VmStdout -match "\[VM Error\]") {
            Write-Host "         $VmStdout" -ForegroundColor DarkRed
        }
    } else {
        $global:PassedCount++
        Write-Host "  [PASS] ($LanguageLabel) [VM-Exec] $FileName" -ForegroundColor Green
    }
}

$EnIni = Join-Path $ProjectRoot "tests\ijkl_en.ini"
$PtIni = Join-Path $ProjectRoot "tests\ijkl_pt.ini"
$EnFiles = Get-ChildItem -Path (Join-Path $ProjectRoot "examples\en-US\*.ijk") | Sort-Object Name
$PtFiles = Get-ChildItem -Path (Join-Path $ProjectRoot "examples\pt-BR\*.ijk") | Sort-Object Name
$AdditionalEn = Join-Path $ProjectRoot "tests\test_english.ijk"
$AdditionalPt = Join-Path $ProjectRoot "tests\test_lexer.ijk"

# ------------------------------------------------------------------------------
# FASE 1: Análise Léxica (Lexer --tokens)
# ------------------------------------------------------------------------------
Write-Host "`n>>> [STAGE 1/5] Lexical Analysis (--tokens):" -ForegroundColor Magenta

Write-Host "`n  >> examples/en-US:" -ForegroundColor Yellow
foreach ($File in $EnFiles) {
    Run-CompilerStageTest -FilePath $File.FullName -IniFile $EnIni -LanguageLabel "en-US" -StageFlag "--tokens" -StageName "Lexer"
}

Write-Host "`n  >> examples/pt-BR:" -ForegroundColor Yellow
foreach ($File in $PtFiles) {
    Run-CompilerStageTest -FilePath $File.FullName -IniFile $PtIni -LanguageLabel "pt-BR" -StageFlag "--tokens" -StageName "Lexer"
}

if (Test-Path $AdditionalEn) {
    Run-CompilerStageTest -FilePath $AdditionalEn -IniFile $EnIni -LanguageLabel "en-US" -StageFlag "--tokens" -StageName "Lexer"
}
if (Test-Path $AdditionalPt) {
    Run-CompilerStageTest -FilePath $AdditionalPt -IniFile $PtIni -LanguageLabel "pt-BR" -StageFlag "--tokens" -StageName "Lexer"
}

# ------------------------------------------------------------------------------
# FASE 2: Análise Sintática e Construção da AST (Parser --ast)
# ------------------------------------------------------------------------------
Write-Host "`n>>> [STAGE 2/5] Syntactic Analysis & AST Construction (--ast):" -ForegroundColor Magenta

Write-Host "`n  >> examples/en-US:" -ForegroundColor Yellow
foreach ($File in $EnFiles) {
    Run-CompilerStageTest -FilePath $File.FullName -IniFile $EnIni -LanguageLabel "en-US" -StageFlag "--ast" -StageName "Parser/AST"
}

Write-Host "`n  >> examples/pt-BR:" -ForegroundColor Yellow
foreach ($File in $PtFiles) {
    Run-CompilerStageTest -FilePath $File.FullName -IniFile $PtIni -LanguageLabel "pt-BR" -StageFlag "--ast" -StageName "Parser/AST"
}

if (Test-Path $AdditionalEn) {
    Run-CompilerStageTest -FilePath $AdditionalEn -IniFile $EnIni -LanguageLabel "en-US" -StageFlag "--ast" -StageName "Parser/AST"
}
if (Test-Path $AdditionalPt) {
    Run-CompilerStageTest -FilePath $AdditionalPt -IniFile $PtIni -LanguageLabel "pt-BR" -StageFlag "--ast" -StageName "Parser/AST"
}

# ------------------------------------------------------------------------------
# FASE 3: Tabela de Símbolos e Escopos (Symtab --symtab)
# ------------------------------------------------------------------------------
Write-Host "`n>>> [STAGE 3/5] Symbol Table & Scopes Analysis (--symtab):" -ForegroundColor Magenta

Write-Host "`n  >> examples/en-US:" -ForegroundColor Yellow
foreach ($File in $EnFiles) {
    Run-CompilerStageTest -FilePath $File.FullName -IniFile $EnIni -LanguageLabel "en-US" -StageFlag "--symtab" -StageName "Symtab"
}

Write-Host "`n  >> examples/pt-BR:" -ForegroundColor Yellow
foreach ($File in $PtFiles) {
    Run-CompilerStageTest -FilePath $File.FullName -IniFile $PtIni -LanguageLabel "pt-BR" -StageFlag "--symtab" -StageName "Symtab"
}

if (Test-Path $AdditionalEn) {
    Run-CompilerStageTest -FilePath $AdditionalEn -IniFile $EnIni -LanguageLabel "en-US" -StageFlag "--symtab" -StageName "Symtab"
}
if (Test-Path $AdditionalPt) {
    Run-CompilerStageTest -FilePath $AdditionalPt -IniFile $PtIni -LanguageLabel "pt-BR" -StageFlag "--symtab" -StageName "Symtab"
}

# ------------------------------------------------------------------------------
# FASE 4: Análise Semântica e Geração de Código Intermediário (--codegen)
# ------------------------------------------------------------------------------
Write-Host "`n>>> [STAGE 4/5] Semantic Validation & CI Code Generation (--codegen):" -ForegroundColor Magenta

Write-Host "`n  >> examples/en-US:" -ForegroundColor Yellow
foreach ($File in $EnFiles) {
    Run-CompilerStageTest -FilePath $File.FullName -IniFile $EnIni -LanguageLabel "en-US" -StageFlag "--codegen" -StageName "Codegen"
}

Write-Host "`n  >> examples/pt-BR:" -ForegroundColor Yellow
foreach ($File in $PtFiles) {
    Run-CompilerStageTest -FilePath $File.FullName -IniFile $PtIni -LanguageLabel "pt-BR" -StageFlag "--codegen" -StageName "Codegen"
}

if (Test-Path $AdditionalEn) {
    Run-CompilerStageTest -FilePath $AdditionalEn -IniFile $EnIni -LanguageLabel "en-US" -StageFlag "--codegen" -StageName "Codegen"
}
if (Test-Path $AdditionalPt) {
    Run-CompilerStageTest -FilePath $AdditionalPt -IniFile $PtIni -LanguageLabel "pt-BR" -StageFlag "--codegen" -StageName "Codegen"
}

# ------------------------------------------------------------------------------
# FASE 5: Execução na Máquina Virtual (ijkl-m)
# ------------------------------------------------------------------------------
Write-Host "`n>>> [STAGE 5/5] Virtual Machine Execution (ijkl-m):" -ForegroundColor Magenta

Write-Host "`n  >> examples/en-US:" -ForegroundColor Yellow
foreach ($File in $EnFiles) {
    Run-VmExecutionTest -FilePath $File.FullName -IniFile $EnIni -LanguageLabel "en-US"
}

Write-Host "`n  >> examples/pt-BR:" -ForegroundColor Yellow
foreach ($File in $PtFiles) {
    Run-VmExecutionTest -FilePath $File.FullName -IniFile $PtIni -LanguageLabel "pt-BR"
}

$TotalCount++
try {
    & (Join-Path $PSScriptRoot "run_random_tests.ps1") -Configuration $Configuration -CompilerPath $BinaryPath -VmPath $VmBinaryPath
    $PassedCount++
} catch {
    Write-Host "[FAIL] Random parity: $_" -ForegroundColor Red
    $FailedCount++
}

$TotalCount++
try {
    & (Join-Path $PSScriptRoot "run_console_tests.ps1") -Configuration $Configuration -CompilerPath $BinaryPath -VmPath $VmBinaryPath
    $PassedCount++
} catch {
    Write-Host "[FAIL] Console parity: $_" -ForegroundColor Red
    $FailedCount++
}

Write-Host "`n======================================================================" -ForegroundColor Cyan
Write-Host "Test Results Summary: $PassedCount Passed, $FailedCount Failed (Total: $TotalCount)" -ForegroundColor $(if ($FailedCount -eq 0) { "Green" } else { "Red" })
Write-Host "======================================================================" -ForegroundColor Cyan

if ($FailedCount -gt 0) {
    exit 1
} else {
    exit 0
}
