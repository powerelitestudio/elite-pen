[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$manifestPath = Join-Path $repoRoot 'tools\toolchain.json'
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$installPath = Join-Path $repoRoot $manifest.installDirectory
$compilerPath = Join-Path $installPath 'bin\clang++.exe'

if (Test-Path -LiteralPath $compilerPath) {
    Write-Host "Toolchain already available: $installPath"
    exit 0
}

$downloadDirectory = Join-Path $repoRoot '.tools\downloads'
$archivePath = Join-Path $downloadDirectory (Split-Path -Leaf $manifest.url)
New-Item -ItemType Directory -Force -Path $downloadDirectory | Out-Null

if (-not (Test-Path -LiteralPath $archivePath)) {
    Write-Host "Downloading $($manifest.name) $($manifest.version)..."
    Invoke-WebRequest -Uri $manifest.url -OutFile $archivePath -UseBasicParsing
}

$actualHash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash
if ($actualHash -ne $manifest.sha256) {
    throw "Toolchain checksum mismatch. Expected $($manifest.sha256), received $actualHash."
}

$toolsDirectory = Join-Path $repoRoot '.tools'
Expand-Archive -LiteralPath $archivePath -DestinationPath $toolsDirectory -Force

if (-not (Test-Path -LiteralPath $compilerPath)) {
    throw "Toolchain extraction completed but clang++ was not found at $compilerPath."
}

Write-Host "Toolchain ready: $installPath"
