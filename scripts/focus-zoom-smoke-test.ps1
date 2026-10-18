[CmdletBinding()]
param(
    [string]$ExecutablePath,
    [switch]$Recordable,
    [switch]$SourceFixture
)

$ErrorActionPreference='Stop'
$repoRoot=Split-Path -Parent $PSScriptRoot
Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class FocusNative {
 [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left,Top,Right,Bottom; }
 public delegate bool WindowCallback(IntPtr window,IntPtr data);
 [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr value);
 [DllImport("user32.dll")] public static extern bool EnumWindows(WindowCallback callback,IntPtr data);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window,out uint pid);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr window,StringBuilder text,int length);
 [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr window,out RECT value);
 [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr window,uint message,IntPtr w,IntPtr l);
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr window,uint message,IntPtr w,IntPtr l);
 [DllImport("user32.dll")] public static extern bool SetCursorPos(int x,int y);
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr window);
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr window,int command);
 [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr window,IntPtr after,int x,int y,int cx,int cy,uint flags);
 [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint from,uint to,bool attach);
 [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
 [DllImport("user32.dll")] public static extern void keybd_event(byte key,byte scan,uint flags,UIntPtr extra);
 [DllImport("user32.dll")] public static extern bool RedrawWindow(IntPtr window,IntPtr rect,IntPtr region,uint flags);
 [DllImport("dwmapi.dll")] public static extern int DwmFlush();
 public static IntPtr Find(uint pid,string className) {
  IntPtr found=IntPtr.Zero;
  EnumWindows((window,data)=>{
   uint owner; GetWindowThreadProcessId(window,out owner);
   if(owner!=pid) return true;
   var name=new StringBuilder(256); GetClassName(window,name,name.Capacity);
   if(string.IsNullOrEmpty(className) ? IsWindowVisible(window) : name.ToString()==className){found=window;return false;}
   return true;
  },IntPtr.Zero);
  return found;
 }
 public static bool Foreground(IntPtr window) {
  uint pid;uint current=GetCurrentThreadId();uint other=GetWindowThreadProcessId(GetForegroundWindow(),out pid);
  bool attached=current!=other&&AttachThreadInput(current,other,true);
  try {ShowWindow(window,5);SetWindowPos(window,new IntPtr(-1),0,0,0,0,0x43);SetForegroundWindow(window);}
  finally {if(attached)AttachThreadInput(current,other,false);}
  if(GetForegroundWindow()!=window){
   keybd_event(0x12,0,0,UIntPtr.Zero);SetForegroundWindow(window);keybd_event(0x12,0,2,UIntPtr.Zero);
  }
  return GetForegroundWindow()==window;
 }
}
'@
$null=[FocusNative]::SetThreadDpiAwarenessContext([IntPtr](-4))

if($SourceFixture){
    Add-Type -AssemblyName System.Windows.Forms,System.Drawing
    Add-Type -ReferencedAssemblies @('System.Windows.Forms','System.Windows.Forms.Primitives',
        'System.Drawing.Common','System.Drawing.Primitives','System.ComponentModel.Primitives',
        'System.ComponentModel.TypeConverter','System.Runtime',
        'System.Private.Windows.Core','System.Private.Windows.GdiPlus') @'
using System;
using System.Drawing;
using System.Windows.Forms;
public class FocusFixture : Form {
 private Timer timer;
 private int frame;
 public FocusFixture() {
  Text="Elite Pen Focus QA Source"; FormBorderStyle=FormBorderStyle.None;
  StartPosition=FormStartPosition.Manual; Bounds=Screen.PrimaryScreen.Bounds;
  DoubleBuffered=true;TopMost=true;
  timer=new Timer(); timer.Interval=180; timer.Tick+=(s,e)=>{frame++;Invalidate();};timer.Start();
 }
 protected override void WndProc(ref Message message) {
  if(message.Msg==0x80BE){message.Result=new IntPtr(frame);return;}
  base.WndProc(ref message);
 }
 protected override void OnPaint(PaintEventArgs e) {
  e.Graphics.Clear(Color.FromArgb(240,243,250));
  e.Graphics.FillRectangle(Brushes.RoyalBlue,100,140,850,620);
  e.Graphics.FillRectangle(Brushes.Goldenrod,1020,160,750,600);
  e.Graphics.FillRectangle(Brushes.MediumSeaGreen,100,800,1670,300);
  using(var title=new Font("Segoe UI",30,FontStyle.Bold)) {
   e.Graphics.DrawString("ELITE PEN · ENFOQUE EN VIVO",title,Brushes.Black,100,60);
   e.Graphics.DrawString("ZONA 1",title,Brushes.White,140,165);
   e.Graphics.DrawString("ZONA 2",title,Brushes.Black,1060,190);
   e.Graphics.DrawString("CONTEXTO COMPLETO",title,Brushes.White,140,820);
  }
  e.Graphics.FillRectangle(frame%2==0?Brushes.Red:Brushes.Lime,420,280,80,80);
  using(var font=new Font("Segoe UI",18)) e.Graphics.DrawString("Contenido vivo: "+frame,font,Brushes.White,140,640);
 }
 protected override void Dispose(bool disposing){if(disposing&&timer!=null)timer.Dispose();base.Dispose(disposing);}
}
'@
    [Windows.Forms.Application]::Run([FocusFixture]::new())
    exit
}

