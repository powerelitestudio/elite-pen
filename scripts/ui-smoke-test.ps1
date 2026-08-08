[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [string]$ExecutablePath,
    [switch]$RealDesktopCapture
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$executable = if ($ExecutablePath) { [IO.Path]::GetFullPath($ExecutablePath) } else {
    Join-Path $repoRoot "build\$($Configuration.ToLowerInvariant())\Elite Pen.exe"
}
if (-not (Test-Path -LiteralPath $executable)) { throw "Missing executable: $executable" }

Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class ElitePenUiNative {
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    public delegate bool WindowCallback(IntPtr window, IntPtr data);
    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr context);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string className, string title);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr window, StringBuilder value, int count);
    [DllImport("user32.dll")] public static extern bool EnumWindows(WindowCallback callback, IntPtr data);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr window, uint message, IntPtr wparam, IntPtr lparam);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr window, uint message, IntPtr wparam, IntPtr lparam);
    [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr window, int id);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr window, out RECT rectangle);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern bool SetWindowText(IntPtr window, string value);
    public static int CountClass(string className) {
        int count = 0;
        EnumWindows((window, data) => {
            var value = new StringBuilder(256);
            GetClassName(window, value, value.Capacity);
            if (value.ToString() == className) count++;
            return true;
        }, IntPtr.Zero);
        return count;
    }
}
'@

