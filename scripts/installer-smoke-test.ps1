[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$installer = Join-Path $repoRoot 'dist\installer\Elite Pen Setup 1.1.1.exe'
$target = Join-Path $repoRoot 'artifacts\qa\installer-smoke'
$resolvedRepo = [IO.Path]::GetFullPath($repoRoot).TrimEnd('\') + '\'
$resolvedTarget = [IO.Path]::GetFullPath($target)
if (-not $resolvedTarget.StartsWith($resolvedRepo, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Unsafe installer test target: $resolvedTarget"
}
if (-not (Test-Path -LiteralPath $installer)) { throw "Missing installer: $installer" }
if (Test-Path -LiteralPath $target) { Remove-Item -LiteralPath $target -Recurse -Force }

try {
    $installArguments = @(
        '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/NOICONS',
        "/DIR=`"$resolvedTarget`""
    )
    $installation = Start-Process -FilePath $installer -ArgumentList $installArguments `
        -PassThru -Wait -WindowStyle Hidden
    if ($installation.ExitCode -ne 0) { throw "Installer exited with $($installation.ExitCode)." }

    $installedExecutable = Join-Path $target 'Elite Pen.exe'
    if (-not (Test-Path -LiteralPath $installedExecutable)) {
        throw 'Installer completed but Elite Pen.exe is missing.'
    }
    $powerShell = (Get-Process -Id $PID).Path
    $uiTestScript = Join-Path $PSScriptRoot 'ui-smoke-test.ps1'
    $uiTestArguments = @('-NoProfile', '-File', "`"$uiTestScript`"",
                         '-ExecutablePath', "`"$installedExecutable`"")
    $uiTest = Start-Process -FilePath $powerShell -ArgumentList $uiTestArguments `
        -PassThru -Wait -WindowStyle Hidden
    if ($uiTest.ExitCode -ne 0) { throw 'Installed application failed its UI smoke test.' }
    [GC]::Collect()
    [GC]::WaitForPendingFinalizers()
    Start-Sleep -Milliseconds 500

    $uninstaller = Join-Path $target 'unins000.exe'
    if (-not (Test-Path -LiteralPath $uninstaller)) { throw 'Uninstaller is missing.' }
    $uninstallation = Start-Process -FilePath $uninstaller `
        -ArgumentList @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART') `
        -PassThru -Wait -WindowStyle Hidden
    if ($uninstallation.ExitCode -ne 0) { throw "Uninstaller exited with $($uninstallation.ExitCode)." }
    for ($index = 0; $index -lt 40 -and (Test-Path -LiteralPath $target); $index++) {
        Start-Sleep -Milliseconds 250
    }
    if (Test-Path -LiteralPath $uninstaller) {
        $secondPass = Start-Process -FilePath $uninstaller `
            -ArgumentList @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART') `
            -PassThru -Wait -WindowStyle Hidden
        if ($secondPass.ExitCode -ne 0) { throw "Second uninstall pass exited with $($secondPass.ExitCode)." }
        for ($index = 0; $index -lt 40 -and (Test-Path -LiteralPath $target); $index++) {
            Start-Sleep -Milliseconds 250
        }
    }
} finally {
    if (Test-Path -LiteralPath $target) {
        Start-Sleep -Milliseconds 500
        Remove-Item -LiteralPath $target -Recurse -Force
    }
}

Write-Output 'Elite Pen installer smoke test: install, launch, UI and uninstall passed'
