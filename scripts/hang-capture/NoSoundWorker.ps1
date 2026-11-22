param([Parameter(Mandatory=$true)][string]$SpecPath)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
. (Join-Path $PSScriptRoot 'Common.ps1')
$spec=Get-Content -LiteralPath $SpecPath -Raw -Encoding UTF8 | ConvertFrom-Json
$out=$spec.Output
function Part([string]$Name,[scriptblock]$Action) {
    try {& $Action | Out-String -Width 260 | Set-Content -LiteralPath (Join-Path $out ($Name+'.txt')) -Encoding UTF8}
    catch {
        if($_.FullyQualifiedErrorId -like 'NoMatchingEventsFound*') {'No matching events.' | Set-Content -LiteralPath (Join-Path $out ($Name+'.txt')) -Encoding UTF8}
        else {Add-CaptureError $out $Name $_.Exception.Message}
    }
}
try {
    if($spec.Mode -eq 'Logs') {
        foreach($root in @($spec.AppData,$spec.LocalData)) {
            if(-not $root){continue};$kind=if($root -eq $spec.AppData){'roaming'}else{'local'}
            foreach($edition in @('FengYin','FengYinDJ')) {
                foreach($name in @('plugin-host.log','audio-engine.log','audio-source-diagnostic.log','audio-timing-diagnostic.log')) {
                    $file=Join-Path (Join-Path $root $edition) $name
                    if(Test-Path -LiteralPath $file){Part ($spec.Label+'-'+$kind+'-'+$edition+'-'+$name) {Read-LogTail $file}}
                }
                foreach($name in @('FengYin.settings','FengYinMidi.settings')) {
                    $file=Join-Path (Join-Path $root $edition) $name
                    if(Test-Path -LiteralPath $file){Part ($spec.Label+'-'+$kind+'-'+$edition+'-'+$name) {Read-AudioPreferences $file | Format-List}}
                }
            }
        }
    } elseif($spec.Mode -eq 'Observe') {
        Add-Type -Path (Join-Path $PSScriptRoot 'AudioSnapshot.cs')
        for($i=0;$i -lt 4;$i++) {
            $valid=@(foreach($target in $spec.Targets){
                try {
                    $p=Get-Process -Id $target.Id -ErrorAction Stop
                    if($p.StartTime.ToUniversalTime().ToFileTimeUtc() -eq [long]$target.StartFileTime) {
                        [pscustomobject]@{Id=$p.Id;Name=$p.ProcessName;StartFileTime=$target.StartFileTime;CPUSeconds=$p.TotalProcessorTime.TotalSeconds;PrivateBytes=$p.PrivateMemorySize64;WorkingSet=$p.WorkingSet64;Handles=$p.HandleCount;Threads=$p.Threads.Count}
                    }
                }catch{Add-CaptureError $out 'process-observation' $_.Exception.Message}
            })
            Write-JsonFile ([pscustomobject]@{Time=(Get-Date).ToString('o');Processes=$valid}) (Join-Path $out ('process-'+$i+'.json'))
            $ids=[uint32[]]@($valid | ForEach-Object {$_.Id})
            Write-JsonFile ([FengYinAudioSnapshot]::Read($ids)) (Join-Path $out ('audio-'+$i+'.json'))
            if($i -lt 3){Start-Sleep -Seconds 4}
        }
    } elseif($spec.Mode -eq 'Context') {
        Part 'system' {
            Get-Date -Format o;Get-TimeZone | Select-Object Id,BaseUtcOffset
            Get-CimInstance Win32_OperatingSystem | Select-Object Caption,Version,BuildNumber,LastBootUpTime,FreePhysicalMemory | Format-List
            Get-CimInstance Win32_ComputerSystem | Select-Object Manufacturer,Model,TotalPhysicalMemory | Format-List
            Get-Service Audiosrv,AudioEndpointBuilder | Select-Object Name,Status,StartType | Format-Table
        }
        Part 'audio-devices' {Get-CimInstance Win32_SoundDevice | Select-Object Name,Status,ConfigManagerErrorCode,PNPDeviceID | Format-List}
        Part 'audio-drivers' {Get-CimInstance Win32_PnPSignedDriver -Filter "DeviceClass='MEDIA'" | Select-Object DeviceName,DriverProviderName,DriverVersion,DriverDate | Format-List}
        Part 'present-yamaha' {Get-PnpDevice -PresentOnly | Where-Object {$_.FriendlyName -match '(?i)Yamaha|YDS.?150'} | Select-Object Status,Class,FriendlyName,InstanceId,Problem | Format-List}
        Part 'render-endpoint-names' {
            $key='HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\MMDevices\Audio\Render'
            if(Test-Path $key){Get-ChildItem $key | ForEach-Object {
                $v=Get-ItemProperty (Join-Path $_.PSPath 'Properties') -ErrorAction SilentlyContinue
                [pscustomobject]@{Id=$_.PSChildName;Name=$v.'{a45c254e-df1c-4efd-8020-67d146a850e0},14'}
            } | Format-List}
        }
        Part 'audio-errors' {
            Get-WinEvent -FilterHashtable @{LogName='Application';StartTime=(Get-Date).AddHours(-4);Id=1000,1001,1002} -MaxEvents 300 |
                Where-Object {$_.Message -match '(?i)FengYin|audiodg|风吟'} | Select-Object TimeCreated,Id,ProviderName,Message | Format-List
        }
        Part 'audio-system-errors' {
            Get-WinEvent -FilterHashtable @{LogName='System';StartTime=(Get-Date).AddHours(-4);Level=1,2,3} -MaxEvents 300 |
                Where-Object {$_.ProviderName -match '(?i)Audio|Kernel-PnP|UserPnp' -or $_.Message -match '(?i)audiosrv|AudioEndpointBuilder|Realtek|YDS.?150'} | Select-Object TimeCreated,Id,ProviderName,Message | Format-List
        }
    } else {throw 'Unknown no-sound collection mode'}
} catch {Add-CaptureError $out $spec.Mode $_.Exception.Message;exit 1}
