[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [string]$ExecutablePath
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$sourceExecutable = if ($ExecutablePath) {
    [IO.Path]::GetFullPath($ExecutablePath)
} else {
    Join-Path $repoRoot "build\$($Configuration.ToLowerInvariant())\Elite Pen.exe"
}
if (-not (Test-Path -LiteralPath $sourceExecutable)) {
    throw "Missing executable: $sourceExecutable"
}

$qaSandbox = Join-Path ([IO.Path]::GetTempPath()) `
    ("elite-pen-zoom-capture-qa-" + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $qaSandbox | Out-Null
$executable = Join-Path $qaSandbox 'Elite Pen.exe'
Copy-Item -LiteralPath $sourceExecutable -Destination $executable
Set-Content -LiteralPath (Join-Path $qaSandbox 'portable.flag') `
    -Value 'Elite Pen isolated zoom capture QA' -Encoding ascii -NoNewline

Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class ElitePenZoomCaptureQa {
    [StructLayout(LayoutKind.Sequential)] public struct RECT {
        public int Left, Top, Right, Bottom;
    }
    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr value);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindowEx(
        IntPtr parent, IntPtr after, string className, string title);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(
        IntPtr window, out uint processId);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(
        IntPtr window, uint message, IntPtr wparam, IntPtr lparam);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(
        IntPtr window, out RECT rectangle);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")] public static extern bool GetWindowDisplayAffinity(
        IntPtr window, out uint affinity);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr window);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    public static IntPtr FindForProcess(string className, string title, uint processId) {
        for (var window = FindWindowEx(IntPtr.Zero, IntPtr.Zero, className, title);
             window != IntPtr.Zero;
             window = FindWindowEx(IntPtr.Zero, window, className, title)) {
            uint candidate;
            GetWindowThreadProcessId(window, out candidate);
            if (candidate == processId) return window;
        }
        return IntPtr.Zero;
    }
}
'@

$null = [ElitePenZoomCaptureQa]::SetProcessDpiAwarenessContext([IntPtr](-4))
$previousInstance = $env:ELITE_PEN_QA_INSTANCE_ID
$env:ELITE_PEN_QA_INSTANCE_ID = [Guid]::NewGuid().ToString('N')
$process = $null
try {
    $process = Start-Process -FilePath $executable -WindowStyle Hidden -PassThru
    for ($elapsed = 0; $elapsed -lt 5000; $elapsed += 25) {
        $process.Refresh()
        $palette = $process.MainWindowHandle
        if ($palette -ne [IntPtr]::Zero) { break }
        Start-Sleep -Milliseconds 25
    }
    if ($palette -eq [IntPtr]::Zero) { throw 'Palette did not start.' }
    # The palette HWND can exist a few milliseconds before Controller finishes
    # constructing the pre-created zoom windows. Wait for that dependency before
    # invoking the same action as Ctrl+Shift+Z.
    for ($elapsed = 0; $elapsed -lt 5000; $elapsed += 25) {
        $zoom = [ElitePenZoomCaptureQa]::FindForProcess(
            'ElitePen.Zoom', 'Zoom — Elite Pen', [uint32]$process.Id)
        if ($zoom -ne [IntPtr]::Zero) { break }
        Start-Sleep -Milliseconds 25
    }
    if ($zoom -eq [IntPtr]::Zero) { throw 'Zoom module did not initialize.' }
    Start-Sleep -Milliseconds 250
    $null = [ElitePenZoomCaptureQa]::SetForegroundWindow($palette)
    $null = [ElitePenZoomCaptureQa]::SetCursorPos(500, 420)
    $null = [ElitePenZoomCaptureQa]::SendMessage(
        $palette, 0x0312, [IntPtr]7, [IntPtr]::Zero)
    for ($elapsed = 0; $elapsed -lt 3000; $elapsed += 25) {
        if ([ElitePenZoomCaptureQa]::IsWindowVisible($zoom)) { break }
        Start-Sleep -Milliseconds 25
    }
    if ($zoom -eq [IntPtr]::Zero -or
        -not [ElitePenZoomCaptureQa]::IsWindowVisible($zoom)) {
        throw 'Zoom did not activate visibly.'
    }
    $null = [ElitePenZoomCaptureQa]::SendMessage(
        $zoom, 0x0100, [IntPtr][char]'F', [IntPtr]::Zero)
    Start-Sleep -Milliseconds 350

    [uint32]$zoomAffinity = 999
    if (-not [ElitePenZoomCaptureQa]::GetWindowDisplayAffinity(
        $zoom, [ref]$zoomAffinity)) {
        throw 'Windows could not report the zoom capture affinity.'
    }
    $ink = [ElitePenZoomCaptureQa]::FindForProcess(
        'ElitePen.ZoomInk', 'Anotaciones de zoom — Elite Pen', [uint32]$process.Id)
    [uint32]$inkAffinity = 999
    if ($ink -eq [IntPtr]::Zero -or
        -not [ElitePenZoomCaptureQa]::GetWindowDisplayAffinity(
            $ink, [ref]$inkAffinity)) {
        throw 'Windows could not report the zoom annotation capture affinity.'
    }
    if ($zoomAffinity -ne 0 -or $inkAffinity -ne 0) {
        throw "Zoom is still excluded from capture (zoom=$zoomAffinity; ink=$inkAffinity)."
    }

    $bounds = New-Object ElitePenZoomCaptureQa+RECT
    $null = [ElitePenZoomCaptureQa]::GetWindowRect($zoom, [ref]$bounds)
    $width = $bounds.Right - $bounds.Left
    $height = $bounds.Bottom - $bounds.Top
    if ($width -lt 200 -or $height -lt 200) {
        throw "Fullscreen zoom has invalid capture bounds: ${width}x${height}."
    }
    $bitmap = [Drawing.Bitmap]::new($width, $height)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.CopyFromScreen(
            $bounds.Left, $bounds.Top, 0, 0, $bitmap.Size,
            [Drawing.CopyPixelOperation]::SourceCopy)
        $samples = 0
        $nonBlack = 0
        for ($y = 12; $y -lt $height; $y += [Math]::Max(16, [Math]::Floor($height / 40))) {
            for ($x = 12; $x -lt $width; $x += [Math]::Max(16, [Math]::Floor($width / 40))) {
                $pixel = $bitmap.GetPixel($x, $y)
                $samples++
                if ($pixel.R -gt 8 -or $pixel.G -gt 8 -or $pixel.B -gt 8) { $nonBlack++ }
            }
        }
        if ($samples -lt 100) { throw "Zoom capture produced only $samples samples." }
        $ratio = $nonBlack / $samples
        if ($ratio -lt 0.03) {
            throw "Captured fullscreen zoom is effectively black (non-black ratio=$ratio)."
        }
    } finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
    Write-Output ("Elite Pen zoom capture: passed " +
        "(WDA_NONE; non-black ratio=$([Math]::Round($ratio, 3)))")
} finally {
    if ($process) {
        if (-not $process.HasExited) {
            $null = $process.CloseMainWindow()
            if (-not $process.WaitForExit(1500)) {
                $process.Kill()
                $process.WaitForExit(1500) | Out-Null
            }
        }
        $process.Dispose()
    }
    $env:ELITE_PEN_QA_INSTANCE_ID = $previousInstance
    if (Test-Path -LiteralPath $qaSandbox) {
        Remove-Item -LiteralPath $qaSandbox -Recurse -Force -ErrorAction SilentlyContinue
    }
}
