[CmdletBinding()]
param(
    [string]$OutputPath,
    [string]$ClassName = 'ElitePen.Palette',
    [string]$WindowTitle = 'Elite Pen'
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not $OutputPath) {
    $OutputPath = Join-Path $repoRoot 'artifacts\qa\palette-live.png'
}

Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class ElitePenCaptureNative {
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll", CharSet=CharSet.Unicode)]
    public static extern IntPtr FindWindow(string className, string windowName);
    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr window, out RECT rectangle);
    [DllImport("user32.dll")]
    public static extern bool SetWindowDisplayAffinity(IntPtr window, uint affinity);
    [DllImport("user32.dll")]
    public static extern bool SetProcessDpiAwarenessContext(IntPtr context);
}
'@

$null = [ElitePenCaptureNative]::SetProcessDpiAwarenessContext([IntPtr](-4))
$window = [ElitePenCaptureNative]::FindWindow($ClassName, $WindowTitle)
if ($window -eq [IntPtr]::Zero) { throw 'Elite Pen palette window was not found.' }
$rectangle = New-Object ElitePenCaptureNative+RECT
if (-not [ElitePenCaptureNative]::GetWindowRect($window, [ref]$rectangle)) {
    throw 'Could not read the Elite Pen palette bounds.'
}

$width = $rectangle.Right - $rectangle.Left
$height = $rectangle.Bottom - $rectangle.Top
$null = [ElitePenCaptureNative]::SetWindowDisplayAffinity($window, 0)
$bitmap = New-Object System.Drawing.Bitmap($width, $height,
    [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
try {
    $graphics.CopyFromScreen($rectangle.Left, $rectangle.Top, 0, 0,
        (New-Object System.Drawing.Size($width, $height)))
    $directory = Split-Path -Parent $OutputPath
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
    $bitmap.Save($OutputPath, [System.Drawing.Imaging.ImageFormat]::Png)
} finally {
    $null = [ElitePenCaptureNative]::SetWindowDisplayAffinity($window, 0x11)
    $graphics.Dispose()
    $bitmap.Dispose()
}
Write-Output $OutputPath
