param([Parameter(Mandatory=$true)][string]$SpecPath)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
$spec=Get-Content -LiteralPath $SpecPath -Raw -Encoding UTF8 | ConvertFrom-Json
$out=$spec.Output;$clock=[Diagnostics.Stopwatch]::StartNew();$states=@{};$lastProcesses='';$job=$null;$seq=0;$lastSnapshot=-30.0;$lastLogInventory=-10.0;$files=@();$pending='initial';$doneCommands=@{}
$sid=[Security.Principal.WindowsIdentity]::GetCurrent().User.Value
$mutex=[Threading.Mutex]::new($false,('Local\FengYinBleCapture-'+$sid));$owns=$false
function Timeline([string]$Kind,$Data) {Add-Record (Join-Path $out 'timeline.jsonl') ([ordered]@{Time=(Get-Date).ToString('o');Utc=[DateTime]::UtcNow.ToString('o');Elapsed=$clock.Elapsed.TotalSeconds;Kind=$Kind;Data=$Data})}
function Set-Status([string]$Phase,[string]$Message) {Save-Json ([ordered]@{Phase=$Phase;Message=$Message;Elapsed=[int]$clock.Elapsed.TotalSeconds;Output=$out}) (Join-Path $out 'status.json')}
function New-Worker([string]$Mode,[string]$Reason,[int]$Seconds) {
    $script:seq++;$label=('{0:d3}-{1}' -f $seq,$Mode.ToLowerInvariant())
    $file=Join-Path $out ($label+'-request.json')
    Save-Json ([ordered]@{Mode=$Mode;Output=$out;Label=$label;Reason=$Reason;LogRoots=$spec.LogRoots;Started=$spec.Started}) $file
    Timeline 'snapshot-request' @{Label=$label;Reason=$Reason}
    Start-OwnedProcess (Join-Path $PSScriptRoot 'Worker.ps1') $file $out $label $Seconds
}
function Poll-Logs {
    if($clock.Elapsed.TotalSeconds-$script:lastLogInventory -ge 3){$script:files=@(Get-LogFiles $spec.LogRoots);$script:lastLogInventory=$clock.Elapsed.TotalSeconds}
    $dest=Join-Path $out 'fengyin-ble-timeline.jsonl'
    if((Test-Path -LiteralPath $dest) -and (Get-Item -LiteralPath $dest).Length -gt 33554432){
        if(-not $script:logCapped){Record-Error $out 'log-cap' '32 MB log cap reached; further host logs omitted';$script:logCapped=$true};return
    }
    foreach($file in $script:files){try {Read-BleIncrement $file.FullName $states $dest $clock.Elapsed.TotalSeconds}catch {if(-not $script:readErrors.ContainsKey($file.FullName)){Record-Error $out 'read-log' $_.Exception.Message;$script:readErrors[$file.FullName]=$true}}}
}
$readErrors=@{};$logCapped=$false
try {
    try {$owns=$mutex.WaitOne(0)}catch [Threading.AbandonedMutexException]{$owns=$true}
    if(-not $owns){Set-Status 'duplicate' '已有一份蓝牙采集在运行，请使用原窗口。';exit 3}
    Save-Json ([ordered]@{ToolVersion='2026.11.25.1';Started=$spec.Started;IsAdmin=([Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator));
        Mode='Read-only timeline. No MIDI/GATT open, scan, pairing, driver changes, event-channel changes or upload.';Limits='Cached paired/connected properties do not prove working MIDI. Poll timestamps are observation time, not exact event time. Disabled event channels and missing permissions are reported. No HCI/air capture or firmware read.'}) (Join-Path $out 'session.json')
    Timeline 'started' @{ExpectedVersion='1.1.47';MaxSeconds=$spec.MaxSeconds}
    $context=New-Worker 'Context' 'initial' 35
    $job=New-Worker 'Snapshot' 'initial' 25;$lastSnapshot=0;$pending=''
    Set-Status 'capturing' '正在采集。请在风吟中连接，遇到成功或失败时点击对应按钮。'
    $finishReason='time-limit'
    while($clock.Elapsed.TotalSeconds -lt $spec.MaxSeconds){
        Poll-Logs
        $procs=@(Get-FengYinProcesses)
        $key=ConvertTo-Json -InputObject @($procs | Select-Object Id,Name,Started,Path,Version,Error) -Compress
        if($key -ne $lastProcesses){Timeline 'fengyin-processes-changed' $procs;$lastProcesses=$key;$pending='fengyin-start-or-exit'}
        foreach($command in @(Get-ChildItem -LiteralPath $out -Filter 'command-*.json' -File)){
            if($doneCommands.ContainsKey($command.Name)){continue}
            try {$c=Get-Content -LiteralPath $command.FullName -Raw -Encoding UTF8 | ConvertFrom-Json}catch {continue}
            if($c.Kind -in @('failed','connected','disconnected','forgotten','note')){Timeline ('user-'+$c.Kind) $c;$pending=$c.Kind}
            $doneCommands[$command.Name]=$true
        }
        if($context -and -not $context.Finished){$null=Poll-OwnedProcess $context}
        if($job -and -not $job.Finished){$null=Poll-OwnedProcess $job}
        if((Test-Path -LiteralPath (Join-Path $out 'finish.flag'))){$finishReason='user-finished';break}
        if($spec.ParentId -gt 0){
            $parent=Get-Process -Id $spec.ParentId -ErrorAction SilentlyContinue
            if(-not $parent -or $parent.StartTime.ToUniversalTime().ToFileTimeUtc() -ne $spec.ParentStart){$finishReason='interface-closed';break}
        }
        if(((-not $job) -or $job.Finished) -and ($pending -or $clock.Elapsed.TotalSeconds-$lastSnapshot -ge 20)){
            $reason=if($pending){$pending}else{'periodic'};$job=New-Worker 'Snapshot' $reason 25;$lastSnapshot=$clock.Elapsed.TotalSeconds;$pending=''
        }
        Start-Sleep -Milliseconds 1000
    }
    Set-Status 'packing' '正在保存设备状态和系统事件，通常需要约一分钟，请不要关闭此窗口。'
    Timeline 'finishing' @{Reason=$finishReason}
    foreach($j in @($job,$context)){if($j){while(-not (Poll-OwnedProcess $j)){Poll-Logs;Start-Sleep -Milliseconds 250}}}
    $job=New-Worker 'Snapshot' 'final' 25
    while(-not (Poll-OwnedProcess $job)){Poll-Logs;Start-Sleep -Milliseconds 500}
    $job=New-Worker 'Events' 'final' 70
    while(-not (Poll-OwnedProcess $job)){Poll-Logs;Start-Sleep -Milliseconds 500}
    Poll-Logs;Timeline 'finished' @{Reason=$finishReason}
    $errors=Test-Path -LiteralPath (Join-Path $out 'errors.jsonl')
    $logCount=@(Get-Content -LiteralPath (Join-Path $out 'fengyin-ble-timeline.jsonl') -ErrorAction SilentlyContinue).Count
    $summary=[ordered]@{Finished=(Get-Date).ToString('o');Status=if($errors){'collected-with-gaps'}else{'collected'};HostLogRows=$logCount;
        Message='Collection result, not a connectivity pass/fail. Inspect timeline, errors, channel availability and per-probe status.';
        Limitations=@('No raw BLE packets or pairing keys','No active GATT/MIDI probe to avoid changing the failed state','Disabled Windows logs remain disabled','Historical host lines have no guaranteed event timestamp; ObservedAt is read time','Brief process changes under one second can be missed','No automatic root-cause determination')}
    Save-Json $summary (Join-Path $out 'result.json')
    $manifest=@(Get-ChildItem -LiteralPath $out -File | Where-Object {Test-EvidenceName $_.Name} | ForEach-Object {[pscustomobject]@{Name=$_.Name;Bytes=$_.Length;SHA256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}})
    Save-Json $manifest (Join-Path $out 'manifest.json')
    $zip=Write-EvidenceZip $out
    Set-Status 'done' ('采集完成。请发送桌面上的 '+[IO.Path]::GetFileName($zip))
} catch {
    Record-Error $out 'engine' $_.Exception.ToString()
    try {if($job -and -not $job.Finished){$job.Deadline=0;$null=Poll-OwnedProcess $job};if($context -and -not $context.Finished){$context.Deadline=0;$null=Poll-OwnedProcess $context}}catch{}
    try {Save-Json @{Status='interrupted-partial';Error=$_.Exception.Message} (Join-Path $out 'result.json');$null=Write-EvidenceZip $out}catch{}
    Set-Status 'error' '采集未全部完成。已有资料已保留，请发送桌面诊断 ZIP；若没有ZIP，请压缩同名文件夹。'
    exit 2
} finally {if($owns){$mutex.ReleaseMutex()};$mutex.Dispose()}
