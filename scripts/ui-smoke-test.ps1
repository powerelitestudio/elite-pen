[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [string]$ExecutablePath,
    [switch]$RealDesktopCapture
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$sourceExecutable = if ($ExecutablePath) { [IO.Path]::GetFullPath($ExecutablePath) } else {
    Join-Path $repoRoot "build\$($Configuration.ToLowerInvariant())\Elite Pen.exe"
}
if (-not (Test-Path -LiteralPath $sourceExecutable)) {
    throw "Missing executable: $sourceExecutable"
}

# UI QA must never read or mutate the user's installed/portable preferences.
# Run the exact binary from a fresh portable sandbox and discard all state.
$qaSandbox = Join-Path ([IO.Path]::GetFullPath([IO.Path]::GetTempPath())) `
    ("elite-pen-ui-qa-" + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $qaSandbox | Out-Null
$executable = Join-Path $qaSandbox 'Elite Pen.exe'
Copy-Item -LiteralPath $sourceExecutable -Destination $executable
Set-Content -LiteralPath (Join-Path $qaSandbox 'portable.flag') `
    -Value 'Elite Pen isolated UI QA' -Encoding ascii -NoNewline

Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class ElitePenUiNative {
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
    [StructLayout(LayoutKind.Sequential)] public struct ICONINFO {
        public bool IsIcon; public uint HotspotX, HotspotY; public IntPtr Mask, Color;
    }
    [StructLayout(LayoutKind.Sequential)] public struct BITMAP {
        public int Type, Width, Height, WidthBytes;
        public ushort Planes, BitsPerPixel;
        public IntPtr Bits;
    }
    public delegate bool WindowCallback(IntPtr window, IntPtr data);
    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr context);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string className, string title);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindowEx(
        IntPtr parent, IntPtr childAfter, string className, string title);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr window, StringBuilder value, int count);
    [DllImport("user32.dll")] public static extern bool EnumWindows(WindowCallback callback, IntPtr data);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")] public static extern bool IsWindowEnabled(IntPtr window);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(
        IntPtr window, out uint processId);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr window, uint message, IntPtr wparam, IntPtr lparam);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr window, uint message, IntPtr wparam, IntPtr lparam);
    [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr window, int id);
    [DllImport("user32.dll")] public static extern int GetWindowLong(IntPtr window, int index);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr window, out RECT rectangle);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr window, IntPtr insertAfter,
        int x, int y, int width, int height, uint flags);
    [DllImport("user32.dll")] public static extern IntPtr WindowFromPoint(POINT point);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern bool GetCursorPos(out POINT point);
    [DllImport("user32.dll")] public static extern void mouse_event(
        uint flags, uint dx, uint dy, uint data, UIntPtr extraInfo);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern bool SetWindowText(IntPtr window, string value);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(
        IntPtr window, StringBuilder value, int count);
    [DllImport("user32.dll")] public static extern IntPtr LoadCursor(IntPtr instance, IntPtr name);
    [DllImport("user32.dll")] public static extern IntPtr GetCursor();
    [DllImport("user32.dll")] public static extern bool GetIconInfo(IntPtr cursor, out ICONINFO information);
    [DllImport("gdi32.dll", EntryPoint="GetObjectW")] public static extern int GetBitmapObject(
        IntPtr bitmap, int size, out BITMAP information);
    [DllImport("gdi32.dll")] public static extern bool DeleteObject(IntPtr value);
    [DllImport("user32.dll")] public static extern bool OpenClipboard(IntPtr owner);
    [DllImport("user32.dll")] public static extern bool CloseClipboard();
    [DllImport("user32.dll")] public static extern IntPtr GetClipboardData(uint format);
    [DllImport("kernel32.dll")] public static extern IntPtr GlobalLock(IntPtr memory);
    [DllImport("kernel32.dll")] public static extern bool GlobalUnlock(IntPtr memory);
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
    public static string WindowText(IntPtr window) {
        var value = new StringBuilder(2048);
        GetWindowText(window, value, value.Capacity);
        return value.ToString();
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

function Save-WindowImage([IntPtr]$Window, [string]$Name) {
    $bounds = New-Object ElitePenUiNative+RECT
    if (-not [ElitePenUiNative]::GetWindowRect($Window, [ref]$bounds)) { return }
    $width = $bounds.Right - $bounds.Left
    $height = $bounds.Bottom - $bounds.Top
    if ($width -le 0 -or $height -le 0) { return }
    Add-Type -AssemblyName System.Drawing
    $bitmap = New-Object System.Drawing.Bitmap($width, $height)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        try {
            $graphics.CopyFromScreen($bounds.Left, $bounds.Top, 0, 0, $bitmap.Size)
            $bitmap.Save((Join-Path $script:captureDirectory $Name),
                         [System.Drawing.Imaging.ImageFormat]::Png)
        } catch {
            # A locked or disconnected Windows desktop can reject diagnostic
            # screenshots even though the UI automation remains available.
            Write-Warning "Optional UI screenshot '$Name' was unavailable: $($_.Exception.Message)"
        }
    } finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
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
        'ElitePen.ZoomInk' { 'Anotaciones de zoom — Elite Pen' }
        'ElitePen.ZoomTarget' { 'Objetivo de lupa — Elite Pen' }
        'ElitePen.ZoomEditToolbar' { 'Zoom editable — Navegar — Elite Pen' }
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
    $process = Start-Process -FilePath $executable -WindowStyle Hidden -PassThru
    $palette = Wait-Window 'ElitePen.Palette'
    Assert-Ui ($palette -ne [IntPtr]::Zero) 'Palette window did not start.'
    if ($palette -eq [IntPtr]::Zero) { throw 'Palette unavailable; remaining UI checks cannot run.' }

    Assert-Ui ([ElitePenUiNative]::CountClass('ElitePen.Overlay') -ge 1) 'No monitor overlay was created.'
    $defaultTheme = [ElitePenUiNative]::SendMessage(
        $palette, 0x8068, [IntPtr]::Zero, [IntPtr]::Zero)
    Assert-Ui ($defaultTheme.ToInt64() -eq 1) `
        'A fresh Elite Pen session did not start in the light appearance.'
    $globalHotkeys = [ElitePenUiNative]::SendMessage(
        $palette, 0x806C, [IntPtr]::Zero, [IntPtr]::Zero)
    Assert-Ui ($globalHotkeys.ToInt64() -eq 1) `
        'The new default global shortcut set could not be registered.'

    # Freehand drawing uses a native DPI-aware pencil, never the generic crosshair.
    $drawingOverlay = Wait-Window 'ElitePen.Overlay'
    $drawingCursor = [ElitePenUiNative]::SendMessage(
        $drawingOverlay, 0x805F, [IntPtr]::Zero, [IntPtr]::Zero)
    $crosshairCursor = [ElitePenUiNative]::LoadCursor([IntPtr]::Zero, [IntPtr]32515)
    Assert-Ui ($drawingCursor -ne [IntPtr]::Zero -and $drawingCursor -ne $crosshairCursor) `
        'Pen mode still uses the generic crosshair instead of the pencil cursor.'
    if ($drawingCursor -ne [IntPtr]::Zero) {
        $cursorIcon = New-Object ElitePenUiNative+ICONINFO
        $cursorInfoAvailable = [ElitePenUiNative]::GetIconInfo($drawingCursor, [ref]$cursorIcon)
        Assert-Ui $cursorInfoAvailable 'Pencil cursor bitmap could not be inspected.'
        if ($cursorInfoAvailable) {
            try {
                $cursorBitmap = New-Object ElitePenUiNative+BITMAP
                $bitmapAvailable = [ElitePenUiNative]::GetBitmapObject(
                    $cursorIcon.Color,
                    [Runtime.InteropServices.Marshal]::SizeOf($cursorBitmap),
                    [ref]$cursorBitmap) -ne 0
                Assert-Ui $bitmapAvailable 'Pencil cursor has no color bitmap.'
                if ($bitmapAvailable) {
                    Assert-Ui ($cursorBitmap.Width -ge 32 -and $cursorBitmap.Height -ge 32) `
                        'Pencil cursor is not large enough for a crisp native rendering.'
                    Assert-Ui ($cursorIcon.HotspotX -le [Math]::Floor($cursorBitmap.Width / 4) -and
                               $cursorIcon.HotspotY -ge [Math]::Floor($cursorBitmap.Height * 3 / 4)) `
                        'Pencil cursor hotspot is not anchored to its graphite tip.'
                }
            } finally {
                if ($cursorIcon.Mask -ne [IntPtr]::Zero) {
                    $null = [ElitePenUiNative]::DeleteObject($cursorIcon.Mask)
                }
                if ($cursorIcon.Color -ne [IntPtr]::Zero) {
                    $null = [ElitePenUiNative]::DeleteObject($cursorIcon.Color)
                }
            }
        }
    }

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

    # Direct global color actions behind Ctrl+Shift+1..6.
    for ($colorIndex = 0; $colorIndex -lt $quickColors.Count; $colorIndex++) {
        $hotkeyId = 24 + $colorIndex
        $null = [ElitePenUiNative]::SendMessage(
            $palette, 0x0312, [IntPtr]$hotkeyId, [IntPtr]::Zero)
        $activeColor = [ElitePenUiNative]::SendMessage(
            $palette, 0x805B, [IntPtr]::Zero, [IntPtr]::Zero)
        Assert-Ui ($activeColor.ToInt64() -eq $quickColors[$colorIndex].Value) `
            "Direct color hotkey $hotkeyId selected the wrong color."
    }

    # Both Ctrl+Shift+7 and Ctrl+Shift++ open the complete color selector.
    foreach ($colorPanelHotkeyId in @(19, 30)) {
        $null = [ElitePenUiNative]::SendMessage(
            $palette, 0x0312, [IntPtr]$colorPanelHotkeyId, [IntPtr]::Zero)
        $hotkeyColors = Wait-Window 'ElitePen.Colors' 1000
        Assert-Ui ($hotkeyColors -ne [IntPtr]::Zero -and
                   [ElitePenUiNative]::IsWindowVisible($hotkeyColors)) `
            "Color panel hotkey $colorPanelHotkeyId did not open the selector."
        $null = [ElitePenUiNative]::SendMessage(
            $palette, 0x0312, [IntPtr]$colorPanelHotkeyId, [IntPtr]::Zero)
    }

    # Ctrl+Shift+wheel advances exactly one visual thickness on product surfaces.
    $wheelHook = [ElitePenUiNative]::SendMessage(
        $palette, 0x806B, [IntPtr]::Zero, [IntPtr]::Zero)
    Assert-Ui ($wheelHook.ToInt64() -eq 1) 'Thickness wheel route is unavailable.'
    Click-PaletteWindow $palette 23 49
    $null = [ElitePenUiNative]::SendMessage($palette, 0x806A, [IntPtr]1, [IntPtr]::Zero)
    $wheelThickness = [ElitePenUiNative]::SendMessage(
        $palette, 0x805C, [IntPtr]::Zero, [IntPtr]::Zero)
    Assert-Ui ($wheelThickness.ToInt64() -eq 70) 'Wheel-up did not advance thickness from 4 to 7 px.'
    $null = [ElitePenUiNative]::SendMessage($palette, 0x806A, [IntPtr]0, [IntPtr]::Zero)
    $wheelThickness = [ElitePenUiNative]::SendMessage(
        $palette, 0x805C, [IntPtr]::Zero, [IntPtr]::Zero)
    Assert-Ui ($wheelThickness.ToInt64() -eq 40) 'Wheel-down did not return thickness from 7 to 4 px.'

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
        $settingsBounds = New-Object ElitePenUiNative+RECT
        $null = [ElitePenUiNative]::GetWindowRect($settings, [ref]$settingsBounds)
        Assert-Ui (($settingsBounds.Bottom - $settingsBounds.Top) -eq 590) 'Tabbed Settings window has an unexpected height.'
        $generalTab = [ElitePenUiNative]::GetDlgItem($settings, 4101)
        $shortcutsTab = [ElitePenUiNative]::GetDlgItem($settings, 4102)
        $helpTab = [ElitePenUiNative]::GetDlgItem($settings, 4104)
        $shortcutGuide = [ElitePenUiNative]::GetDlgItem($settings, 4103)
        $helpPanel = [ElitePenUiNative]::GetDlgItem($settings, 4105)
        $helpWebsite = [ElitePenUiNative]::GetDlgItem($settings, 4500)
        $helpSource = [ElitePenUiNative]::GetDlgItem($settings, 4501)
        Assert-Ui ($generalTab -ne [IntPtr]::Zero -and $shortcutsTab -ne [IntPtr]::Zero -and
                   $helpTab -ne [IntPtr]::Zero -and $helpPanel -ne [IntPtr]::Zero -and
                   $helpWebsite -ne [IntPtr]::Zero -and $helpSource -ne [IntPtr]::Zero -and
                   $shortcutGuide -ne [IntPtr]::Zero) 'Settings tabs or shortcut guide are missing.'
        $null = [ElitePenUiNative]::SendMessage($shortcutsTab, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)
        Assert-Ui ([ElitePenUiNative]::IsWindowVisible($shortcutGuide)) 'Shortcuts tab did not reveal the complete guide.'
        Start-Sleep -Milliseconds 120
        Save-WindowImage $settings 'settings-shortcuts.png'
        $shortcutAccessibleText = [ElitePenUiNative]::WindowText($shortcutGuide)
        Assert-Ui ($shortcutAccessibleText.Contains('Shift Línea') -and
                   $shortcutAccessibleText.Contains('Ctrl Rectángulo') -and
                   $shortcutAccessibleText.Contains('Tab Elipse') -and
                   $shortcutAccessibleText.Contains('Ctrl+Shift Flecha') -and
                   $shortcutAccessibleText.Contains('Shift+Tab Flecha curva') -and
                   $shortcutAccessibleText.Contains('E Zoom editable')) `
            'Shortcut guide does not document pencil gestures and editable zoom.'
        $firstHotkey = [ElitePenUiNative]::GetDlgItem($settings, 4200)
        $lastHotkey = [ElitePenUiNative]::GetDlgItem($settings, 4205)
        $firstHotkeyEditor = [ElitePenUiNative]::GetDlgItem($settings, 4400)
        $lastHotkeyEditor = [ElitePenUiNative]::GetDlgItem($settings, 4405)
        $shortcutScrollbar = [ElitePenUiNative]::GetDlgItem($settings, 4408)
        $resetHotkeys = [ElitePenUiNative]::GetDlgItem($settings, 4300)
        Assert-Ui ($firstHotkey -ne [IntPtr]::Zero -and $lastHotkey -ne [IntPtr]::Zero -and
                   $firstHotkeyEditor -ne [IntPtr]::Zero -and
                   $lastHotkeyEditor -ne [IntPtr]::Zero -and
                   $shortcutScrollbar -ne [IntPtr]::Zero -and
                   $resetHotkeys -ne [IntPtr]::Zero -and
                   [ElitePenUiNative]::IsWindowVisible($firstHotkey) -and
                   [ElitePenUiNative]::IsWindowVisible($lastHotkey) -and
                   [ElitePenUiNative]::IsWindowVisible($firstHotkeyEditor) -and
                   [ElitePenUiNative]::IsWindowVisible($shortcutScrollbar)) `
            'Scrollable shortcut rows or their pencil editors are missing from Settings.'
        $null = [ElitePenUiNative]::SendMessage($settings, 0x0115, [IntPtr]7, $shortcutScrollbar)
        Assert-Ui ([ElitePenUiNative]::IsWindowVisible($firstHotkeyEditor) -and
                   [ElitePenUiNative]::WindowText($lastHotkey) -eq 'E') `
            'Shortcut list did not expose the configurable E action for editable zoom.'
        $null = [ElitePenUiNative]::SendMessage($helpTab, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)
        Assert-Ui ([ElitePenUiNative]::IsWindowVisible($helpPanel) -and
                   [ElitePenUiNative]::IsWindowVisible($helpWebsite) -and
                   [ElitePenUiNative]::IsWindowVisible($helpSource) -and
                   -not [ElitePenUiNative]::IsWindowVisible($shortcutGuide)) `
            'Help tab did not expose its product information and official website action.'
        $helpAccessibleText = [ElitePenUiNative]::WindowText($helpPanel)
        Assert-Ui ($helpAccessibleText.Contains('Elite Pen 2.8.0') -and
                   $helpAccessibleText.Contains('Apache License 2.0') -and
                   $helpAccessibleText.Contains('Power Elite Studio')) `
            'Help tab is missing the version, open-source license, or developer identity.'
        Start-Sleep -Milliseconds 120
        Save-WindowImage $settings 'settings-help.png'
        $null = [ElitePenUiNative]::SendMessage($generalTab, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)
        Assert-Ui (-not [ElitePenUiNative]::IsWindowVisible($shortcutGuide)) 'General tab did not hide the shortcut guide.'
        Assert-Ui (-not [ElitePenUiNative]::IsWindowVisible($helpPanel)) `
            'General tab did not hide the Help content.'
        Assert-Ui (-not [ElitePenUiNative]::IsWindowVisible($helpSource)) `
            'General tab did not hide the source repository action.'

        $darkTheme = [ElitePenUiNative]::GetDlgItem($settings, 4012)
        $lightTheme = [ElitePenUiNative]::GetDlgItem($settings, 4013)
        Assert-Ui ($darkTheme -ne [IntPtr]::Zero -and $lightTheme -ne [IntPtr]::Zero) `
            'Dark and light appearance choices are missing.'
        $null = [ElitePenUiNative]::SendMessage($lightTheme, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)
        $activeTheme = [ElitePenUiNative]::SendMessage($palette, 0x8068, [IntPtr]::Zero, [IntPtr]::Zero)
        Assert-Ui ($activeTheme.ToInt64() -eq 1) 'Light appearance was not applied to the product surfaces.'
        $null = [ElitePenUiNative]::SendMessage($darkTheme, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)
        $activeTheme = [ElitePenUiNative]::SendMessage($palette, 0x8068, [IntPtr]::Zero, [IntPtr]::Zero)
        Assert-Ui ($activeTheme.ToInt64() -eq 0) 'Dark appearance was not restored after theme switching.'
        $null = [ElitePenUiNative]::SendMessage($lightTheme, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)

        # Paleta is the default presentation. Lineal reuses the same command
        # engine, resizes in place and can return live without restarting.
        $controlMode = [ElitePenUiNative]::GetDlgItem($settings, 4014)
        Assert-Ui ($controlMode -ne [IntPtr]::Zero) 'Presentation selector is missing.'
        $defaultMode = [ElitePenUiNative]::SendMessage(
            $palette, 0x8070, [IntPtr]::Zero, [IntPtr]::Zero)
        Assert-Ui ($defaultMode.ToInt64() -eq 0) 'Painter palette is not the default presentation.'
        $null = [ElitePenUiNative]::SendMessage($controlMode, 0x014E, [IntPtr]1, [IntPtr]::Zero)
        $null = [ElitePenUiNative]::SendMessage($settings, 0x0111, [IntPtr]0x00010FAE, $controlMode)
        $linearMode = [ElitePenUiNative]::SendMessage(
            $palette, 0x8070, [IntPtr]::Zero, [IntPtr]::Zero)
        $linearBounds = New-Object ElitePenUiNative+RECT
        $null = [ElitePenUiNative]::GetWindowRect($palette, [ref]$linearBounds)
        Assert-Ui ($linearMode.ToInt64() -eq 1 -and
                   ($linearBounds.Right - $linearBounds.Left) -eq 46 -and
                   ($linearBounds.Bottom - $linearBounds.Top) -eq 406) `
            'Lineal mode did not apply its compact vertical geometry.'
        Click-PaletteWindow $palette 56 634
        $linearColor = [ElitePenUiNative]::SendMessage(
            $palette, 0x805B, [IntPtr]::Zero, [IntPtr]::Zero)
        Assert-Ui ($linearColor.ToInt64() -eq 4287323382) `
            'Lineal mode did not route its purple swatch through the shared color engine.'
        Click-PaletteWindow $palette 49 571
        $linearThickness = [ElitePenUiNative]::SendMessage(
            $palette, 0x805C, [IntPtr]::Zero, [IntPtr]::Zero)
        Assert-Ui ($linearThickness.ToInt64() -eq 120) `
            'Lineal mode did not route its thickness controls through the shared engine.'
        $null = [ElitePenUiNative]::SendMessage($palette, 0x0312, [IntPtr]23, [IntPtr]::Zero)
        $linearCollapsedBounds = New-Object ElitePenUiNative+RECT
        $null = [ElitePenUiNative]::GetWindowRect($palette, [ref]$linearCollapsedBounds)
        Assert-Ui (($linearCollapsedBounds.Right - $linearCollapsedBounds.Left) -eq 46 -and
                   ($linearCollapsedBounds.Bottom - $linearCollapsedBounds.Top) -eq 49) `
            'Lineal hibernation did not collapse to its compact Elite pill.'
        $null = [ElitePenUiNative]::SendMessage($palette, 0x0312, [IntPtr]23, [IntPtr]::Zero)
        $null = [ElitePenUiNative]::SendMessage($controlMode, 0x014E, [IntPtr]0, [IntPtr]::Zero)
        $null = [ElitePenUiNative]::SendMessage($settings, 0x0111, [IntPtr]0x00010FAE, $controlMode)
        $paletteMode = [ElitePenUiNative]::SendMessage(
            $palette, 0x8070, [IntPtr]::Zero, [IntPtr]::Zero)
        Assert-Ui ($paletteMode.ToInt64() -eq 0) `
            'Live presentation switch did not restore the painter palette.'

        $paletteSize = [ElitePenUiNative]::GetDlgItem($settings, 4011)
        Assert-Ui ($paletteSize -ne [IntPtr]::Zero) 'Whole-unit size selector is missing.'
        $null = [ElitePenUiNative]::SendMessage($paletteSize, 0x014E, [IntPtr]0, [IntPtr]::Zero)
        $null = [ElitePenUiNative]::SendMessage($settings, 0x0111, [IntPtr]0x00010FAB, $paletteSize)
        $script:paletteScale = 0.48
        $compactBounds = New-Object ElitePenUiNative+RECT
        $null = [ElitePenUiNative]::GetWindowRect($palette, [ref]$compactBounds)
        Assert-Ui (($compactBounds.Right - $compactBounds.Left) -eq 139 -and
                   ($compactBounds.Bottom - $compactBounds.Top) -eq 134) 'Compact size did not scale the complete unit to 80 percent.'
        Click-PaletteWindow $palette 169 53
        $compactColor = [ElitePenUiNative]::SendMessage($palette, 0x805B, [IntPtr]::Zero, [IntPtr]::Zero)
        Assert-Ui ($compactColor.ToInt64() -eq 4293870660) 'Compact palette did not scale its color hit zones.'
        $null = [ElitePenUiNative]::SendMessage($paletteSize, 0x014E, [IntPtr]2, [IntPtr]::Zero)
        $null = [ElitePenUiNative]::SendMessage($settings, 0x0111, [IntPtr]0x00010FAB, $paletteSize)
        $script:paletteScale = 0.75
        $largeBounds = New-Object ElitePenUiNative+RECT
        $null = [ElitePenUiNative]::GetWindowRect($palette, [ref]$largeBounds)
        Assert-Ui (($largeBounds.Right - $largeBounds.Left) -eq 218 -and
                   ($largeBounds.Bottom - $largeBounds.Top) -eq 210) 'Large size did not scale the complete unit to 125 percent.'
        Click-PaletteWindow $palette 23 122
        $largeThickness = [ElitePenUiNative]::SendMessage($palette, 0x805C, [IntPtr]::Zero, [IntPtr]::Zero)
        Assert-Ui ($largeThickness.ToInt64() -eq 200) 'Large palette did not scale its thickness hit zones.'
        $null = [ElitePenUiNative]::SendMessage($paletteSize, 0x014E, [IntPtr]3, [IntPtr]::Zero)
        $null = [ElitePenUiNative]::SendMessage($settings, 0x0111, [IntPtr]0x00010FAB, $paletteSize)
        $script:paletteScale = 0.90
        $veryLargeBounds = New-Object ElitePenUiNative+RECT
        $null = [ElitePenUiNative]::GetWindowRect($palette, [ref]$veryLargeBounds)
        Assert-Ui (($veryLargeBounds.Right - $veryLargeBounds.Left) -eq 261 -and
                   ($veryLargeBounds.Bottom - $veryLargeBounds.Top) -eq 252) 'Very large size did not scale the complete unit to 150 percent.'
        Click-PaletteWindow $palette 69 60
        $veryLargeColor = [ElitePenUiNative]::SendMessage($palette, 0x805B, [IntPtr]::Zero, [IntPtr]::Zero)
        Assert-Ui ($veryLargeColor.ToInt64() -eq 4294950445) 'Very large palette did not scale its color hit zones.'
        $null = [ElitePenUiNative]::SendMessage($paletteSize, 0x014E, [IntPtr]1, [IntPtr]::Zero)
        $null = [ElitePenUiNative]::SendMessage($settings, 0x0111, [IntPtr]0x00010FAB, $paletteSize)
        $script:paletteScale = 0.60
        $standardBounds = New-Object ElitePenUiNative+RECT
        $null = [ElitePenUiNative]::GetWindowRect($palette, [ref]$standardBounds)
        Assert-Ui (($standardBounds.Right - $standardBounds.Left) -eq 174 -and
                   ($standardBounds.Bottom - $standardBounds.Top) -eq 168) 'Standard size did not restore the current 100 percent dimensions.'

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

    # Ctrl+Shift+D toggles the whole unit in either direction.
    $null = [ElitePenUiNative]::SendMessage($palette, 0x0312, [IntPtr]23, [IntPtr]::Zero)
    $shortcutCollapsed = [ElitePenUiNative]::SendMessage(
        $palette, 0x8060, [IntPtr]::Zero, [IntPtr]::Zero)
    $null = [ElitePenUiNative]::SendMessage($palette, 0x0312, [IntPtr]23, [IntPtr]::Zero)
    $shortcutExpanded = [ElitePenUiNative]::SendMessage(
        $palette, 0x8060, [IntPtr]::Zero, [IntPtr]::Zero)
    Assert-Ui ($shortcutCollapsed.ToInt64() -eq 1 -and
               $shortcutExpanded.ToInt64() -eq 0) `
        'Collapse shortcut did not toggle Elite Pen in both directions.'

    # Hibernation leaves a small expansion target while the rest of the mini
    # palette remains a draggable surface.
    Click-PaletteWindow $palette 126 113
    $collapsed = [ElitePenUiNative]::SendMessage($palette, 0x8060, [IntPtr]::Zero, [IntPtr]::Zero)
    $collapsedBounds = New-Object ElitePenUiNative+RECT
    $null = [ElitePenUiNative]::GetWindowRect($palette, [ref]$collapsedBounds)
    Assert-Ui ($collapsed.ToInt64() -eq 1) 'Palette collapse control did not enter hibernation.'
    Assert-Ui (($collapsedBounds.Right - $collapsedBounds.Left) -eq 52 -and
               ($collapsedBounds.Bottom - $collapsedBounds.Top) -eq 50) `
        'Hibernated palette is not 70 percent smaller than the standard unit.'
    Click-Window $palette 4 4
    $stillCollapsed = [ElitePenUiNative]::SendMessage($palette, 0x8060, [IntPtr]::Zero, [IntPtr]::Zero)
    Assert-Ui ($stillCollapsed.ToInt64() -eq 1) `
        'Clicking outside the compact expand icon expanded the palette unexpectedly.'
    $dragStart = [IntPtr]((4 -shl 16) -bor 4)
    $dragMove = [IntPtr]((16 -shl 16) -bor 24)
    $null = [ElitePenUiNative]::SendMessage($palette, 0x0201, [IntPtr]1, $dragStart)
    $null = [ElitePenUiNative]::SendMessage($palette, 0x0200, [IntPtr]1, $dragMove)
    $null = [ElitePenUiNative]::SendMessage($palette, 0x0202, [IntPtr]0, $dragMove)
    $movedBounds = New-Object ElitePenUiNative+RECT
    $null = [ElitePenUiNative]::GetWindowRect($palette, [ref]$movedBounds)
    Assert-Ui ($movedBounds.Left -ne $collapsedBounds.Left -or
               $movedBounds.Top -ne $collapsedBounds.Top) `
        'The compact palette could not be repositioned from its non-icon area.'
    Click-Window $palette 27 23
    $expanded = [ElitePenUiNative]::SendMessage($palette, 0x8060, [IntPtr]::Zero, [IntPtr]::Zero)
    $expandedBounds = New-Object ElitePenUiNative+RECT
    $null = [ElitePenUiNative]::GetWindowRect($palette, [ref]$expandedBounds)
    Assert-Ui ($expanded.ToInt64() -eq 0 -and
               ($expandedBounds.Right - $expandedBounds.Left) -eq 174 -and
               ($expandedBounds.Bottom - $expandedBounds.Top) -eq 168) `
        'Expansion did not restore the complete standard palette.'

    # Escape exits both board modes without discarding annotations.
    $boardOverlay = Wait-Window 'ElitePen.Overlay'
    $null = [ElitePenUiNative]::SendMessage($palette, 0x0312, [IntPtr]3, [IntPtr]::Zero)
    $whiteboardMode = [ElitePenUiNative]::SendMessage($palette, 0x8063, [IntPtr]::Zero, [IntPtr]::Zero)
    Assert-Ui ($whiteboardMode.ToInt64() -eq 1) 'Whiteboard shortcut did not enter whiteboard mode.'
    $boardStart = [IntPtr]((300 -shl 16) -bor 360)
    $boardFinish = [IntPtr]((380 -shl 16) -bor 520)
    $null = [ElitePenUiNative]::SendMessage($boardOverlay, 0x0201, [IntPtr]1, $boardStart)
    $null = [ElitePenUiNative]::SendMessage($boardOverlay, 0x0200, [IntPtr]1, $boardFinish)
    $null = [ElitePenUiNative]::SendMessage($boardOverlay, 0x0202, [IntPtr]0, $boardFinish)
    Start-Sleep -Milliseconds 80
    Assert-Ui ([ElitePenUiNative]::IsWindowVisible($palette) -and
               [ElitePenUiNative]::IsAboveClass($palette, 'ElitePen.Overlay')) `
        'Whiteboard covered the palette after completing its first stroke.'
    $whiteboardPaletteBounds = New-Object ElitePenUiNative+RECT
    $null = [ElitePenUiNative]::GetWindowRect($palette, [ref]$whiteboardPaletteBounds)
    $whiteboardPaletteProbe = New-Object ElitePenUiNative+POINT
    $whiteboardPaletteProbe.X = [Math]::Floor(
        ($whiteboardPaletteBounds.Left + $whiteboardPaletteBounds.Right) / 2)
    $whiteboardPaletteProbe.Y = [Math]::Floor(
        ($whiteboardPaletteBounds.Top + $whiteboardPaletteBounds.Bottom) / 2)
    $whiteboardPaletteAtPoint = [ElitePenUiNative]::WindowFromPoint($whiteboardPaletteProbe)
    $whiteboardPaletteClass = New-Object System.Text.StringBuilder 128
    $null = [ElitePenUiNative]::GetClassName(
        $whiteboardPaletteAtPoint, $whiteboardPaletteClass, 128)
    Assert-Ui ($whiteboardPaletteClass.ToString() -eq 'ElitePen.Palette') `
        'Whiteboard intercepted input over the palette after the first stroke.'
    $null = [ElitePenUiNative]::SendMessage($boardOverlay, 0x0100, [IntPtr]27, [IntPtr]::Zero)
    $boardAfterEscape = [ElitePenUiNative]::SendMessage($palette, 0x8063, [IntPtr]::Zero, [IntPtr]::Zero)
    Assert-Ui ($boardAfterEscape.ToInt64() -eq 0) 'Escape did not leave whiteboard mode.'
    $null = [ElitePenUiNative]::SendMessage($palette, 0x0312, [IntPtr]8, [IntPtr]::Zero)
    $blackboardMode = [ElitePenUiNative]::SendMessage($palette, 0x8063, [IntPtr]::Zero, [IntPtr]::Zero)
    Assert-Ui ($blackboardMode.ToInt64() -eq 2) 'Blackboard shortcut did not enter blackboard mode.'
    $null = [ElitePenUiNative]::SendMessage($boardOverlay, 0x0100, [IntPtr]27, [IntPtr]::Zero)
    $boardAfterEscape = [ElitePenUiNative]::SendMessage($palette, 0x8063, [IntPtr]::Zero, [IntPtr]::Zero)
    Assert-Ui ($boardAfterEscape.ToInt64() -eq 0) 'Escape did not leave blackboard mode.'

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
        $extendedStyle = [ElitePenUiNative]::GetWindowLong($textWindow, -20)
        Assert-Ui (($extendedStyle -band 0x00200000) -ne 0 -and
                   ($extendedStyle -band 0x00080000) -eq 0) `
            'Inline text editor still uses the opaque layered-window backing surface.'
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

    # Native zoom freezes with P, draws on an independent document, preserves
    # that document on resume and scopes clear/undo/redo to the zoom session.
    # Exercise the exact global action behind Ctrl+Shift+Z.
    $null = [ElitePenUiNative]::SendMessage($palette, 0x0312, [IntPtr]7, [IntPtr]::Zero)
    $zoom = Wait-Window 'ElitePen.Zoom' 1000
    Assert-Ui ($zoom -ne [IntPtr]::Zero -and [ElitePenUiNative]::IsWindowVisible($zoom)) 'Zoom window did not activate.'
    if ($zoom -ne [IntPtr]::Zero) {
        $zoomInk = Wait-Window 'ElitePen.ZoomInk' 1000
        Assert-Ui ($zoomInk -ne [IntPtr]::Zero -and
                   -not [ElitePenUiNative]::IsWindowVisible($zoomInk)) `
            'Live zoom exposed the annotation layer before freezing.'
        $null = [ElitePenUiNative]::SendMessage($zoom, 0x0100, [IntPtr][char]'F', [IntPtr]::Zero)
        Start-Sleep -Milliseconds 100
        $full = New-Object ElitePenUiNative+RECT
        $null = [ElitePenUiNative]::GetWindowRect($zoom, [ref]$full)
        $fullView = [ElitePenUiNative]::SendMessage(
            $zoom, 0x806D, [IntPtr]::Zero, [IntPtr]::Zero)
        Assert-Ui ($fullView.ToInt64() -eq 0) 'F did not select fullscreen zoom.'
        $null = [ElitePenUiNative]::SendMessage($zoom, 0x0100, [IntPtr][char]'L', [IntPtr]::Zero)
        Start-Sleep -Milliseconds 100
        $lens = New-Object ElitePenUiNative+RECT
        $null = [ElitePenUiNative]::GetWindowRect($zoom, [ref]$lens)
        $lensView = [ElitePenUiNative]::SendMessage(
            $zoom, 0x806D, [IntPtr]::Zero, [IntPtr]::Zero)
        $lensGeometryWidth = [ElitePenUiNative]::SendMessage(
            $zoom, 0x806E, [IntPtr]::Zero, [IntPtr]::Zero).ToInt64()
        Assert-Ui ($lensView.ToInt64() -eq 1) 'L did not select lens zoom.'
        Assert-Ui (($lens.Right - $lens.Left) -lt ($full.Right - $full.Left)) `
            "Lens zoom did not use a compact window (full $($full.Right - $full.Left) px; lens $($lens.Right - $lens.Left) px; internal $lensGeometryWidth px)."
        $zoomTarget = Wait-Window 'ElitePen.ZoomTarget' 1000
        Assert-Ui ($zoomTarget -ne [IntPtr]::Zero -and
                   [ElitePenUiNative]::IsWindowVisible($zoomTarget)) `
            'Lens mode did not keep its focus target visible.'
        if ($zoomTarget -ne [IntPtr]::Zero) {
            $targetBefore = New-Object ElitePenUiNative+RECT
            $null = [ElitePenUiNative]::GetWindowRect($zoomTarget, [ref]$targetBefore)
            $beforeFocusX = $targetBefore.Left + [Math]::Round(
                ($targetBefore.Right - $targetBefore.Left) * 0.43)
            $beforeFocusY = $targetBefore.Top + [Math]::Round(
                ($targetBefore.Bottom - $targetBefore.Top) * 0.43)
            $moveX = if ($beforeFocusX -lt (($full.Left + $full.Right) / 2)) {
                $full.Right - 420
            } else { $full.Left + 420 }
            $moveY = if ($beforeFocusY -lt (($full.Top + $full.Bottom) / 2)) {
                $full.Bottom - 320
            } else { $full.Top + 320 }
            $cursorMoved = [ElitePenUiNative]::SetCursorPos($moveX, $moveY)
            # Exercise the same 16 ms refresh deterministically. Windows can
            # coalesce WM_TIMER while an elevated desktop QA process is polling.
            $null = [ElitePenUiNative]::SendMessage(
                $zoom, 0x0113, [IntPtr]1, [IntPtr]::Zero)
            Start-Sleep -Milliseconds 120
            $actualCursor = New-Object ElitePenUiNative+POINT
            $null = [ElitePenUiNative]::GetCursorPos([ref]$actualCursor)
            $targetAfter = New-Object ElitePenUiNative+RECT
            $null = [ElitePenUiNative]::GetWindowRect($zoomTarget, [ref]$targetAfter)
            Assert-Ui ([ElitePenUiNative]::IsWindowVisible($zoomTarget)) `
                'Lens focus target disappeared while the pointer moved.'
            Assert-Ui ([ElitePenUiNative]::IsAboveClass($palette, 'ElitePen.ZoomTarget')) `
                'Lens focus target covered the palette while following the pointer.'
            if ($cursorMoved -and $actualCursor.X -eq $moveX -and
                $actualCursor.Y -eq $moveY) {
                $movementMessage = "Lens focus target did not follow pointer at " +
                    "$($actualCursor.X),$($actualCursor.Y); target remained at " +
                    "$($targetAfter.Left),$($targetAfter.Top)."
                Assert-Ui ($targetAfter.Left -ne $targetBefore.Left -or
                           $targetAfter.Top -ne $targetBefore.Top) `
                    $movementMessage
                $targetFocusX = $targetAfter.Left + [Math]::Round(
                    ($targetAfter.Right - $targetAfter.Left) * 0.43)
                $targetFocusY = $targetAfter.Top + [Math]::Round(
                    ($targetAfter.Bottom - $targetAfter.Top) * 0.43)
                $sourceFocusX = [ElitePenUiNative]::SendMessage(
                    $zoom, 0x8066, [IntPtr]::Zero, [IntPtr]::Zero).ToInt64()
                $sourceFocusY = [ElitePenUiNative]::SendMessage(
                    $zoom, 0x8067, [IntPtr]::Zero, [IntPtr]::Zero).ToInt64()
                Assert-Ui ([Math]::Abs($targetFocusX - $sourceFocusX) -le 2 -and
                           [Math]::Abs($targetFocusY - $sourceFocusY) -le 2) `
                    'Lens target was not synchronized with the actual magnified source center.'
            }
        }
        $lensCursor = [ElitePenUiNative]::GetCursor()
        $arrowCursor = [ElitePenUiNative]::LoadCursor([IntPtr]::Zero, [IntPtr]32512)
        Assert-Ui ($lensCursor -ne [IntPtr]::Zero -and $lensCursor -ne $arrowCursor) `
            'Lens mode did not activate its dedicated magnifying target cursor.'
        $null = [ElitePenUiNative]::SendMessage($zoom, 0x0100, [IntPtr][char]'D', [IntPtr]::Zero)
        $null = [ElitePenUiNative]::SendMessage($zoom, 0x0100, [IntPtr][char]'I', [IntPtr]::Zero)
        $null = [ElitePenUiNative]::SendMessage($zoom, 0x0100, [IntPtr][char]'0', [IntPtr]::Zero)
        $null = [ElitePenUiNative]::SendMessage($zoom, 0x0100, [IntPtr][char]'0', [IntPtr]::Zero)
        $null = [ElitePenUiNative]::SendMessage($zoom, 0x0100, [IntPtr][char]'F', [IntPtr]::Zero)
        $magnifier = [ElitePenUiNative]::FindWindowEx(
            $zoom, [IntPtr]::Zero, 'Magnifier', 'Elite Pen Magnifier')
        Assert-Ui ($magnifier -ne [IntPtr]::Zero) 'Native Magnifier child is missing.'

        # E adds a non-destructive editable zoom workflow. Its document is
        # stored in source-space, while P and the existing F/L/D behavior remain
        # independent and are exercised again below.
        $null = [ElitePenUiNative]::SendMessage(
            $zoom, 0x0100, [IntPtr][char]'E', [IntPtr]::Zero)
        Start-Sleep -Milliseconds 120
        $editToolbar = Wait-Window 'ElitePen.ZoomEditToolbar' 1000
        $editState = [ElitePenUiNative]::SendMessage(
            $zoom, 0x8072, [IntPtr]::Zero, [IntPtr]::Zero).ToInt64()
        $editInkStyle = [ElitePenUiNative]::GetWindowLong($zoomInk, -20)
        $editZoomStyle = [ElitePenUiNative]::GetWindowLong($zoom, -20)
        $editMagnifierStyle = [ElitePenUiNative]::GetWindowLong($magnifier, -20)
        $editForeground = [ElitePenUiNative]::GetForegroundWindow()
        [uint32]$editForegroundProcess = 0
        $null = [ElitePenUiNative]::GetWindowThreadProcessId(
            $editForeground, [ref]$editForegroundProcess)
        Assert-Ui ($editState -eq 1 -and $editToolbar -ne [IntPtr]::Zero -and
                   [ElitePenUiNative]::IsWindowVisible($editToolbar) -and
                   -not [ElitePenUiNative]::IsWindowVisible($zoomInk) -and
                   -not [ElitePenUiNative]::IsWindowEnabled($magnifier) -and
                   (($editZoomStyle -band 0x20) -ne 0) -and
                   (($editZoomStyle -band 0x08000000) -ne 0) -and
                   (($editMagnifierStyle -band 0x20) -ne 0) -and
                   $editForegroundProcess -ne [uint32]$process.Id -and
                   (($editInkStyle -band 0x20) -ne 0) -and
                   (($editInkStyle -band 0x00200000) -ne 0) -and
                   (($editInkStyle -band 0x00080000) -eq 0)) `
            ("E did not enter interactive MANO with a hidden ink surface and input " +
             "pass-through (state=$editState; toolbar=$editToolbar; " +
             "toolbarVisible=$([ElitePenUiNative]::IsWindowVisible($editToolbar)); " +
             "inkVisible=$([ElitePenUiNative]::IsWindowVisible($zoomInk)); " +
             "magnifierEnabled=$([ElitePenUiNative]::IsWindowEnabled($magnifier)); " +
             "foregroundProcess=$editForegroundProcess; eliteProcess=$($process.Id); " +
             "zoomStyle=$editZoomStyle; magnifierStyle=$editMagnifierStyle; " +
             "inkStyle=$editInkStyle).")
        Assert-Ui ([ElitePenUiNative]::WindowText($editToolbar).Contains('Navegar') -and
                   [ElitePenUiNative]::IsAboveClass($palette, 'ElitePen.ZoomEditToolbar') -and
                   [ElitePenUiNative]::IsAboveClass($editToolbar, 'ElitePen.Zoom')) `
            'Editable zoom toolbar is not accessible in the expected topmost order.'

        $editState = [ElitePenUiNative]::SendMessage(
            $zoom, 0x8074, [IntPtr]2, [IntPtr]::Zero).ToInt64()
        Start-Sleep -Milliseconds 80
        $editInkStyle = [ElitePenUiNative]::GetWindowLong($zoomInk, -20)
        $editZoomStyle = [ElitePenUiNative]::GetWindowLong($zoom, -20)
        $editSnapshot = [ElitePenUiNative]::SendMessage(
            $zoomInk, 0x8064, [IntPtr]::Zero, [IntPtr]::Zero).ToInt64()
        Assert-Ui ($editState -eq 2 -and (($editInkStyle -band 0x20) -eq 0) -and
                   (($editZoomStyle -band 0x20) -eq 0) -and
                   [ElitePenUiNative]::IsWindowEnabled($magnifier) -and
                   [ElitePenUiNative]::IsWindowVisible($zoomInk) -and
                   $editSnapshot -eq 1 -and
                   [ElitePenUiNative]::WindowText($editToolbar).Contains('Anotar')) `
            'Editable zoom did not preserve the enlarged frame and expose annotation input.'

        $null = [ElitePenUiNative]::SendMessage(
            $zoom, 0x0100, [IntPtr]0x20, [IntPtr]::Zero)
        Start-Sleep -Milliseconds 60
        $spaceState = [ElitePenUiNative]::SendMessage(
            $zoom, 0x8072, [IntPtr]::Zero, [IntPtr]::Zero).ToInt64()
        $spaceStyle = [ElitePenUiNative]::GetWindowLong($zoomInk, -20)
        $spaceZoomStyle = [ElitePenUiNative]::GetWindowLong($zoom, -20)
        Assert-Ui ($spaceState -eq 1 -and (($spaceStyle -band 0x20) -ne 0) -and
                   (($spaceZoomStyle -band 0x20) -ne 0) -and
                   -not [ElitePenUiNative]::IsWindowEnabled($magnifier) -and
                   -not [ElitePenUiNative]::IsWindowVisible($zoomInk)) `
            'Space did not return editable zoom safely to MANO navigation.'
        $editState = [ElitePenUiNative]::SendMessage(
            $zoom, 0x8074, [IntPtr]2, [IntPtr]::Zero).ToInt64()
        Assert-Ui ($editState -eq 2) `
            'Editable zoom could not return to annotation after the Space regression check.'

        Select-Tool $palette 1
        $editStart = [IntPtr]((220 -shl 16) -bor 300)
        $editFinish = [IntPtr]((300 -shl 16) -bor 450)
        $null = [ElitePenUiNative]::SendMessage($zoomInk, 0x0201, [IntPtr]1, $editStart)
        $null = [ElitePenUiNative]::SendMessage($zoomInk, 0x0200, [IntPtr]1, $editFinish)
        $null = [ElitePenUiNative]::SendMessage($zoomInk, 0x0202, [IntPtr]0, $editFinish)
        $editItems = [ElitePenUiNative]::SendMessage(
            $zoom, 0x8073, [IntPtr]::Zero, [IntPtr]::Zero).ToInt64()
        $firstViewX = [ElitePenUiNative]::SendMessage(
            $zoom, 0x8076, [IntPtr]::Zero, [IntPtr]::Zero).ToInt64()
        $firstViewY = [ElitePenUiNative]::SendMessage(
            $zoom, 0x8077, [IntPtr]::Zero, [IntPtr]::Zero).ToInt64()
        Assert-Ui ($editItems -eq 1 -and [Math]::Abs($firstViewX - 300) -le 2 -and
                   [Math]::Abs($firstViewY - 220) -le 2) `
            'Editable zoom did not store its first pen stroke in source coordinates.'

        Select-Tool $palette 5
        $shapeStart = [IntPtr]((350 -shl 16) -bor 520)
        $shapeFinish = [IntPtr]((440 -shl 16) -bor 680)
        $null = [ElitePenUiNative]::SendMessage($zoomInk, 0x0201, [IntPtr]1, $shapeStart)
        $null = [ElitePenUiNative]::SendMessage($zoomInk, 0x0200, [IntPtr]1, $shapeFinish)
        $null = [ElitePenUiNative]::SendMessage($zoomInk, 0x0202, [IntPtr]0, $shapeFinish)
        Select-Tool $palette 9
        Click-Window $zoomInk 760 360
        $editText = Wait-Window 'ElitePen.TextInput' 1000
        if ($editText -ne [IntPtr]::Zero) {
            foreach ($character in 'Texto E'.ToCharArray()) {
                $null = [ElitePenUiNative]::SendMessage(
                    $editText, 0x0102, [IntPtr][int]$character, [IntPtr]::Zero)
            }
            $null = [ElitePenUiNative]::SendMessage(
                $editText, 0x805D, [IntPtr]::Zero, [IntPtr]::Zero)
        }
        $editItems = [ElitePenUiNative]::SendMessage(
            $zoom, 0x8073, [IntPtr]::Zero, [IntPtr]::Zero).ToInt64()
        Assert-Ui ($editText -ne [IntPtr]::Zero -and $editItems -eq 3) `
            'Editable zoom did not support geometry and inline text in one document.'

        Select-Tool $palette 9
        Click-Window $zoomInk 820 410
        $pendingEditText = Wait-Window 'ElitePen.TextInput' 1000
        if ($pendingEditText -ne [IntPtr]::Zero) {
            foreach ($character in 'Texto al navegar'.ToCharArray()) {
                $null = [ElitePenUiNative]::SendMessage(
                    $pendingEditText, 0x0102, [IntPtr][int]$character, [IntPtr]::Zero)
            }
        }
        $navigateState = [ElitePenUiNative]::SendMessage(
            $zoom, 0x8074, [IntPtr]1, [IntPtr]::Zero).ToInt64()
        $itemsAfterNavigate = [ElitePenUiNative]::SendMessage(
            $zoom, 0x8073, [IntPtr]::Zero, [IntPtr]::Zero).ToInt64()
        Assert-Ui ($pendingEditText -ne [IntPtr]::Zero -and
                   $navigateState -eq 1 -and $itemsAfterNavigate -eq 4 -and
                   -not [ElitePenUiNative]::IsWindowVisible($pendingEditText)) `
            'Returning to MANO did not commit pending text to the editable document.'
        $null = [ElitePenUiNative]::SendMessage(
            $zoom, 0x8074, [IntPtr]2, [IntPtr]::Zero)

        $null = [ElitePenUiNative]::SendMessage(
            $palette, 0x0312, [IntPtr]4, [IntPtr]::Zero)
        $afterEditUndo = [ElitePenUiNative]::SendMessage(
            $zoom, 0x8073, [IntPtr]::Zero, [IntPtr]::Zero).ToInt64()
        $null = [ElitePenUiNative]::SendMessage(
            $palette, 0x0312, [IntPtr]5, [IntPtr]::Zero)
        $afterEditRedo = [ElitePenUiNative]::SendMessage(
            $zoom, 0x8073, [IntPtr]::Zero, [IntPtr]::Zero).ToInt64()
        Assert-Ui ($afterEditUndo -eq 3 -and $afterEditRedo -eq 4) `
            'Undo and redo did not remain scoped to editable zoom.'

        $capturesBeforeEdit = (Get-ChildItem -LiteralPath $captureDirectory `
            -Filter '*.png' -File).Count
        Select-Tool $palette 10
        $editCaptureStart = [IntPtr]((180 -shl 16) -bor 240)
        $editCaptureFinish = [IntPtr]((320 -shl 16) -bor 460)
        $null = [ElitePenUiNative]::SendMessage(
            $zoomInk, 0x0201, [IntPtr]1, $editCaptureStart)
        $null = [ElitePenUiNative]::SendMessage(
            $zoomInk, 0x0200, [IntPtr]1, $editCaptureFinish)
        $null = [ElitePenUiNative]::SendMessage(
            $zoomInk, 0x0202, [IntPtr]0, $editCaptureFinish)
        Start-Sleep -Milliseconds 300
        $editCaptures = @(Get-ChildItem -LiteralPath $captureDirectory `
            -Filter '*.png' -File | Sort-Object LastWriteTimeUtc)
        $editCaptureValid = $false
        if ($editCaptures.Count -eq $capturesBeforeEdit + 1) {
            Add-Type -AssemblyName System.Drawing
            $editBitmap = [System.Drawing.Bitmap]::FromFile($editCaptures[-1].FullName)
            try {
                $editCaptureValid = $editBitmap.Width -eq 220 -and
                    $editBitmap.Height -eq 140
            } finally {
                $editBitmap.Dispose()
            }
        }
        Assert-Ui ($editCaptureValid -and
                   [ElitePenUiNative]::IsWindowVisible($palette) -and
                   [ElitePenUiNative]::IsWindowVisible($editToolbar)) `
            'Editable zoom capture did not flatten the selected 220 x 140 view or restore its controls.'

        $null = [ElitePenUiNative]::SendMessage(
            $zoom, 0x8074, [IntPtr]1, [IntPtr]::Zero)
        $anchorBeforeX = [ElitePenUiNative]::SendMessage(
            $zoom, 0x8076, [IntPtr]::Zero, [IntPtr]::Zero).ToInt64()
        $anchorBeforeY = [ElitePenUiNative]::SendMessage(
            $zoom, 0x8077, [IntPtr]::Zero, [IntPtr]::Zero).ToInt64()
        $null = [ElitePenUiNative]::SendMessage(
            $zoom, 0x8078, [IntPtr]75, [IntPtr]45)
        Start-Sleep -Milliseconds 80
        $anchorAfterPanX = [ElitePenUiNative]::SendMessage(
            $zoom, 0x8076, [IntPtr]::Zero, [IntPtr]::Zero).ToInt64()
        $anchorAfterPanY = [ElitePenUiNative]::SendMessage(
            $zoom, 0x8077, [IntPtr]::Zero, [IntPtr]::Zero).ToInt64()
        if ($anchorBeforeX -eq $anchorAfterPanX -and
            $anchorBeforeY -eq $anchorAfterPanY) {
            $null = [ElitePenUiNative]::SendMessage(
                $zoom, 0x8078, [IntPtr](-75), [IntPtr](-45))
            $anchorAfterPanX = [ElitePenUiNative]::SendMessage(
                $zoom, 0x8076, [IntPtr]::Zero, [IntPtr]::Zero).ToInt64()
            $anchorAfterPanY = [ElitePenUiNative]::SendMessage(
                $zoom, 0x8077, [IntPtr]::Zero, [IntPtr]::Zero).ToInt64()
        }
        $editToolbarBounds = New-Object ElitePenUiNative+RECT
        $null = [ElitePenUiNative]::GetWindowRect(
            $editToolbar, [ref]$editToolbarBounds)
        $editToolbarWidth = $editToolbarBounds.Right - $editToolbarBounds.Left
        $editToolbarHeight = $editToolbarBounds.Bottom - $editToolbarBounds.Top
        $factorBeforeToolbar = [ElitePenUiNative]::SendMessage(
            $zoom, 0x8079, [IntPtr]::Zero, [IntPtr]::Zero).ToInt64()
        Click-Window $editToolbar ([Math]::Floor($editToolbarWidth * 0.34)) `
            ([Math]::Floor($editToolbarHeight * 0.50))
        $factorAfterToolbar = [ElitePenUiNative]::SendMessage(
            $zoom, 0x8079, [IntPtr]::Zero, [IntPtr]::Zero).ToInt64()
        $anchorAfterZoomX = [ElitePenUiNative]::SendMessage(
            $zoom, 0x8076, [IntPtr]::Zero, [IntPtr]::Zero).ToInt64()
        $anchorAfterZoomY = [ElitePenUiNative]::SendMessage(
            $zoom, 0x8077, [IntPtr]::Zero, [IntPtr]::Zero).ToInt64()
        Assert-Ui (($anchorBeforeX -ne $anchorAfterPanX -or
                    $anchorBeforeY -ne $anchorAfterPanY) -and
                   $factorAfterToolbar -gt $factorBeforeToolbar) `
            ("Source-anchored annotations did not transform during pan and zoom " +
             "(before=$anchorBeforeX,$anchorBeforeY; " +
             "pan=$anchorAfterPanX,$anchorAfterPanY; " +
             "zoom=$anchorAfterZoomX,$anchorAfterZoomY; " +
             "factor=$factorBeforeToolbar->$factorAfterToolbar).")

        $null = [ElitePenUiNative]::SendMessage(
            $zoom, 0x8074, [IntPtr]2, [IntPtr]::Zero)
        $null = [ElitePenUiNative]::SendMessage(
            $palette, 0x0312, [IntPtr]6, [IntPtr]::Zero)
        $clearedEditItems = [ElitePenUiNative]::SendMessage(
            $zoom, 0x8073, [IntPtr]::Zero, [IntPtr]::Zero).ToInt64()
        $null = [ElitePenUiNative]::SendMessage(
            $palette, 0x0312, [IntPtr]4, [IntPtr]::Zero)
        $restoredEditItems = [ElitePenUiNative]::SendMessage(
            $zoom, 0x8073, [IntPtr]::Zero, [IntPtr]::Zero).ToInt64()
        Assert-Ui ($clearedEditItems -eq 0 -and $restoredEditItems -eq 4) `
            'Clear and undo did not preserve editable zoom history independently.'

        $null = [ElitePenUiNative]::SendMessage(
            $zoom, 0x8074, [IntPtr]::Zero, [IntPtr]::Zero)
        Start-Sleep -Milliseconds 80
        $editState = [ElitePenUiNative]::SendMessage(
            $zoom, 0x8072, [IntPtr]::Zero, [IntPtr]::Zero).ToInt64()
        $classicZoomStyle = [ElitePenUiNative]::GetWindowLong($zoom, -20)
        Assert-Ui ($editState -eq 0 -and
                   (($classicZoomStyle -band 0x20) -eq 0) -and
                   [ElitePenUiNative]::IsWindowEnabled($magnifier) -and
                   -not [ElitePenUiNative]::IsWindowVisible($editToolbar)) `
            'Closing editable zoom did not restore the original live zoom workflow.'

        if ($magnifier -ne [IntPtr]::Zero) {
            $clickPoint = New-Object ElitePenUiNative+POINT
            $clickPoint.X = $full.Left + [Math]::Floor(($full.Right - $full.Left) * 0.58)
            $clickPoint.Y = $full.Top + [Math]::Floor(($full.Bottom - $full.Top) * 0.68)
            $physicalTarget = [ElitePenUiNative]::WindowFromPoint($clickPoint)
            $targetClass = New-Object System.Text.StringBuilder 128
            $null = [ElitePenUiNative]::GetClassName($physicalTarget, $targetClass, 128)
            Assert-Ui ($targetClass.ToString() -ne 'ElitePen.Overlay' -and
                       $targetClass.ToString() -ne 'ElitePen.ZoomTarget') `
                "Physical zoom click was intercepted by $($targetClass.ToString())."
            $clickPointerMoved = [ElitePenUiNative]::SetCursorPos(
                $clickPoint.X, $clickPoint.Y)
            if ($clickPointerMoved) {
                [ElitePenUiNative]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)
                [ElitePenUiNative]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)
            } else {
                # Restricted desktops can reject synthetic pointer movement.
                # Exercise the equivalent freeze path instead of producing a
                # false product failure in that environment.
                $null = [ElitePenUiNative]::SendMessage(
                    $zoom, 0x0100, [IntPtr][char]'P', [IntPtr]::Zero)
            }
        }
        Start-Sleep -Milliseconds 180
        $frozen = [ElitePenUiNative]::SendMessage($zoom, 0x8061, [IntPtr]::Zero, [IntPtr]::Zero)
        if ($frozen.ToInt64() -eq 0) {
            $null = [ElitePenUiNative]::SendMessage(
                $zoom, 0x0100, [IntPtr][char]'P', [IntPtr]::Zero)
            Start-Sleep -Milliseconds 180
            $frozen = [ElitePenUiNative]::SendMessage(
                $zoom, 0x8061, [IntPtr]::Zero, [IntPtr]::Zero)
            if ($frozen.ToInt64() -eq 0) {
                $null = [ElitePenUiNative]::SendMessage(
                    $zoom, 0x806F, [IntPtr]::Zero, [IntPtr]::Zero)
                Start-Sleep -Milliseconds 180
                $frozen = [ElitePenUiNative]::SendMessage(
                    $zoom, 0x8061, [IntPtr]::Zero, [IntPtr]::Zero)
            }
        }
        $snapshotReady = [ElitePenUiNative]::SendMessage(
            $zoomInk, 0x8064, [IntPtr]::Zero, [IntPtr]::Zero)
        $freezeTool = [ElitePenUiNative]::SendMessage($palette, 0x805A, [IntPtr]::Zero, [IntPtr]::Zero)
        Assert-Ui ($zoomInk -ne [IntPtr]::Zero -and [ElitePenUiNative]::IsWindowVisible($zoomInk) -and
                   $frozen.ToInt64() -eq 1) 'Zoom did not freeze into its annotation surface.'
        Assert-Ui ($snapshotReady.ToInt64() -eq 1) `
            'Frozen zoom accepted an empty or black snapshot.'
        Assert-Ui ($freezeTool.ToInt64() -eq 1) 'Freezing zoom did not activate the pen automatically.'
        Assert-Ui ([ElitePenUiNative]::IsAboveClass($palette, 'ElitePen.ZoomInk') -and
                   [ElitePenUiNative]::IsAboveClass($palette, 'ElitePen.Zoom')) `
            'Zoom surfaces covered the palette while annotation mode was active.'
        if ($zoomInk -ne [IntPtr]::Zero) {
            Click-PaletteWindow $palette 169 53
            $start = [IntPtr]((220 -shl 16) -bor 300)
            $finish = [IntPtr]((290 -shl 16) -bor 430)
            $null = [ElitePenUiNative]::SendMessage($zoomInk, 0x0201, [IntPtr]1, $start)
            $null = [ElitePenUiNative]::SendMessage($zoomInk, 0x0200, [IntPtr]1, $finish)
            $null = [ElitePenUiNative]::SendMessage($zoomInk, 0x0202, [IntPtr]0, $finish)
            $zoomItems = [ElitePenUiNative]::SendMessage($zoomInk, 0x8062, [IntPtr]::Zero, [IntPtr]::Zero)
            Assert-Ui ($zoomItems.ToInt64() -eq 1) 'Frozen zoom did not accept pen annotations.'
            Assert-Ui ([ElitePenUiNative]::IsWindowVisible($palette) -and
                       [ElitePenUiNative]::IsAboveClass($palette, 'ElitePen.ZoomInk') -and
                       [ElitePenUiNative]::IsAboveClass($palette, 'ElitePen.Zoom') -and
                       [ElitePenUiNative]::IsAboveClass($zoomInk, 'ElitePen.Zoom')) `
                'The native zoom image covered the completed annotation or palette.'
            $inkBoundsDuringZoom = New-Object ElitePenUiNative+RECT
            $null = [ElitePenUiNative]::GetWindowRect($zoomInk, [ref]$inkBoundsDuringZoom)
            $inkProbe = New-Object ElitePenUiNative+POINT
            $inkProbe.X = $inkBoundsDuringZoom.Left + 430
            $inkProbe.Y = $inkBoundsDuringZoom.Top + 290
            $inkAtPoint = [ElitePenUiNative]::WindowFromPoint($inkProbe)
            $inkClass = New-Object System.Text.StringBuilder 128
            $null = [ElitePenUiNative]::GetClassName($inkAtPoint, $inkClass, 128)
            Assert-Ui ($inkClass.ToString() -eq 'ElitePen.ZoomInk') `
                'Completed zoom ink was not the visible interactive layer at its stroke.'
            $paletteBoundsDuringZoom = New-Object ElitePenUiNative+RECT
            $null = [ElitePenUiNative]::GetWindowRect($palette, [ref]$paletteBoundsDuringZoom)
            $paletteProbe = New-Object ElitePenUiNative+POINT
            # Probe a visibly painted command, not the transparent negative
            # space inside the irregular painter-palette silhouette.
            $paletteProbe.X = $paletteBoundsDuringZoom.Left +
                (Convert-PaletteCoordinate 169)
            $paletteProbe.Y = $paletteBoundsDuringZoom.Top +
                (Convert-PaletteCoordinate 53)
            $paletteAtPoint = [ElitePenUiNative]::WindowFromPoint($paletteProbe)
            $paletteClass = New-Object System.Text.StringBuilder 128
            $null = [ElitePenUiNative]::GetClassName($paletteAtPoint, $paletteClass, 128)
            Assert-Ui ($paletteClass.ToString() -eq 'ElitePen.Palette') `
                "Frozen zoom still intercepted physical input over the palette; hit $($paletteClass.ToString()) at $($paletteProbe.X),$($paletteProbe.Y)."

            # Screenshot selection on a frozen zoom must flatten the exact enlarged
            # frame and its red annotation, save it, and copy the same bitmap.
            $capturesBeforeZoom = (Get-ChildItem -LiteralPath $captureDirectory -Filter '*.png' -File).Count
            Select-Tool $palette 10
            $zoomCaptureStart = [IntPtr]((190 -shl 16) -bor 260)
            $zoomCaptureFinish = [IntPtr]((320 -shl 16) -bor 470)
            $null = [ElitePenUiNative]::SendMessage($zoomInk, 0x0201, [IntPtr]1, $zoomCaptureStart)
            $null = [ElitePenUiNative]::SendMessage($zoomInk, 0x0200, [IntPtr]1, $zoomCaptureFinish)
            $null = [ElitePenUiNative]::SendMessage($zoomInk, 0x0202, [IntPtr]0, $zoomCaptureFinish)
            Start-Sleep -Milliseconds 350
            $zoomCaptures = @(Get-ChildItem -LiteralPath $captureDirectory -Filter '*.png' -File |
                Sort-Object LastWriteTimeUtc)
            Assert-Ui ($zoomCaptures.Count -eq $capturesBeforeZoom + 1) `
                'Frozen zoom capture did not create exactly one PNG.'
            $clipboardOpened = [ElitePenUiNative]::OpenClipboard([IntPtr]::Zero)
            Assert-Ui $clipboardOpened 'Frozen zoom capture did not leave an accessible clipboard image.'
            if ($clipboardOpened) {
                try {
                    $clipboardDib = [ElitePenUiNative]::GetClipboardData(8)
                    $clipboardPixels = if ($clipboardDib -ne [IntPtr]::Zero) {
                        [ElitePenUiNative]::GlobalLock($clipboardDib)
                    } else { [IntPtr]::Zero }
                    try {
                        $clipboardWidth = if ($clipboardPixels -ne [IntPtr]::Zero) {
                            [Runtime.InteropServices.Marshal]::ReadInt32($clipboardPixels, 4)
                        } else { 0 }
                        $clipboardHeight = if ($clipboardPixels -ne [IntPtr]::Zero) {
                            [Math]::Abs([Runtime.InteropServices.Marshal]::ReadInt32($clipboardPixels, 8))
                        } else { 0 }
                        Assert-Ui ($clipboardWidth -eq 210 -and $clipboardHeight -eq 130) `
                            'Frozen zoom capture did not copy the selected 210 x 130 image.'
                    } finally {
                        if ($clipboardPixels -ne [IntPtr]::Zero) {
                            $null = [ElitePenUiNative]::GlobalUnlock($clipboardDib)
                        }
                    }
                } finally {
                    $null = [ElitePenUiNative]::CloseClipboard()
                }
            }
            if ($zoomCaptures.Count -gt $capturesBeforeZoom) {
                Add-Type -AssemblyName System.Drawing
                $zoomBitmap = [System.Drawing.Bitmap]::FromFile($zoomCaptures[-1].FullName)
                try {
                    $redPixels = 0
                    for ($y = 0; $y -lt $zoomBitmap.Height; $y += 2) {
                        for ($x = 0; $x -lt $zoomBitmap.Width; $x += 2) {
                            $pixel = $zoomBitmap.GetPixel($x, $y)
                            if ($pixel.R -gt 180 -and $pixel.G -lt 125 -and $pixel.B -lt 125) {
                                $redPixels++
                            }
                        }
                    }
                    Assert-Ui ($redPixels -ge 4) `
                        'Frozen zoom PNG did not contain the visible red annotation.'
                } finally {
                    $zoomBitmap.Dispose()
                }
            }
            Click-PaletteWindow $palette 169 53
            $zoomColor = [ElitePenUiNative]::SendMessage(
                $palette, 0x805B, [IntPtr]::Zero, [IntPtr]::Zero)
            Assert-Ui ($zoomColor.ToInt64() -eq 4293870660 -and
                       [ElitePenUiNative]::IsAboveClass($palette, 'ElitePen.ZoomInk') -and
                       [ElitePenUiNative]::IsAboveClass($palette, 'ElitePen.Zoom') -and
                       [ElitePenUiNative]::IsAboveClass($zoomInk, 'ElitePen.Zoom')) `
                'Changing color during frozen zoom hid or deactivated the palette.'
            Select-Tool $palette 5
            $zoomGeometry = [ElitePenUiNative]::SendMessage(
                $palette, 0x805A, [IntPtr]::Zero, [IntPtr]::Zero)
            Assert-Ui ($zoomGeometry.ToInt64() -eq 5 -and
                       [ElitePenUiNative]::IsAboveClass($palette, 'ElitePen.ZoomInk') -and
                       [ElitePenUiNative]::IsAboveClass($palette, 'ElitePen.Zoom') -and
                       [ElitePenUiNative]::IsAboveClass($zoomInk, 'ElitePen.Zoom')) `
                'Geometry selection was not available above the frozen zoom image.'
            $null = [ElitePenUiNative]::SendMessage($zoomInk, 0x0100, [IntPtr][char]'P', [IntPtr]::Zero)
            Start-Sleep -Milliseconds 120
            $resumed = [ElitePenUiNative]::SendMessage($zoom, 0x8061, [IntPtr]::Zero, [IntPtr]::Zero)
            $zoomItems = [ElitePenUiNative]::SendMessage($zoomInk, 0x8062, [IntPtr]::Zero, [IntPtr]::Zero)
            Assert-Ui ($resumed.ToInt64() -eq 0 -and $zoomItems.ToInt64() -eq 1) `
                'Zoom annotations did not persist when live zoom resumed.'
            $null = [ElitePenUiNative]::SendMessage($palette, 0x0312, [IntPtr]6, [IntPtr]::Zero)
            $clearedZoomItems = [ElitePenUiNative]::SendMessage($zoomInk, 0x8062, [IntPtr]::Zero, [IntPtr]::Zero)
            Assert-Ui ($clearedZoomItems.ToInt64() -eq 0) 'Clear did not target the zoom document independently.'
            $null = [ElitePenUiNative]::SendMessage($palette, 0x0312, [IntPtr]4, [IntPtr]::Zero)
            $restoredZoomItems = [ElitePenUiNative]::SendMessage($zoomInk, 0x8062, [IntPtr]::Zero, [IntPtr]::Zero)
            Assert-Ui ($restoredZoomItems.ToInt64() -eq 1) 'Undo did not restore zoom annotations.'
            $null = [ElitePenUiNative]::SendMessage($palette, 0x0312, [IntPtr]5, [IntPtr]::Zero)
            $redoneZoomItems = [ElitePenUiNative]::SendMessage($zoomInk, 0x8062, [IntPtr]::Zero, [IntPtr]::Zero)
            Assert-Ui ($redoneZoomItems.ToInt64() -eq 0) 'Redo did not clear zoom annotations again.'

            # Repeat freeze and annotation in Lens mode. It shares the same
            # ordering contract, but uses a compact native Magnifier root.
            $null = [ElitePenUiNative]::SendMessage($zoom, 0x0100, [IntPtr][char]'L', [IntPtr]::Zero)
            Start-Sleep -Milliseconds 100
            $null = [ElitePenUiNative]::SendMessage($zoom, 0x0100, [IntPtr][char]'P', [IntPtr]::Zero)
            Start-Sleep -Milliseconds 120
            $lensStart = [IntPtr]((160 -shl 16) -bor 260)
            $lensFinish = [IntPtr]((220 -shl 16) -bor 370)
            $null = [ElitePenUiNative]::SendMessage($zoomInk, 0x0201, [IntPtr]1, $lensStart)
            $null = [ElitePenUiNative]::SendMessage($zoomInk, 0x0200, [IntPtr]1, $lensFinish)
            $null = [ElitePenUiNative]::SendMessage($zoomInk, 0x0202, [IntPtr]0, $lensFinish)
            $lensItems = [ElitePenUiNative]::SendMessage(
                $zoomInk, 0x8062, [IntPtr]::Zero, [IntPtr]::Zero)
            Assert-Ui ($lensItems.ToInt64() -eq 1 -and
                       [ElitePenUiNative]::IsAboveClass($palette, 'ElitePen.ZoomInk') -and
                       [ElitePenUiNative]::IsAboveClass($palette, 'ElitePen.Zoom') -and
                       [ElitePenUiNative]::IsAboveClass($zoomInk, 'ElitePen.Zoom')) `
                'Lens freeze did not preserve its completed annotation and palette above the image.'
            $null = [ElitePenUiNative]::SendMessage($zoomInk, 0x0100, [IntPtr][char]'P', [IntPtr]::Zero)
            Start-Sleep -Milliseconds 120
            $null = [ElitePenUiNative]::SendMessage($zoom, 0x0100, [IntPtr]27, [IntPtr]::Zero)
            Start-Sleep -Milliseconds 120
            Assert-Ui (-not [ElitePenUiNative]::IsWindowVisible($zoom)) 'Escape did not exit zoom.'
        }
    }

    Assert-Ui (-not $process.HasExited) 'Application exited unexpectedly during UI checks.'
} finally {
    $palette = [ElitePenUiNative]::FindWindow('ElitePen.Palette', 'Elite Pen')
    if ($palette -ne [IntPtr]::Zero) {
        $null = [ElitePenUiNative]::PostMessage($palette, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
    }
    if ($process) {
        $process.WaitForExit(3000) | Out-Null
        if (-not $process.HasExited) {
            $process.Kill()
            $process.WaitForExit(3000) | Out-Null
        }
        $process.Dispose()
    }
    if (Test-Path -LiteralPath $qaSandbox) {
        for ($attempt = 0; $attempt -lt 40; $attempt++) {
            try {
                Remove-Item -LiteralPath $qaSandbox -Recurse -Force
                break
            } catch {
                if ($attempt -eq 39) {
                    Write-Warning "UI QA completed, but its temporary directory is still locked: $qaSandbox"
                    break
                }
                Start-Sleep -Milliseconds 250
            }
        }
    }
    $env:ELITE_PEN_QA_CAPTURE_DIR = $previousCaptureDirectory
    $env:ELITE_PEN_QA_SYNTHETIC_CAPTURE = $previousSyntheticCapture
}

# The clipboard payload must outlive Elite Pen itself. This catches native bitmap
# handles that appeared valid only while the producer process remained open.
$clipboardOpenedAfterExit = [ElitePenUiNative]::OpenClipboard([IntPtr]::Zero)
Assert-Ui $clipboardOpenedAfterExit 'Clipboard image was unavailable after Elite Pen exited.'
if ($clipboardOpenedAfterExit) {
    try {
        $clipboardDibAfterExit = [ElitePenUiNative]::GetClipboardData(8)
        $clipboardPixelsAfterExit = if ($clipboardDibAfterExit -ne [IntPtr]::Zero) {
            [ElitePenUiNative]::GlobalLock($clipboardDibAfterExit)
        } else { [IntPtr]::Zero }
        try {
            $widthAfterExit = if ($clipboardPixelsAfterExit -ne [IntPtr]::Zero) {
                [Runtime.InteropServices.Marshal]::ReadInt32($clipboardPixelsAfterExit, 4)
            } else { 0 }
            $heightAfterExit = if ($clipboardPixelsAfterExit -ne [IntPtr]::Zero) {
                [Math]::Abs([Runtime.InteropServices.Marshal]::ReadInt32(
                    $clipboardPixelsAfterExit, 8))
            } else { 0 }
            Assert-Ui ($widthAfterExit -eq 210 -and $heightAfterExit -eq 130) `
                'Clipboard image did not survive Elite Pen shutdown.'
        } finally {
            if ($clipboardPixelsAfterExit -ne [IntPtr]::Zero) {
                $null = [ElitePenUiNative]::GlobalUnlock($clipboardDibAfterExit)
            }
        }
    } finally {
        $null = [ElitePenUiNative]::CloseClipboard()
    }
}

if ($failures.Count -gt 0) {
    $failures | ForEach-Object { Write-Error $_ }
    exit 1
}
Write-Output 'Elite Pen UI smoke test: all checks passed'
