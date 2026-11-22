param([Parameter(Mandatory=$true)][string]$SpecPath)
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
. (Join-Path $PSScriptRoot 'Common.ps1')
$spec=Get-Content -LiteralPath $SpecPath -Encoding UTF8 -Raw | ConvertFrom-Json
$out=$spec.Output
function Save-Snapshot($Target,[string]$Label) {
    $app=Get-Process -Id $Target.Id -ErrorAction Stop
    if($app.StartTime.ToUniversalTime().ToFileTimeUtc() -ne [long]$Target.StartFileTime){throw 'Process ID reused; snapshot refused'}
    $app.Refresh()
    Write-JsonFile ([pscustomobject]@{Time=(Get-Date).ToString('o');PID=$app.Id;Name=$app.ProcessName;
        StartFileTime=$Target.StartFileTime;CPUSeconds=$app.TotalProcessorTime.TotalSeconds;
        PrivateBytes=$app.PrivateMemorySize64;WorkingSet=$app.WorkingSet64;Handles=$app.HandleCount;
        Threads=$app.Threads.Count;MainWindow=('0x'+$app.MainWindowHandle.ToInt64().ToString('x'))}) (Join-Path $out ($Label+'-process.json'))
    $threads=@(foreach($thread in $app.Threads){
        try {
            $reason=$null;if($thread.ThreadState -eq [Diagnostics.ThreadState]::Wait){$reason=[string]$thread.WaitReason}
            [pscustomobject]@{TID=$thread.Id;State=[string]$thread.ThreadState;WaitReason=$reason;CPUms=$thread.TotalProcessorTime.TotalMilliseconds}
        } catch {}
    })
    Write-JsonFile $threads (Join-Path $out ($Label+'-threads.json'))
    [FengYin.StallCapture.Windows]::Take([uint32]$app.Id) | ConvertTo-Json -Depth 10 |
        Set-Content -LiteralPath (Join-Path $out ($Label+'-windows.json')) -Encoding UTF8
}
function Capture-Target($Target,[string]$Label) {
    try {
        Save-Snapshot $Target $Label
        $drive=New-Object IO.DriveInfo([IO.Path]::GetPathRoot($out))
        if($drive.AvailableFreeSpace -lt 256MB){throw 'Insufficient free disk space; dump skipped'}
        $dmp=Join-Path $out ($Label+'.dmp')
        [FengYin.HangCapture.DumpNative]::Write([uint32]$Target.Id,[long]$Target.StartFileTime,[string]$Target.Name,$dmp)
        Write-JsonFile ([pscustomobject]@{PID=$Target.Id;StartFileTime=$Target.StartFileTime;Completed=(Get-Date).ToString('o');
            Bytes=(Get-Item -LiteralPath $dmp).Length;SHA256=(Get-FileHash -LiteralPath $dmp -Algorithm SHA256).Hash;
            Kind='Compact stacks/handles/modules/thread/memory-region metadata; not full memory';Valid=$true}) (Join-Path $out ($Label+'-dump.json'))
        return $true
    } catch {Add-CaptureError $out $Label $_.Exception.Message;return $false}
}
function Context-Part([string]$Name,[scriptblock]$Action) {
    try {& $Action | Out-String -Width 260 | Set-Content -LiteralPath (Join-Path $out ($Name+'.txt')) -Encoding UTF8}
    catch {
        if($_.FullyQualifiedErrorId -like 'NoMatchingEventsFound*') {
            'No matching events in the requested time window.' | Set-Content -LiteralPath (Join-Path $out ($Name+'.txt')) -Encoding UTF8
        } else {Add-CaptureError $out $Name $_.Exception.Message}
    }
}
try {
    if($spec.Mode -in @('Round','WebView')) {
        Add-Type -Path (Join-Path $PSScriptRoot 'DumpNative.cs')
        Add-Type -Path (Join-Path $PSScriptRoot 'WindowSnapshot.cs')
    }
    if($spec.Mode -eq 'Round') {
        $success=0
        foreach($target in $spec.Targets) {
            if(Capture-Target $target ('round'+$spec.Round+'-pid'+$target.Id)){$success++}
        }
        Write-JsonFile ([pscustomobject]@{Attempted=@($spec.Targets).Count;Succeeded=$success;Round=$spec.Round}) (Join-Path $out ('round'+$spec.Round+'-result.json'))
    } elseif($spec.Mode -eq 'WebView') {
        # Command lines stay in memory; save only a derived fixed role.
        $rows=@(Get-CimInstance Win32_Process -Filter "Name='msedgewebview2.exe'" | ForEach-Object {
            try {
                $p=Get-Process -Id $_.ProcessId -ErrorAction Stop
                $role='browser';if($_.CommandLine -match '--type=([\w-]+)'){$role=$Matches[1]}
                [pscustomobject]@{ProcessId=$p.Id;ParentProcessId=$_.ParentProcessId;StartFileTime=$p.StartTime.ToUniversalTime().ToFileTimeUtc();Name=$_.Name;Role=$role}
            } catch {}
        })
        $roots=@($spec.Targets | Where-Object {
            try {[FengYin.HangCapture.DumpNative]::Identity([int]$_.Id) -eq [long]$_.StartFileTime} catch {$false}
        })
        $related=@(Select-RelatedWebViews $rows $roots)
        Write-JsonFile $related (Join-Path $out 'fengyin-webview-tree.json')
        $chosen=@($related | Sort-Object @{Expression={if($_.Role -eq 'renderer'){0}elseif($_.Role -eq 'browser'){1}else{2}}},ProcessId | Select-Object -First 3)
        foreach($row in $chosen) {
            $target=[pscustomobject]@{Id=$row.ProcessId;StartFileTime=$row.StartFileTime;Name='msedgewebview2'}
            $null=Capture-Target $target ('webview-'+$row.Role+'-pid'+$row.ProcessId)
        }
    } elseif($spec.Mode -eq 'Logs') {
        foreach($parent in @($spec.AppData,$spec.LocalData)) {
            if(-not $parent){continue}
            foreach($edition in @('FengYin','FengYinDJ')) {
                foreach($name in @('plugin-host.log','audio-engine.log','audio-timing-diagnostic.log','audio-source-diagnostic.log')) {
                    $file=Join-Path (Join-Path $parent $edition) $name
                    if(Test-Path -LiteralPath $file) {
                        $kind=if($parent -eq $spec.AppData){'roaming'}else{'local'}
                        Context-Part ($kind+'-'+$edition+'-'+$name) {Read-LogTail $file}
                    }
                }
            }
        }
    } elseif($spec.Mode -eq 'Environment') {
        Context-Part 'system' {
            Get-Date -Format o
            Get-TimeZone | Select-Object Id,BaseUtcOffset
            Get-CimInstance Win32_OperatingSystem | Select-Object Caption,Version,BuildNumber,LastBootUpTime,FreePhysicalMemory,TotalVisibleMemorySize
            Get-CimInstance Win32_ComputerSystem | Select-Object Manufacturer,Model,TotalPhysicalMemory
            Get-CimInstance Win32_Processor | Select-Object Name,NumberOfCores,NumberOfLogicalProcessors
        }
        Context-Part 'graphics-drivers' {Get-CimInstance Win32_VideoController | Select-Object Name,DriverVersion,DriverDate,Status}
        Context-Part 'app-events' {
            Get-WinEvent -FilterHashtable @{LogName='Application';StartTime=(Get-Date).AddHours(-4);Id=1000,1001,1002} -MaxEvents 150 -ErrorAction Stop |
                Where-Object {$_.Message -match '(?i)FengYin|风吟'} |
                Select-Object TimeCreated,Id,ProviderName,LevelDisplayName,Message | Format-List
        }
        Context-Part 'graphics-system-events' {
            Get-WinEvent -FilterHashtable @{LogName='System';StartTime=(Get-Date).AddHours(-4);Level=1,2,3} -MaxEvents 200 -ErrorAction Stop |
                Where-Object {$_.ProviderName -match '^(Display|Microsoft-Windows-WHEA-Logger|nvlddmkm|amdwddmg|igfx)$'} |
                Select-Object TimeCreated,Id,ProviderName,LevelDisplayName,Message | Format-List
        }
    } else {throw 'Unknown worker mode'}
} catch {Add-CaptureError $out $spec.Mode $_.Exception.Message;exit 1}
