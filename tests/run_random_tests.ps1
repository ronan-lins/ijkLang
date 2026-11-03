# Vetores independentes, paridade IEEE 754 e estado compartilhado CI/nativo.
param([string]$Configuration = "Debug", [string]$CompilerPath = "", [string]$VmPath = "")
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Numerics
$ProjectRoot = (Get-Item $PSScriptRoot).Parent.FullName
if (-not $CompilerPath) { $CompilerPath = Join-Path $ProjectRoot "bin\$Configuration\ijkl.exe" }
if (-not $VmPath) { $VmPath = Join-Path $ProjectRoot "bin\$Configuration\ijkl-m.exe" }
$TemporaryDirectory = Join-Path ([IO.Path]::GetTempPath()) ([Guid]::NewGuid().ToString())
New-Item -ItemType Directory -Path $TemporaryDirectory | Out-Null
$Expected = [IO.File]::ReadAllText((Join-Path $PSScriptRoot "random\vectors.expected")).Replace("`r`n", "`n")
$Passed = 0
$Programs = @{}
function Invoke-RandomProgram {
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
            throw "Timeout na VM"
        }
        $OutputText = $OutputTask.GetAwaiter().GetResult().Replace("`r`n", "`n")
        $ErrorText = $ErrorTask.GetAwaiter().GetResult()
        if ($Process.ExitCode -ne 0 -or $ErrorText) { throw "VM: exit=$($Process.ExitCode), stderr=$ErrorText" }
        return $OutputText
    } finally {
        if (-not $Process.HasExited) { $Process.Kill(); $Process.WaitForExit() }
        $Process.Dispose()
    }
}
function Get-OracleNext {
    param([long]$Seed)
    $Multiplier = [System.Numerics.BigInteger]::Parse("6364136223846793005")
    $Increment = [System.Numerics.BigInteger]::Parse("1442695040888963407")
    $Modulus = [System.Numerics.BigInteger]::Parse("18446744073709551616")
    $State = (([System.Numerics.BigInteger]$Seed * $Multiplier) + $Increment) % $Modulus
    return [long][System.Numerics.BigInteger]::op_RightShift($State, 33)
}
try {
    foreach ($Mode in @("ci", "native")) {
        foreach ($Fixture in @("vectors", "aliases", "time_seed")) {
            $Target = Join-Path $TemporaryDirectory "$Fixture-$Mode.ci"
            $Language = if ($Fixture -eq "aliases") { "pt" } else { "en" }
            $CompilerArguments = @("-ini", (Join-Path $PSScriptRoot "ijkl_$Language.ini"))
            if ($Mode -eq "native") { $CompilerArguments += "--native-builtins" }
            $CompilerArguments += @((Join-Path $PSScriptRoot "random\$Fixture.ijk"), "-o", $Target)
            Push-Location $ProjectRoot
            try {
                & $CompilerPath @CompilerArguments
                if ($LASTEXITCODE -ne 0) { throw "Falha ao compilar $Fixture ($Mode)" }
            } finally { Pop-Location }
            $Emitted = [IO.File]::ReadAllText($Target)
            foreach ($Builtin in @("seed", "seed_with_time", "next", "next_range", "next_real")) {
                if ($Emitted.Contains("call #__builtin_random_$Builtin;") -ne ($Mode -eq "native")) {
                    throw "Despacho incorreto: $Fixture / $Mode / $Builtin"
                }
            }
            $Programs["$Mode/$Fixture"] = $Target
        }
        foreach ($Repetition in @(1, 2)) {
            $Actual = Invoke-RandomProgram $Programs["$Mode/vectors"]
            if ($Actual -cne $Expected) { throw "Vetores divergentes: $Mode" }
            $Samples = $Actual.Trim().Split("`n")
            if ($Samples.Count -ne 460) { throw "Contagem inesperada de vetores" }
            $RangeSamples = for ($Index = 2; $Index -lt 385; $Index += 3) { $Samples[$Index] }
            if (($RangeSamples | Select-Object -Unique).Count -lt 50) { throw "Dispersao insuficiente" }
            $Passed++
            Write-Host "PASS ${Mode}: 460 oracle values, run $Repetition"
        }
        $Actual = Invoke-RandomProgram $Programs["$Mode/aliases"]
        if ($Actual -cne "2117897364`n3`n0.295513`n") { throw "Aliases divergentes" }
        $Passed++
        Write-Host "PASS ${Mode}: Portuguese aliases"
        $PreviousSeeds = @()
        foreach ($Repetition in @(1, 2)) {
            if ($Repetition -eq 2) { Start-Sleep -Milliseconds 1200 }
            $Values = (Invoke-RandomProgram $Programs["$Mode/time_seed"]).Trim().Split("`n")
            if ($Values.Count -ne 3) { throw "Saida de relogio invalida" }
            $Before = [long]$Values[0]; $After = [long]$Values[1]; $Generated = [long]$Values[2]
            $Window = ($After - $Before + 86400) % 86400
            if ($Before -lt 0 -or $Before -ge 86400 -or $After -lt 0 -or $After -ge 86400 -or $Window -gt 5) {
                throw "Janela temporal invalida"
            }
            $Seeds = @(for ($Offset = 0; $Offset -le $Window; $Offset++) { ($Before + $Offset) % 86400 })
            $Candidates = @($Seeds | ForEach-Object { Get-OracleNext $_ })
            if ($Generated -notin $Candidates) { throw "seedWithTime diverge do oraculo" }
            if ($Repetition -eq 2 -and @($Seeds | Where-Object { $_ -in $PreviousSeeds }).Count -gt 0) {
                throw "A janela temporal nao avancou"
            }
            $PreviousSeeds = $Seeds
            $Passed++
            Write-Host "PASS ${Mode}: seedWithTime, run $Repetition"
        }
    }
    $Expression = '(?s)\n#ijkL\.Math\.Random\.next\$void:.*?(?=\n#ijkL\.Math\.Random\.nextReal\$void:)'
    $CiText = [IO.File]::ReadAllText($Programs["ci/vectors"])
    $NativeText = [IO.File]::ReadAllText($Programs["native/vectors"])
    $Section = [regex]::Match($CiText, $Expression)
    if (-not $Section.Success -or [regex]::Matches($NativeText, $Expression).Count -ne 1) { throw "Secao CI nao encontrada" }
    $MixedText = [regex]::Replace($NativeText, $Expression, [System.Text.RegularExpressions.MatchEvaluator]{ param($Match) $Section.Value })
    $MixedPath = Join-Path $TemporaryDirectory "mixed.ci"
    $Encoding = New-Object System.Text.UTF8Encoding -ArgumentList $false
    [IO.File]::WriteAllText($MixedPath, $MixedText, $Encoding)
    if ((Invoke-RandomProgram $MixedPath) -cne $Expected) { throw "Estado CI/nativo nao compartilhado" }
    $Passed++
    Write-Host "PASS mixed CI/native: 460 oracle values"
    Write-Host "Random: $Passed checks passed; exact integer/IEEE 754 parity verified"
} finally {
    Remove-Item -LiteralPath $TemporaryDirectory -Recurse -Force
}
