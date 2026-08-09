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
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
    public delegate bool WindowCallback(IntPtr window, IntPtr data);
    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr context);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string className, string title);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr window, StringBuilder value, int count);
    [DllImport("user32.dll")] public static extern bool EnumWindows(WindowCallback callback, IntPtr data);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr window, uint message, IntPtr wparam, IntPtr lparam);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr window, uint message, IntPtr wparam, IntPtr lparam);
    [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr window, int id);
    [DllImport("user32.dll")] public static extern int GetWindowLong(IntPtr window, int index);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr window, out RECT rectangle);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr window, IntPtr insertAfter,
        int x, int y, int width, int height, uint flags);
    [DllImport("user32.dll")] public static extern IntPtr WindowFromPoint(POINT point);
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
    public static bool IsAboveClass(IntPtr reference, string className) {
        bool referenceSeen = false;
        bool valid = true;
        EnumWindows((window, data) => {
            if (window == reference) {
                referenceSeen = true;
                return true;
            }
            var value = new StringBuilder(256);
            GetClassName(window, value, value.Capacity);
            if (value.ToString() == className && IsWindowVisible(window) && !referenceSeen) {
                valid = false;
                return false;
            }
            return true;
        }, IntPtr.Zero);
        return referenceSeen && valid;
    }
    public static IntPtr FindClassContainingPoint(string className, int x, int y) {
        IntPtr found = IntPtr.Zero;
        EnumWindows((window, data) => {
            var value = new StringBuilder(256);
            RECT rectangle;
            GetClassName(window, value, value.Capacity);
            if (value.ToString() == className && IsWindowVisible(window) &&
                GetWindowRect(window, out rectangle) && x >= rectangle.Left &&
                x < rectangle.Right && y >= rectangle.Top && y < rectangle.Bottom) {
                found = window;
                return false;
            }
            return true;
        }, IntPtr.Zero);
        return found;
    }
}
'@

$null = [ElitePenUiNative]::SetProcessDpiAwarenessContext([IntPtr](-4))
$failures = [System.Collections.Generic.List[string]]::new()
$paletteScale = 0.60
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

function Convert-PaletteCoordinate([int]$Value) {
    return [int][Math]::Round($Value * $script:paletteScale, [MidpointRounding]::AwayFromZero)
}

function Click-PaletteWindow([IntPtr]$Window, [int]$X, [int]$Y) {
    Click-Window $Window (Convert-PaletteCoordinate $X) (Convert-PaletteCoordinate $Y)
}

function Select-Tool([IntPtr]$Palette, [int]$Index) {
    Click-PaletteWindow $Palette 180 205
    $tools = Wait-Window 'ElitePen.Tools' 1000
    Assert-Ui ($tools -ne [IntPtr]::Zero -and [ElitePenUiNative]::IsWindowVisible($tools)) 'Tool panel did not open.'
    if ($tools -eq [IntPtr]::Zero) { return }
    $column = $Index % 2
    $row = [Math]::Floor($Index / 2)
    Click-Window $tools (96 + 169 * $column) (68 + 48 * $row)
}

function Click-ThroughOverlay([IntPtr]$Palette, [int]$X, [int]$Y) {
    $paletteRectangle = New-Object ElitePenUiNative+RECT
    $null = [ElitePenUiNative]::GetWindowRect($Palette, [ref]$paletteRectangle)
    $screenX = $paletteRectangle.Left + (Convert-PaletteCoordinate $X)
    $screenY = $paletteRectangle.Top + (Convert-PaletteCoordinate $Y)
    $underlay = [ElitePenUiNative]::FindClassContainingPoint('ElitePen.Overlay', $screenX, $screenY)
    Assert-Ui ($underlay -ne [IntPtr]::Zero) "No overlay was found beneath palette command $X,$Y."
    if ($underlay -eq [IntPtr]::Zero) { return }
    $underlayRectangle = New-Object ElitePenUiNative+RECT
    $null = [ElitePenUiNative]::GetWindowRect($underlay, [ref]$underlayRectangle)
    $localX = $screenX - $underlayRectangle.Left
    $localY = $screenY - $underlayRectangle.Top
    $parameter = [IntPtr](($localY -shl 16) -bor ($localX -band 0xffff))
    $null = [ElitePenUiNative]::SendMessage($underlay, 0x0201, [IntPtr]1, $parameter)
    $null = [ElitePenUiNative]::SendMessage($underlay, 0x0202, [IntPtr]0, $parameter)
    Start-Sleep -Milliseconds 80
}

