# Valida os dois modos com stdin redirecionado, incluindo pipe aberto sem dados.
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
$Passed = 0
$Outputs = @{}
function Invoke-ConsoleProbe {
    param([string]$ProgramPath, [string]$InputText, [bool]$CloseInput = $true)
    $StartInfo = New-Object System.Diagnostics.ProcessStartInfo
    $StartInfo.FileName = $VmPath
    $StartInfo.Arguments = "`"$ProgramPath`""
    $StartInfo.WorkingDirectory = $ProjectRoot
    $StartInfo.UseShellExecute = $false
    $StartInfo.RedirectStandardInput = $true
    $StartInfo.RedirectStandardOutput = $true
    $StartInfo.RedirectStandardError = $true
    $Process = [System.Diagnostics.Process]::Start($StartInfo)
    try {
        $Process.StandardInput.Write($InputText)
        $Process.StandardInput.Flush()
        if ($CloseInput) { $Process.StandardInput.Close() }
        if (-not $Process.WaitForExit(5000)) {
            $Process.Kill()
            $Process.WaitForExit()
            throw "Console bloqueou por mais de 5 segundos"
        }
        $OutputText = $Process.StandardOutput.ReadToEnd().Replace("`r`n", "`n")
        $ErrorText = $Process.StandardError.ReadToEnd()
        if ($Process.ExitCode -ne 0 -or $ErrorText) {
            throw "VM falhou: exit=$($Process.ExitCode), stderr=$ErrorText"
        }
        return $OutputText
    } finally {
        if (-not $Process.HasExited) { $Process.Kill(); $Process.WaitForExit() }
        $Process.Dispose()
    }
}
try {
    foreach ($Mode in @("ci", "native")) {
        $Programs = @{}
        foreach ($Fixture in @("probe", "mixed", "aliases", "poll")) {
            $Target = Join-Path $TemporaryDirectory "$Fixture-$Mode.ci"
            $Language = if ($Fixture -eq "aliases") { "pt" } else { "en" }
            $CompilerArguments = @("-ini", (Join-Path $PSScriptRoot "ijkl_$Language.ini"))
            if ($Mode -eq "native") { $CompilerArguments += "--native-builtins" }
            $CompilerArguments += @((Join-Path $PSScriptRoot "console\$Fixture.ijk"), "-o", $Target)
            Push-Location $ProjectRoot
            try {
                & $CompilerPath @CompilerArguments
                if ($LASTEXITCODE -ne 0) { throw "Falha ao compilar $Fixture ($Mode)" }
            } finally { Pop-Location }
            $Emitted = [IO.File]::ReadAllText($Target)
            foreach ($Builtin in @("has_key", "read_key_async")) {
                if ($Emitted.Contains("call #__builtin_console_$Builtin;") -ne ($Mode -eq "native")) {
                    throw "Despacho incorreto: $Fixture, $Mode, $Builtin"
                }
            }
            if ($Mode -eq "ci" -and (-not $Emitted.Contains("in 0x0005, r0;") -or -not $Emitted.Contains("in 0x0006, r0;"))) {
                throw "Implementacao CI ausente"
            }
            $Programs[$Fixture] = $Target
        }
        $Cases = @(
            @{ Name = "empty"; Fixture = "probe"; InputText = ""; Expected = "0`n0`n0`n0`n0`n0`n0`n" },
            @{ Name = "buffered"; Fixture = "probe"; InputText = "AB"; Expected = "1`n1`n65`n1`n66`n0`n0`n" },
            @{ Name = "escape"; Fixture = "probe"; InputText = [string][char]27; Expected = "1`n1`n27`n0`n0`n0`n0`n" },
            @{ Name = "nul"; Fixture = "probe"; InputText = [string][char]0; Expected = "1`n1`n0`n0`n0`n0`n0`n" },
            @{ Name = "mixed"; Fixture = "mixed"; InputText = "hello`nZ"; Expected = "1`nhello`n1`n90`n" },
            @{ Name = "aliases"; Fixture = "aliases"; InputText = "Z"; Expected = "1`n90`n" }
        )
        foreach ($Case in $Cases) {
            $Actual = Invoke-ConsoleProbe -ProgramPath $Programs[$Case.Fixture] -InputText $Case.InputText
            if ($Actual -cne $Case.Expected) { throw "Saida incorreta: $Mode / $($Case.Name): $Actual" }
            if ($Mode -eq "native" -and $Actual -cne $Outputs[$Case.Name]) { throw "Paridade divergente" }
            $Outputs[$Case.Name] = $Actual
            $Passed++
            Write-Host "PASS ${Mode}: $($Case.Name)"
        }
        $Actual = Invoke-ConsoleProbe -ProgramPath $Programs["probe"] -InputText "" -CloseInput $false
        if ($Actual -cne $Cases[0].Expected) { throw "Pipe aberto sem dados retornou resultado incorreto" }
        $Passed++
        Write-Host "PASS ${Mode}: open-empty-pipe"
    }
    Write-Host "Console: $Passed passed; CI/native parity verified"
} finally {
    Remove-Item -LiteralPath $TemporaryDirectory -Recurse -Force
}
