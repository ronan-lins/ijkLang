# ==============================================================================
# ijkLang (ijkL) - Integration Test Suite for Multi-Unit Modular System
# Validates Requirements 1 to 16
# ==============================================================================

param(
    [string]$Configuration = "Debug"
)

Remove-Item env:https_proxy -ErrorAction SilentlyContinue
Remove-Item env:http_proxy -ErrorAction SilentlyContinue
[System.Environment]::SetEnvironmentVariable("https_proxy", $null, "Process")
[System.Environment]::SetEnvironmentVariable("HTTPS_PROXY", $null, "Process")
[System.Environment]::SetEnvironmentVariable("http_proxy",  $null, "Process")
[System.Environment]::SetEnvironmentVariable("HTTP_PROXY",  $null, "Process")

$ErrorActionPreference = "Stop"

$ProjectRoot = (Get-Item $PSScriptRoot).Parent.FullName
$CompilerBinary = Join-Path $ProjectRoot "bin\$Configuration\ijkl.exe"
$VmBinary = Join-Path $ProjectRoot "bin\$Configuration\ijkl-m.exe"
$IniFile = Join-Path $ProjectRoot "bin\$Configuration\ijkl.ini"

if (-not (Test-Path $CompilerBinary)) {
    Write-Error "Compiler binary not found at $CompilerBinary. Please build the project first."
}
if (-not (Test-Path $VmBinary)) {
    Write-Error "VM binary not found at $VmBinary. Please build the project first."
}

Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host "ijkLang (ijkL) - Multi-Unit Modular Architecture Integration Tests" -ForegroundColor Cyan
Write-Host "Compiler: $CompilerBinary" -ForegroundColor Gray
Write-Host "VM:       $VmBinary" -ForegroundColor Gray
Write-Host "INI:      $IniFile" -ForegroundColor Gray
Write-Host "======================================================================" -ForegroundColor Cyan

$PassedCount = 0
$FailedCount = 0
$TotalCount = 0

function Assert-Test {
    param(
        [string]$TestName,
        [bool]$Success,
        [string]$Detail = ""
    )

    $global:TotalCount++
    if ($Success) {
        $global:PassedCount++
        Write-Host "  [PASS] $TestName" -ForegroundColor Green
    } else {
        $global:FailedCount++
        Write-Host "  [FAIL] $TestName" -ForegroundColor Red
        if ($Detail) {
            Write-Host "         $Detail" -ForegroundColor DarkRed
        }
    }
}

$BuildDir = Join-Path $ProjectRoot "build\integration"
if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
}

# ------------------------------------------------------------------------------
# Test 1: Variant 1 (A.B.C.ijk)
# ------------------------------------------------------------------------------
$T1Source = Join-Path $ProjectRoot "tests\integration\test_variant1\Principal.ijk"
$T1Ci = Join-Path $BuildDir "test_variant1.ci"
$p = Start-Process -FilePath $CompilerBinary -ArgumentList "-ini `"$IniFile`" -o `"$T1Ci`" `"$T1Source`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t1_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t1_out.txt")
$t1_compile_ok = ($p.ExitCode -eq 0 -and (Test-Path $T1Ci))

if ($t1_compile_ok) {
    $pvm = Start-Process -FilePath $VmBinary -ArgumentList "`"$T1Ci`"" -NoNewWindow -PassThru -Wait
    $t1_ok = ($pvm.ExitCode -eq 20)
    Assert-Test "Variant 1 (A.B.C.ijk) - Compilation and VM Execution (return = 20)" $t1_ok "Expected exit code 20, got $($pvm.ExitCode)"
} else {
    $err = Get-Content (Join-Path $BuildDir "t1_err.txt") -Raw -ErrorAction SilentlyContinue
    Assert-Test "Variant 1 (A.B.C.ijk) - Compilation" $false "Compilation failed: $err"
}

# ------------------------------------------------------------------------------
# Test 2: Variant 2 (A.B/C.ijk)
# ------------------------------------------------------------------------------
$T2Source = Join-Path $ProjectRoot "tests\integration\test_variant2\Principal.ijk"
$T2Ci = Join-Path $BuildDir "test_variant2.ci"
$p = Start-Process -FilePath $CompilerBinary -ArgumentList "-ini `"$IniFile`" -o `"$T2Ci`" `"$T2Source`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t2_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t2_out.txt")
$t2_compile_ok = ($p.ExitCode -eq 0 -and (Test-Path $T2Ci))

if ($t2_compile_ok) {
    $pvm = Start-Process -FilePath $VmBinary -ArgumentList "`"$T2Ci`"" -NoNewWindow -PassThru -Wait
    $t2_ok = ($pvm.ExitCode -eq 18)
    Assert-Test "Variant 2 (A.B/C.ijk) - Compilation and VM Execution (return = 18)" $t2_ok "Expected exit code 18, got $($pvm.ExitCode)"
} else {
    $err = Get-Content (Join-Path $BuildDir "t2_err.txt") -Raw -ErrorAction SilentlyContinue
    Assert-Test "Variant 2 (A.B/C.ijk) - Compilation" $false "Compilation failed: $err"
}

# ------------------------------------------------------------------------------
# Test 3: Variant 3 (A/B/C.ijk)
# ------------------------------------------------------------------------------
$T3Source = Join-Path $ProjectRoot "tests\integration\test_variant3\Principal.ijk"
$T3Ci = Join-Path $BuildDir "test_variant3.ci"
$p = Start-Process -FilePath $CompilerBinary -ArgumentList "-ini `"$IniFile`" -o `"$T3Ci`" `"$T3Source`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t3_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t3_out.txt")
$t3_compile_ok = ($p.ExitCode -eq 0 -and (Test-Path $T3Ci))

