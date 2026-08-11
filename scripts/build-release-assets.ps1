[CmdletBinding()]
param(
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$version = (Get-Content -LiteralPath (Join-Path $repoRoot 'VERSION') -Raw).Trim()
$portableDirectory = Join-Path $repoRoot 'dist\Elite Pen'
$installer = Join-Path $repoRoot "dist\installer\Elite Pen Setup $version.exe"
$releaseDirectory = Join-Path $repoRoot 'dist\release'
$portableArchive = Join-Path $releaseDirectory "Elite.Pen.Portable.$version.zip"
$releaseInstaller = Join-Path $releaseDirectory "Elite.Pen.Setup.$version.exe"

if ($SkipBuild) {
    & (Join-Path $PSScriptRoot 'publish-portable.ps1') -SkipBuild
} else {
    & (Join-Path $PSScriptRoot 'publish-portable.ps1')
}
if (-not $?) { throw 'Portable packaging failed.' }

& (Join-Path $PSScriptRoot 'build-installer.ps1') -SkipPortableBuild
if (-not $?) { throw 'Installer packaging failed.' }

$resolvedRepo = [IO.Path]::GetFullPath($repoRoot).TrimEnd('\') + '\'
$resolvedRelease = [IO.Path]::GetFullPath($releaseDirectory)
if (-not $resolvedRelease.StartsWith($resolvedRepo, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to replace release directory outside repository: $resolvedRelease"
}
if (Test-Path -LiteralPath $releaseDirectory) {
    Remove-Item -LiteralPath $releaseDirectory -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $releaseDirectory | Out-Null

Compress-Archive -Path (Join-Path $portableDirectory '*') `
    -DestinationPath $portableArchive -CompressionLevel Optimal
Copy-Item -LiteralPath $installer -Destination $releaseInstaller

$assets = Get-ChildItem -LiteralPath $releaseDirectory -File |
    Where-Object Name -ne 'SHA256SUMS.txt' |
    Sort-Object Name
$checksumLines = $assets | ForEach-Object {
    "{0}  {1}" -f (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash, $_.Name
}
$checksumPath = Join-Path $releaseDirectory 'SHA256SUMS.txt'
$checksumLines | Set-Content -LiteralPath $checksumPath -Encoding ascii

Write-Output "Release assets: $releaseDirectory"
$assets | ForEach-Object { Write-Output " - $($_.Name)" }
Write-Output " - SHA256SUMS.txt"
