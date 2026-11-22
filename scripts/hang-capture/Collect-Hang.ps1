param([string]$OutputRoot=[Environment]::GetFolderPath('Desktop'),[int]$TargetProcessId=0)
$ErrorActionPreference='Stop'
if([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT -or -not [Environment]::Is64BitProcess){
    throw 'Please run Start.cmd on the 64-bit Windows computer running FengYin.'
}
. (Join-Path $PSScriptRoot 'Common.ps1')
$lock=New-Object Threading.Mutex($false,('Local\FengYinHangCapture-'+[Security.Principal.WindowsIdentity]::GetCurrent().User.Value))
$ownsLock=$false
try {
    try {$ownsLock=$lock.WaitOne(0)} catch [Threading.AbandonedMutexException] {$ownsLock=$true}
    if(-not $ownsLock){Write-Host '已有采集窗口正在运行，请等待该窗口完成，不要重复启动。' -ForegroundColor Yellow;exit 3}
if(-not $OutputRoot){$OutputRoot=$PSScriptRoot}
$null=New-Item -ItemType Directory -Path $OutputRoot -Force
$out=Join-Path $OutputRoot ('FengYin-Hang-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N').Substring(0,6))
$null=New-Item -ItemType Directory -Path $out
$started=(Get-Date).ToString('o')
$timedOutDump=$false
Write-Host '风吟卡死现场采集' -ForegroundColor Cyan
Write-Host '请保持卡死现场，不要关闭或重启风吟。正在自动采集，无需操作。'
Write-Host '转储会短暂增加开销，可能含内存中的路径/文本；只保存本地，不上传。'
function Run-Part([string]$Mode,[int]$Round=0,[int]$Seconds=45,[switch]$IsDump) {
    $label=$Mode.ToLower()+$Round
    $spec=Join-Path $out ($label+'-request.json')
    Write-JsonFile ([pscustomobject]@{Mode=$Mode;Round=$Round;Output=$out;Targets=@($script:targets);AppData=$env:APPDATA;LocalData=$env:LOCALAPPDATA}) $spec
    $result=Invoke-CaptureWorker (Join-Path $PSScriptRoot 'Worker.ps1') $spec $out $label $Seconds -DumpWorker:$IsDump
    if($result -eq 124 -and $IsDump){$script:timedOutDump=$true}
    if($result -ne 0){Add-CaptureError $out $label ('worker exit '+$result)}
}
$targets=@()
try {
    $candidates=@(Get-Process -Name 'FengYin','FengYinDJ' -ErrorAction SilentlyContinue | Where-Object {
        $TargetProcessId -eq 0 -or $_.Id -eq $TargetProcessId
    } | Sort-Object @{Expression={if($_.MainWindowHandle -ne [IntPtr]::Zero){0}else{1}}},Id)
    foreach($p in ($candidates | Select-Object -First 2)) {
        try {$targets += [pscustomobject]@{Id=$p.Id;Name=$p.ProcessName;StartFileTime=$p.StartTime.ToUniversalTime().ToFileTimeUtc();Path=$p.Path;Version=$p.MainModule.FileVersionInfo.FileVersion}}
        catch {Add-CaptureError $out 'identity' $_.Exception.Message}
    }
    Write-JsonFile ([pscustomobject]@{Started=$started;Marker='Collector launch, not an automatically detected hang time';CandidateCount=$candidates.Count;Targets=@($targets);CollectorVersion='2026.11.22.1';IsAdministrator=([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)}) (Join-Path $out 'session.json')
    if($targets.Count) {
        Write-Host '[1/4] 保存第一次卡死线程现场…'
        Run-Part 'Round' 1 100 -IsDump
        if(-not $timedOutDump) {
            Start-Sleep -Seconds 4
            Write-Host '[2/4] 保存第二次线程现场与窗口状态…'
            Run-Part 'Round' 2 100 -IsDump
        }
        if(-not $timedOutDump) {
            Write-Host '[3/4] 检查属于风吟的网页界面进程…'
            Run-Part 'WebView' 0 120 -IsDump
        }
    } else {
        Add-CaptureError $out 'targets' 'No running FengYin/FengYinDJ target. No hang dump captured; application was not started.'
        Write-Host '没有找到运行中的风吟；只能收集已有日志，无法取得卡死现场。' -ForegroundColor Yellow
    }
    Write-Host '[4/4] 收集相关日志、系统事件和版本信息…'
    Run-Part 'Logs' 0 25
    Run-Part 'Environment' 0 45
} catch {Add-CaptureError $out 'collection' $_.Exception.Message}
$valid=@(Get-ChildItem -LiteralPath $out -Filter '*.dmp' -File)
$main1=@($valid | Where-Object Name -like 'round1-*').Count
$main2=@($valid | Where-Object Name -like 'round2-*').Count
$pairs=@($targets | Where-Object {(Test-Path -LiteralPath (Join-Path $out ('round1-pid'+$_.Id+'.dmp'))) -and (Test-Path -LiteralPath (Join-Path $out ('round2-pid'+$_.Id+'.dmp')))})
$status=if($pairs.Count -gt 0 -and -not $timedOutDump){'captured-two-main-snapshots'}else{'partial-no-complete-hang-pair'}
Write-JsonFile ([pscustomobject]@{Started=$started;Finished=(Get-Date).ToString('o');Status=$status;MainRound1=$main1;MainRound2=$main2;PairedTargetIds=@($pairs | ForEach-Object {$_.Id});DumpCount=$valid.Count;DumpWorkerStillRunning=$timedOutDump;HasErrors=(Test-Path -LiteralPath (Join-Path $out 'errors.jsonl'));Warning='Compact memory snapshots may contain private data. Not proof of root cause or successful repair.'}) (Join-Path $out 'result.json')
@'
本包是卡死现场取证，不是修复，不证明故障根因。
紧凑转储包含线程栈、模块/句柄/内存区域信息，可能包含进程中的路径/文本，不是完整内存。请私下回传，不要公开。
不复制音色文件、录音、视频、许可证或完整用户配置。仅日志做关键字过滤，不能保证内存转储脱敏。
不修改蓝牙/驱动/电源/音色，不重启或结束风吟，不安装服务，不主动联网/上传。
采集会造成短暂开销，窗口切换可能改变故障。采集时间不等于故障起始时间。
如果result.json为partial，或有errors.jsonl，请一起发送；这不代表没有故障。
Microsoft MiniDumpWriteDump: https://learn.microsoft.com/en-us/windows/win32/api/minidumpapiset/nf-minidumpapiset-minidumpwritedump
'@ | Set-Content -LiteralPath (Join-Path $out '说明.txt') -Encoding UTF8
try {
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip=$out+'.zip'
    $files=@(Get-ChildItem -LiteralPath $out -File | Where-Object {$_.Name -notlike '*.partial' -and $_.Name -notlike '*-request.json'})
    $drive=New-Object IO.DriveInfo([IO.Path]::GetPathRoot($out))
    $total=($files | Measure-Object Length -Sum).Sum
    if($drive.AvailableFreeSpace -lt ([long]$total+64MB)){throw 'Not enough disk space to make ZIP; evidence folder retained'}
    $archive=[IO.Compression.ZipFile]::Open($zip,[IO.Compression.ZipArchiveMode]::Create)
    try {foreach($f in $files){$null=[IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive,$f.FullName,$f.Name,[IO.Compression.CompressionLevel]::Fastest)}}finally{$archive.Dispose()}
    Write-Host ''
    if($status -eq 'captured-two-main-snapshots'){Write-Host '已保存两次风吟线程现场。请把以下ZIP发回：' -ForegroundColor Green}
    else {Write-Host '部分采集未完成，请仍发送以下ZIP，便于判断缺了什么：' -ForegroundColor Yellow}
    Write-Host $zip
    Write-Host '同时说明：卡死前在做什么、卡死后吹奏/伴奏是否仍然有声音。'
    if($timedOutDump){Write-Host '转储工作仍可能在完成，请勿立刻结束风吟；如有新的文件产生，保留同名文件夹。' -ForegroundColor Yellow}
    else {Write-Host '采集已结束，现在可以按需重启风吟。'}
} catch {
    Add-CaptureError $out 'archive' $_.Exception.Message
    Write-Host ('打包未完成，请保留并手动压缩此目录：'+$out) -ForegroundColor Yellow
    Write-Host $_.Exception.Message
    exit 2
}
if($status -ne 'captured-two-main-snapshots'){exit 2}

} finally {if($ownsLock){$lock.ReleaseMutex()};$lock.Dispose()}