if ($t3_compile_ok) {
    $pvm = Start-Process -FilePath $VmBinary -ArgumentList "`"$T3Ci`"" -NoNewWindow -PassThru -Wait
    $t3_ok = ($pvm.ExitCode -eq 24)
    Assert-Test "Variant 3 (A/B/C.ijk) - Compilation and VM Execution (return = 24)" $t3_ok "Expected exit code 24, got $($pvm.ExitCode)"
} else {
    $err = Get-Content (Join-Path $BuildDir "t3_err.txt") -Raw -ErrorAction SilentlyContinue
    Assert-Test "Variant 3 (A/B/C.ijk) - Compilation" $false "Compilation failed: $err"
}

# ------------------------------------------------------------------------------
# Test 4: Ambiguity Error (duplicate files in different variant paths)
# ------------------------------------------------------------------------------
$T4Source = Join-Path $ProjectRoot "tests\integration\test_ambiguity\Principal.ijk"
$T4Ci = Join-Path $BuildDir "test_ambiguity.ci"
$p = Start-Process -FilePath $CompilerBinary -ArgumentList "-ini `"$IniFile`" -o `"$T4Ci`" `"$T4Source`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t4_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t4_out.txt")
$t4_err = Get-Content (Join-Path $BuildDir "t4_err.txt") -Raw -ErrorAction SilentlyContinue
$t4_ok = ($p.ExitCode -ne 0 -and ($t4_err -match "Ambiguous module path" -or $t4_err -match "Ambiguous"))
Assert-Test "Ambiguity Detection on Multiple Path Matches (Req 8)" $t4_ok "Compiler should reject ambiguous duplicate files. ExitCode=$($p.ExitCode)"

# ------------------------------------------------------------------------------
# Test 5: Rejection of keywords not in active language (clean syntax error, no language inference)
# ------------------------------------------------------------------------------
$T5Source = Join-Path $ProjectRoot "tests\integration\test_foreign_lang\Principal.ijk"
$T5Ci = Join-Path $BuildDir "test_foreign_lang.ci"
$p = Start-Process -FilePath $CompilerBinary -ArgumentList "-ini `"$IniFile`" -o `"$T5Ci`" `"$T5Source`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t5_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t5_out.txt")
$t5_err = Get-Content (Join-Path $BuildDir "t5_err.txt") -Raw -ErrorAction SilentlyContinue
$t5_out = Get-Content (Join-Path $BuildDir "t5_out.txt") -Raw -ErrorAction SilentlyContinue
$t5_combined = "$t5_err $t5_out"
$t5_ok = ($p.ExitCode -ne 0 -and ($t5_combined -match "Syntax error" -or $t5_combined -match "Error"))
Assert-Test "Strict Syntax Error on Unrecognized Keyword (No Language Inference)" $t5_ok "Compiler should reject unrecognized 'if' in pt-BR mode with syntax error. ExitCode=$($p.ExitCode)"

# ------------------------------------------------------------------------------
# Test 6: Circular dependency between modules (A imports B, B imports A)
# ------------------------------------------------------------------------------
$T6Source = Join-Path $ProjectRoot "tests\integration\test_circular\Principal.ijk"
$T6Ci = Join-Path $BuildDir "test_circular.ci"
$p = Start-Process -FilePath $CompilerBinary -ArgumentList "-ini `"$IniFile`" -o `"$T6Ci`" `"$T6Source`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t6_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t6_out.txt")
$t6_compile_ok = ($p.ExitCode -eq 0 -and (Test-Path $T6Ci))

if ($t6_compile_ok) {
    $pvm = Start-Process -FilePath $VmBinary -ArgumentList "`"$T6Ci`"" -NoNewWindow -PassThru -Wait
    $t6_ok = ($pvm.ExitCode -eq 42)
    Assert-Test "Circular Dependency without Infinite Loops (Req 9) (return = 42)" $t6_ok "Expected exit code 42, got $($pvm.ExitCode)"
} else {
    $err = Get-Content (Join-Path $BuildDir "t6_err.txt") -Raw -ErrorAction SilentlyContinue
    Assert-Test "Circular Dependency without Infinite Loops (Req 9)" $false "Compilation failed: $err"
}

# ------------------------------------------------------------------------------
# Test 7: Symbol collision on short-name resolution (Req 10)
# ------------------------------------------------------------------------------
$T7Source = Join-Path $ProjectRoot "tests\integration\test_collision\Principal.ijk"
$T7Ci = Join-Path $BuildDir "test_collision.ci"
$p = Start-Process -FilePath $CompilerBinary -ArgumentList "-ini `"$IniFile`" -o `"$T7Ci`" `"$T7Source`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t7_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t7_out.txt")
$t7_err = Get-Content (Join-Path $BuildDir "t7_err.txt") -Raw -ErrorAction SilentlyContinue
$t7_ok = ($p.ExitCode -ne 0 -and ($t7_err -match "Ambiguous identifier" -or $t7_err -match "multiple imported modules"))
Assert-Test "Short-Name Symbol Collision Error (Req 10)" $t7_ok "Compiler should report ambiguous collision between Modulo1 and Modulo2. ExitCode=$($p.ExitCode)"

# ------------------------------------------------------------------------------
# Test 8: Full CLI Arguments and Exit Code via r0 (Req 11, 14)
# ------------------------------------------------------------------------------
$T8Source = Join-Path $ProjectRoot "tests\integration\test_cli_args\Principal.ijk"
$T8Ci = Join-Path $BuildDir "test_cli_args.ci"
$p = Start-Process -FilePath $CompilerBinary -ArgumentList "-ini `"$IniFile`" -o `"$T8Ci`" `"$T8Source`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t8_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t8_out.txt")
$t8_compile_ok = ($p.ExitCode -eq 0 -and (Test-Path $T8Ci))

if ($t8_compile_ok) {
    # Executa a VM passando 2 argumentos na linha de comando
    $pvm = Start-Process -FilePath $VmBinary -ArgumentList "`"$T8Ci`" primeiroSegundo argumentoDois" -NoNewWindow -PassThru -Wait
    $t8_ok = ($pvm.ExitCode -eq 42)
    Assert-Test "VM Bootstrap with CLI Arguments and Return via r0 (Req 14) (exit_code = 42)" $t8_ok "Expected exit code 42, got $($pvm.ExitCode)"
} else {
    $err = Get-Content (Join-Path $BuildDir "t8_err.txt") -Raw -ErrorAction SilentlyContinue
    Assert-Test "VM Bootstrap with CLI Arguments and Return via r0 (Req 14)" $false "Compilation failed: $err"
}

# ------------------------------------------------------------------------------
# Test 9: Modular Visibility - Public Interface (Rules 0, 1, 2 Valid)
# ------------------------------------------------------------------------------
$T9Source = Join-Path $ProjectRoot "tests\integration\test_visibility\PrincipalValido.ijk"
$T9Ci = Join-Path $BuildDir "test_visibility_valido.ci"
$p = Start-Process -FilePath $CompilerBinary -ArgumentList "-ini `"$IniFile`" -o `"$T9Ci`" `"$T9Source`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t9_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t9_out.txt")
$t9_compile_ok = ($p.ExitCode -eq 0 -and (Test-Path $T9Ci))

if ($t9_compile_ok) {
    $pvm = Start-Process -FilePath $VmBinary -ArgumentList "`"$T9Ci`"" -NoNewWindow -PassThru -Wait
    $t9_ok = ($pvm.ExitCode -eq 37)
    Assert-Test "Modular Visibility: Public Constants, Types & Public Routines (exit_code = 37)" $t9_ok "Expected exit code 37, got $($pvm.ExitCode)"
} else {
    $err = Get-Content (Join-Path $BuildDir "t9_err.txt") -Raw -ErrorAction SilentlyContinue
    Assert-Test "Modular Visibility: Public Constants, Types & Public Routines" $false "Compilation failed: $err"
}

