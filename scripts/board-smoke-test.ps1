[CmdletBinding()]
param([string]$ExecutablePath,[switch]$PenCompatibilityOnly,[switch]$SkipGlobalHotkeys)
$ErrorActionPreference='Stop'
$repoRoot=Split-Path -Parent $PSScriptRoot
if(-not $ExecutablePath){$ExecutablePath=Join-Path $repoRoot 'build\release\Elite Pen.exe'}
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class BoardNative {
 [StructLayout(LayoutKind.Sequential)] public struct POINT {public int X,Y;}
 [StructLayout(LayoutKind.Sequential)] public struct RECT {public int Left,Top,Right,Bottom;}
 [StructLayout(LayoutKind.Sequential)] public struct POINTER_INFO {
  public uint Type,Id,Frame,Flags;public IntPtr Source,Target;
  public POINT Pixel,Himetric,RawPixel,RawHimetric;public uint Time,History;public int Input;public uint Keys;
  public ulong Performance;public uint Change;
 }
 [StructLayout(LayoutKind.Sequential)] public struct PEN_INFO {public POINTER_INFO Pointer;public uint Flags,Mask,Pressure,Rotation;public int TiltX,TiltY;}
 [StructLayout(LayoutKind.Explicit,Size=152)] public struct TYPE_INFO {[FieldOffset(0)]public uint Type;[FieldOffset(8)]public PEN_INFO Pen;}
 [StructLayout(LayoutKind.Sequential)] public struct MOUSE_INFO {public int X,Y;public uint Data,Flags,Time;public UIntPtr Extra;}
 [StructLayout(LayoutKind.Explicit,Size=40)] public struct MOUSE_INPUT {[FieldOffset(0)]public uint Type;[FieldOffset(8)]public MOUSE_INFO Mouse;}
 public delegate bool Callback(IntPtr w,IntPtr data);
 [DllImport("user32.dll")]public static extern bool EnumWindows(Callback c,IntPtr p);
 [DllImport("user32.dll")]public static extern uint GetWindowThreadProcessId(IntPtr w,out uint pid);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]public static extern int GetClassName(IntPtr w,StringBuilder text,int length);
 [DllImport("user32.dll")]public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr p);
 [DllImport("user32.dll")]public static extern IntPtr SendMessage(IntPtr w,uint m,IntPtr a,IntPtr b);
 [DllImport("user32.dll",SetLastError=true)]public static extern IntPtr SendMessageTimeout(IntPtr w,uint m,IntPtr a,IntPtr b,uint flags,uint timeout,out IntPtr result);
 [DllImport("user32.dll")]public static extern bool PostMessage(IntPtr w,uint m,IntPtr a,IntPtr b);
 [DllImport("user32.dll")]public static extern bool IsWindowVisible(IntPtr w);
 [DllImport("user32.dll")]public static extern bool GetClientRect(IntPtr w,out RECT r);
 [DllImport("user32.dll")]public static extern bool GetWindowRect(IntPtr w,out RECT r);
 [DllImport("user32.dll")]public static extern bool ClientToScreen(IntPtr w,ref POINT p);
 [DllImport("user32.dll")]public static extern IntPtr WindowFromPoint(POINT p);
 [DllImport("user32.dll")]public static extern int GetSystemMetrics(int index);
 [DllImport("user32.dll",SetLastError=true)]public static extern uint SendInput(uint count,[In]MOUSE_INPUT[] input,int size);
 [DllImport("user32.dll")]public static extern bool SetForegroundWindow(IntPtr w);
 [DllImport("user32.dll")]public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")]public static extern bool AttachThreadInput(uint from,uint to,bool attach);
 [DllImport("kernel32.dll")]public static extern uint GetCurrentThreadId();
 [DllImport("user32.dll")]public static extern bool ShowWindow(IntPtr w,int cmd);
 [DllImport("user32.dll")]public static extern bool SetWindowPos(IntPtr w,IntPtr after,int x,int y,int width,int height,uint flags);
 [DllImport("user32.dll")]public static extern bool OpenClipboard(IntPtr w);
 [DllImport("user32.dll")]public static extern bool EmptyClipboard();
 [DllImport("user32.dll")]public static extern IntPtr SetClipboardData(uint type,IntPtr data);
 [DllImport("user32.dll")]public static extern bool CloseClipboard();
 [DllImport("user32.dll")]public static extern void keybd_event(byte key,byte scan,uint flags,UIntPtr data);
 [DllImport("user32.dll",SetLastError=true)]public static extern IntPtr CreateSyntheticPointerDevice(uint type,uint count,uint mode);
 [DllImport("user32.dll",SetLastError=true)]public static extern bool InjectSyntheticPointerInput(IntPtr device,[In]TYPE_INFO[] samples,uint count);
 [DllImport("user32.dll")]public static extern void DestroySyntheticPointerDevice(IntPtr device);
 [DllImport("dwmapi.dll")]public static extern int DwmFlush();
 [DllImport("user32.dll")]public static extern bool RedrawWindow(IntPtr w,IntPtr r,IntPtr region,uint flags);
 public static IntPtr Find(uint pid,string name) {IntPtr result=IntPtr.Zero;EnumWindows((w,p)=>{uint owner;GetWindowThreadProcessId(w,out owner);if(owner!=pid)return true;var text=new StringBuilder(256);GetClassName(w,text,256);if(text.ToString()==name){result=w;return false;}return true;},IntPtr.Zero);return result;}
 public static bool Foreground(IntPtr window){uint pid;uint current=GetCurrentThreadId();uint other=GetWindowThreadProcessId(GetForegroundWindow(),out pid);bool attached=current!=other&&AttachThreadInput(current,other,true);try{ShowWindow(window,5);SetWindowPos(window,new IntPtr(-1),0,0,0,0,0x43);SetForegroundWindow(window);}finally{if(attached)AttachThreadInput(current,other,false);}if(GetForegroundWindow()!=window){keybd_event(0x12,0,0,UIntPtr.Zero);SetForegroundWindow(window);keybd_event(0x12,0,2,UIntPtr.Zero);}return GetForegroundWindow()==window;}
 // InjectSyntheticPointerInput uses coordinates relative to the virtual desktop's top-left.
 public static bool Pen(IntPtr device,int x,int y,uint flags,uint pressure){var sample=new TYPE_INFO();sample.Type=3;sample.Pen.Pointer.Type=3;sample.Pen.Pointer.Id=1;sample.Pen.Pointer.Flags=flags;sample.Pen.Pointer.Pixel=new POINT{X=x-GetSystemMetrics(76),Y=y-GetSystemMetrics(77)};sample.Pen.Mask=1;sample.Pen.Pressure=pressure;return InjectSyntheticPointerInput(device,new[]{sample},1);}
 public static bool Mouse(int x,int y,uint flags,bool pen){var sample=new MOUSE_INPUT();sample.Mouse.X=(int)((long)(x-GetSystemMetrics(76))*65535/(GetSystemMetrics(78)-1));sample.Mouse.Y=(int)((long)(y-GetSystemMetrics(77))*65535/(GetSystemMetrics(79)-1));sample.Mouse.Flags=flags|0xC001;sample.Mouse.Extra=pen?new UIntPtr(0xFF515700):UIntPtr.Zero;return SendInput(1,new[]{sample},Marshal.SizeOf<MOUSE_INPUT>())==1;}
}
'@
$null=[BoardNative]::SetThreadDpiAwarenessContext([IntPtr](-4))
$captures=Join-Path $repoRoot 'artifacts\qa\board-captures'
New-Item -ItemType Directory -Force -Path $captures | Out-Null
$sandbox=Join-Path ([IO.Path]::GetTempPath()) ('elite-pen-board-ui-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path (Join-Path $sandbox 'data\boards') | Out-Null
$exe=Join-Path $sandbox 'Elite Pen.exe'
Copy-Item -LiteralPath $ExecutablePath -Destination $exe
Set-Content -LiteralPath (Join-Path $sandbox 'portable.flag') -Value 'Isolated board QA' -Encoding ascii
Set-Content -LiteralPath (Join-Path $sandbox 'data\settings.ini') -Encoding ascii -Value "[ElitePen]`r`nHotkeySchemeVersion=2`r`nExcludePaletteFromCapture=0`r`nStartInInteractMode=1`r`nHasPalettePosition=1`r`nPaletteX=50`r`nPaletteY=50`r`nHotkeyBoardOpen=7,123`r`n"
$oldInstance=$env:ELITE_PEN_QA_INSTANCE_ID;$oldBoard=$env:ELITE_PEN_BOARD_QA;$oldRealHotkeys=$env:ELITE_PEN_QA_REAL_HOTKEYS
$env:ELITE_PEN_QA_INSTANCE_ID=[Guid]::NewGuid().ToString('N');$env:ELITE_PEN_BOARD_QA='1';$env:ELITE_PEN_QA_REAL_HOTKEYS='1'
if($PenCompatibilityOnly -or $SkipGlobalHotkeys){Remove-Item Env:ELITE_PEN_QA_REAL_HOTKEYS -ErrorAction SilentlyContinue}
$failures=[Collections.Generic.List[string]]::new();$checks=0
function Check([bool]$Condition,[string]$Message){$script:checks++;if(-not $Condition){$script:failures.Add($Message);Write-Output "FAIL: $Message"}}
function Find([int]$ProcessId,[string]$Class){for($n=0;$n -lt 120;$n++){$w=[BoardNative]::Find([uint32]$ProcessId,$Class);if($w -ne [IntPtr]::Zero){return $w};Start-Sleep -Milliseconds 50};throw "Missing $Class"}
function Send([IntPtr]$Window,[uint32]$Message,[long]$W=0,[long]$L=0){$result=[IntPtr]::Zero;if([BoardNative]::SendMessageTimeout($Window,$Message,[IntPtr]$W,[IntPtr]$L,2,7000,[ref]$result) -eq [IntPtr]::Zero){throw "UI command timed out: $Message"};return $result.ToInt64()}
function Qa([int]$Offset,[long]$W=0,[int]$X=0,[int]$Y=0){return Send $script:board (0x80C8+$Offset) $W (($Y -shl 16) -bor ($X -band 0xffff))}
function Button([int]$Command){$x=Qa 33 $Command;$y=Qa 34 $Command;if($x -lt 0 -or $y -lt 0){throw "Control $Command is outside the visible panel"};return @([int]$x,[int]$y)}
function Tap-Command([int]$Command,[bool]$Pen=$false){$p=Button $Command;Mouse $p[0] $p[1] 2 $Pen;Mouse $p[0] $p[1] 4 $Pen;Wait-Frame}
function Wait-Frame {$null=[BoardNative]::DwmFlush();Start-Sleep -Milliseconds 180}
function Key([int[]]$Modifiers,[int]$Code){foreach($m in $Modifiers){[BoardNative]::keybd_event([byte]$m,0,0,[UIntPtr]::Zero)};[BoardNative]::keybd_event([byte]$Code,0,0,[UIntPtr]::Zero);[BoardNative]::keybd_event([byte]$Code,0,2,[UIntPtr]::Zero);for($n=$Modifiers.Count-1;$n -ge 0;$n--){[BoardNative]::keybd_event([byte]$Modifiers[$n],0,2,[UIntPtr]::Zero)};Wait-Frame}
function Mouse([int]$X,[int]$Y,[uint32]$Flags,[bool]$Pen){$p=[BoardNative+POINT]::new();$p.X=$X;$p.Y=$Y;$null=[BoardNative]::ClientToScreen($script:board,[ref]$p);if(-not [BoardNative]::Mouse($p.X,$p.Y,$Flags,$Pen)){throw 'Mouse/pen input injection failed'};Start-Sleep -Milliseconds 45}
function Pointer-Stroke([bool]$Pen,[int]$Y){Mouse 600 $Y 2 $Pen;for($x=625;$x -le 800;$x+=25){Mouse $x $Y 0 $Pen};Mouse 820 $Y 4 $Pen;Wait-Frame}
function Pen-Tap([IntPtr]$Device,[int]$X,[int]$Y){$p=[BoardNative+POINT]::new();$p.X=$X;$p.Y=$Y;$null=[BoardNative]::ClientToScreen($script:board,[ref]$p);$ok=[BoardNative]::Pen($Device,$p.X,$p.Y,0x10016,512);Start-Sleep -Milliseconds 60;$ok=[BoardNative]::Pen($Device,$p.X,$p.Y,0x40012,0) -and $ok;Check $ok 'Windows accepted the native pen tap';Wait-Frame}
function Red-Stroke([int]$Y){$null=Qa 20;$bitmap=[Drawing.Bitmap]::FromFile((Join-Path $sandbox 'data\boards\qa-view.png'));try{$red=0;for($x=620;$x -lt 790;$x++){for($row=$Y-5;$row -lt $Y+6;$row++){$p=$bitmap.GetPixel($x,$row);if($p.R -gt 180 -and $p.G -lt 120 -and $p.B -lt 120){$red++}}};return $red -gt 30}finally{$bitmap.Dispose()}}
function Drag([IntPtr]$Window,[int]$X1,[int]$Y1,[int]$X2,[int]$Y2){$null=Send $Window 0x201 1 (($Y1 -shl 16) -bor ($X1 -band 0xffff));$null=Send $Window 0x200 1 (($Y2 -shl 16) -bor ($X2 -band 0xffff));$null=Send $Window 0x202 0 (($Y2 -shl 16) -bor ($X2 -band 0xffff));Wait-Frame}
function Frame([string]$Name){$null=[BoardNative]::Foreground($script:board);$null=[BoardNative]::RedrawWindow($script:board,[IntPtr]::Zero,[IntPtr]::Zero,0x181);Wait-Frame;$r=[BoardNative+RECT]::new();$null=[BoardNative]::GetWindowRect($script:board,[ref]$r);$image=[Drawing.Bitmap]::new($r.Right-$r.Left,$r.Bottom-$r.Top);$g=[Drawing.Graphics]::FromImage($image);try{$g.CopyFromScreen($r.Left,$r.Top,0,0,$image.Size);$image.Save((Join-Path $captures $Name),[Drawing.Imaging.ImageFormat]::Png)}finally{$g.Dispose();$image.Dispose()}}
function Start-App {$script:app=Start-Process -FilePath $exe -WindowStyle Hidden -PassThru;$script:palette=Find $script:app.Id 'ElitePen.Palette';$null=Send $script:palette 0x312 70;$script:board=Find $script:app.Id 'ElitePenBoardWindow';Wait-Frame}
function Stop-App {
 if(-not $script:app){return}
 if(-not $script:app.HasExited){
  $null=[BoardNative]::PostMessage($script:palette,0x800C,[IntPtr]::Zero,[IntPtr]::Zero)
  if(-not $script:app.WaitForExit(10000)){
   $script:app.Kill();$script:app.WaitForExit(3000)|Out-Null
   throw 'Application shutdown timed out'
  }
 }
 $script:app.Refresh()
 if($script:app.ExitCode -ne 0){throw "Application shutdown failed: $($script:app.ExitCode)"}
}
$app=$null;$board=[IntPtr]::Zero;$palette=[IntPtr]::Zero;$device=[IntPtr]::Zero;$nativePenVerified=$false
$clipboardBackup=[Windows.Forms.DataObject]::new()
try {$oldClipboard=[Windows.Forms.Clipboard]::GetDataObject();if($oldClipboard){foreach($format in $oldClipboard.GetFormats($false)){try{$clipboardBackup.SetData($format,$false,$oldClipboard.GetData($format,$false))}catch{}}}}catch{}
try {
 Start-App
 Check ([BoardNative]::IsWindowVisible($board)) 'Board opens from its global action'
 Check (-not [BoardNative]::IsWindowVisible($palette)) 'Desktop palette hides while drawing on board'
 Check ((Send $palette 0x805E) -eq 0) 'Desktop document starts unchanged'
 if($PenCompatibilityOnly){
  Check ([BoardNative]::Foreground($board)) 'Compatibility test owns foreground focus';Wait-Frame
  $top=[int](Qa 7)
  # This is a real WM_MOUSE stream tagged as pen, as produced by compatibility drivers.
  Tap-Command 104 $true
  Pointer-Stroke $false ($top+160)
  Check ((Qa 0) -eq 1) 'Ordinary mouse still draws after a pen toolbar tap'
  Check (Red-Stroke 160) 'Pen compatibility tap selects the red swatch'
  $null=Qa 12
  Pointer-Stroke $true ($top+230)
  Check ((Qa 0) -eq 1) 'Pen compatibility contact creates exactly one complete stroke'
  Check (Red-Stroke 230) 'Pen compatibility stroke uses the chosen color'
  $null=Qa 17;Check ((Qa 0) -eq 0) 'One undo removes one compatibility pen stroke'
  $null=Qa 18;Check ((Qa 0) -eq 1) 'Redo restores compatibility pen stroke'
  $result=[ordered]@{Checks=$checks;Failures=@($failures);Mode='PenTaggedMouse';Sandbox=$sandbox}
  $result|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $captures 'pen-compatibility-result.json') -Encoding utf8
  if($failures.Count){throw "$($failures.Count) of $checks pen compatibility checks failed"}
  Write-Output "$checks pen compatibility checks passed";return
 }
 if(-not $SkipGlobalHotkeys){
  $null=[BoardNative]::Foreground($board);Key @(0x11,0x10,0x12) 123
  Check (-not [BoardNative]::IsWindowVisible($board)) 'Assigned macropad shortcut closes the board'
  Key @(0x11,0x10,0x12) 123
  Check ([BoardNative]::IsWindowVisible($board)) 'Assigned macropad shortcut reopens the board'
 }
 # F11 is immersive: no title, command rail, or status strip; toolbox stays live.
 $windowed=[BoardNative+RECT]::new();$null=[BoardNative]::GetWindowRect($board,[ref]$windowed)
 $null=[BoardNative]::Foreground($board);Key @() 0x7A
 Check ((Qa 43) -eq 1) 'F11 enters immersive fullscreen'
 Check ((Qa 7) -eq 0 -and (Qa 33 1) -eq -1 -and (Qa 33 14) -eq -1) 'Fullscreen removes the command header and its hit zones'
 $client=[BoardNative+RECT]::new();$null=[BoardNative]::GetClientRect($board,[ref]$client)
 Check ((Qa 44) -eq $client.Bottom) 'Fullscreen gives the entire client height to the canvas'
 Check ((Qa 33 30) -gt 0 -and (Qa 33 31) -gt 0) 'Fullscreen toolbox retains exit and settings buttons'
 Tap-Command 104 $true;Pointer-Stroke $true 200
 Check ((Qa 0) -eq 1 -and (Red-Stroke 200)) 'Compatibility pen draws in fullscreen without a toolbar coordinate offset'
 Drag $board 300 12 550 12
 Check ((Qa 0) -eq 2) 'Former title/header area is now drawable canvas'
 $null=Qa 17;$null=Qa 17
 Check ((Qa 0) -eq 0) 'Fullscreen strokes keep independent undo operations'
 Tap-Command 13;Frame 'whiteboard-fullscreen.png'
 $fullImage=[Drawing.Bitmap]::FromFile((Join-Path $captures 'whiteboard-fullscreen.png'))
 try{
  Check ($fullImage.GetPixel(50,10).R -lt 30) 'Fullscreen top edge contains canvas, not a title or command strip'
  Check ($fullImage.GetPixel(50,$fullImage.Height-10).R -lt 30) 'Fullscreen bottom edge contains canvas, not a status strip'
 }finally{$fullImage.Dispose()}
 Tap-Command 30
 Check ((Qa 43) -eq 0 -and (Qa 7) -eq 50) 'Toolbox button exits fullscreen and restores the header'
 $restored=[BoardNative+RECT]::new();$null=[BoardNative]::GetWindowRect($board,[ref]$restored)
 Check ($restored.Left -eq $windowed.Left -and $restored.Top -eq $windowed.Top -and $restored.Right -eq $windowed.Right -and $restored.Bottom -eq $windowed.Bottom) 'Fullscreen restores the previous window position and size'
 Tap-Command 30;Key @() 0x1B
 Check ((Qa 43) -eq 0 -and [BoardNative]::IsWindowVisible($board)) 'Escape leaves fullscreen while keeping the board open'
 $null=Qa 9 300;Wait-Frame;Tap-Command 30
 Check ((Qa 43) -eq 1 -and (Qa 23) -eq 300 -and (Qa 33 30) -gt 0) '300 percent toolbox stays usable in fullscreen'
 Tap-Command 30;$null=Qa 9 100;Wait-Frame;Tap-Command 13;Tap-Command 100
 $null=Qa 28 40
 $null=Qa 3 64 100 160;$null=Qa 4 64 200 160;$null=Qa 4 1024 350 160;$null=Qa 4 1024 500 160;$null=Qa 5 1024 580 160
 Check ((Qa 0) -eq 1 -and (Qa 8) -ge 4) 'Pressure points use the drawing input path'
 Check ((Qa 21) -eq 64 -and (Qa 22) -eq 1024) 'Pressure is preserved along the stroke'
 $null=Qa 20
 $image=[Drawing.Bitmap]::FromFile((Join-Path $sandbox 'data\boards\qa-view.png'))
 try {$thin=0;$thick=0;for($y=110;$y -lt 211;$y++){if($image.GetPixel(150,$y).R -lt 100){$thin++};if($image.GetPixel(450,$y).R -lt 100){$thick++}};Check ($thin -ge 8 -and $thick -gt $thin*1.6) 'Rendered pressure makes the later part visibly wider'}finally{$image.Dispose()}
 Frame 'whiteboard-pressure.png'
 # Exercise real Windows Ink messages as well as the deterministic model path.
 $device=[BoardNative]::CreateSyntheticPointerDevice(3,1,1)
 Check ($device -ne [IntPtr]::Zero) 'Windows synthetic pen device is available'
 if($device -ne [IntPtr]::Zero){
  Check ([BoardNative]::Foreground($board)) 'Native pen target owns foreground focus';Wait-Frame
  $origin=[BoardNative+POINT]::new();$null=[BoardNative]::ClientToScreen($board,[ref]$origin);$top=[int](Qa 7)
  $target=[BoardNative+POINT]::new();$target.X=$origin.X+200;$target.Y=$origin.Y+$top+270
  Check ([BoardNative]::WindowFromPoint($target) -eq $board) 'Pen injection point targets the board client'
  $ok=[BoardNative]::Pen($device,$origin.X+200,$origin.Y+$top+270,0x10016,64);Start-Sleep -Milliseconds 50
  for($n=1;$n -le 10;$n++){$ok=[BoardNative]::Pen($device,$origin.X+200+$n*30,$origin.Y+$top+270,0x20016,[uint32](64+$n*80)) -and $ok;Start-Sleep -Milliseconds 15}
  $ok=[BoardNative]::Pen($device,$origin.X+530,$origin.Y+$top+270,0x40012,0) -and $ok;Wait-Frame
  Write-Output "Native pen: objects=$(Qa 0), points=$(Qa 8), pressure=$(Qa 21)–$(Qa 22)"
  Write-Output "Pointer events: down=$(Qa 30 0) update=$(Qa 30 1) up=$(Qa 30 2) cancel=$(Qa 30 3) error=$(Qa 30 4); screen=$($target.X),$($target.Y)"
  Check $ok 'Windows accepted native pen injection'
  Check ((Qa 0) -eq 2 -and (Qa 8) -ge 3) 'WM_POINTER pen history records a full stroke'
  Check ((Qa 21) -lt 150 -and (Qa 22) -gt 500) 'Native pen pressure varies across samples'
  $nativePenVerified=(Qa 30 0) -gt 0 -and (Qa 0) -eq 2
  # Native controls must consume one contact and then release ownership for the next tool.
  $toggle=Button 13
  Pen-Tap $device $toggle[0] $toggle[1];Check ((Qa 10) -eq 1) 'Native pen toolbar contact toggles blackboard exactly once'
  Pen-Tap $device $toggle[0] $toggle[1];Check ((Qa 10) -eq 0) 'Native pen can immediately toggle whiteboard again'
  $red=Button 104;Pen-Tap $device $red[0] $red[1]
  Pointer-Stroke $false ($top+350)
  Check (Red-Stroke 350) 'Native pen tap selects the red color for subsequent drawing'
  $objects=Qa 0;Pen-Tap $device 850 ($top+410)
  Check ((Qa 0) -eq $objects+1) 'Native pen draws again after using toolbar controls'
  Tap-Command 100
  [BoardNative]::DestroySyntheticPointerDevice($device);$device=[IntPtr]::Zero
 }
 $before=Qa 0;$null=[BoardNative]::Foreground($board);Key @(0x11) 0x5A
 Check ((Qa 0) -eq $before-1) 'Native Ctrl+Z targets the board history'
 Key @(0x11) 0x59;Check ((Qa 0) -eq $before) 'Native Ctrl+Y redoes the board stroke'
 $null=Qa 17;Check ((Qa 0) -eq $before-1) 'Undo board stroke';$null=Qa 18;Check ((Qa 0) -eq $before) 'Redo board stroke'
 # Click selection and move/resize, using actual mouse messages.
 $null=Qa 2 5;$top=[int](Qa 7);Drag $board 150 ($top+160) 150 ($top+160)
 $initialLeft=Qa 25;Drag $board 150 ($top+160) 250 ($top+210)
 Check ((Qa 24) -eq 1) 'Select with a click on ink'
 $left=Qa 25;$width=Qa 27
 Check ([Math]::Abs($left-$initialLeft-10000) -le 2) 'Selection moves in document coordinates'
 $cornerX=[int](($left+$width)/100)
 # Query the exact bottom so the handle test does not assume a pressure padding formula.
 $selectionBottom=[int](Qa 31)
 Drag $board $cornerX ($top+$selectionBottom) ($cornerX+100) ($top+$selectionBottom+10)
 Check ((Qa 27) -gt $width+5000) 'Selection corner scales the object'
 $null=Qa 2 4;$null=Qa 2 5;Drag $board 250 ($top+210) 280 ($top+230)
 Check ((Qa 0) -eq $before) 'Selection after eraser does not erase ink'
 $null=Qa 2 2
 # Embed a clipboard bitmap, release the clipboard, and save the board.
 $bitmap=[Drawing.Bitmap]::new(200,120);$graphics=[Drawing.Graphics]::FromImage($bitmap);$graphics.Clear([Drawing.Color]::RoyalBlue);$graphics.Dispose();[Windows.Forms.Clipboard]::SetImage($bitmap);$bitmap.Dispose()
 $null=Qa 16;Check ((Qa 19) -eq 1 -and (Qa 0) -eq $before+1) 'Clipboard image embeds as a selectable object'
 $null=Qa 14;Check (Test-Path -LiteralPath (Join-Path $sandbox 'data\boards\qa-named.epboard')) 'Named editable board saves locally'
 # Floating controls use real clicks at their current layout positions.
 $objects=Qa 0;Tap-Command 202
 Check ((Qa 36) -eq 800 -and (Qa 0) -eq $objects) 'Visual width chip selects 8 px without adding ink'
 foreach($scale in @(80,100,125,150,200,300)){
  $null=Qa 9 $scale;Wait-Frame
  Check ((Qa 23) -eq $scale) "Panel retains requested scale $scale"
  $p=Button 2;Check ($p[0] -gt 0 -and $p[1] -gt 50) "Pen tool remains visible at scale $scale"
 }
 Check ((Qa 7) -eq 50) 'Panel enlargement preserves the canvas viewport height'
 Frame 'whiteboard-panel-300.png'
 $null=Qa 11;Wait-Frame;Check ((Qa 10) -eq 1) 'Blackboard changes the background'
 $null=Qa 20;$black=[Drawing.Bitmap]::FromFile((Join-Path $sandbox 'data\boards\qa-view.png'));try{Check ($black.GetPixel(10,10).R -lt 30) 'Blackboard renders a dark background'}finally{$black.Dispose()}
 Frame 'blackboard-panel-300.png'
 $p=Button 2;$pt=[BoardNative+POINT]::new();$pt.X=$p[0];$pt.Y=$p[1];$null=[BoardNative]::ClientToScreen($board,[ref]$pt)
 $null=Send $board 0x20A ([long](-120 -shl 16)) (($pt.Y -shl 16) -bor ($pt.X -band 0xffff));Wait-Frame
 Check ((Qa 41) -gt 0) 'Wheel scrolls oversized panel instead of zooming the canvas'
 $null=Qa 9 100;Wait-Frame
 Tap-Command 25;Check ((Qa 38) -eq 1) 'Panel collapses without hiding the canvas'
 Tap-Command 25;Check ((Qa 38) -eq 0) 'Panel expands from its header'
 $left=Qa 39;$panelY=Qa 40
 Drag $board ($left+25) ($panelY+18) ($left-70) ($panelY+60)
 Check ((Qa 39) -eq $left) 'Pinned panel ignores header dragging'
 Tap-Command 24;Check ((Qa 37) -eq 0) 'Pin unlocks panel movement'
 Drag $board ($left+25) ($panelY+18) ($left-70) ($panelY+60)
 Check ((Qa 39) -lt $left-50) 'Unlocked panel moves by its header'
 Tap-Command 24;Check ((Qa 37) -eq 1) 'Panel can be fixed at its new position'
 # Portrait/narrow window at 300%: the scale is retained and the body reflows.
 $bounds=[BoardNative+RECT]::new();$null=[BoardNative]::GetWindowRect($board,[ref]$bounds)
 $null=[BoardNative]::SetWindowPos($board,[IntPtr]::Zero,0,0,760,800,0x16);Wait-Frame
 $null=Qa 9 300;Wait-Frame;$panelWidth=Qa 35
 $p=Button 2;Tap-Command 2
 Check ($panelWidth -le 736 -and (Qa 23) -eq 300) 'Narrow window reflows the panel without silently shrinking the scale'
 Check ((Qa 42) -eq 1) 'Pen remains actionable on narrow window at 300 percent'
 $pt=[BoardNative+POINT]::new();$pt.X=$p[0];$pt.Y=$p[1];$null=[BoardNative]::ClientToScreen($board,[ref]$pt)
 for($n=0;$n -lt 16;$n++){$null=Send $board 0x20A ([long](-120 -shl 16)) (($pt.Y -shl 16) -bor ($pt.X -band 0xffff))}
 Wait-Frame;$p=Button 203;Tap-Command 203
 Check ((Qa 36) -eq 1600) 'Scrolled 300 percent width controls remain actionable'
 Frame 'whiteboard-narrow-300.png'
 $null=[BoardNative]::SetWindowPos($board,[IntPtr]::Zero,0,0,($bounds.Right-$bounds.Left),($bounds.Bottom-$bounds.Top),0x16);Wait-Frame
 $null=Qa 9 300;Wait-Frame
 $null=Qa 6;Check (Test-Path -LiteralPath (Join-Path $sandbox 'data\boards\recovery.epboard')) 'Recovery snapshot exists'
 $null=[BoardNative]::ShowWindow($board,6);Wait-Frame
 Check ([BoardNative]::IsWindowVisible($palette)) 'Minimizing restores the desktop palette'
 $null=Send $palette 0x312 70;Wait-Frame
 Check ((Qa 0) -eq $before+1) 'Minimized board retains its objects'
 $null=Send $board 0x10;Wait-Frame
 Check (-not [BoardNative]::IsWindowVisible($board) -and [BoardNative]::IsWindowVisible($palette)) 'Close returns to desktop without discarding board'
 $null=Send $palette 0x312 70;Wait-Frame
 Check ((Qa 0) -eq $before+1) 'Reopen preserves the same board'
 $null=Qa 6
 # Simulate an abrupt process exit only in the isolated portable copy.
 $null=Qa 11
 $recovery=Join-Path $sandbox 'data\boards\recovery.epboard';$savedAt=(Get-Item -LiteralPath $recovery).LastWriteTimeUtc
 Start-Sleep -Milliseconds 3300
 Check ((Get-Item -LiteralPath $recovery).LastWriteTimeUtc -gt $savedAt) 'Idle recovery saves without an explicit save command'
 $null=Qa 11;Start-Sleep -Milliseconds 3300
 $app.Kill();$app.WaitForExit(5000)|Out-Null
 Start-App
 Check ((Qa 0) -eq $before+1 -and (Qa 19) -eq 1) 'Crash recovery restores ink and embedded image'
 Check ((Qa 23) -eq 300 -and (Qa 10) -eq 1) 'Recovery restores panel size and blackboard'
 $null=Qa 15;Check ((Qa 0) -eq $before+1 -and (Qa 10) -eq 0) 'Explicit named file reload restores its saved background'
 $null=Qa 9 100;Wait-Frame
 $null=Qa 13;$cancelCapture=Find $app.Id 'ElitePenBoardCaptureWindow';$null=Send $cancelCapture 0x201 1 ((200 -shl 16) -bor 200);$null=Send $palette 0x312 70;Wait-Frame
 Check (-not [BoardNative]::IsWindowVisible($cancelCapture)) 'Board shortcut cancels an active capture'
 Check ((Qa 32) -eq 0) 'Cancelling capture releases the mouse'
 $null=Send $palette 0x312 70;Wait-Frame
 $null=Qa 13;$capture=Find $app.Id 'ElitePenBoardCaptureWindow';Wait-Frame
 Drag $capture 200 200 600 460
 Start-Sleep -Milliseconds 600
 Check ((Qa 0) -eq $before+2 -and (Qa 19) -eq 1) 'Screen area capture inserts an editable image'
 Check ([BoardNative]::IsWindowVisible($board) -and -not [BoardNative]::IsWindowVisible($capture)) 'Capture returns to board'
 $null=Qa 20;Check ((Get-Item -LiteralPath (Join-Path $sandbox 'data\boards\qa-view.png')).Length -gt 1000) 'Current view exports as PNG'
 Frame 'board-with-capture.png'
 Check ((Send $palette 0x805E) -eq 0) 'Board actions leave desktop annotations untouched'
 $null=Qa 12;Check ((Qa 0) -eq 0) 'New board starts empty'
 Check (@(Get-ChildItem -LiteralPath (Join-Path $sandbox 'data\boards') -Filter 'sesion-*.epboard').Count -ge 1) 'New and open archive the previous session'
 Stop-App
 $result=[ordered]@{Checks=$checks;Failures=@($failures);Sandbox=$sandbox;NativePen=$nativePenVerified;Captures=$captures}
 $result|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $captures 'result.json') -Encoding utf8
 if($failures.Count){throw "$($failures.Count) of $checks board UI checks failed"}
 Write-Output "$checks board UI checks passed. Evidence: $captures"
} finally {
 if($device -ne [IntPtr]::Zero){[BoardNative]::DestroySyntheticPointerDevice($device)}
 Stop-App
 try{[Windows.Forms.Clipboard]::SetDataObject($clipboardBackup,$true)}catch{}
 $env:ELITE_PEN_QA_INSTANCE_ID=$oldInstance;$env:ELITE_PEN_BOARD_QA=$oldBoard;$env:ELITE_PEN_QA_REAL_HOTKEYS=$oldRealHotkeys
}
