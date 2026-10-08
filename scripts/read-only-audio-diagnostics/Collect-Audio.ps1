param([string]$OutputRoot = $PSScriptRoot)
$ErrorActionPreference = 'Stop'
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$out = Join-Path $OutputRoot ("FengYin-Diagnostics-" + $stamp + '-' + [guid]::NewGuid().ToString('N').Substring(0,6))
New-Item -ItemType Directory -Path $out | Out-Null
function Collect([string]$Name, [scriptblock]$Action) {
    try { & $Action | Out-String -Width 240 | Set-Content -LiteralPath (Join-Path $out ($Name + '.txt')) -Encoding UTF8 }
    catch { $_.Exception.Message | Set-Content -LiteralPath (Join-Path $out ($Name + '-error.txt')) -Encoding UTF8 }
}
Collect 'system' {
    Get-CimInstance Win32_OperatingSystem | Select-Object Caption,Version,BuildNumber,OSArchitecture,LastBootUpTime | Format-List
    Get-CimInstance Win32_ComputerSystem | Select-Object Manufacturer,Model | Format-List
    Get-Date -Format o
}
Collect 'audio-devices' {
    Get-CimInstance Win32_SoundDevice | Select-Object Name,Manufacturer,Status,ConfigManagerErrorCode,PNPDeviceID | Format-List
    Get-PnpDevice -Class AudioEndpoint,MEDIA -ErrorAction Stop | Select-Object Status,Class,FriendlyName,InstanceId,Problem | Format-List
}
Collect 'audio-drivers' {
    Get-CimInstance Win32_PnPSignedDriver | Where-Object { $_.DeviceClass -eq 'MEDIA' } |
        Select-Object DeviceName,DriverProviderName,DriverVersion,DriverDate,InfName,IsSigned | Format-List
}
Collect 'services-processes' {
    Get-Service Audiosrv,AudioEndpointBuilder | Select-Object Name,Status,StartType | Format-Table
    Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.ProcessName -match 'FengYin|ASIO|Voicemeeter' } |
        Select-Object ProcessName,Id,StartTime | Format-Table
}
Collect 'registered-asio' {
    foreach ($root in @('HKLM:\SOFTWARE\ASIO','HKLM:\SOFTWARE\WOW6432Node\ASIO')) {
        if (Test-Path $root) {
            Get-ChildItem $root | ForEach-Object {
                $p = Get-ItemProperty $_.PSPath
                [pscustomobject]@{Registry=$_.Name;Description=$p.Description;CLSID=$p.CLSID} | Format-List
            }
        }
    }
}
Collect 'audio-endpoints' {
    foreach ($flow in @('Render','Capture')) {
        $root = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\MMDevices\Audio\' + $flow
        if (Test-Path $root) {
            Get-ChildItem $root | ForEach-Object {
                $state = Get-ItemProperty $_.PSPath
                $props = Get-ItemProperty (Join-Path $_.PSPath 'Properties') -ErrorAction SilentlyContinue
                [pscustomobject]@{
                    Flow=$flow; EndpointId=$_.PSChildName; DeviceState=$state.DeviceState
                    FriendlyName=$props.'{a45c254e-df1c-4efd-8020-67d146a850e0},14'
                    Description=$props.'{a45c254e-df1c-4efd-8020-67d146a850e0},2'
                } | Format-List
            }
        }
    }
}
Collect 'default-endpoints' {
    Add-Type -Path (Join-Path $PSScriptRoot 'DefaultAudioEndpoints.cs')
    [DefaultAudioEndpoints]::Read()
}
Collect 'installed-audio-components' {
    foreach ($root in @('HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall',
                       'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall',
                       'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall')) {
        Get-ItemProperty ($root + '\*') -ErrorAction SilentlyContinue |
            Where-Object { $_.DisplayName -match 'FengYin|ASIO4ALL|VB-Audio|VB-CABLE|WebView2|Fresh Air' -or $_.PSChildName -like '*BA041761-FF1B-4F84-B055-AE70EC8C883B*' } |
            Select-Object DisplayName,DisplayVersion,Publisher,InstallLocation,InstallDate | Format-List
    }
}
Collect 'recent-audio-crashes' {
    Get-WinEvent -FilterHashtable @{LogName='Application';StartTime=(Get-Date).AddDays(-3);Level=1,2,3} -MaxEvents 2000 -ErrorAction Stop |
        Where-Object { $_.Message -match 'FengYin|ASIO4ALL|audiodg|VB-Audio|VB-CABLE' } |
        Select-Object -First 60 TimeCreated,Id,ProviderName,Message | Format-List
}
Collect 'recent-audio-system-events' {
    Get-WinEvent -FilterHashtable @{LogName='System';StartTime=(Get-Date).AddDays(-3);Level=1,2,3} -MaxEvents 2000 -ErrorAction Stop |
        Where-Object { $_.Message -match 'ASIO4ALL|VB-Audio|VB-CABLE|Realtek|Audiosrv|AudioEndpointBuilder' } |
        Select-Object -First 60 TimeCreated,Id,ProviderName,Message | Format-List
}
Collect 'reboot-state' {
    [pscustomobject]@{
        ComponentServicingRebootPending=(Test-Path 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Component Based Servicing\RebootPending')
        WindowsUpdateRebootPending=(Test-Path 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\WindowsUpdate\Auto Update\RebootRequired')
        PendingFileRenameOperations=[bool](Get-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager' -ErrorAction SilentlyContinue).PendingFileRenameOperations
    } | Format-List
}
Collect 'fengyin-binaries' {
    $locations = @()
    foreach ($root in @('HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall',
                       'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall')) {
        $locations += Get-ItemProperty ($root + '\*') -ErrorAction SilentlyContinue |
            Where-Object { $_.PSChildName -like '*BA041761-FF1B-4F84-B055-AE70EC8C883B*' } |
            Select-Object -ExpandProperty InstallLocation -ErrorAction SilentlyContinue
    }
    foreach ($location in ($locations | Where-Object { $_ } | Select-Object -Unique)) {
        foreach ($name in @('FengYin.exe','FengYinAudioEngine.exe','FengYinAudioWatchdog.exe')) {
            $file = Join-Path $location $name
            if (Test-Path -LiteralPath $file) {
                $info = Get-Item -LiteralPath $file
                [pscustomobject]@{Path=$file;Version=$info.VersionInfo.FileVersion;Bytes=$info.Length;
                    Modified=$info.LastWriteTime;SHA256=(Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash} | Format-List
            } else { "Missing: $file" }
        }
    }
    if (!$locations) { 'No registered FengYin installation found.' }
}
Collect 'microphone-access-policy' {
    foreach ($root in @('HKCU:\Software\Microsoft\Windows\CurrentVersion\CapabilityAccessManager\ConsentStore\microphone',
                       'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\CapabilityAccessManager\ConsentStore\microphone')) {
        if (Test-Path $root) { Get-ItemProperty $root | Select-Object PSPath,Value | Format-List }
    }
}
$roots = @((Join-Path $env:APPDATA 'FengYin'),(Join-Path $env:LOCALAPPDATA 'FengYin')) | Select-Object -Unique
$index = 0
foreach ($root in $roots) {
    $index++
    Collect ("log-inventory-$index") {
        "Directory: $root"
        foreach ($name in @('audio-engine.log','plugin-host.log','FengYin.settings')) {
            $file = Join-Path $root $name
            if (Test-Path -LiteralPath $file) { Get-Item -LiteralPath $file | Select-Object Name,Length,LastWriteTime | Format-List }
            else { "Missing: $name" }
        }
    }
    foreach ($name in @('audio-engine.log','plugin-host.log')) {
        $file = Join-Path $root $name
        if (Test-Path -LiteralPath $file) {
            Collect ("log-$index-" + $name) { Get-Content -LiteralPath $file -Tail 5000 }
        }
    }
    # Only audio preferences, never license keys or the full settings file.
    $settings = Join-Path $root 'FengYin.settings'
    if (Test-Path -LiteralPath $settings) {
        Collect ("audio-preferences-$index") {
            $readerOptions = New-Object System.Xml.XmlReaderSettings
            $readerOptions.DtdProcessing = [System.Xml.DtdProcessing]::Prohibit
            $readerOptions.XmlResolver = $null
            $reader = [System.Xml.XmlReader]::Create($settings,$readerOptions)
            try {
                $xml = New-Object System.Xml.XmlDocument
                $xml.XmlResolver = $null
                $xml.Load($reader)
                $allowed = @('audioEngineEnabled','audioEngineBuffer','audioModeExplicitChoice','audioSetupMode','audioSetupRevision','productionAudioRouteRevision','physicalEndpoint','audioDevice','automaticAudioDevice','audioTuningRevision','audioTunedHardware')
                $xml.SelectNodes('//VALUE') | Where-Object { $allowed -contains $_.GetAttribute('name') } |
                    ForEach-Object { $_.GetAttribute('name') + '=' + $_.GetAttribute('val') }
            } finally { $reader.Dispose() }
        }
    }
}
@'
Read-only audio snapshot. No drivers, settings, services or processes were changed.
No network requests were made. Existing logs may contain local paths and device IDs.
Default endpoints are queried read-only. No live startup, playback or physical sound test was performed.
Missing/denied collectors are saved as error files; other collectors continue.
'@ | Set-Content -LiteralPath (Join-Path $out 'ABOUT.txt') -Encoding UTF8
$zip = $out + '.zip'
Compress-Archive -LiteralPath $out -DestinationPath $zip
Write-Host "DONE. Send this ZIP file:" -ForegroundColor Green
Write-Host $zip
