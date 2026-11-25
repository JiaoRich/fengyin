param([switch]$LocalOnly)
$ErrorActionPreference='Stop'
$source=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../scripts/bluetooth-capture'))
foreach($file in Get-ChildItem -LiteralPath $source -Filter '*.ps1'){
    $tokens=$null;$errors=$null
    $ast=[Management.Automation.Language.Parser]::ParseFile($file.FullName,[ref]$tokens,[ref]$errors)
    if($errors.Count){throw ($errors | Out-String)}
    $commands=@($ast.FindAll({param($n)$n -is [Management.Automation.Language.CommandAst]},$true) | ForEach-Object {$_.GetCommandName()})
    foreach($bad in @('Stop-Process','Restart-Service','Stop-Service','Set-ItemProperty','Invoke-WebRequest','Invoke-RestMethod','Set-ExecutionPolicy','Remove-PnpDevice','Enable-PnpDevice','Disable-PnpDevice')){if($commands -contains $bad){throw "Forbidden command $bad"}}
    $text=Get-Content -LiteralPath $file.FullName -Raw
    if($text -match '\.(PairAsync|UnpairAsync|GetGattServices\w*Async|WriteClientCharacteristicConfigurationDescriptorAsync)\('){throw 'Active Bluetooth call in collector'}
}
. (Join-Path $source 'Common.ps1')
Add-Type -Path (Join-Path $source 'MidiInventory.cs')
$work=Join-Path $pwd ('ble-test-'+[guid]::NewGuid().ToString('N').Substring(0,6));$null=New-Item -ItemType Directory -Path $work
$log=Join-Path $work 'plugin-host.log';$timeline=Join-Path $work 'test.jsonl';$states=@{}
[IO.File]::WriteAllText($log,"unrelated private content`nBluetooth MIDI: stage=read A`nBluetooth MIDI: secret=SECRET_TOKEN`n",[Text.UTF8Encoding]::new($false))
Read-BleIncrement $log $states $timeline 0
Read-BleIncrement $log $states $timeline 1
[IO.File]::AppendAllText($log,'Bluetooth MIDI: par');Read-BleIncrement $log $states $timeline 2
[IO.File]::AppendAllText($log,"tial`n");Read-BleIncrement $log $states $timeline 3
$read=Get-Content -LiteralPath $timeline -Raw
if($read -match 'SECRET_TOKEN|unrelated private' -or ([regex]::Matches($read,'stage=read A')).Count -ne 1 -or ([regex]::Matches($read,'partial')).Count -ne 1){throw 'Increment/filter/partial line failed'}
# Rewrite with same byte length and same creation time: size alone is insufficient.
$old=Get-Content -LiteralPath $log -Raw
[IO.File]::WriteAllText($log,$old.Replace('stage=read A','stage=read B'),[Text.UTF8Encoding]::new($false))
Read-BleIncrement $log $states $timeline 4
if((Get-Content -LiteralPath $timeline -Raw) -notmatch 'stage=read B'){throw 'Same-size rewrite not captured'}
Move-Item -LiteralPath $log -Destination ($log+'.1')
[IO.File]::WriteAllText($log,"Bluetooth MIDI: reopened`n")
Read-BleIncrement $log $states $timeline 5
if((Get-Content -LiteralPath $timeline -Raw) -notmatch 'reopened'){throw 'Reopen/rotation lost'}
$pref=Join-Path $work 'history.settings'
'<PROPERTIES><VALUE name="licenseKey" val="NO_COPY"/><VALUE name="history" val="[{&quot;id&quot;:&quot;test-device&quot;,&quot;name&quot;:&quot;YDS-150 MIDI&quot;,&quot;password&quot;:&quot;NO_COPY&quot;}]"/></PROPERTIES>' | Set-Content -LiteralPath $pref -Encoding UTF8
$history=@(Read-BluetoothHistory $pref)
if($history.Count -ne 1 -or $history[0].Id -ne 'test-device' -or (ConvertTo-Json $history) -match 'NO_COPY'){throw 'History whitelist failed'}
'<!DOCTYPE PROPERTIES [<!ENTITY x SYSTEM "file:///never-read">]><PROPERTIES>&x;</PROPERTIES>' | Set-Content -LiteralPath $pref -Encoding UTF8
$blocked=$false;try{$null=Read-BluetoothHistory $pref}catch{$blocked=$true};if(-not $blocked){throw 'DTD not rejected'}
foreach($n in @('command-abc.json','001-request.json','session-input.json','status.json','x.new','finish.flag')){if(Test-EvidenceName $n){throw 'Private control file included'}}
if(-not (Test-EvidenceName 'timeline.jsonl')){throw 'Evidence excluded'}
Write-Host 'PASS parser, no active Bluetooth/mutations, increment/partial/rotation/same-size rewrite, redaction, secure history'
if($LocalOnly){exit 0}
if([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT -or $PSVersionTable.PSVersion.Major -ne 5){throw 'Run Windows PowerShell 5.1 actual tests'}
$null=[BleCaptureMidi]::Read()
# A duplicate collector must exit without taking another snapshot.
$gate=[Threading.Mutex]::new($false,('Local\FengYinBleCapture-'+[Security.Principal.WindowsIdentity]::GetCurrent().User.Value));$null=$gate.WaitOne(0)
$dup=Join-Path $work 'duplicate';$null=New-Item -ItemType Directory -Path $dup
$dupSpec=Join-Path $dup 'session-input.json';Save-Json @{Output=$dup;MaxSeconds=5;LogRoots=@();Started=(Get-Date).ToString('o')} $dupSpec
try {
    $dj=Start-OwnedProcess (Join-Path $source 'Engine.ps1') $dupSpec $dup 'duplicate' 10
    while(-not (Poll-OwnedProcess $dj)){Start-Sleep -Milliseconds 100}
    if($dj.ExitCode -ne 3 -or (Test-Path -LiteralPath (Join-Path $dup 'session.json'))){Get-ChildItem -LiteralPath $dup -File | ForEach-Object {Write-Host $_.Name;Get-Content -LiteralPath $_.FullName -Raw | Write-Host};throw ('Duplicate collector guard failed exit='+$dj.ExitCode)}
}finally{$gate.ReleaseMutex();$gate.Dispose()}
$slow=Join-Path $work 'slow.ps1';'param([string]$SpecPath);Start-Sleep -Seconds 120' | Set-Content -LiteralPath $slow -Encoding UTF8
$j=Start-OwnedProcess $slow (Join-Path $work 'x.json') $work 'timeout' 1
while(-not (Poll-OwnedProcess $j)){Start-Sleep -Milliseconds 100}
if($j.ExitCode -ne 124 -or $j.Clock.Elapsed.TotalSeconds -gt 10){throw 'Worker timeout failed'}
$copy=Join-Path $work "中文 & 空格 '路径"
$null=New-Item -ItemType Directory -Path $copy
Get-ChildItem -LiteralPath $source -File | Copy-Item -Destination $copy
$roaming=Join-Path $work 'roaming';$logs=Join-Path $roaming 'FengYin';$null=New-Item -ItemType Directory -Path $logs -Force
[IO.File]::WriteAllText((Join-Path $logs 'plugin-host.log'),"Bluetooth MIDI: fixture initial`n")
# Real OS APIs and real engine. No adapter is a reportable result, not a Bluetooth success claim.
$out=Join-Path $work 'FengYin-BLE-test';$null=New-Item -ItemType Directory -Path $out
$spec=Join-Path $out 'session-input.json'
Save-Json @{Output=$out;LogRoots=@($logs);Started=(Get-Date).ToString('o');MaxSeconds=5;ParentId=0} $spec
$j=Start-OwnedProcess (Join-Path $copy 'Engine.ps1') $spec $out 'engine' 160
while(-not (Poll-OwnedProcess $j)){Start-Sleep -Milliseconds 250}
if($j.ExitCode -ne 0 -or -not (Test-Path -LiteralPath ($out+'.zip'))){throw 'Real engine did not produce ZIP'}
$status=Get-Content -LiteralPath (Join-Path $out 'status.json') -Raw -Encoding UTF8 | ConvertFrom-Json
if($status.Phase -ne 'done'){throw 'Engine not done'}
$extracted=Join-Path $work 'expanded';Expand-Archive -LiteralPath ($out+'.zip') -DestinationPath $extracted
$manifest=Get-Content -LiteralPath (Join-Path $extracted 'manifest.json') -Raw -Encoding UTF8 | ConvertFrom-Json
foreach($f in $manifest){if((Get-FileHash -LiteralPath (Join-Path $extracted $f.Name)).Hash -ne $f.SHA256){throw 'ZIP hash mismatch'}}
foreach($mode in @('system','drivers','services','pnp','paired-endpoints','event-channels')){if(-not @(Get-ChildItem -LiteralPath $out -Filter ('*-'+$mode+'.json')).Count){throw "No report for $mode"}}
# Actual WinForms window + real mouse clicks run Click handlers; no engine command shortcut.
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type -TypeDefinition @'
using System;using System.Runtime.InteropServices;
public static class TestMouse {
 [StructLayout(LayoutKind.Sequential)] public struct Point { public int X,Y; public Point(int x,int y){X=x;Y=y;} }
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr w);
 [DllImport("user32.dll")] public static extern bool SetCursorPos(int x,int y);
 [DllImport("user32.dll")] public static extern IntPtr WindowFromPoint(Point p);
 [DllImport("user32.dll")] public static extern void mouse_event(uint flags,uint x,uint y,uint data,UIntPtr extra);
}
'@
$oldApp=$env:APPDATA;$oldLocal=$env:LOCALAPPDATA;$env:APPDATA=$roaming;$env:LOCALAPPDATA=Join-Path $work 'local'
$guiRoot=Join-Path $work 'gui';$null=New-Item -ItemType Directory -Path $guiRoot
$launch="& '"+(Join-Path $copy 'Start.ps1').Replace("'","''")+"' -NoElevation -OutputRoot '"+$guiRoot.Replace("'","''")+"' -MaxSeconds 60"
$p=[Diagnostics.Process]::Start('powershell.exe',('-NoProfile -STA -ExecutionPolicy Bypass -EncodedCommand '+[Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($launch))))
$fixture=Join-Path $work 'FengYin.exe'
Add-Type -TypeDefinition 'using System.Threading;class F{static void Main(){Thread.Sleep(120000);}}' -OutputAssembly $fixture -OutputType ConsoleApplication
$fake=$null
try {
    $ui=$null;$deadline=[Diagnostics.Stopwatch]::StartNew()
    while(-not $ui -and $deadline.Elapsed.TotalSeconds -lt 20){
        Start-Sleep -Milliseconds 500
        $ui=[Windows.Automation.AutomationElement]::RootElement.FindFirst([Windows.Automation.TreeScope]::Children,[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::ProcessIdProperty,$p.Id))
    }
    if(-not $ui){throw 'No actual diagnostic UI'}
    $texts=@($ui.FindAll([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.Condition]::TrueCondition) | ForEach-Object {$_.Current.Name}) -join ' '
    if($texts -notmatch '最多记录20分钟' -or $texts -notmatch '重新打开'){throw 'Full multi-step UI instructions missing'}
    Add-Type -AssemblyName System.Drawing
    $r=$ui.Current.BoundingRectangle;$bmp=[Drawing.Bitmap]::new([int]$r.Width,[int]$r.Height);$g=[Drawing.Graphics]::FromImage($bmp)
    try {$g.CopyFromScreen([int]$r.X,[int]$r.Y,0,0,$bmp.Size);$bmp.Save((Join-Path $pwd 'bluetooth-collector-ui.png'))}finally{$g.Dispose();$bmp.Dispose()}
    $fake=[Diagnostics.Process]::Start($fixture);$id1=$fake.Id;Start-Sleep -Seconds 3
    $fake.Kill();$fake.WaitForExit();$fake.Dispose();$fake=$null;Start-Sleep -Seconds 2
    $fake=[Diagnostics.Process]::Start($fixture);$id2=$fake.Id;Start-Sleep -Seconds 3
    $liveNames=@($ui.FindAll([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.Condition]::TrueCondition) | ForEach-Object {$_.Current.Name}) -join ' '
    if($liveNames -match '（0秒）'){throw 'Visible capture timer did not advance'}
    foreach($name in @('标记连接失败','标记连接成功','标记自行断连','标记已删除配对','结束并打包')){
        $button=$ui.FindFirst([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::NameProperty,$name))
        if(-not $button -or -not $button.Current.IsEnabled -or $button.Current.IsOffscreen){throw "Button not visible/enabled: $name"}
        $null=[TestMouse]::SetForegroundWindow([IntPtr]$ui.Current.NativeWindowHandle);Start-Sleep -Milliseconds 150
        $rect=$button.Current.BoundingRectangle;$x=[int]($rect.X+$rect.Width/2);$y=[int]($rect.Y+$rect.Height/2)
        if([TestMouse]::WindowFromPoint([TestMouse+Point]::new($x,$y)) -ne [IntPtr]$button.Current.NativeWindowHandle){throw "Button is occluded: $name"}
        $null=[TestMouse]::SetCursorPos($x,$y);[TestMouse]::mouse_event(2,0,0,0,[UIntPtr]::Zero);[TestMouse]::mouse_event(4,0,0,0,[UIntPtr]::Zero)
        Start-Sleep -Milliseconds 700
    }
    $deadline.Restart();$zip=$null
    while(-not $zip -and $deadline.Elapsed.TotalSeconds -lt 120){Start-Sleep -Milliseconds 500;$zip=Get-ChildItem -LiteralPath $guiRoot -Filter '*.zip' | Select-Object -First 1}
    if(-not $zip){throw 'UI finish did not produce archive'}
    $folder=Get-ChildItem -LiteralPath $guiRoot -Directory | Select-Object -First 1
    $events=Get-Content -LiteralPath (Join-Path $folder.FullName 'timeline.jsonl') -Raw -Encoding UTF8
    foreach($name in @('user-failed','user-connected','user-disconnected','user-forgotten')){if($events -notmatch $name){throw "UI marker absent $name"}}
    if($events -notmatch ('"Id":'+$id1) -or $events -notmatch ('"Id":'+$id2)){throw 'FengYin exit/restart process sequence missing'}
    # Let completion status propagate before asking the normal window to close.
    Start-Sleep -Seconds 2;$null=$p.CloseMainWindow()
    if(-not $p.WaitForExit(10000)){throw 'Completed UI failed to close'}
} finally {$env:APPDATA=$oldApp;$env:LOCALAPPDATA=$oldLocal;if($fake){if(-not $fake.HasExited){$fake.Kill()};$fake.Dispose()};if(-not $p.HasExited){$p.Kill()};$p.Dispose()}
Save-Json @{Passed=$true;PowerShell=$PSVersionTable.PSVersion.ToString();RealEngine=$true;UiButtons=$true;ZipHashes=$true;Hardware='No Bluetooth hardware on cloud runner; no YDS connection verification'} (Join-Path $pwd 'bluetooth-collector-verification.json')
Write-Host 'PASS actual WinPS5.1 API/worker deadline/engine/Unicode paths/ZIP hashes/visible WinForms buttons and finish'
