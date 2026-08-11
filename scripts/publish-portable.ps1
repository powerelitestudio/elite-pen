[CmdletBinding()]
param(
    [string]$Destination,
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$staging = Join-Path $repoRoot 'dist\Elite Pen'
$releaseDirectory = Join-Path $repoRoot 'build\release'

if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot 'build.ps1') -Configuration Release
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & (Join-Path $releaseDirectory 'elite-pen-tests.exe')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & (Join-Path $releaseDirectory 'elite-pen-performance-tests.exe')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & (Join-Path $releaseDirectory 'preferences-qa\elite-pen-preferences-tests.exe')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$resolvedRepo = [IO.Path]::GetFullPath($repoRoot).TrimEnd('\') + '\'
$resolvedStaging = [IO.Path]::GetFullPath($staging)
if (-not $resolvedStaging.StartsWith($resolvedRepo, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to replace staging outside repository: $resolvedStaging"
}
if (Test-Path -LiteralPath $staging) { Remove-Item -LiteralPath $staging -Recurse -Force }
New-Item -ItemType Directory -Force -Path $staging | Out-Null

Copy-Item -LiteralPath (Join-Path $releaseDirectory 'Elite Pen.exe') -Destination $staging
Copy-Item -LiteralPath (Join-Path $repoRoot 'packaging\README_PORTABLE.txt') `
    -Destination (Join-Path $staging 'LEEME.txt')
Copy-Item -LiteralPath (Join-Path $repoRoot 'LICENSE') `
    -Destination (Join-Path $staging 'LICENSE.txt')
Copy-Item -LiteralPath (Join-Path $repoRoot 'NOTICE') -Destination $staging
Copy-Item -LiteralPath (Join-Path $repoRoot 'TRADEMARKS.md') -Destination $staging
Set-Content -LiteralPath (Join-Path $staging 'portable.flag') `
    -Value 'Elite Pen portable distribution' -Encoding ascii -NoNewline

$executable = Join-Path $staging 'Elite Pen.exe'
$version = (Get-Item -LiteralPath $executable).VersionInfo.FileVersion
$buildInfo = [ordered]@{
    product = 'Elite Pen'
    version = $version
    architecture = 'x64'
    minimumWindows = 'Windows 10 1809'
    builtAtUtc = [DateTime]::UtcNow.ToString('o')
    executableSha256 = (Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash
}
$buildInfo | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $staging 'build-info.json') `
    -Encoding utf8

$checksumLines = Get-ChildItem -LiteralPath $staging -File |
    Where-Object Name -ne 'SHA256SUMS.txt' |
    Sort-Object Name |
    ForEach-Object { "{0}  {1}" -f (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash, $_.Name }
$checksumLines | Set-Content -LiteralPath (Join-Path $staging 'SHA256SUMS.txt') -Encoding ascii

if ($Destination) {
    $destinationFull = [IO.Path]::GetFullPath($Destination)
    $backup = $null
    if (Test-Path -LiteralPath $destinationFull) {
        $installedExecutable = Join-Path $destinationFull 'Elite Pen.exe'
        $installedVersion = if (Test-Path -LiteralPath $installedExecutable) {
            (Get-Item -LiteralPath $installedExecutable).VersionInfo.FileVersion
        } else {
            'unknown'
        }
        $safeVersion = $installedVersion -replace '[^0-9A-Za-z._-]', '-'
        $backup = "$destinationFull.previous-$safeVersion"
        if (Test-Path -LiteralPath $backup) {
            $stamp = [DateTime]::Now.ToString('yyyyMMdd-HHmmss')
            $backup = "$backup-$stamp"
        }
        Move-Item -LiteralPath $destinationFull -Destination $backup
    }
    New-Item -ItemType Directory -Force -Path $destinationFull | Out-Null
    Copy-Item -Path (Join-Path $staging '*') -Destination $destinationFull -Recurse -Force
    $preservedData = if ($backup) { Join-Path $backup 'data' } else { $null }
    if ($preservedData -and (Test-Path -LiteralPath $preservedData)) {
        Copy-Item -LiteralPath $preservedData -Destination $destinationFull -Recurse -Force
    }
    if ($backup) { Write-Output "Previous portable preserved at: $backup" }
    Write-Output "Portable distribution: $destinationFull"
}
Write-Output "Staging distribution: $staging"
