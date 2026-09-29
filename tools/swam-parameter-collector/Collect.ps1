$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
$collectorRoot = $PSScriptRoot
$resultRoot = Join-Path ([Environment]::GetFolderPath('Desktop')) ('SWAM-Parameter-Collection-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $resultRoot | Out-Null
Write-Host 'SWAM 3.9.4 parameter collector / Windows x64'
Write-Host 'Close FengYin and other audio hosts first. No audio device or MIDI input will be opened.'
Write-Host 'This queries a separate plugin instance. It does not edit your presets or MIDI mappings.'
Write-Host 'Reports include plugin names, versions, parameter information and installation paths; send only to trusted support.'
Read-Host 'Press Enter to begin' | Out-Null
$exe = Join-Path $collectorRoot 'SwamParameterCollector.exe'
$requests = Join-Path $collectorRoot 'requests.json'
if (!(Test-Path $exe) -or !(Test-Path $requests)) { throw 'Extract the complete ZIP before running.' }
$statuses = @()
foreach ($kind in @('Alto', 'Tenor')) {
    $pluginName = 'SWAM ' + $kind + ' Sax 3'
    $pluginPath = Join-Path $env:CommonProgramFiles ('VST3\SWAM\Saxophones\' + $pluginName + '.vst3')
    if (!(Test-Path $pluginPath)) {
        $picker = New-Object System.Windows.Forms.OpenFileDialog
        $picker.Title = 'Select ' + $pluginName + ' VST3 module'
        $picker.Filter = 'VST3 plugin (*.vst3)|*.vst3|All files (*.*)|*.*'
        $picker.InitialDirectory = Join-Path $env:CommonProgramFiles 'VST3'
        if ($picker.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK) {
            $statuses += [pscustomobject]@{ instrument=$kind; status='plugin selection cancelled' }; continue
        }
        $pluginPath = $picker.FileName
    }
    Write-Host ('Querying ' + $pluginName + ' ...')
    $out = Join-Path $resultRoot ($kind + '.swamquery.json')
    $argsLine = '"' + $pluginPath + '" "' + $requests + '" "' + $out + '"'
    $proc = Start-Process -FilePath $exe -ArgumentList $argsLine -PassThru -NoNewWindow -RedirectStandardOutput (Join-Path $resultRoot ($kind + '-stdout.txt')) -RedirectStandardError (Join-Path $resultRoot ($kind + '-stderr.txt'))
    if (!$proc.WaitForExit(120000)) {
        # Only the collector process created above is stopped; never kill the host or other plugins.
        $proc.Kill(); $proc.WaitForExit()
        $statuses += [pscustomobject]@{ instrument=$kind; status='collector timed out; send logs' }
    } else {
        $proc.WaitForExit()
        $statuses += [pscustomobject]@{ instrument=$kind; exitCode=$proc.ExitCode; reportExists=(Test-Path $out) }
    }
}
$statuses | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 (Join-Path $resultRoot 'status.json')
$zip = $resultRoot + '.zip'
Compress-Archive -Path (Join-Path $resultRoot '*') -DestinationPath $zip
Write-Host ('Finished. Send this ZIP: ' + $zip)
Write-Host 'If a plugin failed, the ZIP includes logs. Do not reinstall drivers or change plugin settings.'
Start-Process explorer.exe -ArgumentList ('/select,"' + $zip + '"')
