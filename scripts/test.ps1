[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
& (Join-Path $PSScriptRoot 'build.ps1') -Configuration $Configuration -TestsOnly
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$testExecutable = Join-Path $repoRoot "build\$($Configuration.ToLowerInvariant())\elite-pen-tests.exe"
& $testExecutable
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$performanceExecutable = Join-Path $repoRoot "build\$($Configuration.ToLowerInvariant())\elite-pen-performance-tests.exe"
& $performanceExecutable
exit $LASTEXITCODE
