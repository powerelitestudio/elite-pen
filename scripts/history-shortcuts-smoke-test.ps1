[CmdletBinding()]
param([string]$ExecutablePath)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not $ExecutablePath) { $ExecutablePath = Join-Path $repoRoot 'build\release\Elite Pen.exe' }
if (Get-Process -Name 'Elite Pen' -ErrorAction SilentlyContinue) {
    throw 'Close Elite Pen before real-keyboard QA; the test must not affect another instance.'
}
# Real SendInput events go only to a disposable native window, never to the user's
# editor. The app uses the production hook but QA accepts only our tagged events.
Add-Type @'
using System;
using System.Text;
using System.Threading;
using System.Runtime.InteropServices;
public static class HistoryQa {
    public delegate IntPtr WndProc(IntPtr w, uint m, IntPtr a, IntPtr b);
    [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)] public struct WC {
        public uint style; public WndProc proc; public int clsExtra, wndExtra;
        public IntPtr instance, icon, cursor, background; public string menu, name;
    }
    [StructLayout(LayoutKind.Sequential)] public struct MSG {
        public IntPtr hwnd; public uint message; public UIntPtr wp; public IntPtr lp;
        public uint time; public int x,y; public uint extra;
    }
    [StructLayout(LayoutKind.Explicit, Size=40)] public struct INPUT {
        [FieldOffset(0)] public uint type;
        [FieldOffset(8)] public ushort key;
        [FieldOffset(12)] public uint flags;
        [FieldOffset(24)] public UIntPtr extra;
    }
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern ushort RegisterClass(ref WC c);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr CreateWindowEx(uint ex, string cls, string title, uint style, int x,int y,int width,int height,IntPtr parent,IntPtr menu,IntPtr instance,IntPtr param);
    [DllImport("user32.dll")] public static extern IntPtr DefWindowProc(IntPtr w,uint m,IntPtr a,IntPtr b);
    [DllImport("user32.dll")] public static extern int GetMessage(out MSG m,IntPtr w,uint min,uint max);
    [DllImport("user32.dll")] public static extern bool TranslateMessage(ref MSG m);
    [DllImport("user32.dll")] public static extern IntPtr DispatchMessage(ref MSG m);
    [DllImport("user32.dll")] public static extern void PostQuitMessage(int code);
    [DllImport("user32.dll")] public static extern bool DestroyWindow(IntPtr w);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr w);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint from,uint to,bool attach);
    [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr w);
    [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
    [DllImport("user32.dll")] public static extern short GetKeyState(int key);
    [DllImport("user32.dll")] public static extern short GetAsyncKeyState(int key);
    [DllImport("user32.dll")] public static extern uint SendInput(uint n,INPUT[] keys,int size);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr w,uint m,IntPtr a,IntPtr b);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr w,uint m,IntPtr a,IntPtr b);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindowEx(IntPtr parent,IntPtr after,string cls,string title);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr w,out uint pid);
    [DllImport("kernel32.dll")] public static extern IntPtr GetModuleHandle(string name);
    public static IntPtr Window;
    public static int Undos, Redos;
    static WndProc callback = Proc;
    static Thread thread;
    static IntPtr Proc(IntPtr w,uint m,IntPtr a,IntPtr b) {
        if (m==0x100 && (GetKeyState(0x11)&0x8000)!=0) {
            if (a.ToInt64()==90) Interlocked.Increment(ref Undos);
            if (a.ToInt64()==89) Interlocked.Increment(ref Redos);
            return IntPtr.Zero;
        }
        if (m==0x10) { DestroyWindow(w); return IntPtr.Zero; }
        if (m==2) { PostQuitMessage(0); return IntPtr.Zero; }
        return DefWindowProc(w,m,a,b);
    }
    public static void Start() {
        thread = new Thread(() => {
            var cls = new WC { proc=callback, instance=GetModuleHandle(null), name="ElitePen.HistoryQa", background=(IntPtr)6 };
            RegisterClass(ref cls);
            Window=CreateWindowEx(0,cls.name,"Elite Pen - isolated keyboard test",0x10CF0000,300,220,600,400,IntPtr.Zero,IntPtr.Zero,cls.instance,IntPtr.Zero);
            MSG m; while(GetMessage(out m,IntPtr.Zero,0,0)>0) { TranslateMessage(ref m); DispatchMessage(ref m); }
        });
        thread.IsBackground=true; thread.Start();
        for (int i=0; Window==IntPtr.Zero && i<100; ++i) Thread.Sleep(50);
        if (Window==IntPtr.Zero) throw new Exception("QA target window unavailable");
    }
    public static void Stop() { PostMessage(Window,0x10,IntPtr.Zero,IntPtr.Zero); thread.Join(3000); }
    public static IntPtr Find(string cls,uint pid) {
        for(var w=FindWindowEx(IntPtr.Zero,IntPtr.Zero,cls,null); w!=IntPtr.Zero; w=FindWindowEx(IntPtr.Zero,w,cls,null)) {
            uint owner; GetWindowThreadProcessId(w,out owner); if(owner==pid) return w;
        }
        return IntPtr.Zero;
    }
    public static void Press(ushort key,int repeats) {
        SetForegroundWindow(Window); Thread.Sleep(100);
        if (GetForegroundWindow()!=Window) {
            uint pid; uint foregroundThread=GetWindowThreadProcessId(GetForegroundWindow(),out pid);
            uint current=GetCurrentThreadId();
            bool attached=foregroundThread!=0 && AttachThreadInput(current,foregroundThread,true);
            try { BringWindowToTop(Window); SetForegroundWindow(Window); }
            finally { if(attached) AttachThreadInput(current,foregroundThread,false); }
            Thread.Sleep(100);
        }
        if (GetForegroundWindow()!=Window) throw new Exception("Refusing to send keys outside isolated QA target; foreground="+GetForegroundWindow()+", target="+Window);
        foreach(int k in new[]{0x11,0x10,0x12,0x5B,0x5C,89,90})
            if((GetAsyncKeyState(k)&0x8000)!=0) throw new Exception("Release modifier/history keys before keyboard QA");
        var events=new INPUT[repeats+3];
        events[0]=new INPUT {type=1,key=0x11,extra=(UIntPtr)0x4550484B};
        for(int i=0;i<repeats;++i) events[i+1]=new INPUT {type=1,key=key,extra=(UIntPtr)0x4550484B};
        events[repeats+1]=new INPUT {type=1,key=key,flags=2,extra=(UIntPtr)0x4550484B};
        events[repeats+2]=new INPUT {type=1,key=0x11,flags=2,extra=(UIntPtr)0x4550484B};
        if(SendInput((uint)events.Length,events,40)!=events.Length) throw new Exception("SendInput failed");
        Thread.Sleep(180);
    }
}
'@

