# ==============================================================================
# ijkLang (ijkL) - Publish and Distribution Script for Windows / PowerShell
# Rebuilds the solution (Clean + Build) and copies all distribution files to destination
# ==============================================================================

param(
    [Parameter(Position = 0, Mandatory = $false)]
    [string]$Destination,
    [string]$Configuration = "Debug"
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($Destination)) {
    Write-Host "Uso: .\publish.ps1 <diretorio_destino> [-Configuration Debug|Release]" -ForegroundColor Yellow
    Write-Host "Exemplo: .\publish.ps1 ..\ijkDemo" -ForegroundColor Cyan
    exit 1
}

$ProjectRoot = $PSScriptRoot

# Remove variaveis de proxy para evitar colisoes com MSBuild
Remove-Item env:https_proxy -ErrorAction SilentlyContinue
Remove-Item env:HTTPS_PROXY -ErrorAction SilentlyContinue
Remove-Item env:http_proxy -ErrorAction SilentlyContinue
Remove-Item env:HTTP_PROXY -ErrorAction SilentlyContinue

Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host "ijkLang (ijkL) - Compilando e Publicando Distribuicao" -ForegroundColor Cyan
Write-Host "Configuracao: $Configuration" -ForegroundColor Gray
Write-Host "Destino:      $Destination" -ForegroundColor Gray
Write-Host "======================================================================" -ForegroundColor Cyan

# 1. Localiza o MSBuild
$MsBuildPath = "C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\MSBuild.exe"
if (-not (Test-Path $MsBuildPath)) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
        if ($vsPath) {
            $candidate = Join-Path $vsPath "MSBuild\Current\Bin\MSBuild.exe"
            if (Test-Path $candidate) { $MsBuildPath = $candidate }
        }
    }
}

if (-not (Test-Path $MsBuildPath)) {
    $cmd = Get-Command "MSBuild.exe" -ErrorAction SilentlyContinue
    if ($cmd) { $MsBuildPath = $cmd.Source }
}

if (-not (Test-Path $MsBuildPath)) {
    Write-Error "MSBuild.exe nao foi encontrado no ambiente. Execute no Developer Command Prompt."
}

# 2. Executa Clean + Build (Rebuild)
Write-Host "`n[1/3] Executando Rebuild da solucao ijkL.sln ($Configuration)..." -ForegroundColor Magenta
$solutionPath = Join-Path $ProjectRoot "ijkL.sln"
$buildProcess = Start-Process -FilePath $MsBuildPath -ArgumentList "`"$solutionPath`" /t:Rebuild /p:Configuration=$Configuration /nologo" -NoNewWindow -PassThru -Wait
if ($buildProcess.ExitCode -ne 0) {
    Write-Error "Falha na compilacao do ijkLang (ExitCode: $($buildProcess.ExitCode)). Publicacao abortada."
}

# 3. Resolve diretorio de destino
if (-not (Test-Path $Destination)) {
    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
}
$ResolvedDest = (Resolve-Path -Path $Destination).Path

# Se o destino já terminar com 'bin', copia os binários diretamente nele; senão, cria subpasta 'bin'
$DestBin = if ($ResolvedDest.EndsWith("\bin") -or $ResolvedDest.EndsWith("/bin")) {
    $ResolvedDest
} else {
    Join-Path $ResolvedDest "bin"
}

if (-not (Test-Path $DestBin)) {
    New-Item -ItemType Directory -Path $DestBin -Force | Out-Null
}

$SourceBin = Join-Path $ProjectRoot "bin\$Configuration"
if (-not (Test-Path $SourceBin)) {
    $SourceBin = Join-Path $ProjectRoot "bin"
}

# 4. Copia arquivos de distribuicao
Write-Host "`n[2/3] Copiando binarios e vocabularios para '$DestBin'..." -ForegroundColor Magenta

# Binários executáveis
Copy-Item (Join-Path $SourceBin "ijkl.exe") -Destination $DestBin -Force
Copy-Item (Join-Path $SourceBin "ijkl-m.exe") -Destination $DestBin -Force
Write-Host "  -> ijkl.exe" -ForegroundColor Green
Write-Host "  -> ijkl-m.exe" -ForegroundColor Green

# Arquivo de configuração ijkl.ini
$iniSrc = Join-Path $SourceBin "ijkl.ini"
if (-not (Test-Path $iniSrc)) { $iniSrc = Join-Path $ProjectRoot "ijkl.ini" }
Copy-Item $iniSrc -Destination $DestBin -Force
Write-Host "  -> ijkl.ini" -ForegroundColor Green

# Dicionários de palavras-chave keywords/
$destKeywords = Join-Path $DestBin "keywords"
if (-not (Test-Path $destKeywords)) {
    New-Item -ItemType Directory -Path $destKeywords -Force | Out-Null
}
$keywordsSrc = Join-Path $ProjectRoot "keywords"
Get-ChildItem -Path $keywordsSrc -Filter "*.words" | ForEach-Object {
    Copy-Item $_.FullName -Destination $destKeywords -Force
    Write-Host "  -> keywords/$($_.Name)" -ForegroundColor Green
}

# 5. Copia biblioteca padrao do framework (lib/)
Write-Host "`n[3/4] Copiando biblioteca padrao 'lib' para '$ResolvedDest\lib'..." -ForegroundColor Magenta
$destLib = Join-Path $ResolvedDest "lib"
if (-not (Test-Path $destLib)) {
    New-Item -ItemType Directory -Path $destLib -Force | Out-Null
}
Copy-Item (Join-Path $ProjectRoot "lib\*") -Destination $destLib -Recurse -Force
Write-Host "  -> Pasta 'lib' sincronizada em '$destLib'." -ForegroundColor Green

# Se DestBin for diferente de ResolvedDest, copia também para DestBin\lib
$destBinLib = Join-Path $DestBin "lib"
if ($destBinLib -ne $destLib) {
    if (-not (Test-Path $destBinLib)) {
        New-Item -ItemType Directory -Path $destBinLib -Force | Out-Null
    }
    Copy-Item (Join-Path $ProjectRoot "lib\*") -Destination $destBinLib -Recurse -Force
    Write-Host "  -> Pasta 'lib' sincronizada em '$destBinLib'." -ForegroundColor Green
}

# 6. Copia pasta de exemplos
Write-Host "`n[4/4] Copiando pasta de exemplos 'examples' para '$ResolvedDest\examples'..." -ForegroundColor Magenta
$destExamples = Join-Path $ResolvedDest "examples"
if (-not (Test-Path $destExamples)) {
    New-Item -ItemType Directory -Path $destExamples -Force | Out-Null
}
Copy-Item (Join-Path $ProjectRoot "examples\*") -Destination $destExamples -Recurse -Force
Write-Host "  -> Pasta 'examples' sincronizada com sucesso." -ForegroundColor Green

Write-Host "`n======================================================================" -ForegroundColor Cyan
Write-Host "Publicacao concluida com sucesso em: $ResolvedDest" -ForegroundColor Green
Write-Host "======================================================================" -ForegroundColor Cyan
