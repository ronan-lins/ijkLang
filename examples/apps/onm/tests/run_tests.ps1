param(
    [string]$Config = "Debug"
)

Remove-Item env:https_proxy -ErrorAction SilentlyContinue
Remove-Item env:HTTPS_PROXY -ErrorAction SilentlyContinue
Remove-Item env:http_proxy -ErrorAction SilentlyContinue
Remove-Item env:HTTP_PROXY -ErrorAction SilentlyContinue

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Resolve-Path (Join-Path $ScriptDir "..\..\..\..")
$Compiler = Join-Path $ProjectRoot "bin\$Config\ijkl.exe"
$Vm = Join-Path $ProjectRoot "bin\$Config\ijkl-m.exe"
$IniFile = Join-Path $ProjectRoot "tests\ijkl_en.ini"
if (-not (Test-Path $IniFile)) {
    $IniFile = Join-Path $ProjectRoot "bin\$Config\ijkl.ini"
}

Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host " onm — Object Notation Management: Automated Test Suite" -ForegroundColor Cyan
Write-Host " Compiler: $Compiler"
Write-Host " VM:       $Vm"
Write-Host " INI:      $IniFile"
Write-Host "======================================================================" -ForegroundColor Cyan

if (-not (Test-Path $Compiler)) {
    Write-Host "[ERROR] Compiler executable not found: $Compiler" -ForegroundColor Red
    exit 1
}
if (-not (Test-Path $Vm)) {
    Write-Host "[ERROR] VM executable not found: $Vm" -ForegroundColor Red
    exit 1
}

$TotalPassed = 0
$TotalFailed = 0

# ------------------------------------------------------------------------------
# Phase 1: Unit Tests Execution
# ------------------------------------------------------------------------------
Write-Host "`n--- Phase 1: Unit Tests ---" -ForegroundColor Yellow

$UnitTests = Get-ChildItem -Path $ScriptDir -Filter "test_*.ijk" | Sort-Object Name

foreach ($TestFile in $UnitTests) {
    $TestName = [System.IO.Path]::GetFileNameWithoutExtension($TestFile.Name)
    $CiFile = Join-Path $ScriptDir "$TestName.ci"

    # 1. Compile unit test
    $CompileOutput = & $Compiler -ini $IniFile -o $CiFile $TestFile.FullName 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  [FAIL] Compilation error in $TestName" -ForegroundColor Red
        Write-Host $CompileOutput
        $TotalFailed++
        continue
    }

    # 2. Execute compiled test in VM
    $VmOutput = & $Vm $CiFile 2>&1
    $VmExit = $LASTEXITCODE

    if ($VmExit -eq 0 -and ($VmOutput -match "\[PASS\]")) {
        Write-Host "  [PASS] $TestName (exit = $VmExit)" -ForegroundColor Green
        $TotalPassed++
    } else {
        Write-Host "  [FAIL] $TestName (exit = $VmExit)" -ForegroundColor Red
        Write-Host $VmOutput
        $TotalFailed++
    }
}

# ------------------------------------------------------------------------------
# Phase 2: Compilation of onm.ci and onm_native.ci
# ------------------------------------------------------------------------------
Write-Host "`n--- Phase 2: Building Application CI Artifacts ---" -ForegroundColor Yellow

$OnmDir = (Resolve-Path (Join-Path $ScriptDir "..")).Path
$OnmSource = Join-Path $OnmDir "onm.ijk"
$OnmCi = Join-Path $OnmDir "onm.ci"
$OnmNativeCi = Join-Path $OnmDir "onm_native.ci"

# Compile with all instructions emitted
$CompOut1 = & $Compiler -ini $IniFile -o $OnmCi $OnmSource 2>&1
if ($LASTEXITCODE -eq 0) {
    $CiSize = (Get-Item $OnmCi).Length
    Write-Host "  [PASS] onm.ci compiled successfully ($CiSize bytes)" -ForegroundColor Green
    $TotalPassed++
} else {
    Write-Host "  [FAIL] Failed to compile onm.ci" -ForegroundColor Red
    Write-Host $CompOut1
    $TotalFailed++
}

# Compile with --native-builtins (VM native builtin routines)
$CompOut2 = & $Compiler -ini $IniFile --native-builtins -o $OnmNativeCi $OnmSource 2>&1
if ($LASTEXITCODE -eq 0) {
    $NativeSize = (Get-Item $OnmNativeCi).Length
    Write-Host "  [PASS] onm_native.ci compiled successfully ($NativeSize bytes)" -ForegroundColor Green
    $TotalPassed++
} else {
    Write-Host "  [FAIL] Failed to compile onm_native.ci" -ForegroundColor Red
    Write-Host $CompOut2
    $TotalFailed++
}

# ------------------------------------------------------------------------------
# Phase 3: Parity Verification between onm.ci and onm_native.ci
# ------------------------------------------------------------------------------
Write-Host "`n--- Phase 3: Runtime Parity Verification ---" -ForegroundColor Yellow