$process = $null
try {
    $process = Start-Process -FilePath $executable -PassThru
    $palette = Wait-Window 'ElitePen.Palette'
    Assert-Ui ($palette -ne [IntPtr]::Zero) 'Palette window did not start.'
    if ($palette -eq [IntPtr]::Zero) { throw 'Palette unavailable; remaining UI checks cannot run.' }

    Assert-Ui ([ElitePenUiNative]::CountClass('ElitePen.Overlay') -ge 1) 'No monitor overlay was created.'

    $paletteBounds = New-Object ElitePenUiNative+RECT
    $null = [ElitePenUiNative]::GetWindowRect($palette, [ref]$paletteBounds)
    Assert-Ui (($paletteBounds.Right - $paletteBounds.Left) -eq 174) 'Palette width was not reduced by 40 percent.'
    Assert-Ui (($paletteBounds.Bottom - $paletteBounds.Top) -eq 168) 'Palette height was not reduced by 40 percent.'

    # Dragging uses screen-space pointer deltas. Client-space deltas feed the moved
    # window back into the calculation and cause the palette to shake or lag.
    $dragX = Convert-PaletteCoordinate 225
    $dragY = Convert-PaletteCoordinate 150
    $dragStart = [IntPtr](($dragY -shl 16) -bor ($dragX -band 0xffff))
    $null = [ElitePenUiNative]::SendMessage($palette, 0x0201, [IntPtr]1, $dragStart)
    $dragMove = [IntPtr]((($dragY + 25) -shl 16) -bor (($dragX + 40) -band 0xffff))
    $null = [ElitePenUiNative]::SendMessage($palette, 0x0200, [IntPtr]1, $dragMove)
    $firstDragBounds = New-Object ElitePenUiNative+RECT
    $null = [ElitePenUiNative]::GetWindowRect($palette, [ref]$firstDragBounds)
    Assert-Ui ($firstDragBounds.Left -eq $paletteBounds.Left + 40 -and
               $firstDragBounds.Top -eq $paletteBounds.Top + 25) `
        "Palette first drag expected $($paletteBounds.Left + 40),$($paletteBounds.Top + 25) but reached $($firstDragBounds.Left),$($firstDragBounds.Top)."
    $null = [ElitePenUiNative]::SendMessage($palette, 0x0200, [IntPtr]1, $dragMove)
    $secondDragBounds = New-Object ElitePenUiNative+RECT
    $null = [ElitePenUiNative]::GetWindowRect($palette, [ref]$secondDragBounds)
    Assert-Ui ($secondDragBounds.Left -eq $paletteBounds.Left + 80 -and
               $secondDragBounds.Top -eq $paletteBounds.Top + 50) `
        "Palette second drag expected $($paletteBounds.Left + 80),$($paletteBounds.Top + 50) but reached $($secondDragBounds.Left),$($secondDragBounds.Top)."
    $null = [ElitePenUiNative]::SetWindowPos($palette, [IntPtr]::Zero,
        $paletteBounds.Left, $paletteBounds.Top, 0, 0, 0x0015)
    $null = [ElitePenUiNative]::SendMessage($palette, 0x0202, [IntPtr]::Zero, $dragStart)

    # Six quick colors follow the compact visual order requested for 1.2.
    $quickColors = @(
        @{ X = 67;  Y = 107; Value = 4279769115 }, # black
        @{ X = 69;  Y = 60;  Value = 4294950445 }, # yellow
        @{ X = 119; Y = 29;  Value = 4280256741 }, # blue
        @{ X = 169; Y = 53;  Value = 4293870660 }, # red
        @{ X = 184; Y = 96;  Value = 4280468830 }, # green
        @{ X = 160; Y = 130; Value = 4287323382 }  # purple
    )
    foreach ($quickColor in $quickColors) {
        Click-PaletteWindow $palette $quickColor.X $quickColor.Y
        $activeColor = [ElitePenUiNative]::SendMessage($palette, 0x805B, [IntPtr]::Zero, [IntPtr]::Zero)
        Assert-Ui ($activeColor.ToInt64() -eq $quickColor.Value) "Quick color at $($quickColor.X),$($quickColor.Y) did not select its assigned color."
    }

    # Complete color panel and an actual color selection.
    Click-PaletteWindow $palette 113 139
    $colors = Wait-Window 'ElitePen.Colors' 1000
    Assert-Ui ($colors -ne [IntPtr]::Zero -and [ElitePenUiNative]::IsWindowVisible($colors)) 'Color panel did not open.'
    if ($colors -ne [IntPtr]::Zero) { Click-Window $colors 313 93 }
    $selectedColor = [ElitePenUiNative]::SendMessage($palette, 0x805B, [IntPtr]::Zero, [IntPtr]::Zero)
    Assert-Ui ($selectedColor.ToInt64() -eq 4280468830) 'Custom color selection did not update the active ink color.'

    # Thicknesses, visibility, cursor/pen tip and whiteboard toggles.
    Click-PaletteWindow $palette 23 93
    Click-ThroughOverlay $palette 126 83
    Click-ThroughOverlay $palette 126 83
    Click-ThroughOverlay $palette 62 151
    $tipTool = [ElitePenUiNative]::SendMessage($palette, 0x805A, [IntPtr]::Zero, [IntPtr]::Zero)
    Assert-Ui ($tipTool.ToInt64() -eq 0) 'Brush tip did not switch to the normal cursor.'
    Click-ThroughOverlay $palette 62 151
    $tipTool = [ElitePenUiNative]::SendMessage($palette, 0x805A, [IntPtr]::Zero, [IntPtr]::Zero)
    Assert-Ui ($tipTool.ToInt64() -eq 1) 'Brush tip did not switch back to the pen.'
    Click-ThroughOverlay $palette 100 170
    Click-ThroughOverlay $palette 100 170
    $ferruleX = Convert-PaletteCoordinate 100
    $ferruleY = Convert-PaletteCoordinate 170
    $ferrule = [IntPtr](($ferruleY -shl 16) -bor ($ferruleX -band 0xffff))
    $null = [ElitePenUiNative]::SendMessage($palette, 0x0205, [IntPtr]0, $ferrule)
    $null = [ElitePenUiNative]::SendMessage($palette, 0x0205, [IntPtr]0, $ferrule)

    # Palette remains physically above the drawing overlay while Pen is active.
    Assert-Ui ([ElitePenUiNative]::IsAboveClass($palette, 'ElitePen.Overlay')) 'Palette commands were covered by the drawing overlay.'

    # Reproduce the reported failure: send the red-color click through the drawing
    # overlay. The input router must consume it as a palette command, never as ink.
    Click-ThroughOverlay $palette 169 53
    $routedColor = [ElitePenUiNative]::SendMessage($palette, 0x805B, [IntPtr]::Zero, [IntPtr]::Zero)
    Assert-Ui ($routedColor.ToInt64() -eq 4293870660) 'Overlay click was painted instead of selecting red.'

    Click-ThroughOverlay $palette 23 122
    $routedThickness = [ElitePenUiNative]::SendMessage($palette, 0x805C, [IntPtr]::Zero, [IntPtr]::Zero)
    Assert-Ui ($routedThickness.ToInt64() -eq 200) 'Overlay click did not select the thickness command.'
    Click-ThroughOverlay $palette 23 69

    # The clean blue handle opens one complete panel, including Settings.
    Click-ThroughOverlay $palette 180 205
    $toolPanel = Wait-Window 'ElitePen.Tools' 1000
    Assert-Ui ($toolPanel -ne [IntPtr]::Zero -and [ElitePenUiNative]::IsWindowVisible($toolPanel)) 'Clean handle did not open the complete tool panel.'
    if ($toolPanel -ne [IntPtr]::Zero) {
        $toolPanelBounds = New-Object ElitePenUiNative+RECT
        $null = [ElitePenUiNative]::GetWindowRect($toolPanel, [ref]$toolPanelBounds)
        Assert-Ui (($toolPanelBounds.Bottom - $toolPanelBounds.Top) -eq 400) 'Complete tool panel did not expose its Settings row.'
        Click-Window $toolPanel 180 361
    }

    # Settings remains directly accessible through the handle panel.
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
        $defaultThickness = [ElitePenUiNative]::GetDlgItem($settings, 4010)
        $null = [ElitePenUiNative]::SendMessage($defaultThickness, 0x014E, [IntPtr]1, [IntPtr]::Zero)
        $null = [ElitePenUiNative]::SendMessage($settings, 0x0111, [IntPtr]0x00010FAA, $defaultThickness)
        $configuredThickness = [ElitePenUiNative]::SendMessage($palette, 0x805C, [IntPtr]::Zero, [IntPtr]::Zero)
        Assert-Ui ($configuredThickness.ToInt64() -eq 40) 'Settings did not configure the 4 px default thickness.'
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

    # Text uses a transparent, captionless editor directly at the insertion point.
    $itemsBeforeText = [ElitePenUiNative]::SendMessage($palette, 0x805E, [IntPtr]::Zero, [IntPtr]::Zero)
    Select-Tool $palette 9
    Click-Window $overlay 520 360
    $textWindow = Wait-Window 'ElitePen.TextInput' 1000
    Assert-Ui ($textWindow -ne [IntPtr]::Zero -and [ElitePenUiNative]::IsWindowVisible($textWindow)) 'Inline text editor did not appear at the insertion point.'
    if ($textWindow -ne [IntPtr]::Zero) {
        $style = [ElitePenUiNative]::GetWindowLong($textWindow, -16)
        Assert-Ui (($style -band 0x00C00000) -eq 0) 'Inline text unexpectedly has a dialog caption.'
        foreach ($character in 'Elite Pen QA'.ToCharArray()) {
            $null = [ElitePenUiNative]::SendMessage($textWindow, 0x0102, [IntPtr][int]$character, [IntPtr]::Zero)
        }
        $null = [ElitePenUiNative]::SendMessage($textWindow, 0x805D, [IntPtr]::Zero, [IntPtr]::Zero)
        Assert-Ui (-not [ElitePenUiNative]::IsWindowVisible($textWindow)) 'Inline text editor did not commit cleanly.'
        $itemsAfterText = [ElitePenUiNative]::SendMessage($palette, 0x805E, [IntPtr]::Zero, [IntPtr]::Zero)
        Assert-Ui ($itemsAfterText.ToInt64() -eq $itemsBeforeText.ToInt64() + 1) 'Inline text did not create a drawable.'
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
    Click-ThroughOverlay $palette 239 238
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
