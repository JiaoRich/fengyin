$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
$collectorRoot = $PSScriptRoot
$exe = Join-Path $collectorRoot 'SwamParameterCollector.exe'
$requests = Join-Path $collectorRoot 'requests.json'
if (!(Test-Path $exe) -or !(Test-Path $requests)) { throw 'Extract the complete ZIP before running.' }
$resultRoot = Join-Path ([Environment]::GetFolderPath('Desktop')) ('SWAM-Full-Collection-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $resultRoot | Out-Null
Write-Host 'SWAM full-suite parameter collector 2.0 / Windows x64'
Write-Host 'Close FengYin and other audio hosts. This does NOT edit presets, MIDI settings or drivers.'
Write-Host 'Only installed SWAM VST3 instruments are queried, each in an isolated process.'
Write-Host 'Actual plugin versions and local paths are recorded; send results to trusted support only.'
$roots = @((Join-Path $env:CommonProgramFiles 'VST3'))
if ($env:LOCALAPPDATA) { $roots += (Join-Path $env:LOCALAPPDATA 'Programs\Common\VST3') }
while ((Read-Host 'Add a custom VST3 folder? Type Y, or press Enter to scan standard folders') -match '^[yY]$') {
    $picker = New-Object System.Windows.Forms.FolderBrowserDialog
    $picker.Description = 'Select the folder containing SWAM VST3 plugins (not sample libraries)'
    if ($picker.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) { $roots += $picker.SelectedPath }
    $picker.Dispose()
}
$paths = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
$scanWarnings = New-Object 'System.Collections.Generic.List[string]'
function Find-SwamPlugins([string]$folder) {
    if (!(Test-Path -LiteralPath $folder)) { return }
    try { $items = @(Get-ChildItem -LiteralPath $folder -Force -ErrorAction Stop) }
    catch { $scanWarnings.Add('Cannot read folder: ' + $folder); return }
    foreach ($item in $items) {
        if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { continue }
        if ($item.Extension -ieq '.vst3') {
            if ($item.FullName -match '(?i)swam') { [void]$paths.Add($item.FullName) }
            # Do not collect a bundle's inner binary a second time.
        } elseif ($item.PSIsContainer) { Find-SwamPlugins $item.FullName }
    }
}
foreach ($folder in ($roots | Select-Object -Unique)) { Find-SwamPlugins $folder }
$candidates = @($paths | Sort-Object)
$statuses = New-Object 'System.Collections.Generic.List[object]'
$index = 0
foreach ($pluginPath in $candidates) {
    $index++
    $label = '{0:D3}-{1}' -f $index, ([IO.Path]::GetFileNameWithoutExtension($pluginPath) -replace '[^a-zA-Z0-9._-]', '_')
    Write-Host ('[{0}/{1}] {2}' -f $index,$candidates.Count,$pluginPath)
    $out = Join-Path $resultRoot ($label + '.swamquery.json')
    $info = New-Object System.Diagnostics.ProcessStartInfo
    $info.FileName = $exe
    $info.Arguments = '"' + $pluginPath + '" "' + $requests + '" "' + $out + '"'
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $proc = New-Object System.Diagnostics.Process
    $proc.StartInfo = $info
    $status = [ordered]@{ path=$pluginPath; reportExists=$false; status='not started' }
    try {
        [void]$proc.Start()
        $stdout = $proc.StandardOutput.ReadToEndAsync()
        $stderr = $proc.StandardError.ReadToEndAsync()
        if (!$proc.WaitForExit(180000)) {
            $proc.Kill(); $proc.WaitForExit()
            $status.status = 'timeout; only this collector process was stopped'
        } else {
            $proc.WaitForExit()
            $status.exitCode = $proc.ExitCode
            $status.status = if ($proc.ExitCode -eq 0) { 'finished' } else { 'collector reported a failure; inspect log' }
        }
        [IO.File]::WriteAllText((Join-Path $resultRoot ($label + '-stdout.txt')), $stdout.GetAwaiter().GetResult())
        [IO.File]::WriteAllText((Join-Path $resultRoot ($label + '-stderr.txt')), $stderr.GetAwaiter().GetResult())
        $status.reportExists = Test-Path -LiteralPath $out
        if ($status.reportExists) {
            $report = Get-Content -LiteralPath $out -Raw -Encoding UTF8 | ConvertFrom-Json
            $status.pluginName = $report.pluginName
            $status.pluginVersion = $report.pluginVersion
            $status.parameterCount = $report.collectedParameterCount
            $status.parametersUnchanged = $report.parameterValuesUnchanged
            $status.missingOptionalTargets = @($report.missingParameters)
        }
    } catch {
        $status.status = 'error: ' + $_.Exception.Message
        try { if (!$proc.HasExited) { $proc.Kill(); $proc.WaitForExit() } } catch {}
    } finally { $proc.Dispose() }
    $statuses.Add([pscustomobject]$status)
    [pscustomobject]@{ collectorVersion='2.0.0'; candidateCount=$candidates.Count; completed=$index; scanRoots=$roots; scanWarnings=@($scanWarnings.ToArray()); plugins=@($statuses.ToArray()) } |
        ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 (Join-Path $resultRoot 'status.json')
}
if ($candidates.Count -eq 0) {
    [pscustomobject]@{ collectorVersion='2.0.0'; candidateCount=0; scanRoots=$roots; scanWarnings=@($scanWarnings.ToArray()); message='No SWAM VST3 found. Run again and select your custom VST3 installation folder.' } |
        ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 (Join-Path $resultRoot 'status.json')
}
$zip = $resultRoot + '.zip'
Compress-Archive -Path (Join-Path $resultRoot '*') -DestinationPath $zip
Write-Host ('Finished. Send the whole ZIP: ' + $zip)
Write-Host 'Partial results and errors are included. A successful build is not a playback verification.'
Start-Process explorer.exe -ArgumentList ('/select,"' + $zip + '"')