# ------------------------------------------------------------------------------
# Test 10: Modular Visibility - Accessing Private Routine Fails
# ------------------------------------------------------------------------------
$T10Source = Join-Path $ProjectRoot "tests\integration\test_visibility\PrincipalRotinaPrivada.ijk"
$T10Ci = Join-Path $BuildDir "test_visibility_priv_routine.ci"
$p = Start-Process -FilePath $CompilerBinary -ArgumentList "-ini `"$IniFile`" -o `"$T10Ci`" `"$T10Source`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t10_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t10_out.txt")
$t10_err = Get-Content (Join-Path $BuildDir "t10_err.txt") -Raw -ErrorAction SilentlyContinue
$t10_ok = ($p.ExitCode -ne 0 -and ($t10_err -match "is private and cannot be accessed"))
Assert-Test "Modular Visibility: Accessing Private Routine Fails with Semantic Error" $t10_ok "Compiler should reject accessing private routine from another unit. ExitCode=$($p.ExitCode), Err=$t10_err"

# ------------------------------------------------------------------------------
# Test 11: Modular Visibility - Accessing Private Module Variable Fails
# ------------------------------------------------------------------------------
$T11Source = Join-Path $ProjectRoot "tests\integration\test_visibility\PrincipalVariavelPrivada.ijk"
$T11Ci = Join-Path $BuildDir "test_visibility_priv_var.ci"
$p = Start-Process -FilePath $CompilerBinary -ArgumentList "-ini `"$IniFile`" -o `"$T11Ci`" `"$T11Source`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t11_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t11_out.txt")
$t11_err = Get-Content (Join-Path $BuildDir "t11_err.txt") -Raw -ErrorAction SilentlyContinue
$t11_ok = ($p.ExitCode -ne 0 -and ($t11_err -match "is private to unit"))
Assert-Test "Modular Visibility: Accessing Private Module Variable Fails with Semantic Error" $t11_ok "Compiler should reject accessing private variable from another unit. ExitCode=$($p.ExitCode), Err=$t11_err"

# ------------------------------------------------------------------------------
# Test 12: Modular Visibility - Unit Routine Without Explicit publico/privado Fails
# ------------------------------------------------------------------------------
$T12Source = Join-Path $ProjectRoot "tests\integration\test_visibility\UnidadeSemModificador.ijk"
$T12Ci = Join-Path $BuildDir "test_visibility_missing_modifier.ci"
$p = Start-Process -FilePath $CompilerBinary -ArgumentList "-ini `"$IniFile`" -o `"$T12Ci`" `"$T12Source`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t12_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t12_out.txt")
$t12_err = Get-Content (Join-Path $BuildDir "t12_err.txt") -Raw -ErrorAction SilentlyContinue
$t12_ok = ($p.ExitCode -ne 0 -and ($t12_err -match "Expected 'publico' or 'privado'"))
Assert-Test "Modular Visibility: Unit Routine Without Visibility Modifier Fails at Parser" $t12_ok "Compiler should require publico or privado in units. ExitCode=$($p.ExitCode), Err=$t12_err"

# ------------------------------------------------------------------------------
# Test 13: Modular Visibility - Program Routine Without Explicit publico/privado Fails
# ------------------------------------------------------------------------------
$T13Source = Join-Path $ProjectRoot "tests\integration\test_visibility\ProgramaSemModificador.ijk"
$T13Ci = Join-Path $BuildDir "test_visibility_prog_missing_modifier.ci"
$p = Start-Process -FilePath $CompilerBinary -ArgumentList "-ini `"$IniFile`" -o `"$T13Ci`" `"$T13Source`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t13_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t13_out.txt")
$t13_err = Get-Content (Join-Path $BuildDir "t13_err.txt") -Raw -ErrorAction SilentlyContinue
$t13_ok = ($p.ExitCode -ne 0 -and ($t13_err -match "Expected 'publico' or 'privado'"))
Assert-Test "Modular Visibility: Program Routine Without Visibility Modifier Fails at Parser" $t13_ok "Compiler should require publico or privado in programs. ExitCode=$($p.ExitCode), Err=$t13_err"

# ------------------------------------------------------------------------------
# Test 14: Entry Function 'principal' Declared as 'publico' Fails with Semantic Error
# ------------------------------------------------------------------------------
$T14Source = Join-Path $ProjectRoot "tests\integration\test_visibility\PrincipalPublico.ijk"
$T14Ci = Join-Path $BuildDir "test_visibility_principal_publico.ci"
$p = Start-Process -FilePath $CompilerBinary -ArgumentList "-ini `"$IniFile`" -o `"$T14Ci`" `"$T14Source`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t14_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t14_out.txt")
$t14_err = Get-Content (Join-Path $BuildDir "t14_err.txt") -Raw -ErrorAction SilentlyContinue
$t14_ok = ($p.ExitCode -ne 0 -and ($t14_err -match "must be declared as 'privado'"))
Assert-Test "Modular Visibility: Entry Function Declared as 'publico' Fails with Semantic Error" $t14_ok "Compiler must reject publico entry point. ExitCode=$($p.ExitCode), Err=$t14_err"

