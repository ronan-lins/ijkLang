# ==============================================================================
# Parity test runner for ijkL.Text.split and ijkL.Text.splitLines
# Tests CI mode (standard) vs Native mode (--native-builtins)
# ==============================================================================
param(
    [string]$Configuration = "Debug",
    [string]$CompilerPath = "",
    [string]$VmPath = ""
)

$ErrorActionPreference = "Stop"
$ProjectRoot = (Get-Item $PSScriptRoot).Parent.FullName

if (-not $CompilerPath) { $CompilerPath = Join-Path $ProjectRoot "bin\$Configuration\ijkl.exe" }
if (-not $VmPath) { $VmPath = Join-Path $ProjectRoot "bin\$Configuration\ijkl-m.exe" }

$TemporaryDirectory = Join-Path ([IO.Path]::GetTempPath()) ([Guid]::NewGuid().ToString())
New-Item -ItemType Directory -Path $TemporaryDirectory | Out-Null

$TestSource = Join-Path $PSScriptRoot "text\split_tests.ijk"
$IniPath = Join-Path $PSScriptRoot "ijkl_en.ini"

function Invoke-VmProgram {
    param([string]$ProgramPath)
    $StartInfo = New-Object System.Diagnostics.ProcessStartInfo
    $StartInfo.FileName = $VmPath
    $StartInfo.Arguments = "`"$ProgramPath`""
    $StartInfo.WorkingDirectory = $ProjectRoot
    $StartInfo.UseShellExecute = $false
    $StartInfo.RedirectStandardOutput = $true
    $StartInfo.RedirectStandardError = $true

    $Process = [System.Diagnostics.Process]::Start($StartInfo)
    try {
        $OutputTask = $Process.StandardOutput.ReadToEndAsync()
        $ErrorTask = $Process.StandardError.ReadToEndAsync()

        if (-not $Process.WaitForExit(30000)) {
            $Process.Kill()
            $Process.WaitForExit()
            throw "Timeout na VM ao executar $ProgramPath"
        }

        $OutputText = $OutputTask.GetAwaiter().GetResult().Replace("`r`n", "`n").Trim()
        $ErrorText = $ErrorTask.GetAwaiter().GetResult()

        if ($Process.ExitCode -ne 0) {
            throw "VM falhou com exit code $($Process.ExitCode): $OutputText $ErrorText"
        }

        return $OutputText
    } finally {
        if (-not $Process.HasExited) { $Process.Kill(); $Process.WaitForExit() }
        $Process.Dispose()
    }
}

try {
    # 1. Compile in Standard CI Mode
    $CiOutput = Join-Path $TemporaryDirectory "split_tests_ci.ci"
    Push-Location $ProjectRoot
    try {
        & $CompilerPath -ini $IniPath $TestSource -o $CiOutput
        if ($LASTEXITCODE -ne 0) { throw "Falha ao compilar no modo CI" }
    } finally { Pop-Location }

    # 2. Compile in Native Builtins Mode
    $NativeOutput = Join-Path $TemporaryDirectory "split_tests_native.ci"
    Push-Location $ProjectRoot
    try {
        & $CompilerPath -ini $IniPath --native-builtins $TestSource -o $NativeOutput
        if ($LASTEXITCODE -ne 0) { throw "Falha ao compilar no modo --native-builtins" }
    } finally { Pop-Location }

    # 3. Execute CI mode
    $CiResult = Invoke-VmProgram -ProgramPath $CiOutput
    if ($CiResult -ne "ALL_SPLIT_TESTS_PASSED") {
        throw "Modo CI falhou: $CiResult"
    }
    Write-Host "PASS ci: All split and splitLines tests passed" -ForegroundColor Green

    # 4. Execute Native mode
    $NativeResult = Invoke-VmProgram -ProgramPath $NativeOutput
    if ($NativeResult -ne "ALL_SPLIT_TESTS_PASSED") {
        throw "Modo Native falhou: $NativeResult"
    }
    Write-Host "PASS native: All split and splitLines tests passed" -ForegroundColor Green

    # 5. Parity check
    if ($CiResult -ne $NativeResult) {
        throw "Discrepancia entre CI e Native! CI: '$CiResult' vs Native: '$NativeResult'"
    }
    Write-Host "PASS parity: CI and Native outputs are 100% IDENTICAL" -ForegroundColor Green
    Write-Host "Text Split: All checks passed; exact parity verified" -ForegroundColor Cyan

} finally {
    Remove-Item -Path $TemporaryDirectory -Recurse -Force -ErrorAction SilentlyContinue
}
