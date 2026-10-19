param([ValidateRange(30,300)][int]$Seconds=180,
      [string]$OriginalAppData=$env:APPDATA,[string]$OriginalLocalData=$env:LOCALAPPDATA,
      [switch]$NoPrompt,[string]$OutputBase=$PSScriptRoot)
$ErrorActionPreference='Stop'
$principal=New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
if(-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    # The application is never launched from this elevated process.
    $arguments='-NoProfile -ExecutionPolicy Bypass -File "'+$PSCommandPath+'" -Seconds '+$Seconds+' -OriginalAppData "'+$OriginalAppData+'" -OriginalLocalData "'+$OriginalLocalData+'"'
    $child=Start-Process powershell.exe -Verb RunAs -ArgumentList $arguments -PassThru -Wait
    exit $child.ExitCode
}
$Out=Join-Path $OutputBase ('Video-stall-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N').Substring(0,6))
New-Item -ItemType Directory -Path $Out | Out-Null
. (Join-Path $PSScriptRoot 'NativeCommands.ps1')
$Wpr=Join-Path $env:SystemRoot 'System32/wpr.exe'
$Session='FengYinVideo-'+[guid]::NewGuid().ToString('N')
$Tracing=$false
$StartAttempted=$false
$CaptureError=$null
function Run-Wpr([string]$Arguments,[string]$Label,[int]$Timeout=30,[string]$Instance=$Session) {
    $result=Invoke-CapturedNative $Wpr ($Arguments+' -instancename '+$Instance) $Out $Label $Timeout
    [pscustomobject]@{Command=$Label;Session=$Instance;ExitCode=$result.ExitCode} | ConvertTo-Json -Compress | Add-Content -LiteralPath (Join-Path $Out 'commands.jsonl') -Encoding UTF8
    if($result.ExitCode -ne 0){throw ('WPR command failed: '+$Label+' ('+$result.ExitCode+') '+$result.Output+' '+$result.Error)}
}
function Save-Logs([string]$Label) {
    foreach($pair in @(@('roaming',$OriginalAppData),@('local',$OriginalLocalData))) {
        foreach($name in @('audio-engine.log','plugin-host.log','audio-timing-diagnostic.log','audio-source-diagnostic.log')) {
            $file=Join-Path (Join-Path $pair[1] 'FengYin') $name
            if(Test-Path -LiteralPath $file) {
                try {Get-Content -LiteralPath $file -Encoding UTF8 -Tail 12000 | Set-Content -LiteralPath (Join-Path $Out ($Label+'-'+$pair[0]+'-'+$name)) -Encoding UTF8}
                catch { $_.Exception.Message | Add-Content -LiteralPath (Join-Path $Out 'log-errors.txt') }
            }
        }
    }
}
try {
    if(-not [Environment]::Is64BitProcess){throw 'Use 64-bit Windows PowerShell.'}
    if(-not (Test-Path -LiteralPath $Wpr)){throw 'Windows Performance Recorder is unavailable; no trace was started.'}
    Add-Type -Path (Join-Path $PSScriptRoot 'WindowSnapshot.cs')
    # Persist the unique ID BEFORE attempting start, so even partial failures are recoverable.
    [pscustomobject]@{Started=(Get-Date).ToString('o');Seconds=$Seconds;CollectorPID=$PID;Session=$Session;LogicalProcessors=[Environment]::ProcessorCount;WprVersion=(Get-Item -LiteralPath $Wpr).VersionInfo.FileVersion;OSVersion=[Environment]::OSVersion.VersionString;Kind='CPU ETW and read-only window state; not a fix'} |
        ConvertTo-Json | Set-Content -LiteralPath (Join-Path $Out 'session.json') -Encoding UTF8
    $inventory=Invoke-CapturedNative (Join-Path $env:SystemRoot 'System32/logman.exe') 'query -ets' $Out 'trace-inventory' 30
    if($inventory.ExitCode -ne 0){throw 'Cannot inventory prior collector sessions; refusing to start another trace.'}
    foreach($prior in @(Get-OwnedStallSessionNames $inventory.Output)) {
        if($prior -eq $Session){continue}
        Write-Host 'Saving a previous FengYin collector trace before starting this run.' -ForegroundColor Yellow
        Run-Wpr ('-stop "'+(Join-Path $Out ($prior+'.etl'))+'" "FengYin previous diagnostic" -skipPdbGen') ('recover-'+$prior) 120 $prior
    }
    Save-Logs 'before'
    Write-Host 'Start this collector BEFORE reproducing. UAC/window switching can temporarily recover an existing fault.' -ForegroundColor Yellow
    Write-Host 'Press ENTER. Collecting lasts 180 seconds by default; do not return to this window during the fault.'
    Write-Host 'After CAPTURING: open or return to FengYin normally, play briefly, select the same video, then keep playing.'
    if(-not $NoPrompt){$null=Read-Host}
    # Memory-mode CPU trace is bounded by the built-in profile buffers, unlike unbounded file mode.
    $StartAttempted=$true
    Run-Wpr '-start CPU' 'wpr-start'
    $Tracing=$true
    Run-Wpr '-status' 'wpr-confirm-running'
    Write-Host ('CAPTURING '+$Seconds+' seconds. Return to FengYin now. No settings will change.') -ForegroundColor Green
    $clock=[Diagnostics.Stopwatch]::StartNew()
    $identities=@{}
    while($clock.Elapsed.TotalSeconds -lt $Seconds) {
        $stamp=(Get-Date).ToString('o')
        # Process names and IDs only; do not collect command lines, titles, document contents or unrelated browser windows.
        $items=@(Get-CimInstance Win32_Process -Filter "Name='FengYin.exe' OR Name='msedgewebview2.exe'")
        $selected=@{}
        foreach($item in $items){if($item.Name -eq 'FengYin.exe'){$selected[[int]$item.ProcessId]=$true}}
        for($depth=0;$depth -lt 8;$depth++) {
            foreach($item in $items){if($selected.ContainsKey([int]$item.ParentProcessId)){$selected[[int]$item.ProcessId]=$true}}
        }
        foreach($item in $items) {
            $processId=[int]$item.ProcessId
            if(-not $selected.ContainsKey($processId)){continue}
            try {
                $app=Get-Process -Id $processId -ErrorAction Stop
                $identity=[string]$processId+':'+$app.StartTime.ToUniversalTime().Ticks
                [pscustomobject]@{Time=$stamp;Name=$item.Name;PID=$processId;ParentPID=$item.ParentProcessId;Started=$app.StartTime.ToString('o');UserMs=$app.UserProcessorTime.TotalMilliseconds;KernelMs=$app.PrivilegedProcessorTime.TotalMilliseconds;PrivateBytes=$app.PrivateMemorySize64;Handles=$app.HandleCount} |
                    Export-Csv -LiteralPath (Join-Path $Out 'processes.csv') -NoTypeInformation -Append -Encoding UTF8
                if($item.Name -ne 'FengYin.exe'){continue}
                if(-not $identities.ContainsKey($identity)) {
                    $identities[$identity]=$true
                    if($app.Path){[pscustomobject]@{PID=$processId;Started=$app.StartTime.ToString('o');SHA256=(Get-FileHash -LiteralPath $app.Path -Algorithm SHA256).Hash} | ConvertTo-Json -Compress | Add-Content -LiteralPath (Join-Path $Out 'binaries.jsonl') -Encoding UTF8}
                }
                [FengYin.StallCapture.Windows]::Take([uint32]$processId) | ConvertTo-Json -Depth 8 -Compress | Add-Content -LiteralPath (Join-Path $Out 'windows.jsonl') -Encoding UTF8
                foreach($thread in $app.Threads) {
                    try {[pscustomobject]@{Time=$stamp;PID=$processId;TID=$thread.Id;UserMs=$thread.UserProcessorTime.TotalMilliseconds;KernelMs=$thread.PrivilegedProcessorTime.TotalMilliseconds;State=[string]$thread.ThreadState} | Export-Csv -LiteralPath (Join-Path $Out 'threads.csv') -NoTypeInformation -Append -Encoding UTF8} catch {}
                }
            } catch { $_.Exception.Message | Add-Content -LiteralPath (Join-Path $Out 'sample-errors.txt') }
        }
        Start-Sleep -Seconds 2
    }
    Run-Wpr '-status' 'wpr-status'
    if(-not (Test-Path -LiteralPath (Join-Path $Out 'windows.jsonl'))) {
        throw 'No FengYin window snapshots were captured; this is not a successful diagnostic run.'
    }
} catch {
    $CaptureError=$_.Exception.Message
    $CaptureError | Set-Content -LiteralPath (Join-Path $Out 'capture-error.txt') -Encoding UTF8
} finally {
    if($Tracing) {
        # Use the inbox Windows 10 grammar. Trace compression is done by ZIP,
        # not the newer optional WPR switch introduced with the Windows 11 ADK.
        try {Run-Wpr ('-stop "'+(Join-Path $Out 'cpu.etl')+'" "FengYin CPU diagnostic" -skipPdbGen') 'wpr-stop' 120;$Tracing=$false}
        catch {
            $CaptureError=$_.Exception.Message
            $CaptureError | Set-Content -LiteralPath (Join-Path $Out 'trace-stop-error.txt') -Encoding UTF8
            # Never cancel another tool's trace; the unique instance is mandatory on every command.
            try {Run-Wpr '-cancel' 'wpr-cancel-own' 30;$Tracing=$false} catch {$_ | Out-String | Set-Content -LiteralPath (Join-Path $Out 'cancel-error.txt')}
        }
    } elseif($StartAttempted) {
        # Start can fail after partially creating sessions. Our GUID was saved before the attempt.
        try {Run-Wpr '-cancel' 'wpr-cancel-partial-start' 30} catch {$_ | Out-String | Set-Content -LiteralPath (Join-Path $Out 'partial-start-cleanup.txt')}
    }
    Save-Logs 'after'
    @('CPU trace can contain system process/module names and paths; do not publish it.',
      'Window snapshots contain handles, classes, rectangles, enabled/visible state, focus/capture and hit-test samples.',
      'No audio/video recording, screenshots, full process dumps, text contents, tone or license settings collected.',
      'No application launch/restart/termination, input injection, focus/priority/power/driver/buffer/tone changes.',
      'Tracing adds overhead. This is diagnostic evidence, NOT a latency or sound-quality acceptance pass.',
      'Memory-mode ETW may retain only a recent interval; inspect wpr-status and trace lost-event counters.',
      'Sources: https://learn.microsoft.com/en-us/windows-hardware/test/wpt/wpr-command-line-options',
      'https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getguithreadinfo') |
        Set-Content -LiteralPath (Join-Path $Out 'ABOUT.txt') -Encoding UTF8
    $archive=$Out+'.zip'
    try {Compress-Archive -LiteralPath $Out -DestinationPath $archive;Write-Host ('Send this ZIP: '+$archive) -ForegroundColor Green}
    catch {$CaptureError=$_.Exception.Message;Write-Host ('ZIP failed. Evidence directory: '+$Out) -ForegroundColor Red}
}
if($CaptureError){Write-Host $CaptureError -ForegroundColor Red;exit 1}