# ------------------------------------------------------------------------------
# Test 15: Importing Program Unit Fails with Error
# ------------------------------------------------------------------------------
$T15Source = Join-Path $ProjectRoot "tests\integration\test_visibility\ImportarPrograma.ijk"
$T15Ci = Join-Path $BuildDir "test_visibility_import_program.ci"
$p = Start-Process -FilePath $CompilerBinary -ArgumentList "-ini `"$IniFile`" -o `"$T15Ci`" `"$T15Source`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t15_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t15_out.txt")
$t15_err = Get-Content (Join-Path $BuildDir "t15_err.txt") -Raw -ErrorAction SilentlyContinue
$t15_ok = ($p.ExitCode -ne 0 -and ($t15_err -match "Cannot import program unit"))
Assert-Test "Modular Architecture: Importing Program Unit Fails with Error" $t15_ok "Compiler must reject importing program units. ExitCode=$($p.ExitCode), Err=$t15_err"

# ------------------------------------------------------------------------------
# Test 16: Declaring Entry Function in Module Unit Fails with Semantic Error
# ------------------------------------------------------------------------------
$T16Source = Join-Path $ProjectRoot "tests\integration\test_visibility\UnidadeComPrincipal.ijk"
$T16Ci = Join-Path $BuildDir "test_visibility_unit_with_main.ci"
$p = Start-Process -FilePath $CompilerBinary -ArgumentList "-ini `"$IniFile`" -o `"$T16Ci`" `"$T16Source`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t16_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t16_out.txt")
$t16_err = Get-Content (Join-Path $BuildDir "t16_err.txt") -Raw -ErrorAction SilentlyContinue
$t16_ok = ($p.ExitCode -ne 0 -and ($t16_err -match "cannot be declared inside a module unit"))
Assert-Test "Modular Architecture: Entry Function in Module Unit Fails with Semantic Error" $t16_ok "Compiler must reject entry point in module unit. ExitCode=$($p.ExitCode), Err=$t16_err"

# ------------------------------------------------------------------------------
# Test 17: Declaring Public Routine in Program Unit Fails with Semantic Error
# ------------------------------------------------------------------------------
$T17Source = Join-Path $ProjectRoot "tests\integration\test_visibility\ProgramaRotinaPublica.ijk"
$T17Ci = Join-Path $BuildDir "test_visibility_prog_public_routine.ci"
$p = Start-Process -FilePath $CompilerBinary -ArgumentList "-ini `"$IniFile`" -o `"$T17Ci`" `"$T17Source`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t17_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t17_out.txt")
$t17_err = Get-Content (Join-Path $BuildDir "t17_err.txt") -Raw -ErrorAction SilentlyContinue
$t17_ok = ($p.ExitCode -ne 0 -and ($t17_err -match "cannot be declared as 'publico'"))
Assert-Test "Modular Visibility: Routine in Program Unit Declared as 'publico' Fails with Semantic Error" $t17_ok "Compiler must reject public routines in program units. ExitCode=$($p.ExitCode), Err=$t17_err"

# ------------------------------------------------------------------------------
# Test 18: Loose Statements Directly in Namespace Body Fail with Syntax Error
# ------------------------------------------------------------------------------
$T18Source = Join-Path $ProjectRoot "tests\integration\test_lifecycle\CorpoComComando.ijk"
$T18Ci = Join-Path $BuildDir "test_lifecycle_corpo_comando.ci"
$p = Start-Process -FilePath $CompilerBinary -ArgumentList "-ini `"$IniFile`" -o `"$T18Ci`" `"$T18Source`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t18_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t18_out.txt")
$t18_err = Get-Content (Join-Path $BuildDir "t18_err.txt") -Raw -ErrorAction SilentlyContinue
$t18_ok = ($p.ExitCode -ne 0 -and ($t18_err -match "Executable statements are not allowed directly in namespace body"))
Assert-Test "Pure Declarations: Loose Executable Statements in Namespace Body Fail at Parser" $t18_ok "Compiler must reject loose statements in namespace body. ExitCode=$($p.ExitCode), Err=$t18_err"

# ------------------------------------------------------------------------------
# Test 19: Lifecycle Routine Declared as Public Fails with Semantic Error
# ------------------------------------------------------------------------------
$T19Source = Join-Path $ProjectRoot "tests\integration\test_lifecycle\CicloVidaPublico.ijk"
$T19Ci = Join-Path $BuildDir "test_lifecycle_publico.ci"
$p = Start-Process -FilePath $CompilerBinary -ArgumentList "-ini `"$IniFile`" -o `"$T19Ci`" `"$T19Source`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t19_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t19_out.txt")
$t19_err = Get-Content (Join-Path $BuildDir "t19_err.txt") -Raw -ErrorAction SilentlyContinue
$t19_ok = ($p.ExitCode -ne 0 -and ($t19_err -match "must be declared as 'privado'"))
Assert-Test "Lifecycle Routines: Lifecycle Procedure Declared as 'publico' Fails with Semantic Error" $t19_ok "Compiler must reject public lifecycle routine. ExitCode=$($p.ExitCode), Err=$t19_err"

# ------------------------------------------------------------------------------
# Test 20: Lifecycle Routine with Return Value Fails with Semantic Error
# ------------------------------------------------------------------------------
$T20Source = Join-Path $ProjectRoot "tests\integration\test_lifecycle\CicloVidaComRetorno.ijk"
$T20Ci = Join-Path $BuildDir "test_lifecycle_retorno.ci"
$p = Start-Process -FilePath $CompilerBinary -ArgumentList "-ini `"$IniFile`" -o `"$T20Ci`" `"$T20Source`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t20_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t20_out.txt")
$t20_err = Get-Content (Join-Path $BuildDir "t20_err.txt") -Raw -ErrorAction SilentlyContinue
$t20_ok = ($p.ExitCode -ne 0 -and ($t20_err -match "must be a procedure"))
Assert-Test "Lifecycle Routines: Lifecycle Function with Return Value Fails with Semantic Error" $t20_ok "Compiler must reject lifecycle routine with return value. ExitCode=$($p.ExitCode), Err=$t20_err"

# ------------------------------------------------------------------------------
# Test 21: Lifecycle Routine with Parameters Fails with Semantic Error
# ------------------------------------------------------------------------------
$T21Source = Join-Path $ProjectRoot "tests\integration\test_lifecycle\CicloVidaComParametros.ijk"
$T21Ci = Join-Path $BuildDir "test_lifecycle_params.ci"
$p = Start-Process -FilePath $CompilerBinary -ArgumentList "-ini `"$IniFile`" -o `"$T21Ci`" `"$T21Source`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t21_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t21_out.txt")
$t21_err = Get-Content (Join-Path $BuildDir "t21_err.txt") -Raw -ErrorAction SilentlyContinue
$t21_ok = ($p.ExitCode -ne 0 -and ($t21_err -match "cannot take any parameters"))
Assert-Test "Lifecycle Routines: Lifecycle Routine with Parameters Fails with Semantic Error" $t21_ok "Compiler must reject lifecycle routine with parameters. ExitCode=$($p.ExitCode), Err=$t21_err"

