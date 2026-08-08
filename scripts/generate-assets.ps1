[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$outputDirectory = Join-Path $repoRoot 'resources\generated'
$iconPath = Join-Path $outputDirectory 'elite_pen.ico'
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null

Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class ElitePenIconNative {
    [DllImport("user32.dll")]
    public static extern bool DestroyIcon(IntPtr icon);
}
'@

$bitmap = New-Object System.Drawing.Bitmap(64, 64,
    [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$graphics.Clear([System.Drawing.Color]::Transparent)
try {
    $cream = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 243, 227, 202))
    $ink = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(255, 35, 35, 38), 2)
    $palette = New-Object System.Drawing.Drawing2D.GraphicsPath
    $palette.AddBezier(11, 8, 25, -1, 51, 5, 54, 23)
    $palette.AddBezier(54, 23, 57, 40, 44, 45, 34, 43)
    $palette.AddBezier(34, 43, 30, 52, 21, 51, 15, 45)
    $palette.AddBezier(15, 45, 9, 39, 16, 31, 10, 27)
    $palette.AddBezier(10, 27, 3, 23, 4, 13, 11, 8)
    $palette.CloseFigure()
    $graphics.FillPath($cream, $palette)
    $graphics.DrawPath($ink, $palette)
    $graphics.FillEllipse([System.Drawing.Brushes]::Black, 15, 14, 7, 7)
    $graphics.FillEllipse((New-Object Drawing.SolidBrush([Drawing.Color]::FromArgb(255,255,190,45))), 41, 15, 7, 7)
    $graphics.FillEllipse((New-Object Drawing.SolidBrush([Drawing.Color]::FromArgb(255,31,136,229))), 41, 27, 7, 7)
    $graphics.FillEllipse((New-Object Drawing.SolidBrush([Drawing.Color]::FromArgb(255,239,68,68))), 29, 35, 7, 7)
    $handle = New-Object Drawing.Pen([Drawing.Color]::FromArgb(255,39,169,210), 7)
    $handle.StartCap = [Drawing.Drawing2D.LineCap]::Round
    $handle.EndCap = [Drawing.Drawing2D.LineCap]::Round
    $graphics.DrawLine($handle, 24, 43, 57, 59)
    $graphics.FillEllipse((New-Object Drawing.SolidBrush([Drawing.Color]::FromArgb(255,87,64,96))), 12, 37, 12, 10)
    $graphics.DrawLine((New-Object Drawing.Pen([Drawing.Color]::White, 6)), 21, 42, 28, 45)
    $iconHandle = $bitmap.GetHicon()
    $icon = [System.Drawing.Icon]::FromHandle($iconHandle)
    $stream = [System.IO.File]::Open($iconPath, [System.IO.FileMode]::Create)
    try { $icon.Save($stream) } finally { $stream.Dispose(); $icon.Dispose() }
    $null = [ElitePenIconNative]::DestroyIcon($iconHandle)
} finally {
    $graphics.Dispose()
    $bitmap.Dispose()
}
