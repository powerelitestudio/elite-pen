[CmdletBinding()]
param(
    [switch]$SkipPortableBuild
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not $SkipPortableBuild) {
    & (Join-Path $PSScriptRoot 'publish-portable.ps1')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$candidates = @(
    (Join-Path $repoRoot '.tools\inno-setup-7.0.2\ISCC.exe'),
    (Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 7\ISCC.exe'),
    (Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 6\ISCC.exe'),
    (Join-Path $env:ProgramFiles 'Inno Setup 7\ISCC.exe'),
    (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe')
)
$compiler = $candidates | Where-Object { $_ -and (Test-Path -LiteralPath $_) } |
    Select-Object -First 1
if (-not $compiler) {
    throw 'Inno Setup compiler was not found. Install JRSoftware.InnoSetup for the current user.'
}

& $compiler (Join-Path $repoRoot 'packaging\ElitePen.iss')
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$installer = Join-Path $repoRoot 'dist\installer\Elite Pen Setup 1.4.0.exe'
Write-Output "Installer: $installer"