# ------------------------------------------------------------------------------
# Test 22: Module and Program Lifecycle Routines Execution in VM
# ------------------------------------------------------------------------------
$T22Source = Join-Path $ProjectRoot "tests\integration\test_lifecycle\ProgramaCiclo.ijk"
$T22Ci = Join-Path $BuildDir "test_lifecycle_exec.ci"
$p = Start-Process -FilePath $CompilerBinary -ArgumentList "-ini `"$IniFile`" -o `"$T22Ci`" `"$T22Source`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t22_c_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t22_c_out.txt")
$t22_c_ok = ($p.ExitCode -eq 0 -and (Test-Path $T22Ci))
if ($t22_c_ok) {
    $pvm = Start-Process -FilePath $VmBinary -ArgumentList "`"$T22Ci`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t22_vm_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t22_vm_out.txt")
    $t22_ok = ($pvm.ExitCode -eq 45)
    Assert-Test "Lifecycle Routines: Initialization and Teardown Execution in VM (exit_code = 45)" $t22_ok "Expected exit code 45 (15 + 30). Got $($pvm.ExitCode)"
} else {
    Assert-Test "Lifecycle Routines: Initialization and Teardown Execution in VM (exit_code = 45)" $false "Compilation failed"
}

# ------------------------------------------------------------------------------
# Test 23: Default Output File (.ci) Inferred When -o is Omitted
# ------------------------------------------------------------------------------
$T23Source = Join-Path $BuildDir "test_default_output.ijk"
$T23ExpectedCi = Join-Path $BuildDir "test_default_output.ci"
[System.IO.File]::WriteAllText($T23Source, "programa TesteDefaultOutput { privado principal(): inteiro { retorne 50; } }")
if (Test-Path $T23ExpectedCi) {
    Remove-Item $T23ExpectedCi -Force -ErrorAction SilentlyContinue
}
$p = Start-Process -FilePath $CompilerBinary -ArgumentList "-ini `"$IniFile`" `"$T23Source`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t23_c_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t23_c_out.txt")
$t23_compiled = ($p.ExitCode -eq 0 -and (Test-Path $T23ExpectedCi))

if ($t23_compiled) {
    $pvm = Start-Process -FilePath $VmBinary -ArgumentList "`"$T23ExpectedCi`"" -NoNewWindow -PassThru -Wait
    $t23_ok = ($pvm.ExitCode -eq 50)
    Assert-Test "Default Output: Inferred <source>.ci When -o is Omitted (exit_code = 50)" $t23_ok "Expected exit code 50. Got $($pvm.ExitCode)"
    Remove-Item $T23ExpectedCi -Force -ErrorAction SilentlyContinue
    Remove-Item $T23Source -Force -ErrorAction SilentlyContinue
} else {
    $err = Get-Content (Join-Path $BuildDir "t23_c_err.txt") -Raw -ErrorAction SilentlyContinue
    Assert-Test "Default Output: Inferred <source>.ci When -o is Omitted" $false "Failed to generate $T23ExpectedCi. Err=$err"
}

# ------------------------------------------------------------------------------
# Test 24: Extension Inference (.ijk in compiler, .ci in VM)
# ------------------------------------------------------------------------------
$T24Base = Join-Path $BuildDir "test_ext_inference"
$T24Source = "$T24Base.ijk"
$T24ExpectedCi = "$T24Base.ci"
[System.IO.File]::WriteAllText($T24Source, "programa TesteExtInference { privado principal(): inteiro { retorne 77; } }")
if (Test-Path $T24ExpectedCi) {
    Remove-Item $T24ExpectedCi -Force -ErrorAction SilentlyContinue
}
# Compila passando apenas o caminho base, sem a extensao .ijk
$p = Start-Process -FilePath $CompilerBinary -ArgumentList "-ini `"$IniFile`" `"$T24Base`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t24_c_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t24_c_out.txt")
$t24_compiled = ($p.ExitCode -eq 0 -and (Test-Path $T24ExpectedCi))

if ($t24_compiled) {
    # Executa a VM passando apenas o caminho base, sem a extensao .ci
    $pvm = Start-Process -FilePath $VmBinary -ArgumentList "`"$T24Base`"" -NoNewWindow -PassThru -Wait
    $t24_ok = ($pvm.ExitCode -eq 77)
    Assert-Test "Extension Inference: Compiler infers .ijk and VM infers .ci (exit_code = 77)" $t24_ok "Expected exit code 77. Got $($pvm.ExitCode)"
    Remove-Item $T24ExpectedCi -Force -ErrorAction SilentlyContinue
    Remove-Item $T24Source -Force -ErrorAction SilentlyContinue
} else {
    $err = Get-Content (Join-Path $BuildDir "t24_c_err.txt") -Raw -ErrorAction SilentlyContinue
    Assert-Test "Extension Inference: Compiler infers .ijk and VM infers .ci" $false "Failed to compile $T24Base without extension. Err=$err"
}

# ------------------------------------------------------------------------------
# Test 25: Routine Overloading & Name Mangling (VM execution exit_code = 42)
# ------------------------------------------------------------------------------
$T25Source = Join-Path $ProjectRoot "tests\test_overloading.ijk"
$T25Ci = Join-Path $BuildDir "test_overloading.ci"
$p = Start-Process -FilePath $CompilerBinary -ArgumentList "-ini `"$IniFile`" -o `"$T25Ci`" `"$T25Source`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t25_c_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t25_c_out.txt")
$t25_compiled = ($p.ExitCode -eq 0 -and (Test-Path $T25Ci))

if ($t25_compiled) {
    $pvm = Start-Process -FilePath $VmBinary -ArgumentList "`"$T25Ci`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t25_vm_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t25_vm_out.txt")
    $t25_ok = ($pvm.ExitCode -eq 42)
    Assert-Test "Routine Overloading: Functions, Procedures, Widening & VM Execution (exit_code = 42)" $t25_ok "Expected exit code 42. Got $($pvm.ExitCode)"
} else {
    $err = Get-Content (Join-Path $BuildDir "t25_c_err.txt") -Raw -ErrorAction SilentlyContinue
    Assert-Test "Routine Overloading: Compilation" $false "Failed to compile $T25Source. Err=$err"
}

# ------------------------------------------------------------------------------
# Test 26: Overloading Semantic Error on Duplicate Parameter Types
# ------------------------------------------------------------------------------
$T26Source = Join-Path $BuildDir "test_err_dup.ijk"
[System.IO.File]::WriteAllText($T26Source, "programa TesteDup { privado somar(inteiro a): inteiro { retorne a; } privado somar(inteiro b): inteiro { retorne b; } privado principal(): inteiro { retorne 0; } }")
$p = Start-Process -FilePath $CompilerBinary -ArgumentList "-ini `"$IniFile`" `"$T26Source`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t26_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t26_out.txt")
$t26_err = Get-Content (Join-Path $BuildDir "t26_err.txt") -Raw -ErrorAction SilentlyContinue
$t26_ok = ($p.ExitCode -ne 0 -and $t26_err -match "Redeclaration of routine 'somar' with identical parameter types")
Assert-Test "Overloading: Duplicate Parameter Types Fails with Semantic Error" $t26_ok "Expected error on duplicate parameter types. ExitCode=$($p.ExitCode), Err=$t26_err"
Remove-Item $T26Source -Force -ErrorAction SilentlyContinue