$null = [ElitePenUiNative]::SetProcessDpiAwarenessContext([IntPtr](-4))
$failures = [System.Collections.Generic.List[string]]::new()
$captureDirectory = Join-Path $repoRoot 'artifacts\qa\ui-captures'
if (Test-Path -LiteralPath $captureDirectory) {
    Remove-Item -LiteralPath $captureDirectory -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $captureDirectory | Out-Null
$previousCaptureDirectory = $env:ELITE_PEN_QA_CAPTURE_DIR
$previousSyntheticCapture = $env:ELITE_PEN_QA_SYNTHETIC_CAPTURE
$env:ELITE_PEN_QA_CAPTURE_DIR = $captureDirectory
if ($RealDesktopCapture) {
    Remove-Item Env:ELITE_PEN_QA_SYNTHETIC_CAPTURE -ErrorAction SilentlyContinue
} else {
    $env:ELITE_PEN_QA_SYNTHETIC_CAPTURE = '1'
}

function Assert-Ui([bool]$Condition, [string]$Message) {
    if (-not $Condition) { $script:failures.Add($Message) }
}

function Wait-Window([string]$ClassName, [int]$TimeoutMilliseconds = 5000) {
    $title = switch ($ClassName) {
        'ElitePen.Palette' { 'Elite Pen' }
        'ElitePen.Overlay' { 'Elite Pen Overlay' }
        'ElitePen.Colors' { 'Colores — Elite Pen' }
        'ElitePen.Tools' { 'Herramientas — Elite Pen' }
        'ElitePen.Settings' { 'Configuracion — Elite Pen' }
        'ElitePen.TextInput' { 'Insertar texto — Elite Pen' }
        'ElitePen.Zoom' { 'Zoom — Elite Pen' }
        default { $null }
    }
    $elapsed = 0
    while ($elapsed -lt $TimeoutMilliseconds) {
        $window = [ElitePenUiNative]::FindWindow($ClassName, $title)
        if ($window -ne [IntPtr]::Zero) { return $window }
        Start-Sleep -Milliseconds 50
        $elapsed += 50
    }
    return [IntPtr]::Zero
}

function Click-Window([IntPtr]$Window, [int]$X, [int]$Y) {
    $parameter = [IntPtr](($Y -shl 16) -bor ($X -band 0xffff))
    $null = [ElitePenUiNative]::SendMessage($Window, 0x0201, [IntPtr]1, $parameter)
    $null = [ElitePenUiNative]::SendMessage($Window, 0x0202, [IntPtr]0, $parameter)
    Start-Sleep -Milliseconds 80
}

function Select-Tool([IntPtr]$Palette, [int]$Index) {
    Click-Window $Palette 220 220
    $tools = Wait-Window 'ElitePen.Tools' 1000
    Assert-Ui ($tools -ne [IntPtr]::Zero -and [ElitePenUiNative]::IsWindowVisible($tools)) 'Tool panel did not open.'
    if ($tools -eq [IntPtr]::Zero) { return }
    $column = $Index % 2
    $row = [Math]::Floor($Index / 2)
    Click-Window $tools (96 + 169 * $column) (68 + 48 * $row)
}

$process = $null
try {
    $process = Start-Process -FilePath $executable -PassThru
    $palette = Wait-Window 'ElitePen.Palette'
    Assert-Ui ($palette -ne [IntPtr]::Zero) 'Palette window did not start.'
    if ($palette -eq [IntPtr]::Zero) { throw 'Palette unavailable; remaining UI checks cannot run.' }

    Assert-Ui ([ElitePenUiNative]::CountClass('ElitePen.Overlay') -ge 1) 'No monitor overlay was created.'

    # Complete color panel and an actual color selection.
    Click-Window $palette 68 108
    $colors = Wait-Window 'ElitePen.Colors' 1000
    Assert-Ui ($colors -ne [IntPtr]::Zero -and [ElitePenUiNative]::IsWindowVisible($colors)) 'Color panel did not open.'
    if ($colors -ne [IntPtr]::Zero) { Click-Window $colors 313 93 }
    $selectedColor = [ElitePenUiNative]::SendMessage($palette, 0x805B, [IntPtr]::Zero, [IntPtr]::Zero)
    Assert-Ui ($selectedColor.ToInt64() -eq 4280468830) 'Custom color selection did not update the active ink color.'

    # Thicknesses, visibility, cursor/pen tip and whiteboard toggles.
    Click-Window $palette 23 93
    Click-Window $palette 126 83
    Click-Window $palette 126 83
    Click-Window $palette 62 151
    $tipTool = [ElitePenUiNative]::SendMessage($palette, 0x805A, [IntPtr]::Zero, [IntPtr]::Zero)
    Assert-Ui ($tipTool.ToInt64() -eq 0) 'Brush tip did not switch to the normal cursor.'
    Click-Window $palette 62 151
    $tipTool = [ElitePenUiNative]::SendMessage($palette, 0x805A, [IntPtr]::Zero, [IntPtr]::Zero)
    Assert-Ui ($tipTool.ToInt64() -eq 1) 'Brush tip did not switch back to the pen.'
    Click-Window $palette 100 170
    Click-Window $palette 100 170
    $ferrule = [IntPtr]((170 -shl 16) -bor 100)
    $null = [ElitePenUiNative]::SendMessage($palette, 0x0205, [IntPtr]0, $ferrule)
    $null = [ElitePenUiNative]::SendMessage($palette, 0x0205, [IntPtr]0, $ferrule)

    # Settings is a real window and can close without ending the application.
    Click-Window $palette 123 30
    $settings = Wait-Window 'ElitePen.Settings' 1000
    Assert-Ui ($settings -ne [IntPtr]::Zero -and [ElitePenUiNative]::IsWindowVisible($settings)) 'Settings window did not open.'
    if ($settings -ne [IntPtr]::Zero) {
        $highlight = [ElitePenUiNative]::GetDlgItem($settings, 4008)
        $null = [ElitePenUiNative]::SendMessage($highlight, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)
        $fade = [ElitePenUiNative]::GetDlgItem($settings, 4009)
        $null = [ElitePenUiNative]::SendMessage($fade, 0x014E, [IntPtr]1, [IntPtr]::Zero)
        $null = [ElitePenUiNative]::SendMessage($settings, 0x0111, [IntPtr]0x00010FA9, $fade)
        $null = [ElitePenUiNative]::SendMessage($fade, 0x014E, [IntPtr]0, [IntPtr]::Zero)
        $null = [ElitePenUiNative]::SendMessage($settings, 0x0111, [IntPtr]0x00010FA9, $fade)
        $null = [ElitePenUiNative]::SendMessage($highlight, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)
        $null = [ElitePenUiNative]::SendMessage($settings, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
    }

    $overlay = Wait-Window 'ElitePen.Overlay'
    Assert-Ui ($overlay -ne [IntPtr]::Zero) 'Overlay disappeared during UI test.'

    # Pen, highlighter, line, rectangle, ellipse, straight and curved arrows.
    foreach ($toolIndex in @(1, 2, 4, 5, 6, 7, 8)) {
        Select-Tool $palette $toolIndex
        Click-Window $overlay (260 + 5 * $toolIndex) (260 + 3 * $toolIndex)
        $start = [IntPtr](((260 + 3 * $toolIndex) -shl 16) -bor ((260 + 5 * $toolIndex) -band 0xffff))
        $finish = [IntPtr](((340 + 3 * $toolIndex) -shl 16) -bor ((390 + 5 * $toolIndex) -band 0xffff))
        $null = [ElitePenUiNative]::SendMessage($overlay, 0x0201, [IntPtr]1, $start)
        $null = [ElitePenUiNative]::SendMessage($overlay, 0x0200, [IntPtr]1, $finish)
        $null = [ElitePenUiNative]::SendMessage($overlay, 0x0202, [IntPtr]0, $finish)
    }

    # Text opens the IME-compatible editor and commits a real string.
    Select-Tool $palette 9
    Click-Window $overlay 520 360
    $textWindow = Wait-Window 'ElitePen.TextInput' 1000
    Assert-Ui ($textWindow -ne [IntPtr]::Zero -and [ElitePenUiNative]::IsWindowVisible($textWindow)) 'Text editor did not open.'
    if ($textWindow -ne [IntPtr]::Zero) {
        $edit = [ElitePenUiNative]::GetDlgItem($textWindow, 3001)
        $null = [ElitePenUiNative]::SetWindowText($edit, 'Elite Pen QA')
        $null = [ElitePenUiNative]::SendMessage($textWindow, 0x0111, [IntPtr]1, [IntPtr]::Zero)
    }

    # Region capture writes PNG to the isolated QA folder and copies a bitmap.
    Select-Tool $palette 10
    $selectedTool = [ElitePenUiNative]::SendMessage($palette, 0x805A, [IntPtr]::Zero, [IntPtr]::Zero)
    Assert-Ui ($selectedTool.ToInt64() -eq 10) "Screenshot selection resolved to tool $($selectedTool.ToInt64()) instead of 10."
    $captureStart = [IntPtr]((420 -shl 16) -bor 520)
    $captureFinish = [IntPtr]((520 -shl 16) -bor 700)
    $null = [ElitePenUiNative]::SendMessage($overlay, 0x0201, [IntPtr]1, $captureStart)
    $null = [ElitePenUiNative]::SendMessage($overlay, 0x0200, [IntPtr]1, $captureFinish)
    $null = [ElitePenUiNative]::SendMessage($overlay, 0x0202, [IntPtr]0, $captureFinish)
    Start-Sleep -Milliseconds 300
    Assert-Ui ((Get-ChildItem -LiteralPath $captureDirectory -Filter '*.png' -File).Count -ge 1) 'Screenshot tool did not create a PNG.'

    # Eraser gesture, undo, redo and undoable clear.
    Select-Tool $palette 3
    Click-Window $overlay 390 340
    $null = [ElitePenUiNative]::SendMessage($palette, 0x0312, [IntPtr]4, [IntPtr]::Zero)
    $null = [ElitePenUiNative]::SendMessage($palette, 0x0312, [IntPtr]5, [IntPtr]::Zero)
    Click-Window $palette 315 269
    $null = [ElitePenUiNative]::SendMessage($palette, 0x0312, [IntPtr]4, [IntPtr]::Zero)
    $null = [ElitePenUiNative]::SendMessage($palette, 0x0312, [IntPtr]5, [IntPtr]::Zero)

    # Native zoom enters and leaves cleanly.
    Select-Tool $palette 11
    $zoom = Wait-Window 'ElitePen.Zoom' 1000
    Assert-Ui ($zoom -ne [IntPtr]::Zero -and [ElitePenUiNative]::IsWindowVisible($zoom)) 'Zoom window did not activate.'
    if ($zoom -ne [IntPtr]::Zero) {
        $null = [ElitePenUiNative]::SendMessage($zoom, 0x0100, [IntPtr][char]'F', [IntPtr]::Zero)
        Start-Sleep -Milliseconds 100
        $full = New-Object ElitePenUiNative+RECT
        $null = [ElitePenUiNative]::GetWindowRect($zoom, [ref]$full)
        $null = [ElitePenUiNative]::SendMessage($zoom, 0x0100, [IntPtr][char]'L', [IntPtr]::Zero)
        Start-Sleep -Milliseconds 100
        $lens = New-Object ElitePenUiNative+RECT
        $null = [ElitePenUiNative]::GetWindowRect($zoom, [ref]$lens)
        Assert-Ui (($lens.Right - $lens.Left) -lt ($full.Right - $full.Left)) 'Lens zoom did not use a compact window.'
        $null = [ElitePenUiNative]::SendMessage($zoom, 0x0100, [IntPtr][char]'D', [IntPtr]::Zero)
        $null = [ElitePenUiNative]::SendMessage($zoom, 0x0100, [IntPtr][char]'I', [IntPtr]::Zero)
        $null = [ElitePenUiNative]::SendMessage($zoom, 0x0100, [IntPtr][char]'0', [IntPtr]::Zero)
        $null = [ElitePenUiNative]::SendMessage($zoom, 0x0100, [IntPtr][char]'0', [IntPtr]::Zero)
        $null = [ElitePenUiNative]::SendMessage($zoom, 0x0100, [IntPtr][char]'F', [IntPtr]::Zero)
    }
    $null = [ElitePenUiNative]::SendMessage($palette, 0x0312, [IntPtr]7, [IntPtr]::Zero)

    Assert-Ui (-not $process.HasExited) 'Application exited unexpectedly during UI checks.'
} finally {
    $palette = [ElitePenUiNative]::FindWindow('ElitePen.Palette', 'Elite Pen')
    if ($palette -ne [IntPtr]::Zero) {
        $null = [ElitePenUiNative]::PostMessage($palette, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
    }
    if ($process) {
        $process.WaitForExit(3000) | Out-Null
        if (-not $process.HasExited) { $process.Kill() }
        $process.Dispose()
    }
    $env:ELITE_PEN_QA_CAPTURE_DIR = $previousCaptureDirectory
    $env:ELITE_PEN_QA_SYNTHETIC_CAPTURE = $previousSyntheticCapture
}

if ($failures.Count -gt 0) {
    $failures | ForEach-Object { Write-Error $_ }
    exit 1
}
Write-Output 'Elite Pen UI smoke test: all checks passed'