$qaSandbox = Join-Path ([IO.Path]::GetTempPath()) ('elite-pen-history-qa-' + [Guid]::NewGuid().ToString('N'))
$null = New-Item -ItemType Directory -Path $qaSandbox
Copy-Item -LiteralPath $ExecutablePath -Destination (Join-Path $qaSandbox 'Elite Pen.exe')
Set-Content -LiteralPath (Join-Path $qaSandbox 'portable.flag') -Value 'Isolated keyboard QA'
$oldInstance = $env:ELITE_PEN_QA_INSTANCE_ID
$oldKeys = $env:ELITE_PEN_QA_HISTORY_KEYS
$oldCapture = $env:ELITE_PEN_QA_SYNTHETIC_CAPTURE
$originalForeground = [HistoryQa]::GetForegroundWindow()
$process = $null
$checks = 0
function Send-History([IntPtr]$Window, [uint32]$Message, [long]$Value=0, [long]$Extra=0) {
    [HistoryQa]::SendMessage($Window,$Message,[IntPtr]$Value,[IntPtr]$Extra).ToInt64()
}
function Assert-History([bool]$Condition, [string]$Label) {
    if (-not $Condition) { throw $Label }
    $script:checks++
    Write-Output "PASS: $Label"
}
function Find-History([string]$Class) {
    for ($i=0; $i -lt 100; $i++) {
        $w=[HistoryQa]::Find($Class,[uint32]$process.Id)
        if ($w -ne [IntPtr]::Zero) { return $w }
        Start-Sleep -Milliseconds 50
    }
    throw "Missing window $Class"
}
function Draw-History([IntPtr]$Window, [int]$Y=330) {
    $null=Send-History $Window 0x201 1 (($Y -shl 16) -bor 440)
    $null=Send-History $Window 0x200 1 ((($Y+10) -shl 16) -bor 620)
    $null=Send-History $Window 0x202 0 ((($Y+10) -shl 16) -bor 620)
}
function Check-Roundtrip([IntPtr]$Surface, [IntPtr]$Counter, [uint32]$Query, [string]$Label) {
    $before=Send-History $Counter $Query
    Draw-History $Surface
    Draw-History $Surface 380
    Assert-History ((Send-History $Counter $Query) -eq $before+2) "${Label}: two strokes"
    $external=[HistoryQa]::Undos+[HistoryQa]::Redos
    [HistoryQa]::Press(90,3)
    Assert-History ((Send-History $Counter $Query) -eq $before+1) "${Label}: Ctrl+Z once despite repeats"
    [HistoryQa]::Press(89,1)
    Assert-History ((Send-History $Counter $Query) -eq $before+2) "${Label}: Ctrl+Y restores stroke"
    Assert-History (([HistoryQa]::Undos+[HistoryQa]::Redos) -eq $external) "${Label}: no shortcut leak to underlying app"
}
function Check-Passthrough([IntPtr]$Counter, [uint32]$Query, [string]$Label) {
    $before=Send-History $Counter $Query
    $undo=[HistoryQa]::Undos; $redo=[HistoryQa]::Redos
    [HistoryQa]::Press(90,1); [HistoryQa]::Press(89,1)
    Assert-History ((Send-History $Counter $Query) -eq $before) "${Label}: annotation history unchanged"
    Assert-History ([HistoryQa]::Undos -eq $undo+1 -and [HistoryQa]::Redos -eq $redo+1) "${Label}: underlying app receives Ctrl+Z/Y"
}
try {
    [HistoryQa]::Start()
    $env:ELITE_PEN_QA_INSTANCE_ID=[Guid]::NewGuid().ToString('N')
    $env:ELITE_PEN_QA_HISTORY_KEYS='1'
    $env:ELITE_PEN_QA_SYNTHETIC_CAPTURE='1'
    $process=Start-Process -FilePath (Join-Path $qaSandbox 'Elite Pen.exe') -WindowStyle Hidden -PassThru
    $palette=Find-History 'ElitePen.Palette'
    $overlay=Find-History 'ElitePen.Overlay'
    Assert-History ((Send-History $palette 0x8080) -eq 1) 'Real keyboard hook installed'
    Check-Roundtrip $overlay $palette 0x805E 'Desktop with external focus'
    $null=Send-History $palette 0x312 3
    Check-Roundtrip $overlay $palette 0x805E 'Whiteboard'
    $null=Send-History $palette 0x312 8
    Check-Roundtrip $overlay $palette 0x805E 'Blackboard'
    $null=Send-History $palette 0x312 8
    $null=Send-History $palette 0x312 1
    Check-Passthrough $palette 0x805E 'Interact mode'
    $null=Send-History $palette 0x312 9
    $null=Send-History $palette 0x312 6
    Assert-History ((Send-History $palette 0x805E) -eq 0) 'Clear removes annotations'
    [HistoryQa]::Press(90,1)
    Assert-History ((Send-History $palette 0x805E) -eq 6) 'Ctrl+Z restores a clear operation'
    [HistoryQa]::Press(89,1)
    Assert-History ((Send-History $palette 0x805E) -eq 0) 'Ctrl+Y reapplies clear'
    $external=[HistoryQa]::Undos+[HistoryQa]::Redos
    [HistoryQa]::Press(89,1)
    Assert-History ((Send-History $palette 0x805E) -eq 0 -and
        ([HistoryQa]::Undos+[HistoryQa]::Redos) -eq $external) 'Empty redo does not leak to the app'
    $null=Send-History $palette 0x312 12
    Check-Roundtrip $overlay $palette 0x805E 'Line geometry'
    [HistoryQa]::Press(90,1)
    Draw-History $overlay 420
    [HistoryQa]::Press(89,1)
    Assert-History ((Send-History $palette 0x805E) -eq 2) 'New drawing invalidates redo branch'
    $null=Send-History $palette 0x312 11
    Draw-History $overlay
    Assert-History ((Send-History $palette 0x805E) -eq 1) 'Eraser removes one line'
    [HistoryQa]::Press(90,1)
    Assert-History ((Send-History $palette 0x805E) -eq 2) 'Ctrl+Z restores eraser operation'
    [HistoryQa]::Press(89,1)
    Assert-History ((Send-History $palette 0x805E) -eq 1) 'Ctrl+Y reapplies eraser operation'
    [HistoryQa]::Press(90,1)
    $null=Send-History $palette 0x312 9
    $null=Send-History $overlay 0x201 1 ((490 -shl 16) -bor 440)
    [HistoryQa]::Press(90,1)
    $null=Send-History $overlay 0x202 0 ((500 -shl 16) -bor 620)
    Assert-History ((Send-History $palette 0x805E) -eq 1) 'Undo cancels active preview without a delayed stroke'
    [HistoryQa]::Press(89,1)
    $null=Send-History $palette 0x312 22
    $settings=Find-History 'ElitePen.Settings'
    Check-Passthrough $palette 0x805E 'Settings open'
    $null=Send-History $settings 0x10
    $null=Send-History $palette 0x312 17
    Draw-History $overlay
    $text=Find-History 'ElitePen.TextInput'
    $null=Send-History $text 0x102 65
    Check-Passthrough $palette 0x805E 'Inline text active'
    $null=Send-History $text 0x805D
    $null=Send-History $palette 0x312 9
    [HistoryQa]::Press(90,1)
    Assert-History ((Send-History $palette 0x805E) -eq 2) 'Committed text is undoable as an annotation'
    $null=Send-History $palette 0x312 7
    $zoom=Find-History 'ElitePen.Zoom'
    $ink=Find-History 'ElitePen.ZoomInk'
    Check-Passthrough $ink 0x8062 'Live zoom'
    $null=Send-History $zoom 0x806F
    Assert-History ((Send-History $zoom 0x8061) -eq 1) 'Zoom frozen'
    Check-Roundtrip $ink $ink 0x8062 'Frozen zoom'
    $null=Send-History $zoom 0x8074 2
    Assert-History ((Send-History $zoom 0x8072) -eq 2) 'Editable zoom annotation mode'
    Check-Roundtrip $ink $ink 0x8073 'Editable zoom pencil'
    $null=Send-History $zoom 0x8074 1
    Check-Passthrough $ink 0x8073 'Editable zoom hand'
    Write-Output "Elite Pen real-keyboard QA: $checks checks passed."
} finally {
    if ($process -and -not $process.HasExited) {
        $window=[HistoryQa]::Find('ElitePen.Palette',[uint32]$process.Id)
        $null=[HistoryQa]::PostMessage($window,0x800C,[IntPtr]::Zero,[IntPtr]::Zero)
        if (-not $process.WaitForExit(5000)) { $process.Kill(); $process.WaitForExit() }
    }
    [HistoryQa]::Stop()
    $null=[HistoryQa]::SetForegroundWindow($originalForeground)
    $env:ELITE_PEN_QA_INSTANCE_ID=$oldInstance
    $env:ELITE_PEN_QA_HISTORY_KEYS=$oldKeys
    $env:ELITE_PEN_QA_SYNTHETIC_CAPTURE=$oldCapture
    $expectedRoot=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')+'\'
    if ([IO.Path]::GetFullPath($qaSandbox).StartsWith($expectedRoot,[StringComparison]::OrdinalIgnoreCase) -and
        [IO.Path]::GetFileName($qaSandbox).StartsWith('elite-pen-history-qa-')) {
        try { Remove-Item -LiteralPath $qaSandbox -Recurse -Force } catch { Write-Warning "QA sandbox retained: $qaSandbox" }
    }
}