# ------------------------------------------------------------------------------
# Test 27: Overloading Semantic Error on Ambiguous Call
# ------------------------------------------------------------------------------
$T27Source = Join-Path $BuildDir "test_err_ambig.ijk"
[System.IO.File]::WriteAllText($T27Source, "programa TesteAmbig { privado proc(real a, inteiro b): inteiro { retorne 1; } privado proc(inteiro a, real b): inteiro { retorne 2; } privado principal(): inteiro { retorne proc(1, 2); } }")
$p = Start-Process -FilePath $CompilerBinary -ArgumentList "-ini `"$IniFile`" `"$T27Source`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t27_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t27_out.txt")
$t27_err = Get-Content (Join-Path $BuildDir "t27_err.txt") -Raw -ErrorAction SilentlyContinue
$t27_ok = ($p.ExitCode -ne 0 -and $t27_err -match "Call to overloaded routine 'proc' is ambiguous")
Assert-Test "Overloading: Ambiguous Overload Call Fails with Semantic Error" $t27_ok "Expected ambiguous call error. ExitCode=$($p.ExitCode), Err=$t27_err"
Remove-Item $T27Source -Force -ErrorAction SilentlyContinue

# ------------------------------------------------------------------------------
# Test 28: Overloading Semantic Error on Collision with Variable
# ------------------------------------------------------------------------------
$T28Source = Join-Path $BuildDir "test_err_collision.ijk"
[System.IO.File]::WriteAllText($T28Source, "programa TesteColisao { inteiro item = 10; privado item(inteiro a): inteiro { retorne a; } privado principal(): inteiro { retorne 0; } }")
$p = Start-Process -FilePath $CompilerBinary -ArgumentList "-ini `"$IniFile`" `"$T28Source`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t28_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t28_out.txt")
$t28_err = Get-Content (Join-Path $BuildDir "t28_err.txt") -Raw -ErrorAction SilentlyContinue
$t28_ok = ($p.ExitCode -ne 0 -and $t28_err -match "Redeclaration of identifier 'item' in the same scope")
Assert-Test "Overloading: Collision Between Routine and Variable Fails with Semantic Error" $t28_ok "Expected collision error. ExitCode=$($p.ExitCode), Err=$t28_err"
Remove-Item $T28Source -Force -ErrorAction SilentlyContinue

# ------------------------------------------------------------------------------
# Test 29: Overloading Semantic Error on Overloading Entry Point
# ------------------------------------------------------------------------------
$T29Source = Join-Path $BuildDir "test_err_main.ijk"
[System.IO.File]::WriteAllText($T29Source, "programa TesteOverloadMain { privado principal(inteiro a): inteiro { retorne a; } privado principal(): inteiro { retorne 0; } }")
$p = Start-Process -FilePath $CompilerBinary -ArgumentList "-ini `"$IniFile`" `"$T29Source`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t29_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t29_out.txt")
$t29_err = Get-Content (Join-Path $BuildDir "t29_err.txt") -Raw -ErrorAction SilentlyContinue
$t29_ok = ($p.ExitCode -ne 0 -and $t29_err -match "Routine 'principal' cannot be overloaded")
Assert-Test "Overloading: Overloading Entry Point 'principal' Fails with Semantic Error" $t29_ok "Expected entry point overload error. ExitCode=$($p.ExitCode), Err=$t29_err"
Remove-Item $T29Source -Force -ErrorAction SilentlyContinue

# ------------------------------------------------------------------------------
# Test 30: Portuguese Framework ijkL.Conversao (Module & Member Aliases, VM exit_code = 52)
# ------------------------------------------------------------------------------
$T30Source = Join-Path $BuildDir "test_conversao_pt.ijk"
$T30Ci = Join-Path $BuildDir "test_conversao_pt.ci"
[System.IO.File]::WriteAllText($T30Source, @"
importe ijkL.Conversao;

programa TesteConversaoPT {
    privado principal(): inteiro {
        inteiro a = paraInteiro("42");
        inteiro b = ijkL.Conversao.paraInteiro("10");
        retorne a + b;
    }
}
"@)
$p = Start-Process -FilePath $CompilerBinary -ArgumentList "-ini `"$IniFile`" -o `"$T30Ci`" `"$T30Source`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t30_c_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t30_c_out.txt")
$t30_compiled = ($p.ExitCode -eq 0 -and (Test-Path $T30Ci))

if ($t30_compiled) {
    $pvm = Start-Process -FilePath $VmBinary -ArgumentList "`"$T30Ci`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t30_vm_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t30_vm_out.txt")
    $t30_ok = ($pvm.ExitCode -eq 52)
    Assert-Test "Framework ijkL.Conversao: Portuguese Module & Member Aliases (exit_code = 52)" $t30_ok "Expected exit code 52. Got $($pvm.ExitCode)"
} else {
    $err = Get-Content (Join-Path $BuildDir "t30_c_err.txt") -Raw -ErrorAction SilentlyContinue
    Assert-Test "Framework ijkL.Conversao: Compilation" $false "Failed to compile $T30Source. Err=$err"
}
Remove-Item $T30Source -Force -ErrorAction SilentlyContinue

# ------------------------------------------------------------------------------
# Test 31: English Framework ijkL.Convert (Direct Canonical, -l en-US, VM exit_code = 43)
# ------------------------------------------------------------------------------
$T31Source = Join-Path $BuildDir "test_convert_en.ijk"
$T31Ci = Join-Path $BuildDir "test_convert_en.ci"
[System.IO.File]::WriteAllText($T31Source, @"
import ijkL.Convert;

program TestConvertEN {
    private main(): int {
        int a = toInt("40");
        int b = ijkL.Convert.toInt("3");
        return a + b;
    }
}
"@)
$p = Start-Process -FilePath $CompilerBinary -ArgumentList "-ini `"$IniFile`" -l en-US -o `"$T31Ci`" `"$T31Source`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t31_c_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t31_c_out.txt")
$t31_compiled = ($p.ExitCode -eq 0 -and (Test-Path $T31Ci))

if ($t31_compiled) {
    $pvm = Start-Process -FilePath $VmBinary -ArgumentList "`"$T31Ci`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t31_vm_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t31_vm_out.txt")
    $t31_ok = ($pvm.ExitCode -eq 43)
    Assert-Test "Framework ijkL.Convert: English Canonical Resolution & Overloading (exit_code = 43)" $t31_ok "Expected exit code 43. Got $($pvm.ExitCode)"
} else {
    $err = Get-Content (Join-Path $BuildDir "t31_c_err.txt") -Raw -ErrorAction SilentlyContinue
    Assert-Test "Framework ijkL.Convert: Compilation" $false "Failed to compile $T31Source. Err=$err"
}
Remove-Item $T31Source -Force -ErrorAction SilentlyContinue

