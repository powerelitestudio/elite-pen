[CmdletBinding()]
param(
    [string]$ExecutablePath,
    [ValidateSet('Idle','Board','Zoom','Focus')][string[]]$Modes=@('Idle','Board','Zoom','Focus'),
    [ValidateRange(1,5)][int]$Iterations=2,
    [switch]$ExpectFailure
)
$ErrorActionPreference='Stop'
$repoRoot=Split-Path -Parent $PSScriptRoot
if(-not $ExecutablePath){$ExecutablePath=Join-Path $repoRoot 'build\release\Elite Pen.exe'}
Add-Type @'
using System;using System.Text;using System.Runtime.InteropServices;
public static class ShutdownNative {
 public delegate bool Callback(IntPtr w,IntPtr p);
 [DllImport("user32.dll")]public static extern bool EnumWindows(Callback callback,IntPtr data);
 [DllImport("user32.dll")]public static extern uint GetWindowThreadProcessId(IntPtr w,out uint pid);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]public static extern int GetClassName(IntPtr w,StringBuilder s,int n);
 [DllImport("user32.dll")]public static extern bool IsWindowVisible(IntPtr w);
 [DllImport("user32.dll")]public static extern IntPtr SendMessageTimeout(IntPtr w,uint m,IntPtr a,IntPtr b,uint flags,uint timeout,out IntPtr result);
 [DllImport("user32.dll")]public static extern bool PostMessage(IntPtr w,uint m,IntPtr a,IntPtr b);
 public static IntPtr Find(uint pid,string name){IntPtr found=IntPtr.Zero;EnumWindows((w,p)=>{uint owner;GetWindowThreadProcessId(w,out owner);var s=new StringBuilder(256);GetClassName(w,s,256);if(owner==pid&&s.ToString()==name){found=w;return false;}return true;},IntPtr.Zero);return found;}
 public static void Send(IntPtr w,uint m,long a=0,long b=0){IntPtr result;if(w==IntPtr.Zero||SendMessageTimeout(w,m,new IntPtr(a),new IntPtr(b),2,10000,out result)==IntPtr.Zero)throw new Exception("Lifecycle target did not respond");}
}
'@
function Find([int]$ProcessId,[string]$Name){for($n=0;$n -lt 100;$n++){$w=[ShutdownNative]::Find([uint32]$ProcessId,$Name);if($w -ne [IntPtr]::Zero){return $w};Start-Sleep -Milliseconds 50};throw "Missing lifecycle window: $Name"}
$oldInstance=$env:ELITE_PEN_QA_INSTANCE_ID;$oldHotkeys=$env:ELITE_PEN_QA_REAL_HOTKEYS
$root=Join-Path ([IO.Path]::GetTempPath()) ('elite-pen-shutdown-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root|Out-Null
$records=[Collections.Generic.List[object]]::new();$app=$null
try {
 Remove-Item Env:ELITE_PEN_QA_REAL_HOTKEYS -ErrorAction SilentlyContinue
 for($cycle=1;$cycle -le $Iterations;$cycle++) {foreach($mode in $Modes) {
  $folder=Join-Path $root ($mode+'-'+$cycle);New-Item -ItemType Directory -Path $folder|Out-Null
  $exe=Join-Path $folder 'Elite Pen.exe';Copy-Item -LiteralPath $ExecutablePath -Destination $exe
  Set-Content -LiteralPath (Join-Path $folder 'portable.flag') -Value 'Isolated lifecycle QA' -Encoding ascii
  $env:ELITE_PEN_QA_INSTANCE_ID=[Guid]::NewGuid().ToString('N')
  $app=Start-Process -FilePath $exe -WindowStyle Hidden -PassThru
  $palette=Find $app.Id 'ElitePen.Palette'
  if($mode -eq 'Board'){
   [ShutdownNative]::Send($palette,0x312,70);$window=Find $app.Id 'ElitePenBoardWindow'
   if(-not [ShutdownNative]::IsWindowVisible($window)){throw 'Board did not activate'}
  }elseif($mode -eq 'Zoom'){
   [ShutdownNative]::Send($palette,0x312,7);$window=Find $app.Id 'ElitePen.Zoom'
   if(-not [ShutdownNative]::IsWindowVisible($window)){throw 'Zoom did not activate'}
  }elseif($mode -eq 'Focus'){
   [ShutdownNative]::Send($palette,0x312,47);$window=Find $app.Id 'ElitePen.Focus'
   [ShutdownNative]::Send($window,0x201,1,((100 -shl 16) -bor 100))
   [ShutdownNative]::Send($window,0x202,0,((400 -shl 16) -bor 500))
   $window=Find $app.Id 'ElitePen.Zoom'
   if(-not [ShutdownNative]::IsWindowVisible($window)){throw 'Focus did not activate'}
  }
  Start-Sleep -Milliseconds 250
  $null=[ShutdownNative]::PostMessage($palette,0x800C,[IntPtr]::Zero,[IntPtr]::Zero)
  if(-not $app.WaitForExit(15000)){throw "Shutdown timed out: $mode"}
  $app.Refresh();$code=$app.ExitCode
  $records.Add([ordered]@{Mode=$mode;Cycle=$cycle;ExitCode=$code;ProcessId=$app.Id;Executable=$exe})
  Write-Output "$mode cycle $cycle exit=$code"
  if($ExpectFailure -and $code -eq 0){throw 'Baseline unexpectedly exited successfully'}
  if(-not $ExpectFailure -and $code -ne 0){throw "Shutdown crashed: $mode exit=$code"}
 }}
 $evidence=Join-Path $repoRoot 'artifacts\qa';New-Item -ItemType Directory -Force -Path $evidence|Out-Null
 $records|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $evidence 'shutdown-result.json') -Encoding utf8
 Write-Output "$($records.Count) lifecycle checks passed; expectedFailure=$ExpectFailure; sandbox=$root"
} finally {
 if($app -and -not $app.HasExited){$app.Kill();$app.WaitForExit(3000)|Out-Null}
 $env:ELITE_PEN_QA_INSTANCE_ID=$oldInstance;$env:ELITE_PEN_QA_REAL_HOTKEYS=$oldHotkeys
}
