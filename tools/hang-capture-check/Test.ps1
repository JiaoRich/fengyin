param([switch]$LocalOnly)
$ErrorActionPreference='Stop'
$source=Join-Path (Split-Path $PSScriptRoot -Parent) '../scripts/hang-capture'
$source=[IO.Path]::GetFullPath($source)
foreach($file in Get-ChildItem -LiteralPath $source -Filter '*.ps1') {
    $t=$null;$e=$null
    $ast=[Management.Automation.Language.Parser]::ParseFile($file.FullName,[ref]$t,[ref]$e)
    if($e.Count){throw ($e | Out-String)}
    $commands=@($ast.FindAll({param($n) $n -is [Management.Automation.Language.CommandAst]},$true) | ForEach-Object {$_.GetCommandName()})
    foreach($bad in @('Stop-Process','Restart-Service','Stop-Service','Set-ItemProperty','Invoke-WebRequest','Invoke-RestMethod','Set-ExecutionPolicy')) {
        if($commands -contains $bad){throw "Unsafe mutation/network command: $bad"}
    }
}
Add-Type -Path (Join-Path $source 'DumpNative.cs')
Add-Type -Path (Join-Path $source 'WindowSnapshot.cs')
if([Runtime.InteropServices.Marshal]::OffsetOf([type][FengYin.HangCapture.DumpNative+CallbackHeader],'Type').ToInt32() -ne (4+[IntPtr]::Size)){throw 'SDK pack4 ABI mismatch'}
. (Join-Path $source 'Common.ps1')
$work=Join-Path $pwd ('hang-test-'+[guid]::NewGuid().ToString('N').Substring(0,6))
$null=New-Item -ItemType Directory -Path $work
$log=Join-Path $work 'filter.log'
@('prefix','LICENSE_KEY=SECRET_A','Authorization: Bearer SECRET_B','plugin load done','PRIVATE unrelated text','api_key=SECRET_C') | Set-Content -LiteralPath $log -Encoding UTF8
$text=Read-LogTail $log
if($text -match 'SECRET_[ABC]' -or $text -notmatch 'plugin load done'){throw 'Log filter regression'}
$roots=@([pscustomobject]@{Id=10;StartFileTime=100})
$rows=@(
    [pscustomobject]@{ProcessId=11;ParentProcessId=10;StartFileTime=101;Name='msedgewebview2.exe'},
    [pscustomobject]@{ProcessId=12;ParentProcessId=11;StartFileTime=102;Name='msedgewebview2.exe'},
    [pscustomobject]@{ProcessId=13;ParentProcessId=10;StartFileTime=99;Name='msedgewebview2.exe'},
    [pscustomobject]@{ProcessId=14;ParentProcessId=999;StartFileTime=101;Name='msedgewebview2.exe'},
    [pscustomobject]@{ProcessId=15;ParentProcessId=10;StartFileTime=101;Name='unrelated.exe'})
