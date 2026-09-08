[CmdletBinding()]
param([string]$ExecutablePath)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not $ExecutablePath) { $ExecutablePath = Join-Path $repoRoot 'build\release\Elite Pen.exe' }
$qaSandbox = Join-Path ([IO.Path]::GetTempPath()) ('elite-pen-lens-qa-' + [Guid]::NewGuid().ToString('N'))
$null = New-Item -ItemType Directory -Path $qaSandbox
Copy-Item -LiteralPath $ExecutablePath -Destination (Join-Path $qaSandbox 'Elite Pen.exe')
Set-Content -LiteralPath (Join-Path $qaSandbox 'portable.flag') -Value 'Isolated lens QA'
Add-Type -AssemblyName System.Windows.Forms
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class EliteLensQa {
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr context);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindowEx(IntPtr parent, IntPtr after, string cls, string title);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window, out uint pid);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr window, uint message, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr window, out RECT rect);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
    public static IntPtr Find(string cls, uint pid) {
        for (var w = FindWindowEx(IntPtr.Zero, IntPtr.Zero, cls, null); w != IntPtr.Zero;
             w = FindWindowEx(IntPtr.Zero, w, cls, null)) {
            uint found; GetWindowThreadProcessId(w, out found);
            if (pid == found) return w;
        }
        return IntPtr.Zero;
    }
}
'@
$null = [EliteLensQa]::SetProcessDpiAwarenessContext([IntPtr](-4))
function Send-Lens([IntPtr]$Window, [uint32]$Message, [long]$Value = 0, [long]$Extra = 0) {
    [EliteLensQa]::SendMessage($Window, $Message, [IntPtr]$Value, [IntPtr]$Extra).ToInt64()
}
function Assert-Lens([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}
$previousInstance = $env:ELITE_PEN_QA_INSTANCE_ID
$previousSynthetic = $env:ELITE_PEN_QA_SYNTHETIC_CAPTURE
$process = $null
$cases = 0
try {
    $env:ELITE_PEN_QA_INSTANCE_ID = [Guid]::NewGuid().ToString('N')
    $env:ELITE_PEN_QA_SYNTHETIC_CAPTURE = '1'
    $process = Start-Process -FilePath (Join-Path $qaSandbox 'Elite Pen.exe') -WindowStyle Hidden -PassThru
    for ($attempt = 0; $attempt -lt 200; $attempt++) {
        $palette = [EliteLensQa]::Find('ElitePen.Palette', [uint32]$process.Id)
        $zoom = [EliteLensQa]::Find('ElitePen.Zoom', [uint32]$process.Id)
        $magnifier = if ($zoom -ne [IntPtr]::Zero) {
            [EliteLensQa]::FindWindowEx($zoom, [IntPtr]::Zero, 'Magnifier', 'Elite Pen Magnifier')
        } else { [IntPtr]::Zero }
        if ($palette -ne [IntPtr]::Zero -and $magnifier -ne [IntPtr]::Zero) { break }
        Start-Sleep -Milliseconds 25
    }
    Assert-Lens ($zoom -ne [IntPtr]::Zero -and $magnifier -ne [IntPtr]::Zero) 'Zoom did not initialize its native Magnifier child.'
    Start-Sleep -Milliseconds 250
    $null = Send-Lens $palette 0x0312 7
    $null = Send-Lens $zoom 0x0100 ([int][char]'L')
    Assert-Lens ((Send-Lens $zoom 0x806D) -eq 1) 'L did not select lens mode.'
    foreach ($screen in [Windows.Forms.Screen]::AllScreens) {
        $bounds = $screen.Bounds
        $points = @(
            @($bounds.Left, $bounds.Top), @(($bounds.Right-1), $bounds.Top),
            @($bounds.Left, ($bounds.Bottom-1)), @(($bounds.Right-1), ($bounds.Bottom-1)),
            @($bounds.Left, ($bounds.Top + [int]($bounds.Height/2))),
            @(($bounds.Right-1), ($bounds.Top + [int]($bounds.Height/2))),
            @(($bounds.Left + [int]($bounds.Width/2)), $bounds.Top),
            @(($bounds.Left + [int]($bounds.Width/2)), ($bounds.Bottom-1)))
        for ($reset = 0; $reset -lt 5; $reset++) { $null = Send-Lens $zoom 0x0100 0xDB }
        for ($size = 0; $size -lt 5; $size++) {
            foreach ($point in $points) {
                $x = [int]$point[0]; $y = [int]$point[1]
                # Only a process carrying QA_INSTANCE_ID accepts this input.
                # Exercise the live engine without racing or taking control
                # of the user's physical mouse.
                Assert-Lens ((Send-Lens $zoom 0x807E $x $y) -eq 1) 'Isolated QA source input was rejected.'
                $focusX = Send-Lens $zoom 0x8066
                $focusY = Send-Lens $zoom 0x8067
                Assert-Lens ($focusX -eq $x -and $focusY -eq $y) "Lens focus left pointer at edge ($x,$y): focus=($focusX,$focusY)."
                $view = New-Object EliteLensQa+RECT
                $null = [EliteLensQa]::GetWindowRect($zoom, [ref]$view)
                Assert-Lens ($view.Left -ge $bounds.Left -and $view.Top -ge $bounds.Top -and
                    $view.Right -le $bounds.Right -and $view.Bottom -le $bounds.Bottom) 'Lens body left its monitor.'
                Assert-Lens ([EliteLensQa]::IsWindowVisible($magnifier)) "Native Magnifier hidden: handle=$magnifier; edge=($x,$y); view=$($view.Left),$($view.Top),$($view.Right),$($view.Bottom)."
                $source = New-Object EliteLensQa+RECT
                Assert-Lens ((Send-Lens $zoom 0x807F 4) -eq 1) 'Windows rejected the lens source.'
                $source.Left = Send-Lens $zoom 0x807F 0
                $source.Top = Send-Lens $zoom 0x807F 1
                $source.Right = Send-Lens $zoom 0x807F 2
                $source.Bottom = Send-Lens $zoom 0x807F 3
                Assert-Lens ($source.Left -ge $bounds.Left -and $source.Top -ge $bounds.Top -and
                    $source.Right -le $bounds.Right -and $source.Bottom -le $bounds.Bottom) 'Magnifier source exceeded its monitor.'
                $child = New-Object EliteLensQa+RECT
                $null = [EliteLensQa]::GetWindowRect($magnifier, [ref]$child)
                $factor = (Send-Lens $zoom 0x8079) / 100.0
                $mappedX = $child.Left - $view.Left + ($x-$source.Left)*$factor
                $mappedY = $child.Top - $view.Top + ($y-$source.Top)*$factor
                $radius = ($view.Right-$view.Left)/2.0
                Assert-Lens ([Math]::Abs($mappedX-$radius) -lt 3 -and [Math]::Abs($mappedY-$radius) -lt 3) 'Native output shifted the edge pixel away from lens center.'
                $cases++
            }
            $null = Send-Lens $zoom 0x0100 0xDD
        }
        $null = Send-Lens $zoom 0x0100 ([int][char]'P')
        Assert-Lens ((Send-Lens $zoom 0x8061) -eq 1 -and
            (Send-Lens $zoom 0x8066) -eq $x -and (Send-Lens $zoom 0x8067) -eq $y) 'Lens failed to retain focus when freezing at an edge.'
        $null = Send-Lens $zoom 0x0100 ([int][char]'P')
        Assert-Lens ((Send-Lens $zoom 0x8061) -eq 0) 'Lens failed to resume at a monitor edge.'
    }
    Write-Output "Lens edge QA passed: $cases native edge/size/monitor cases; freeze/resume passed (synthetic capture)."
} finally {
    if ($process -and -not $process.HasExited) {
        $palette = [EliteLensQa]::Find('ElitePen.Palette', [uint32]$process.Id)
        if ($palette -ne [IntPtr]::Zero) { $null = Send-Lens $palette 0x800C }
        if (-not $process.WaitForExit(3000)) {
            Stop-Process -Id $process.Id -Force
            $null = $process.WaitForExit(3000)
        }
    }
    $env:ELITE_PEN_QA_INSTANCE_ID = $previousInstance
    $env:ELITE_PEN_QA_SYNTHETIC_CAPTURE = $previousSynthetic
    $resolvedQa = [IO.Path]::GetFullPath($qaSandbox)
    $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    if ($resolvedQa.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase) -and
        [IO.Path]::GetFileName($resolvedQa).StartsWith('elite-pen-lens-qa-')) {
        try { Remove-Item -LiteralPath $resolvedQa -Recurse -Force }
        catch { Write-Warning "QA cleanup deferred for $resolvedQa : $($_.Exception.Message)" }
    }
}
