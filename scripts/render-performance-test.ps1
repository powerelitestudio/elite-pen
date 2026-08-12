[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [int]$StrokeCount = 5000,
    [int]$FrameCount = 240
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$executable = Join-Path $repoRoot "build\$($Configuration.ToLowerInvariant())\Elite Pen.exe"
if (-not (Test-Path -LiteralPath $executable)) { throw "Missing executable: $executable" }
if (Get-Process -Name 'Elite Pen' -ErrorAction SilentlyContinue) {
    throw 'Close Elite Pen before running the render performance test.'
}

$sandbox = Join-Path ([IO.Path]::GetFullPath([IO.Path]::GetTempPath())) `
    ("elite-pen-render-qa-" + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $sandbox | Out-Null
Copy-Item -LiteralPath $executable -Destination (Join-Path $sandbox 'Elite Pen.exe')
Set-Content -LiteralPath (Join-Path $sandbox 'portable.flag') `
    -Value 'Elite Pen isolated render QA' -Encoding ascii -NoNewline

Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class ElitePenRenderNative {
    [StructLayout(LayoutKind.Sequential)] public struct RECT {
        public int Left, Top, Right, Bottom;
    }
    public delegate bool WindowCallback(IntPtr window, IntPtr data);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)]
    public static extern IntPtr FindWindow(string className, string title);
    [DllImport("user32.dll")]
    public static extern bool EnumWindows(WindowCallback callback, IntPtr data);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)]
    public static extern int GetClassName(IntPtr window, StringBuilder value, int count);
    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr window, out RECT rectangle);
    [DllImport("user32.dll")]
    public static extern IntPtr SendMessage(IntPtr window, uint message,
        IntPtr wparam, IntPtr lparam);
    [DllImport("user32.dll")]
    public static extern bool PostMessage(IntPtr window, uint message,
        IntPtr wparam, IntPtr lparam);
    [DllImport("user32.dll")]
    public static extern bool RedrawWindow(IntPtr window, IntPtr update,
        IntPtr region, uint flags);
    public static IntPtr PrimaryOverlay() {
        IntPtr found = IntPtr.Zero;
        EnumWindows((window, data) => {
            var name = new StringBuilder(128);
            RECT bounds;
            GetClassName(window, name, name.Capacity);
            if (name.ToString() == "ElitePen.Overlay" &&
                GetWindowRect(window, out bounds) &&
                bounds.Left <= 300 && bounds.Right > 300 &&
                bounds.Top <= 300 && bounds.Bottom > 300) {
                found = window;
                return false;
            }
            return true;
        }, IntPtr.Zero);
        return found;
    }
}
'@

function New-LParam([int]$X, [int]$Y) {
    return [IntPtr](($Y -shl 16) -bor ($X -band 0xffff))
}

