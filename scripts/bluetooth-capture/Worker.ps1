param([Parameter(Mandatory=$true)][string]$SpecPath)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
$spec=Get-Content -LiteralPath $SpecPath -Raw -Encoding UTF8 | ConvertFrom-Json
$out=$spec.Output;$label=$spec.Label
function Capture([string]$Part,[scriptblock]$Read) {
    $clock=[Diagnostics.Stopwatch]::StartNew()
    try {$items=@(& $Read);Save-Json ([ordered]@{At=(Get-Date).ToString('o');Status='read';Items=$items;Milliseconds=$clock.ElapsedMilliseconds}) (Join-Path $out ($label+'-'+$Part+'.json'))}
    catch {Save-Json ([ordered]@{At=(Get-Date).ToString('o');Status='unavailable';Error=$_.Exception.Message}) (Join-Path $out ($label+'-'+$Part+'.json'));Record-Error $out ($label+'-'+$Part) $_.Exception.Message}
}
function Get-CachedYdsEndpoints {
    Add-Type -AssemblyName System.Runtime.WindowsRuntime
    $null=[Windows.Devices.Enumeration.DeviceInformation,Windows.Devices.Enumeration,ContentType=WindowsRuntime]
    $null=[Windows.Devices.Enumeration.DeviceInformationKind,Windows.Devices.Enumeration,ContentType=WindowsRuntime]
    $resultType=[Windows.Devices.Enumeration.DeviceInformationCollection,Windows.Devices.Enumeration,ContentType=WindowsRuntime]
    $properties=[string[]]@('System.Devices.Aep.IsPaired','System.Devices.Aep.IsConnected','System.Devices.Aep.IsPresent','System.Devices.Aep.DeviceAddress','System.Devices.Aep.ProtocolId','System.Devices.Aep.SignalStrength','System.Devices.Aep.Bluetooth.Le.IsConnectable','System.Devices.ContainerId')
    # Device enumeration only. No BluetoothLEDevice, GATT request, watcher, connection or unpair call.
    $query='System.ItemNameDisplay:~<"YDS"'
    $op=[Windows.Devices.Enumeration.DeviceInformation]::FindAllAsync($query,$properties,[Windows.Devices.Enumeration.DeviceInformationKind]::AssociationEndpoint)
    $method=[System.WindowsRuntimeSystemExtensions].GetMethods() | Where-Object {$_.Name -eq 'AsTask' -and $_.IsGenericMethodDefinition -and $_.GetGenericArguments().Count -eq 1 -and $_.GetParameters().Count -eq 1 -and $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncOperation`1'} | Select-Object -First 1
    $task=$method.MakeGenericMethod(@($resultType)).Invoke($null,@($op))
    if(-not $task.Wait(5000)){throw 'Cached endpoint enumeration timed out; no connection was attempted.'}
    foreach($d in $task.Result){
        $props=[ordered]@{};foreach($key in $properties){if($d.Properties.ContainsKey($key)){$props[$key]=$d.Properties[$key]}}
        [pscustomobject]@{Id=$d.Id;Name=$d.Name;IsEnabled=$d.IsEnabled;IsPaired=$d.Pairing.IsPaired;CanPair=$d.Pairing.CanPair;Properties=$props}
    }
}
switch($spec.Mode) {
 'Snapshot' {
    Capture 'services' {Get-Service -Name 'bthserv','BTAGService','BluetoothUserService*','DeviceAssociationService' -ErrorAction SilentlyContinue | Select-Object Name,Status,StartType}
    Capture 'midi' {Add-Type -Path (Join-Path $PSScriptRoot 'MidiInventory.cs');[BleCaptureMidi]::Read()}
    Capture 'pnp' {
        $devices=@(Get-PnpDevice -ErrorAction Stop | Where-Object {$_.Class -eq 'Bluetooth' -or $_.FriendlyName -match 'YDS[- _]?150' -or $_.InstanceId -match '03b80e5a-ede8-4b33-a751-6ce34ec4c700'} | Select-Object -First 180)
        foreach($d in $devices){
            $properties=[ordered]@{}
            foreach($p in @(Get-PnpDeviceProperty -InstanceId $d.InstanceId -ErrorAction SilentlyContinue | Where-Object {$_.KeyName -match '^DEVPKEY_Device_(IsPresent|ProblemCode|ProblemStatus|DevNodeStatus|ContainerId|Parent|Service|DriverVersion|DriverDate|DriverInfPath|Manufacturer|BusReportedDeviceDesc|PowerData)$'})){
                $properties[$p.KeyName]=$p.Data
            }
            [pscustomobject]@{Name=$d.FriendlyName;Class=$d.Class;InstanceId=$d.InstanceId;Status=$d.Status;Problem=$d.Problem;Properties=$properties}
        }
    }
    Capture 'paired-endpoints' {Get-CachedYdsEndpoints}
    Capture 'history' {
        foreach($root in $spec.LogRoots){$file=Join-Path $root 'FengYinBluetoothMidi.settings';if(Test-Path -LiteralPath $file){[pscustomobject]@{File=$file;Items=@(Read-BluetoothHistory $file)}}}
    }
 }
 'Context' {
    Capture 'system' {
        $os=Get-CimInstance Win32_OperatingSystem
        $cs=Get-CimInstance Win32_ComputerSystem
        [pscustomobject]@{Caption=$os.Caption;Version=$os.Version;Build=$os.BuildNumber;LastBoot=$os.LastBootUpTime;Manufacturer=$cs.Manufacturer;Model=$cs.Model;
            TimeZone=(Get-TimeZone).Id;LocalTime=(Get-Date).ToString('o');UtcTime=[DateTime]::UtcNow.ToString('o');PowerShell=$PSVersionTable.PSVersion.ToString();Is64Bit=[Environment]::Is64BitProcess}
    }
    Capture 'drivers' {Get-CimInstance Win32_PnPSignedDriver | Where-Object {$_.DeviceClass -eq 'Bluetooth' -or $_.DeviceName -match 'YDS[- _]?150'} | Select-Object DeviceName,DeviceID,DriverVersion,DriverDate,DriverProviderName,InfName,IsSigned,Signer,Status}
    Capture 'related-processes' {Get-Process -ErrorAction SilentlyContinue | Where-Object {$_.ProcessName -match '^(FengYin.*|YDS.*|MIDIBerry.*|Bluetooth.*|reaper|Kontakt.*|Cubase.*|Live.*|Cakewalk.*)$'} | Select-Object Id,ProcessName,StartTime,Path}
    Capture 'power-plan' {& "$env:SystemRoot\System32\powercfg.exe" /getactivescheme}
    Capture 'power-flags' {
        foreach($d in @(Get-PnpDevice -Class Bluetooth -ErrorAction Stop)){
            $key='Registry::HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Enum\'+$d.InstanceId+'\Device Parameters'
            if(Test-Path -LiteralPath $key){
                $values=[ordered]@{};foreach($n in @('EnhancedPowerManagementEnabled','AllowIdleIrpInD3','SelectiveSuspendEnabled','DeviceSelectiveSuspended')){
                    $v=Get-ItemProperty -LiteralPath $key -Name $n -ErrorAction SilentlyContinue
                    if($null -ne $v){$values[$n]=$v.$n}
                };[pscustomobject]@{InstanceId=$d.InstanceId;Values=$values}
            }
        }
    }
 }
 'Events' {
    $start=[DateTimeOffset]::Parse($spec.Started).LocalDateTime.AddHours(-12)
    $end=Get-Date
    $channels=@('System','Application')
    Capture 'event-channels' {
        $all=@(foreach($pattern in @('*Bluetooth*','*BTH*','*DeviceAssociation*','*DeviceSetupManager*','*UserPnp*','*Kernel-PnP*')){
            Get-WinEvent -ListLog $pattern -ErrorAction SilentlyContinue
        }) | Sort-Object LogName -Unique
        foreach($c in $all){
            if($c.IsEnabled -and $c.LogType -notin @('Analytical','Debug') -and $script:channels.Count -lt 28){$script:channels+=$c.LogName}
            [pscustomobject]@{Name=$c.LogName;Enabled=$c.IsEnabled;Type=[string]$c.LogType;RecordCount=$c.RecordCount;MaximumBytes=$c.MaximumSizeInBytes;
                Note=if(-not $c.IsEnabled){'Disabled before capture; not modified, no evidence available from this channel'}else{'Read only'}}
        }
    }
    foreach($channel in $channels){
        $fileName=($channel -replace '[^a-zA-Z0-9._-]','_')
        Capture ('events-'+$fileName) {
            $filter=@{LogName=$channel;StartTime=$start;EndTime=$end}
            if($channel -eq 'System'){$filter.ProviderName=@('*BTH*','*Bluetooth*','*DeviceAssociation*','Microsoft-Windows-Kernel-PnP','Microsoft-Windows-UserPnp','Microsoft-Windows-Kernel-Power','Microsoft-Windows-Power-Troubleshooter')}
            if($channel -eq 'Application'){$filter.Id=@(1000,1001,1002)}
            $errors=@();$rows=@(Get-WinEvent -FilterHashtable $filter -MaxEvents 1500 -ErrorAction SilentlyContinue -ErrorVariable errors)
            foreach($err in $errors){if($err.FullyQualifiedErrorId -notmatch 'NoMatchingEventsFound|NoMatchingProvidersFound'){throw $err}}
            [pscustomobject]@{QueryStart=$start.ToString('o');QueryEnd=$end.ToString('o');Limit=1500;LimitReached=($rows.Count -ge 1500);Channel=$channel}
            foreach($event in $rows){
                $xml=$event.ToXml();$message=$event.Message
                if($channel -eq 'Application' -and ($xml+' '+$message) -notmatch '(?i)FengYin|Bluetooth|BTH|YDS'){continue}
                [pscustomobject]@{Time=$event.TimeCreated.ToString('o');Id=$event.Id;Provider=$event.ProviderName;Level=$event.LevelDisplayName;RecordId=$event.RecordId;
                    Message=$message;Xml=$xml}
            }
        }
    }
 }
 default {throw 'Unknown worker mode'}
}
Save-Json ([ordered]@{Finished=(Get-Date).ToString('o');Mode=$spec.Mode;Label=$label}) (Join-Path $out ($label+'-complete.json'))