Add-Type -AssemblyName System.Drawing
if(-not $ExecutablePath){$ExecutablePath=Join-Path $repoRoot 'build\release\Elite Pen.exe'}
$source=[IO.Path]::GetFullPath($ExecutablePath)
$wgcHelper=Join-Path $repoRoot 'build\release\wgc-capture.exe'
if($Recordable -and -not(Test-Path -LiteralPath $wgcHelper)){throw 'Build the WGC QA helper first with scripts/build.ps1.'}
if(-not(Test-Path -LiteralPath $source)){throw "Missing executable: $source"}
$sandbox=Join-Path ([IO.Path]::GetFullPath([IO.Path]::GetTempPath())) ('elite-pen-focus-qa-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path (Join-Path $sandbox 'data') | Out-Null
$exe=Join-Path $sandbox 'Elite Pen.exe'
Copy-Item -LiteralPath $source -Destination $exe
Set-Content -LiteralPath (Join-Path $sandbox 'portable.flag') -Value 'Isolated focus QA' -Encoding ascii
$ini=Join-Path $sandbox 'data\settings.ini'
Set-Content -LiteralPath $ini -Encoding ascii -Value "[ElitePen]`r`nHotkeySchemeVersion=2`r`nExcludePaletteFromCapture=0`r`nHasPalettePosition=1`r`nPaletteX=40`r`nPaletteY=40`r`nStartInInteractMode=1`r`n"
$captures=Join-Path $repoRoot 'artifacts\qa\focus-captures'
New-Item -ItemType Directory -Force -Path $captures | Out-Null
$oldInstance=$env:ELITE_PEN_QA_INSTANCE_ID
$oldRealHotkeys=$env:ELITE_PEN_QA_REAL_HOTKEYS
$oldRecordable=$env:ELITE_PEN_QA_RECORDABLE_ZOOM
$env:ELITE_PEN_QA_INSTANCE_ID=[Guid]::NewGuid().ToString('N')
$env:ELITE_PEN_QA_REAL_HOTKEYS='1'
if($Recordable){$env:ELITE_PEN_QA_RECORDABLE_ZOOM='1'}else{Remove-Item Env:ELITE_PEN_QA_RECORDABLE_ZOOM -ErrorAction SilentlyContinue}
$failures=[Collections.Generic.List[string]]::new()
function Check([bool]$Condition,[string]$Message){if(-not $Condition){$script:failures.Add($Message);Write-Output ('FAILED: '+$Message)}}
function Find-Window([int]$ProcessId,[string]$Class){
    for($attempt=0;$attempt -lt 100;$attempt++){
        $window=[FocusNative]::Find([uint32]$ProcessId,$Class)
        if($window -ne [IntPtr]::Zero){return $window}
        Start-Sleep -Milliseconds 50
    }
    throw "Window missing: $Class in process $ProcessId"
}
function Query([IntPtr]$Window,[uint32]$Message,[int]$W=0){return [FocusNative]::SendMessage($Window,$Message,[IntPtr]$W,[IntPtr]::Zero).ToInt64()}
function Command([IntPtr]$Window,[int]$Id){$null=[FocusNative]::SendMessage($Window,0x0111,[IntPtr]$Id,[IntPtr]::Zero)}
function Key([int[]]$Modifiers,[int]$Code){
    foreach($modifier in $Modifiers){[FocusNative]::keybd_event([byte]$modifier,0,0,[UIntPtr]::Zero)}
    $flags=if($Code -in @(0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x2D,0x2E)){1}else{0}
    [FocusNative]::keybd_event([byte]$Code,0,$flags,[UIntPtr]::Zero)
    [FocusNative]::keybd_event([byte]$Code,0,($flags -bor 2),[UIntPtr]::Zero)
    for($index=$Modifiers.Count-1;$index -ge 0;$index--){[FocusNative]::keybd_event([byte]$Modifiers[$index],0,2,[UIntPtr]::Zero)}
    Start-Sleep -Milliseconds 90
}
function Wait-FocusFrame([Nullable[int]]$ExpectedLeft=$null) {
    for($attempt=0;$attempt -lt 80;$attempt++){
        if((Query $script:zoom 0x8085) -eq 0 -and
            ($null -eq $ExpectedLeft -or (Query $script:zoom 0x8087 0) -eq $ExpectedLeft)){return}
        Start-Sleep -Milliseconds 20
    }
    throw 'Focus animation did not finish.'
}
function Drag([IntPtr]$Window,[int]$X1,[int]$Y1,[int]$X2,[int]$Y2){
    $start=[IntPtr](($Y1 -shl 16) -bor ($X1 -band 0xffff))
    $finish=[IntPtr](($Y2 -shl 16) -bor ($X2 -band 0xffff))
    $null=[FocusNative]::SendMessage($Window,0x0201,[IntPtr]1,$start)
    $null=[FocusNative]::SendMessage($Window,0x0200,[IntPtr]1,$finish)
    $null=[FocusNative]::SendMessage($Window,0x0202,[IntPtr]0,$finish)
}
function Save-Frame([IntPtr]$Window,[string]$Name){
    $null=[FocusNative]::RedrawWindow($Window,[IntPtr]::Zero,[IntPtr]::Zero,0x181)
    $null=[FocusNative]::DwmFlush();Start-Sleep -Milliseconds 80
    $bounds=[FocusNative+RECT]::new();$null=[FocusNative]::GetWindowRect($Window,[ref]$bounds)
    if($Recordable){
        $file=Join-Path $captures 'wgc-frame.bmp'
        & $wgcHelper $file $bounds.Left $bounds.Top
        if($LASTEXITCODE -ne 0){throw 'WGC screenshot failed.'}
        $bitmap=[Drawing.Bitmap]::new($file)
        try{$bitmap.Save((Join-Path $captures $Name),[Drawing.Imaging.ImageFormat]::Png)}finally{$bitmap.Dispose()}
        return
    }
    $bitmap=[Drawing.Bitmap]::new($bounds.Right-$bounds.Left,$bounds.Bottom-$bounds.Top)
    $graphics=[Drawing.Graphics]::FromImage($bitmap)
    try{$graphics.CopyFromScreen($bounds.Left,$bounds.Top,0,0,$bitmap.Size);$bitmap.Save((Join-Path $captures $Name),[Drawing.Imaging.ImageFormat]::Png)}
    finally{$graphics.Dispose();$bitmap.Dispose()}
}
function Pixel([int]$X,[int]$Y){
    if($Recordable){
        $values=@(& $wgcHelper (Join-Path $captures 'wgc-pixel.bmp') 0 0 $X $Y 1)
        if($LASTEXITCODE -ne 0){throw 'WGC pixel capture failed.'}
        return [int]$values[0]
    }
    $bitmap=[Drawing.Bitmap]::new(1,1);$graphics=[Drawing.Graphics]::FromImage($bitmap)
    try{$graphics.CopyFromScreen($X,$Y,0,0,$bitmap.Size);return $bitmap.GetPixel(0,0).ToArgb()}
    finally{$graphics.Dispose();$bitmap.Dispose()}
}
function Stop-Qa([Diagnostics.Process]$Target,[IntPtr]$Window,[uint32]$Message=0x800C){
    if($Target -and -not $Target.HasExited){
        $null=[FocusNative]::PostMessage($Window,$Message,[IntPtr]::Zero,[IntPtr]::Zero)
        if(-not $Target.WaitForExit(10000)){$Target.Kill();$Target.WaitForExit(3000)|Out-Null}
    }
}
$app=$null;$fixture=$null;$palette=[IntPtr]::Zero;$fixtureWindow=[IntPtr]::Zero
try {
    $fixture=Start-Process -FilePath (Get-Command pwsh).Source -WindowStyle Hidden -PassThru -ArgumentList @('-NoProfile','-File',('"'+$PSCommandPath+'"'),'-SourceFixture') `
        -RedirectStandardOutput (Join-Path $captures 'fixture-output.log') -RedirectStandardError (Join-Path $captures 'fixture-error.log')
    $fixtureWindow=Find-Window $fixture.Id $null
    $null=[FocusNative]::ShowWindow($fixtureWindow,5)
    $null=[FocusNative]::SetWindowPos($fixtureWindow,[IntPtr](-1),0,0,0,0,0x53)
    $null=[FocusNative]::Foreground($fixtureWindow)
    Save-Frame $fixtureWindow 'fixture-before-focus.png'
    $app=Start-Process -FilePath $exe -WindowStyle Hidden -PassThru
    $palette=Find-Window $app.Id 'ElitePen.Palette'
    $zoom=Find-Window $app.Id 'ElitePen.Zoom'
    $focus=Find-Window $app.Id 'ElitePen.Focus'
    Check ((Query $palette 0x806C) -eq 1) 'The extended global shortcut set did not register.'
    $null=[FocusNative]::SetCursorPos(600,400)
    $null=[FocusNative]::Foreground($fixtureWindow)
    Check ([FocusNative]::GetForegroundWindow() -eq $fixtureWindow) 'The test source could not receive foreground focus.'
    Key @(0x11,0x10) 0x4D
    Wait-FocusFrame
    Check ((Query $zoom 0x8083) -eq 1 -and (Query $zoom 0x8084) -eq 1) 'Ctrl Shift M did not start area selection.'
    Check ((Query $zoom 0x808C) -eq [int]$Recordable.IsPresent) 'The selected native/recordable output path was not used.'
    Drag $focus 300 200 900 600
    Check ((Query $zoom 0x8085) -eq 1) 'A selected focus region did not animate.'
    Wait-FocusFrame
    Check ((Query $zoom 0x8087 0) -eq 300 -and (Query $zoom 0x8087 1) -eq 200) 'The selected source area was not retained.'
    $before=@(0..3 | ForEach-Object {Query $zoom 0x8088 $_})
    if(-not $Recordable){
        $native=@(0..3 | ForEach-Object {Query $zoom 0x807F $_})
        Check (($before -join ',') -eq ($native -join ',')) 'The native Magnifier did not receive the selected source.'
    }
    $null=[FocusNative]::SetCursorPos(20,20);Start-Sleep -Milliseconds 100
    $null=[FocusNative]::SetCursorPos(1600,900);Start-Sleep -Milliseconds 100
    $after=@(0..3 | ForEach-Object {Query $zoom 0x8088 $_})
    Check (($before -join ',') -eq ($after -join ',')) 'Moving the cursor displaced the fixed source.'
    Check ((Query $zoom 0x8061) -eq 0) 'Focus froze the content instead of keeping it live.'
    $frameBefore=Query $fixtureWindow 0x80BE
    $samples=if($Recordable){
        @(& $wgcHelper (Join-Path $captures 'wgc-live.bmp') 0 0 540 360 12 | ForEach-Object {[int]$_})
    }else{@(0..7 | ForEach-Object {Pixel 540 360;Start-Sleep -Milliseconds 110})}
    Check ((Query $fixtureWindow 0x80BE) -gt $frameBefore) 'The animated source stopped updating.'
    if($Recordable -or ($samples -contains [Drawing.Color]::Red.ToArgb()) -or
        ($samples -contains [Drawing.Color]::Lime.ToArgb())){
        Check (($samples -contains [Drawing.Color]::Red.ToArgb()) -and
            ($samples -contains [Drawing.Color]::Lime.ToArgb())) 'The live zoom did not show changing source pixels in its fixed frame.'
    }
    Save-Frame $focus 'focus-area-1.png'
    Key @(0x11,0x12,0x10) 0x4D
    Check ((Query $zoom 0x8084) -eq 2) 'The M-derived mask shortcut did not select masking.'
    Drag $focus 240 220 450 390
    Check ((Query $zoom 0x8086) -eq 1) 'Mask selection did not add one opaque area.'
    Save-Frame $focus 'focus-mask.png'
    Check ((Pixel 300 280) -eq [Drawing.Color]::FromArgb(9,11,17).ToArgb()) 'The opaque mask did not cover its selected pixels.'
    Command $focus 6102
    Check ((Query $zoom 0x8086) -eq 1) 'Temporarily hiding masks deleted them.'
    Command $focus 6102
    $null=[FocusNative]::SendMessage($focus,0x0204,[IntPtr]0,[IntPtr]((280 -shl 16) -bor 300))
    Check ((Query $zoom 0x8086) -eq 0) 'Right clicking a mask did not remove it.'
    Key @(0x11,0x12,0x10) 0x4D;Drag $focus 240 220 450 390
    Key @(0x11,0x12,0x10) 0x70
    Check ((Get-Content -LiteralPath $ini -Raw) -match '(?m)^FocusZone1MaskCount=1\r?$') 'Saving zone 1 did not preserve its mask.'
    Key @(0x11,0x12) 0x4D
    Wait-FocusFrame
    Drag $focus 1050 230 1650 630
    Wait-FocusFrame
    Key @(0x11,0x12,0x10) 0x71
    Check ((Query $zoom 0x8087 0) -eq 1050) 'A second selected region did not replace the framing.'
    Key @(0x11,0x12) 0x70
    Wait-FocusFrame
    Check ((Query $zoom 0x8087 0) -eq 300 -and (Query $zoom 0x8086) -eq 1) 'Recalling zone 1 did not restore its framing and mask.'
    Key @(0x11,0x12) 0x71
    Wait-FocusFrame
    Check ((Query $zoom 0x8087 0) -eq 1050 -and (Query $zoom 0x8086) -eq 0) 'Recalling zone 2 did not isolate its own masks.'
    $zoneLefts=@(300,1050,400,500,600,700,800,900)
    for($slot=2;$slot -lt 8;$slot++){
        Command $focus 6100;Wait-FocusFrame
        Drag $focus $zoneLefts[$slot] 200 ($zoneLefts[$slot]+550) 600;Wait-FocusFrame
        Key @(0x11,0x12,0x10) (0x70+$slot)
    }
    for($slot=0;$slot -lt 8;$slot++){
        Key @(0x11,0x12) (0x70+$slot);Wait-FocusFrame $zoneLefts[$slot]
        Check ((Query $zoom 0x8083) -eq 1 -and (Query $zoom 0x8087 0) -eq $zoneLefts[$slot]) "Zone $($slot+1) could not be recalled through its global macropad shortcut."
    }
    Key @(0x11,0x10) 0x22;Wait-FocusFrame 300
    Check ((Query $zoom 0x8087 0) -eq 300) 'The next-zone shortcut did not wrap from zone 8 to zone 1.'
    Key @(0x11,0x10) 0x21;Wait-FocusFrame 900
    Check ((Query $zoom 0x8087 0) -eq 900) 'The previous-zone shortcut did not wrap from zone 1 to zone 8.'
    Key @(0x11,0x12) 0x71;Wait-FocusFrame
    Command $focus 6104;Wait-FocusFrame
    $full=@(0..3 | ForEach-Object {Query $zoom 0x8088 $_})
    Check (($full[2]-$full[0]) -gt ($before[2]-$before[0])) 'Overview did not animate out to the whole monitor.'
    Command $focus 6104;Wait-FocusFrame
    Command $focus 6504
    Check ((Get-Content -LiteralPath $ini -Raw) -match '(?m)^FocusDurationMs=600\r?$') 'Transition speed was not saved.'
    Command $focus 6100;Wait-FocusFrame
    $null=[FocusNative]::SendMessage($zoom,0x0100,[IntPtr]27,[IntPtr]::Zero);Wait-FocusFrame
    Check ((Query $zoom 0x8083) -eq 1 -and (Query $zoom 0x8084) -eq 0 -and
        (Query $zoom 0x8087 0) -eq 1050) 'Canceling a new selection did not restore the previous framing.'
    Save-Frame $focus 'focus-zone-2.png'
    Key @(0x11,0x10) 0x4D;Wait-FocusFrame
    Check ((Query $zoom 0x8083) -eq 0 -and -not [FocusNative]::IsWindowVisible($focus)) 'Ctrl Shift M did not animate out and close focus.'
    Stop-Qa $app $palette;$app.Dispose();$app=$null
    $app=Start-Process -FilePath $exe -WindowStyle Hidden -PassThru
    $palette=Find-Window $app.Id 'ElitePen.Palette';$zoom=Find-Window $app.Id 'ElitePen.Zoom';$focus=Find-Window $app.Id 'ElitePen.Focus'
    Key @(0x11,0x12) 0x70;Wait-FocusFrame
    Check ((Query $zoom 0x8083) -eq 1 -and (Query $zoom 0x8087 0) -eq 300 -and
        (Query $zoom 0x8086) -eq 1) 'Saved zones and their masks did not survive an application restart.'
    $null=[FocusNative]::SendMessage($zoom,0x0100,[IntPtr]27,[IntPtr]::Zero);Wait-FocusFrame
    Check ((Query $zoom 0x8083) -eq 0) 'Escape did not return to the full desktop.'
    $secondaryMonitor=$null
    if(-not $Recordable){
        Add-Type -AssemblyName System.Windows.Forms
        $secondaryMonitor=[Windows.Forms.Screen]::AllScreens |
            Where-Object {$_.Bounds.Left -lt 0 -or $_.Bounds.Top -lt 0} | Select-Object -First 1
        if($secondaryMonitor){
            Stop-Qa $app $palette;$app.Dispose();$app=$null
            $display=$secondaryMonitor.Bounds
            $savedX=$display.Left+100;$savedY=$display.Top+100
            $text=Get-Content -LiteralPath $ini -Raw
            $text=$text -replace '(?m)^FocusZone8Area=.*$',("FocusZone8Area=$savedX,$savedY,600,400")
            Set-Content -LiteralPath $ini -Value $text -Encoding ascii
            $app=Start-Process -FilePath $exe -WindowStyle Hidden -PassThru
            $palette=Find-Window $app.Id 'ElitePen.Palette';$zoom=Find-Window $app.Id 'ElitePen.Zoom';$focus=Find-Window $app.Id 'ElitePen.Focus'
            $null=[FocusNative]::SetCursorPos(600,400)
            Key @(0x11,0x12) 0x77;Wait-FocusFrame $savedX
            $bounds=[FocusNative+RECT]::new();$null=[FocusNative]::GetWindowRect($focus,[ref]$bounds)
            Check ($bounds.Left -eq $display.Left -and $bounds.Top -eq $display.Top -and
                $bounds.Right -eq $display.Right -and $bounds.Bottom -eq $display.Bottom) 'A saved zone did not restore on its negative-coordinate monitor.'
            $native=@(0..3 | ForEach-Object {Query $zoom 0x807F $_})
            Check ($native[0] -ge $display.Left -and $native[1] -ge $display.Top -and
                $native[2] -le $display.Right -and $native[3] -le $display.Bottom) 'The native source escaped its saved monitor.'
            Save-Frame $focus 'focus-negative-monitor.png'
            Key @(0x11,0x10) 0x4D;Wait-FocusFrame
        }
    }
    Copy-Item -LiteralPath $ini -Destination (Join-Path $captures 'preferences-after-qa.ini')
    [ordered]@{source=$source;sha256=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash;
        recordable=$Recordable.IsPresent;animatedPixelSamples=$samples;
        negativeMonitor=$(if($secondaryMonitor){$secondaryMonitor.DeviceName}else{$null});
        fixedSourceBefore=$before;fixedSourceAfter=$after;failures=@($failures);preferencesIsolated=$true} |
        ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $captures 'focus-result.json')
} finally {
    Stop-Qa $app $palette
    Stop-Qa $fixture $fixtureWindow 0x0010
    if($app){$app.Dispose()};if($fixture){$fixture.Dispose()}
    $env:ELITE_PEN_QA_INSTANCE_ID=$oldInstance
    $env:ELITE_PEN_QA_REAL_HOTKEYS=$oldRealHotkeys
    $env:ELITE_PEN_QA_RECORDABLE_ZOOM=$oldRecordable
    $tempRoot=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')+'\'
    $resolved=[IO.Path]::GetFullPath($sandbox)
    if(-not $resolved.StartsWith($tempRoot,[StringComparison]::OrdinalIgnoreCase) -or
       -not ([IO.Path]::GetFileName($resolved)).StartsWith('elite-pen-focus-qa-')){throw 'QA cleanup path is unsafe.'}
    if(Test-Path -LiteralPath $resolved){Remove-Item -LiteralPath $resolved -Recurse -Force}
}
if($failures.Count){throw ($failures -join [Environment]::NewLine)}
Write-Output 'Elite Pen focus zoom: all native UI checks passed'