$ScenarioFile = Join-Path $ScriptDir "scenario_full.txt"
if (-not (Test-Path $ScenarioFile)) {
    Write-Host "  [FAIL] Scenario script not found: $ScenarioFile" -ForegroundColor Red
    $TotalFailed++
} else {
    $Guid1 = [System.Guid]::NewGuid().ToString()
    $Guid2 = [System.Guid]::NewGuid().ToString()
    $TempDir1 = Join-Path ([System.IO.Path]::GetTempPath()) "onm_test_$Guid1"
    $TempDir2 = Join-Path ([System.IO.Path]::GetTempPath()) "onm_test_$Guid2"

    New-Item -ItemType Directory -Path $TempDir1 -Force | Out-Null
    New-Item -ItemType Directory -Path $TempDir2 -Force | Out-Null

    try {
        Copy-Item $OnmCi (Join-Path $TempDir1 "onm.ci")
        Copy-Item $OnmNativeCi (Join-Path $TempDir2 "onm_native.ci")

        $Out1Path = Join-Path $TempDir1 "out.txt"
        $Err1Path = Join-Path $TempDir1 "err.txt"
        $Out2Path = Join-Path $TempDir2 "out.txt"
        $Err2Path = Join-Path $TempDir2 "err.txt"

        $Proc1 = Start-Process -FilePath $Vm -ArgumentList "onm.ci" -WorkingDirectory $TempDir1 `
            -RedirectStandardInput $ScenarioFile -RedirectStandardOutput $Out1Path `
            -RedirectStandardError $Err1Path -PassThru -Wait

        $Proc2 = Start-Process -FilePath $Vm -ArgumentList "onm_native.ci" -WorkingDirectory $TempDir2 `
            -RedirectStandardInput $ScenarioFile -RedirectStandardOutput $Out2Path `
            -RedirectStandardError $Err2Path -PassThru -Wait

        $Out1 = if (Test-Path $Out1Path) { [System.IO.File]::ReadAllText($Out1Path) } else { "" }
        $Out2 = if (Test-Path $Out2Path) { [System.IO.File]::ReadAllText($Out2Path) } else { "" }

        $Data1Path = Join-Path $TempDir1 "onm.data"
        $Data2Path = Join-Path $TempDir2 "onm.data"
        $Data1 = if (Test-Path $Data1Path) { [System.IO.File]::ReadAllText($Data1Path) } else { "" }
        $Data2 = if (Test-Path $Data2Path) { [System.IO.File]::ReadAllText($Data2Path) } else { "" }

        # Check process exits
        if ($Proc1.ExitCode -eq 0 -and $Proc2.ExitCode -eq 0) {
            Write-Host "  [PASS] Both onm.ci and onm_native.ci exited with code 0" -ForegroundColor Green
            $TotalPassed++
        } else {
            Write-Host "  [FAIL] Non-zero exit code: onm.ci=$($Proc1.ExitCode), onm_native.ci=$($Proc2.ExitCode)" -ForegroundColor Red
            $TotalFailed++
        }

        # Check stdout parity
        if ($Out1 -eq $Out2 -and $Out1.Length -gt 0) {
            Write-Host "  [PASS] Console stdout is 100% IDENTICAL between onm.ci and onm_native.ci" -ForegroundColor Green
            $TotalPassed++
        } else {
            Write-Host "  [FAIL] Console stdout differs between onm.ci and onm_native.ci" -ForegroundColor Red
            Write-Host "--- onm.ci output length: $($Out1.Length) | onm_native.ci output length: $($Out2.Length) ---"
            $TotalFailed++
        }

        # Check persistence file onm.data parity
        if ($Data1 -eq $Data2 -and $Data1.Length -gt 0) {
            Write-Host "  [PASS] Resulting onm.data is 100% IDENTICAL between onm.ci and onm_native.ci" -ForegroundColor Green
            $TotalPassed++
        } else {
            Write-Host "  [FAIL] Resulting onm.data differs between onm.ci and onm_native.ci" -ForegroundColor Red
            $TotalFailed++
        }

    } finally {
        Remove-Item -Recurse -Force $TempDir1 -ErrorAction SilentlyContinue
        Remove-Item -Recurse -Force $TempDir2 -ErrorAction SilentlyContinue
    }
}

# ------------------------------------------------------------------------------
# Summary
# ------------------------------------------------------------------------------
Write-Host "`n======================================================================" -ForegroundColor Cyan
Write-Host " Results: $TotalPassed Passed, $TotalFailed Failed" -ForegroundColor $(if ($TotalFailed -eq 0) { "Green" } else { "Red" })
Write-Host "======================================================================" -ForegroundColor Cyan

if ($TotalFailed -gt 0) {
    exit 1
}
exit 0