# ------------------------------------------------------------------------------
# Test 32: Portuguese Framework ijkL.Texto (length, compare, concat, substring, VM exit_code = 11)
# ------------------------------------------------------------------------------
$T32Source = Join-Path $BuildDir "test_texto_pt.ijk"
$T32Ci = Join-Path $BuildDir "test_texto_pt.ci"
[System.IO.File]::WriteAllText($T32Source, @"
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
"@)
$p = Start-Process -FilePath $CompilerBinary -ArgumentList "-ini `"$IniFile`" -o `"$T32Ci`" `"$T32Source`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t32_c_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t32_c_out.txt")
$t32_compiled = ($p.ExitCode -eq 0 -and (Test-Path $T32Ci))

if ($t32_compiled) {
    $pvm = Start-Process -FilePath $VmBinary -ArgumentList "`"$T32Ci`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t32_vm_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t32_vm_out.txt")
    $t32_ok = ($pvm.ExitCode -eq 11)
    Assert-Test "Framework ijkL.Texto: String & Text Processing Operations (exit_code = 11)" $t32_ok "Expected exit code 11. Got $($pvm.ExitCode)"
} else {
    $err = Get-Content (Join-Path $BuildDir "t32_c_err.txt") -Raw -ErrorAction SilentlyContinue
    Assert-Test "Framework ijkL.Texto: Compilation" $false "Failed to compile $T32Source. Err=$err"
}
Remove-Item $T32Source -Force -ErrorAction SilentlyContinue

# ------------------------------------------------------------------------------
# Test 33: Portuguese Framework ijkL.Colecoes (Dynamic List, Add, Get, Size, Destroy, VM exit_code = 53)
# ------------------------------------------------------------------------------
$T33Source = Join-Path $BuildDir "test_colecoes_pt.ijk"
$T33Ci = Join-Path $BuildDir "test_colecoes_pt.ci"
[System.IO.File]::WriteAllText($T33Source, @"
importe ijkL.Colecoes;

programa TesteColecoesPT {
    tipo Item = registro {
        inteiro valor;
    };

    privado principal(): inteiro {
        List lista = criarLista(2);
        ponteiro Item i1 = reserve(`$Item);
        @i1.valor = 10;
        ponteiro Item i2 = reserve(`$Item);
        @i2.valor = 25;
        ponteiro Item i3 = reserve(`$Item);
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
"@)
$p = Start-Process -FilePath $CompilerBinary -ArgumentList "-ini `"$IniFile`" -o `"$T33Ci`" `"$T33Source`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t33_c_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t33_c_out.txt")
$t33_compiled = ($p.ExitCode -eq 0 -and (Test-Path $T33Ci))

if ($t33_compiled) {
    $pvm = Start-Process -FilePath $VmBinary -ArgumentList "`"$T33Ci`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t33_vm_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t33_vm_out.txt")
    $t33_ok = ($pvm.ExitCode -eq 53)
    Assert-Test "Framework ijkL.Colecoes: Dynamic List Allocation, Growth & Element Access (exit_code = 53)" $t33_ok "Expected exit code 53. Got $($pvm.ExitCode)"
} else {
    $err = Get-Content (Join-Path $BuildDir "t33_c_err.txt") -Raw -ErrorAction SilentlyContinue
    Assert-Test "Framework ijkL.Colecoes: Compilation" $false "Failed to compile $T33Source. Err=$err"
}

# ------------------------------------------------------------------------------
# Test 34: VM Native Builtins Framework Parity (--native-builtins, exit_code = 53)
# ------------------------------------------------------------------------------
$T34Ci = Join-Path $BuildDir "test_colecoes_pt_native.ci"
$p = Start-Process -FilePath $CompilerBinary -ArgumentList "-ini `"$IniFile`" --native-builtins -o `"$T34Ci`" `"$T33Source`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t34_c_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t34_c_out.txt")
$t34_compiled = ($p.ExitCode -eq 0 -and (Test-Path $T34Ci))

if ($t34_compiled) {
    $pvm = Start-Process -FilePath $VmBinary -ArgumentList "`"$T34Ci`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t34_vm_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t34_vm_out.txt")
    $t34_ok = ($pvm.ExitCode -eq 53)
    Assert-Test "Framework Parity: Compilation with --native-builtins and VM Execution (exit_code = 53)" $t34_ok "Expected exit code 53. Got $($pvm.ExitCode)"
} else {
    $err = Get-Content (Join-Path $BuildDir "t34_c_err.txt") -Raw -ErrorAction SilentlyContinue
    Assert-Test "Framework Parity: Compilation with --native-builtins" $false "Failed to compile with --native-builtins. Err=$err"
}
Remove-Item $T33Source -Force -ErrorAction SilentlyContinue