$process = $null
try {
    $process = Start-Process -FilePath (Join-Path $sandbox 'Elite Pen.exe') `
        -WindowStyle Hidden -PassThru
    $palette = [IntPtr]::Zero
    for ($attempt = 0; $attempt -lt 100 -and $palette -eq [IntPtr]::Zero; $attempt++) {
        Start-Sleep -Milliseconds 50
        $palette = [ElitePenRenderNative]::FindWindow('ElitePen.Palette', 'Elite Pen')
    }
    if ($palette -eq [IntPtr]::Zero) { throw 'Palette did not start.' }
    $overlay = [ElitePenRenderNative]::PrimaryOverlay()
    if ($overlay -eq [IntPtr]::Zero) { throw 'Primary drawing overlay is missing.' }

    $populate = [Diagnostics.Stopwatch]::StartNew()
    $stored = [ElitePenRenderNative]::SendMessage(
        $palette, 0x8069, [IntPtr]$StrokeCount, [IntPtr]::Zero).ToInt64()
    $populate.Stop()
    if ($stored -ne $StrokeCount) {
        throw "Stress document stored $stored of $StrokeCount strokes."
    }

    $redrawFlags = 0x0001 -bor 0x0020 -bor 0x0100
    $cold = [Diagnostics.Stopwatch]::StartNew()
    $null = [ElitePenRenderNative]::RedrawWindow(
        $overlay, [IntPtr]::Zero, [IntPtr]::Zero, $redrawFlags)
    $cold.Stop()

    $warm = [Diagnostics.Stopwatch]::StartNew()
    for ($frame = 0; $frame -lt $FrameCount; $frame++) {
        $null = [ElitePenRenderNative]::RedrawWindow(
            $overlay, [IntPtr]::Zero, [IntPtr]::Zero, $redrawFlags)
    }
    $warm.Stop()

    $bounds = New-Object ElitePenRenderNative+RECT
    $null = [ElitePenRenderNative]::GetWindowRect($overlay, [ref]$bounds)
    $clientWidth = $bounds.Right - $bounds.Left
    $clientHeight = $bounds.Bottom - $bounds.Top
    $startX = [Math]::Max(220, [Math]::Floor($clientWidth * 0.18))
    $startY = [Math]::Max(180, [Math]::Floor($clientHeight * 0.72))
    $null = [ElitePenRenderNative]::SendMessage(
        $overlay, 0x0201, [IntPtr]1, (New-LParam $startX $startY))
    $live = [Diagnostics.Stopwatch]::StartNew()
    for ($frame = 1; $frame -le $FrameCount; $frame++) {
        $x = $startX + ($frame % 420)
        $y = $startY + [int]([Math]::Sin($frame * 0.12) * 70)
        $null = [ElitePenRenderNative]::SendMessage(
            $overlay, 0x0200, [IntPtr]1, (New-LParam $x $y))
        $null = [ElitePenRenderNative]::RedrawWindow(
            $overlay, [IntPtr]::Zero, [IntPtr]::Zero, $redrawFlags)
    }
    $live.Stop()
    $null = [ElitePenRenderNative]::SendMessage(
        $overlay, 0x0202, [IntPtr]::Zero, (New-LParam ($startX + 420) $startY))

    # Editable zoom has a separate sparse source-space cache. Populate the same
    # 5,000-object workload, build it on the first LÁPIZ entry, then measure MANO
    # with its ink surface hidden and a warm return to LÁPIZ.
    $null = [ElitePenRenderNative]::SendMessage(
        $palette, 0x0312, [IntPtr]7, [IntPtr]::Zero)
    $zoom = [IntPtr]::Zero
    for ($attempt = 0; $attempt -lt 100 -and $zoom -eq [IntPtr]::Zero; $attempt++) {
        Start-Sleep -Milliseconds 20
        $zoom = [ElitePenRenderNative]::FindWindow('ElitePen.Zoom', 'Zoom — Elite Pen')
    }
    if ($zoom -eq [IntPtr]::Zero) { throw 'Zoom did not start for source-space performance QA.' }
    $null = [ElitePenRenderNative]::SendMessage(
        $zoom, 0x0100, [IntPtr][char]'E', [IntPtr]::Zero)
    $zoomInk = [IntPtr]::Zero
    for ($attempt = 0; $attempt -lt 100 -and $zoomInk -eq [IntPtr]::Zero; $attempt++) {
        Start-Sleep -Milliseconds 20
        $zoomInk = [ElitePenRenderNative]::FindWindow(
            'ElitePen.ZoomInk', 'Anotaciones de zoom — Elite Pen')
    }
    if ($zoomInk -eq [IntPtr]::Zero) { throw 'Editable zoom ink surface is missing.' }

    $editPopulate = [Diagnostics.Stopwatch]::StartNew()
    $editStored = [ElitePenRenderNative]::SendMessage(
        $zoom, 0x8075, [IntPtr]$StrokeCount, [IntPtr]::Zero).ToInt64()
    $editPopulate.Stop()
    if ($editStored -ne $StrokeCount) {
        throw "Editable zoom stored $editStored of $StrokeCount strokes."
    }
    $editCold = [Diagnostics.Stopwatch]::StartNew()
    $editAnnotateState = [ElitePenRenderNative]::SendMessage(
        $zoom, 0x8074, [IntPtr]2, [IntPtr]::Zero).ToInt64()
    $editCold.Stop()
    if ($editAnnotateState -ne 2) {
        throw 'Editable zoom could not enter LÁPIZ for its cold cache measurement.'
    }
    $null = [ElitePenRenderNative]::SendMessage(
        $zoom, 0x8074, [IntPtr]1, [IntPtr]::Zero)
    $editPan = [Diagnostics.Stopwatch]::StartNew()
    for ($frame = 0; $frame -lt $FrameCount; $frame++) {
        $dx = if (($frame % 80) -lt 40) { 2 } else { -2 }
        $dy = if (($frame % 120) -lt 60) { 1 } else { -1 }
        $null = [ElitePenRenderNative]::SendMessage(
            $zoom, 0x8078, [IntPtr]$dx, [IntPtr]$dy)
        $null = [ElitePenRenderNative]::RedrawWindow(
            $zoomInk, [IntPtr]::Zero, [IntPtr]::Zero, $redrawFlags)
    }
    $editPan.Stop()
    $editWarm = [Diagnostics.Stopwatch]::StartNew()
    $editAnnotateState = [ElitePenRenderNative]::SendMessage(
        $zoom, 0x8074, [IntPtr]2, [IntPtr]::Zero).ToInt64()
    $editWarm.Stop()
    if ($editAnnotateState -ne 2) {
        throw 'Editable zoom could not return to LÁPIZ after MANO navigation.'
    }

    $process.Refresh()
    $metrics = [ordered]@{
        strokes = $StrokeCount
        frames = $FrameCount
        populateMs = [Math]::Round($populate.Elapsed.TotalMilliseconds, 3)
        coldRenderMs = [Math]::Round($cold.Elapsed.TotalMilliseconds, 3)
        cachedFrameMeanMs = [Math]::Round($warm.Elapsed.TotalMilliseconds / $FrameCount, 3)
        liveFrameMeanMs = [Math]::Round($live.Elapsed.TotalMilliseconds / $FrameCount, 3)
        editPopulateMs = [Math]::Round($editPopulate.Elapsed.TotalMilliseconds, 3)
        editFirstPencilMs = [Math]::Round($editCold.Elapsed.TotalMilliseconds, 3)
        editHandFrameMeanMs = [Math]::Round($editPan.Elapsed.TotalMilliseconds / $FrameCount, 3)
        editWarmPencilMs = [Math]::Round($editWarm.Elapsed.TotalMilliseconds, 3)
        workingSetMiB = [Math]::Round($process.WorkingSet64 / 1MB, 2)
    }
    [pscustomobject]$metrics

    if ($metrics.populateMs -gt 1500) { throw 'Stress document population exceeded 1500 ms.' }
    if ($metrics.coldRenderMs -gt 3000) { throw 'Cold GPU cache build exceeded 3000 ms.' }
    if ($metrics.cachedFrameMeanMs -gt 8) { throw 'Cached frame mean exceeded 8 ms.' }
    if ($metrics.liveFrameMeanMs -gt 12) { throw 'Live stroke frame mean exceeded 12 ms.' }
    if ($metrics.editPopulateMs -gt 1500) { throw 'Editable zoom population exceeded 1500 ms.' }
    if ($metrics.editFirstPencilMs -gt 3000) { throw 'Editable zoom first LÁPIZ entry exceeded 3000 ms.' }
    if ($metrics.editHandFrameMeanMs -gt 8) { throw 'Editable zoom MANO frame mean exceeded 8 ms.' }
    if ($metrics.editWarmPencilMs -gt 1000) { throw 'Editable zoom warm LÁPIZ entry exceeded 1000 ms.' }
    if ($metrics.workingSetMiB -gt 350) { throw 'Working set exceeded 350 MiB.' }
} finally {
    $palette = [ElitePenRenderNative]::FindWindow('ElitePen.Palette', 'Elite Pen')
    if ($palette -ne [IntPtr]::Zero) {
        $null = [ElitePenRenderNative]::PostMessage(
            $palette, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
    }
    if ($process) {
        $process.WaitForExit(3000) | Out-Null
        if (-not $process.HasExited) {
            $process.Kill()
            $process.WaitForExit(3000) | Out-Null
        }
        $process.Dispose()
    }
    if (Test-Path -LiteralPath $sandbox) {
        for ($attempt = 0; $attempt -lt 40; $attempt++) {
            try {
                Remove-Item -LiteralPath $sandbox -Recurse -Force
                break
            } catch {
                if ($attempt -eq 39) {
                    Write-Warning "Render QA completed, but its temporary directory is still locked: $sandbox"
                    break
                }
                Start-Sleep -Milliseconds 250
            }
        }
    }
}

Write-Output 'Elite Pen render performance: all budgets passed'