$selected=@(Select-RelatedWebViews $rows $roots)
if(($selected.ProcessId -join ',') -ne '11,12'){throw 'Descendant/parent-PID-reuse isolation regression'}
$invalid=Join-Path $work 'invalid.dmp';[IO.File]::WriteAllBytes($invalid,[byte[]](1,2,3))
if([FengYin.HangCapture.DumpNative]::HasRequiredStreams($invalid)){throw 'Invalid dump accepted'}
Write-Host 'PASS PowerShell parser, C# compilation, SDK pack4 ABI, log filter, descendant/PID-reuse isolation, invalid dump rejection'
if($LocalOnly){exit 0}
if([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT){throw 'Windows actual API verification required'}
# Duplicate launch must exit before creating a second collection.
$gate=New-Object Threading.Mutex($false,('Local\FengYinHangCapture-'+[Security.Principal.WindowsIdentity]::GetCurrent().User.Value))
$null=$gate.WaitOne(0)
try {
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $source 'Collect-Hang.ps1') -OutputRoot $work
    if($LASTEXITCODE -ne 3 -or @(Get-ChildItem -LiteralPath $work -Directory -Filter 'FengYin-Hang-*').Count){throw 'Duplicate launch not isolated'}
} finally {$gate.ReleaseMutex();$gate.Dispose()}
# A stuck context worker is bounded; this owned synthetic worker alone is stopped.
$slow=Join-Path $work 'slow.ps1'
'param([string]$SpecPath);Start-Sleep -Seconds 90' | Set-Content -LiteralPath $slow -Encoding UTF8
$clock=[Diagnostics.Stopwatch]::StartNew()
$r=Invoke-CaptureWorker $slow (Join-Path $work 'empty.json') $work 'timeout-test' 1
if($r -ne 124 -or $clock.Elapsed.TotalSeconds -gt 15){throw 'Context deadline not enforced'}
$copy=Join-Path $work "中文 空格 & (1) '目录"
$null=New-Item -ItemType Directory -Path $copy
Get-ChildItem -LiteralPath $source -File | Copy-Item -Destination $copy
$fixture=Join-Path $work 'FengYin.exe'
$childExe=Join-Path $work 'msedgewebview2.exe'
$program=@'
using System;using System.Diagnostics;using System.IO;using System.Threading;using System.Windows.Forms;
class Fixture {
 [STAThread] static void Main(string[] args) {
  if(args.Length>0 && args[0]=="--child"){Thread.Sleep(240000);return;}
  var form=new Form {Text="Synthetic FengYin hang fixture",Width=640,Height=480};
  form.Shown+=(s,e)=>{
   Process.Start(new ProcessStartInfo(Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"msedgewebview2.exe"),"--child --type=renderer"){UseShellExecute=false});
   var timer=new System.Windows.Forms.Timer {Interval=400};
   timer.Tick+=(x,y)=>{timer.Stop();Thread.Sleep(240000);};timer.Start();
  };
  Application.Run(form);
 }
}
'@
Add-Type -TypeDefinition $program -OutputAssembly $fixture -OutputType WindowsApplication -ReferencedAssemblies System.Windows.Forms
Copy-Item -LiteralPath $fixture -Destination $childExe
$main=[Diagnostics.Process]::Start($fixture)
$unrelated=[Diagnostics.Process]::Start($childExe,'--child --type=renderer')
$ownedChild=$null
$oldApp=$env:APPDATA;$oldLocal=$env:LOCALAPPDATA
try {
    Start-Sleep -Seconds 7
    $owned=Get-CimInstance Win32_Process -Filter "Name='msedgewebview2.exe'" | Where-Object ParentProcessId -eq $main.Id | Select-Object -First 1
    if(-not $owned){throw 'Fixture child missing'}
    $ownedChild=Get-Process -Id $owned.ProcessId
    $env:APPDATA=Join-Path $work 'roaming'
    $env:LOCALAPPDATA=Join-Path $work 'local'
    $fake=Join-Path $env:APPDATA 'FengYin'
    $null=New-Item -ItemType Directory -Path $fake -Force
    Copy-Item -LiteralPath $log -Destination (Join-Path $fake 'plugin-host.log')
    'DO_NOT_COPY' | Set-Content -LiteralPath (Join-Path $fake 'license.json')
    $before=$main.StartTime.ToUniversalTime().ToFileTimeUtc()
    # Exact shipped orchestrator and all worker APIs; no mocks or modal shortcuts.
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $copy 'Collect-Hang.ps1') -OutputRoot $work -TargetProcessId $main.Id
    if($LASTEXITCODE -ne 0){throw 'Exact collector failed'}
    $bundle=@(Get-ChildItem -LiteralPath $work -Directory -Filter 'FengYin-Hang-*')
    if($bundle.Count -ne 1){throw 'Expected one evidence folder'}
    $out=$bundle[0].FullName
    $summary=Get-Content -LiteralPath (Join-Path $out 'result.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    if($summary.Status -ne 'captured-two-main-snapshots' -or $summary.MainRound1 -ne 1 -or $summary.MainRound2 -ne 1 -or @($summary.PairedTargetIds).Count -ne 1 -or $summary.PairedTargetIds[0] -ne $main.Id){throw 'Missing two snapshots'}
    $dumps=@(Get-ChildItem -LiteralPath $out -Filter '*.dmp')
    if($dumps.Count -ne 3){throw ('Expected 2 main + 1 descendant dumps, found '+$dumps.Count)}
    foreach($d in $dumps){if(-not [FengYin.HangCapture.DumpNative]::HasRequiredStreams($d.FullName)){throw 'Invalid actual dump streams'}}
    $windows=Get-Content -LiteralPath (Join-Path $out ('round1-pid'+$main.Id+'-windows.json')) -Raw -Encoding UTF8 | ConvertFrom-Json
    if(-not @($windows.Items | Where-Object {$_.Visible -and $_.Hung}).Count){throw 'Actual hung window not detected'}
    $tree=@(Get-Content -LiteralPath (Join-Path $out 'fengyin-webview-tree.json') -Raw -Encoding UTF8 | ConvertFrom-Json)
    if(@($tree | Where-Object ProcessId -eq $unrelated.Id).Count -or -not @($tree | Where-Object ProcessId -eq $ownedChild.Id).Count){throw 'Actual unrelated webview isolation failure'}
    $capturedLog=Get-Content -LiteralPath (Join-Path $out 'roaming-FengYin-plugin-host.log.txt') -Raw -Encoding UTF8
    if($capturedLog -match 'SECRET_[ABC]' -or $capturedLog -notmatch 'plugin load done'){throw 'Actual log filter failure'}
    if(Get-ChildItem -LiteralPath $out -Filter '*license*'){throw 'License copied'}
    $main.Refresh();$unrelated.Refresh()
    if($main.HasExited -or $unrelated.HasExited -or $main.StartTime.ToUniversalTime().ToFileTimeUtc() -ne $before){throw 'Collector ended/restarted a target'}
    $zip=$out+'.zip'
    $extracted=Join-Path $work 'expanded'
    Expand-Archive -LiteralPath $zip -DestinationPath $extracted
    foreach($d in $dumps){if((Get-FileHash -LiteralPath $d.FullName).Hash -ne (Get-FileHash -LiteralPath (Join-Path $extracted $d.Name)).Hash){throw 'ZIP roundtrip mismatch'}}
    # Reject wrong process creation identity before making a dump file.
    $rejected=$false
    try {[FengYin.HangCapture.DumpNative]::Write([uint32]$main.Id,$before-1,'FengYin',(Join-Path $work 'wrong.dmp'))}
    catch {$rejected=$true}
    if(-not $rejected -or (Test-Path -LiteralPath (Join-Path $work 'wrong.dmp'))){throw 'Reused PID/identity mismatch accepted'}
    # No matching target is a partial report, never a successful empty dump.
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $copy 'Collect-Hang.ps1') -OutputRoot $work -TargetProcessId 2147483647
    if($LASTEXITCODE -ne 2){throw 'Missing target should return partial'}
    $folders=@(Get-ChildItem -LiteralPath $work -Directory -Filter 'FengYin-Hang-*')
    $missing=$folders | Where-Object FullName -ne $out
    $partial=Get-Content -LiteralPath (Join-Path $missing.FullName 'result.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    if($partial.DumpCount -ne 0 -or $partial.Status -ne 'partial-no-complete-hang-pair'){throw 'Missing target mislabeled'}
    [pscustomobject]@{Passed=$true;NativeTwoDumps=$true;HungWindow=$true;DescendantOnly=$true;TargetSurvives=$true;WrongIdentityRejected=$true;NoTargetPartial=$true;DuplicateLaunchRejected=$true;ContextDeadline=$true;ChineseSpaceAmpersandQuotePath=$true;ZipRoundtrip=$true;Limits='Synthetic Windows fixture, not customer FengYin or actual deadlock root cause'} |
        ConvertTo-Json | Set-Content -LiteralPath 'hang-collector-verification.json' -Encoding UTF8
    Write-Host 'PASS exact Windows collector, actual two native dumps, hung window, child isolation, target survival, mismatch rejection, no-target partial, Unicode path and ZIP roundtrip'
} finally {
    $env:APPDATA=$oldApp;$env:LOCALAPPDATA=$oldLocal
    # Test-only cleanup of held synthetic fixture Process objects, never production processes by name.
    foreach($p in @($ownedChild,$unrelated,$main)){if($p){try{if(-not $p.HasExited){$p.Kill();$null=$p.WaitForExit(5000)}}catch{};$p.Dispose()}}
}

exit 0
