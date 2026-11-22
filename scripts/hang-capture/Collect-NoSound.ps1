param([string]$OutputRoot=[Environment]::GetFolderPath('Desktop'))
$ErrorActionPreference='Stop'
if([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT -or -not [Environment]::Is64BitProcess){throw 'Please run Start.cmd on 64-bit Windows.'}
. (Join-Path $PSScriptRoot 'Common.ps1')
$lock=New-Object Threading.Mutex($false,('Local\FengYinNoSoundCapture-'+[Security.Principal.WindowsIdentity]::GetCurrent().User.Value));$owned=$false
try {
    try {$owned=$lock.WaitOne(0)}catch [Threading.AbandonedMutexException]{$owned=$true}
    if(-not $owned){Write-Host '已有采集正在运行，请等待完成。';exit 3}
    if(-not $OutputRoot){$OutputRoot=$PSScriptRoot}
    $out=Join-Path $OutputRoot ('FengYin-NoSound-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N').Substring(0,6))
    $null=New-Item -ItemType Directory -Path $out -Force
    $targets=@(foreach($p in (Get-Process -Name 'FengYin','FengYinDJ','FengYinAudioEngine' -ErrorAction SilentlyContinue)) {
        try {[pscustomobject]@{Id=$p.Id;Name=$p.ProcessName;StartFileTime=$p.StartTime.ToUniversalTime().ToFileTimeUtc();Path=$p.Path;Version=$p.MainModule.FileVersionInfo.FileVersion}}
        catch {Add-CaptureError $out 'target' $_.Exception.Message}
    })
    Write-JsonFile ([pscustomobject]@{Started=(Get-Date).ToString('o');Targets=$targets;ToolVersion='2026.11.22.1';Mode='Read-only no-sound snapshot, no MIDI/audio port opening or memory dump'}) (Join-Path $out 'session.json')
    Write-Host '风吟「气息正常但无声」现场采集' -ForegroundColor Cyan
    Write-Host '保持风吟和USB连接，不要重启、换音色或改声音设置。'
    Write-Host '采集期间可轻吹两三次；不需要吹很久或很用力。'
    if(-not $targets.Count){Write-Host '没有找到运行中的风吟，只能收集已有记录。' -ForegroundColor Yellow}
    function Run-Part([string]$Mode,[string]$Label,[int]$Seconds) {
        $spec=Join-Path $out ($Label+'-request.json')
        Write-JsonFile ([pscustomobject]@{Mode=$Mode;Label=$Label;Output=$out;Targets=$script:targets;AppData=$env:APPDATA;LocalData=$env:LOCALAPPDATA}) $spec
        $r=Invoke-CaptureWorker (Join-Path $PSScriptRoot 'NoSoundWorker.ps1') $spec $out $Label $Seconds
        if($r -ne 0){Add-CaptureError $out $Label ('worker exit '+$r)}
    }
    Write-Host '[1/3] 保存当前日志和必要设置…';Run-Part 'Logs' 'before' 20
    Write-Host '[2/3] 观察声音输出状态约12秒…';Run-Part 'Observe' 'observe' 35
    Write-Host '[3/3] 保存后续日志和设备状态…';Run-Part 'Logs' 'after' 20;Run-Part 'Context' 'context' 40
    $observations=@(Get-ChildItem -LiteralPath $out -Filter 'audio-*.json' -File)
    $queryErrors=@($observations | ForEach-Object {Get-Content -LiteralPath $_.FullName -Raw -Encoding UTF8 | ConvertFrom-Json} | Where-Object {$_.Error})
    $appTargets=@($targets | Where-Object {$_.Name -in @('FengYin','FengYinDJ')})
    $status=if($appTargets.Count -and $observations.Count -eq 4 -and $queryErrors.Count -eq 0 -and -not (Test-Path -LiteralPath (Join-Path $out 'errors.jsonl'))){'collected'}else{'partial'}
    Write-JsonFile ([pscustomobject]@{Finished=(Get-Date).ToString('o');Status=$status;AudioObservations=$observations.Count;AudioQueryErrors=$queryErrors.Count;AppCount=$appTargets.Count;
        Limits='Saved preferences are not proof of live state. No raw MIDI was captured. Zero meter alone cannot prove silence. No automatic root-cause claim.'}) (Join-Path $out 'result.json')
    Add-Type -AssemblyName System.IO.Compression
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip=$out+'.zip';$archive=[IO.Compression.ZipFile]::Open($zip,[IO.Compression.ZipArchiveMode]::Create)
    try {foreach($f in (Get-ChildItem -LiteralPath $out -File | Where-Object {$_.Name -notlike '*-request.json'})){
        $null=[IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive,$f.FullName,$f.Name,[IO.Compression.CompressionLevel]::Fastest)
    }}finally{$archive.Dispose()}
    Write-Host '';Write-Host '采集已结束，请把桌面上的这个ZIP发回：' -ForegroundColor Green;Write-Host $zip
    if($status -eq 'partial'){Write-Host '有部分项目未取得，请仍发送ZIP。' -ForegroundColor Yellow}
    Write-Host '现在可以按需重启风吟。请同时告知：问题发生前的操作和当时使用的音色。'
    if($status -eq 'partial'){exit 2}
} catch {Write-Host ('采集未全部完成，请保留并压缩此目录：'+$out) -ForegroundColor Yellow;Write-Host $_.Exception.Message;exit 2}
finally {if($owned){$lock.ReleaseMutex()};$lock.Dispose()}