# ------------------------------------------------------------------------------
# Test 35: Inline Assembly Block (asm { ... }) Execution & Parity (exit_code = 42)
# ------------------------------------------------------------------------------
$T35Source = Join-Path $BuildDir "test_asm_block.ijk"
$T35Ci = Join-Path $BuildDir "test_asm_block.ci"
[System.IO.File]::WriteAllText($T35Source, @"
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
"@)
$p = Start-Process -FilePath $CompilerBinary -ArgumentList "-ini `"$IniFile`" -o `"$T35Ci`" `"$T35Source`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t35_c_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t35_c_out.txt")
$t35_compiled = ($p.ExitCode -eq 0 -and (Test-Path $T35Ci))

if ($t35_compiled) {
    $pvm = Start-Process -FilePath $VmBinary -ArgumentList "`"$T35Ci`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t35_vm_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t35_vm_out.txt")
    $t35_ok = ($pvm.ExitCode -eq 42)
    Assert-Test "Inline Assembly Block (asm { ... }): Code Generation & VM Execution (exit_code = 42)" $t35_ok "Expected exit code 42. Got $($pvm.ExitCode)"
} else {
    $err = Get-Content (Join-Path $BuildDir "t35_c_err.txt") -Raw -ErrorAction SilentlyContinue
    Assert-Test "Inline Assembly Block (asm { ... }): Compilation" $false "Failed to compile $T35Source. Err=$err"
}
Remove-Item $T35Source -Force -ErrorAction SilentlyContinue

# ------------------------------------------------------------------------------
# Test 36: Framework ijkL.Memory: DMA Block Copy, Fill & Compare (exit_code = 42)
# ------------------------------------------------------------------------------
$T36Source = Join-Path $BuildDir "test_memory_dma_integration.ijk"
$T36Ci = Join-Path $BuildDir "test_memory_dma_integration.ci"
[System.IO.File]::WriteAllText($T36Source, @"
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
"@)
$p = Start-Process -FilePath $CompilerBinary -ArgumentList "-ini `"$IniFile`" -o `"$T36Ci`" `"$T36Source`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t36_c_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t36_c_out.txt")
$t36_compiled = ($p.ExitCode -eq 0 -and (Test-Path $T36Ci))

if ($t36_compiled) {
    $pvm = Start-Process -FilePath $VmBinary -ArgumentList "`"$T36Ci`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t36_vm_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t36_vm_out.txt")
    $t36_ok = ($pvm.ExitCode -eq 42)
    Assert-Test "Framework ijkL.Memory: DMA Block Copy, Fill & Compare (exit_code = 42)" $t36_ok "Expected exit code 42. Got $($pvm.ExitCode)"
} else {
    $err = Get-Content (Join-Path $BuildDir "t36_c_err.txt") -Raw -ErrorAction SilentlyContinue
    Assert-Test "Framework ijkL.Memory: Compilation" $false "Failed to compile $T36Source. Err=$err"
}
Remove-Item $T36Source -Force -ErrorAction SilentlyContinue

# ------------------------------------------------------------------------------
# Test 37: Framework ijkL.IO (File, Folder, Path) (exit_code = 42)
# ------------------------------------------------------------------------------
$T37Source = Join-Path $BuildDir "test_io_integration.ijk"
$T37Ci = Join-Path $BuildDir "test_io_integration.ci"
[System.IO.File]::WriteAllText($T37Source, @"
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
"@)
$p = Start-Process -FilePath $CompilerBinary -ArgumentList "-ini `"$IniFile`" -o `"$T37Ci`" `"$T37Source`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t37_c_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t37_c_out.txt")
$t37_compiled = ($p.ExitCode -eq 0 -and (Test-Path $T37Ci))

if ($t37_compiled) {
    $pvm = Start-Process -FilePath $VmBinary -ArgumentList "`"$T37Ci`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t37_vm_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t37_vm_out.txt")
    $t37_ok = ($pvm.ExitCode -eq 42)
    Assert-Test "Framework ijkL.IO (File, Folder, Path): File System Operations (exit_code = 42)" $t37_ok "Expected exit code 42. Got $($pvm.ExitCode)"
} else {
    $err = Get-Content (Join-Path $BuildDir "t37_c_err.txt") -Raw -ErrorAction SilentlyContinue
    Assert-Test "Framework ijkL.IO (File, Folder, Path): Compilation" $false "Failed to compile $T37Source. Err=$err"
}
Remove-Item $T37Source -Force -ErrorAction SilentlyContinue

# ------------------------------------------------------------------------------
# Test 38: Standard Framework Extensions: Math, Text & Collections (exit_code = 42)
# ------------------------------------------------------------------------------
$T38Source = Join-Path $BuildDir "test_framework_extensions_integration.ijk"
$T38Ci = Join-Path $BuildDir "test_framework_extensions_integration.ci"
[System.IO.File]::WriteAllText($T38Source, @"
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
"@)
$p = Start-Process -FilePath $CompilerBinary -ArgumentList "-ini `"$IniFile`" -o `"$T38Ci`" `"$T38Source`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t38_c_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t38_c_out.txt")
$t38_compiled = ($p.ExitCode -eq 0 -and (Test-Path $T38Ci))

if ($t38_compiled) {
    $pvm = Start-Process -FilePath $VmBinary -ArgumentList "`"$T38Ci`"" -NoNewWindow -PassThru -Wait -RedirectStandardError (Join-Path $BuildDir "t38_vm_err.txt") -RedirectStandardOutput (Join-Path $BuildDir "t38_vm_out.txt")
    $t38_ok = ($pvm.ExitCode -eq 42)
    Assert-Test "Framework Extensions: Math, Text & Collections (exit_code = 42)" $t38_ok "Expected exit code 42. Got $($pvm.ExitCode)"
} else {
    $err = Get-Content (Join-Path $BuildDir "t38_c_err.txt") -Raw -ErrorAction SilentlyContinue
    Assert-Test "Framework Extensions: Compilation" $false "Failed to compile $T38Source. Err=$err"
}
Remove-Item $T38Source -Force -ErrorAction SilentlyContinue

Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host "Integration Results: $global:PassedCount Passed, $global:FailedCount Failed, $global:TotalCount Total" -ForegroundColor $(if ($global:FailedCount -eq 0) { "Green" } else { "Red" })
Write-Host "======================================================================" -ForegroundColor Cyan

if ($global:FailedCount -gt 0) {
    exit 1
} else {
    exit 0
}

