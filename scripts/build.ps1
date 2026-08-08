[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [switch]$TestsOnly
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$manifest = Get-Content -LiteralPath (Join-Path $repoRoot 'tools\toolchain.json') -Raw | ConvertFrom-Json
$toolchain = Join-Path $repoRoot $manifest.installDirectory
$compiler = Join-Path $toolchain 'bin\clang++.exe'
$resourceCompiler = Join-Path $toolchain 'bin\windres.exe'

if (-not (Test-Path -LiteralPath $compiler)) {
    throw 'Portable compiler is missing. Run scripts/bootstrap-toolchain.ps1 first.'
}

$configurationName = $Configuration.ToLowerInvariant()
$outputDirectory = Join-Path $repoRoot "build\$configurationName"
$objectDirectory = Join-Path $outputDirectory 'obj'
New-Item -ItemType Directory -Force -Path $objectDirectory | Out-Null

$common = @(
    '-std=c++20', '-Wall', '-Wextra', '-Wpedantic', '-Wconversion',
    '-DUNICODE', '-D_UNICODE', '-DWIN32_LEAN_AND_MEAN', '-DNOMINMAX',
    '-D_WIN32_WINNT=0x0A00', '-DWINVER=0x0A00',
    '-I', (Join-Path $repoRoot 'include'),
    '-I', (Join-Path $repoRoot 'src')
)
if ($Configuration -eq 'Release') {
    $common += @('-O2', '-DNDEBUG', '-ffunction-sections', '-fdata-sections')
} else {
    $common += @('-O0', '-g', '-DELITE_PEN_DEBUG')
}

function Invoke-Compiler {
    param([string[]]$Arguments)
    & $compiler @Arguments
    if ($LASTEXITCODE -ne 0) { throw "Compiler failed with exit code $LASTEXITCODE." }
}

Write-Host "Building core tests ($Configuration)..."
$testExecutable = Join-Path $outputDirectory 'elite-pen-tests.exe'
Invoke-Compiler ($common + @(
    (Join-Path $repoRoot 'src\core.cpp'),
    (Join-Path $repoRoot 'tests\core_tests.cpp'),
    '-o', $testExecutable,
    '-static', '-pthread'
))

$performanceExecutable = Join-Path $outputDirectory 'elite-pen-performance-tests.exe'
Invoke-Compiler ($common + @(
    (Join-Path $repoRoot 'src\core.cpp'),
    (Join-Path $repoRoot 'tests\performance_tests.cpp'),
    '-o', $performanceExecutable,
    '-static', '-pthread'
))

if (-not $TestsOnly) {
    $applicationSources = Get-ChildItem -LiteralPath (Join-Path $repoRoot 'src') -Filter '*.cpp' |
        Where-Object { $_.Name -ne 'core.cpp' } |
        ForEach-Object { $_.FullName }
    if ($applicationSources.Count -eq 0) {
        Write-Warning 'Application sources are not present yet; only tests were built.'
    } else {
        Write-Host "Building Elite Pen ($Configuration)..."
        & (Join-Path $PSScriptRoot 'generate-assets.ps1')
        if ($LASTEXITCODE -ne 0) { throw "Asset generation failed with exit code $LASTEXITCODE." }
        $resourceObject = Join-Path $objectDirectory 'elite_pen_resource.o'
        & $resourceCompiler -I (Join-Path $repoRoot 'resources') `
            (Join-Path $repoRoot 'resources\elite_pen.rc') $resourceObject
        if ($LASTEXITCODE -ne 0) { throw "Resource compiler failed with exit code $LASTEXITCODE." }

        $executable = Join-Path $outputDirectory 'Elite Pen.exe'
        $libraries = @(
            '-ld2d1', '-ldwrite', '-ld3d11', '-ldxgi', '-ldcomp', '-ldwmapi',
            '-lcomctl32', '-lcomdlg32', '-lshell32', '-lole32', '-luuid',
            '-lshlwapi', '-lwindowscodecs', '-luser32', '-lgdi32', '-lkernel32'
        )
        $linkFlags = @('-municode', '-mwindows', '-static', '-Wl,--gc-sections')
        Invoke-Compiler ($common + @((Join-Path $repoRoot 'src\core.cpp')) +
            $applicationSources + @($resourceObject, '-o', $executable) + $linkFlags + $libraries)
        Write-Host "Application: $executable"
    }
}

Write-Host "Tests: $testExecutable"
Write-Host "Performance tests: $performanceExecutable"
