param([ValidateRange(60,1800)][int]$Seconds = 600)
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$Exe = Join-Path $Root 'FengYin.exe'
if (-not (Test-Path -LiteralPath $Exe)) { throw 'Keep this test folder inside the complete portable runtime.' }
if (Get-Process -Name FengYin -ErrorAction SilentlyContinue) {
    throw 'Exit the existing FengYin normally first. This tool will not terminate it.'
}
$Out = Join-Path $Root ('Test-results-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0,6))
New-Item -ItemType Directory -Path $Out | Out-Null
function SaveLogs([string]$Label) {
    foreach ($Pair in @(@('roaming',$env:APPDATA),@('local',$env:LOCALAPPDATA))) {
        foreach ($Name in @('audio-engine.log','plugin-host.log','audio-timing-diagnostic.log','audio-source-diagnostic.log')) {
            $Path = Join-Path (Join-Path $Pair[1] 'FengYin') $Name
            if (Test-Path -LiteralPath $Path) {
                try { Get-Content -LiteralPath $Path -Tail 20000 | Set-Content -LiteralPath (Join-Path $Out ($Label+'-'+$Pair[0]+'-'+$Name)) -Encoding UTF8 }
                catch { $_.Exception.Message | Add-Content -LiteralPath (Join-Path $Out 'errors.txt') }
            }
        }
    }
}
$Names = @('FENGYIN_AUDIO_TIMING','FENGYIN_RECORDING_DIAGNOSTICS','FENGYIN_PERFORMANCE_NOTES')
$Saved = @{}
foreach ($Name in $Names) { $Saved[$Name] = [Environment]::GetEnvironmentVariable($Name,'Process') }
try {
    SaveLogs 'before'
    [pscustomobject]@{Started=(Get-Date).ToString('o');ExeSHA256=(Get-FileHash -LiteralPath $Exe -Algorithm SHA256).Hash;LogicalProcessors=[Environment]::ProcessorCount} |
        ConvertTo-Json | Set-Content -LiteralPath (Join-Path $Out 'session.json') -Encoding UTF8
    $env:FENGYIN_AUDIO_TIMING = '1'
    $env:FENGYIN_RECORDING_DIAGNOSTICS = '1'
    $env:FENGYIN_PERFORMANCE_NOTES = '0' # Never generate notes automatically.
    $App = Start-Process -FilePath $Exe -WorkingDirectory $Root -PassThru
    Write-Host 'Run the four checks in the included instructions. Exit FengYin normally to save evidence.' -ForegroundColor Green
    Write-Host 'No settings, drivers, power plans or recordings are changed by this collector.'
    $Watch = [Diagnostics.Stopwatch]::StartNew()
    while (-not $App.HasExited -and $Watch.Elapsed.TotalSeconds -lt $Seconds) {
        try {
            $App.Refresh()
            [pscustomobject]@{Time=(Get-Date).ToString('o');PID=$App.Id;CPUTimeMs=$App.TotalProcessorTime.TotalMilliseconds;WorkingSet=$App.WorkingSet64;Responding=$App.Responding} |
                Export-Csv -LiteralPath (Join-Path $Out 'process.csv') -NoTypeInformation -Append -Encoding UTF8
            foreach ($Thread in $App.Threads) {
                try {
                    [pscustomobject]@{Time=(Get-Date).ToString('o');PID=$App.Id;TID=$Thread.Id;CPUTimeMs=$Thread.TotalProcessorTime.TotalMilliseconds;State=[string]$Thread.ThreadState} |
                        Export-Csv -LiteralPath (Join-Path $Out 'threads.csv') -NoTypeInformation -Append -Encoding UTF8
                } catch { } # Threads may exit during enumeration.
            }
        } catch { $_.Exception.Message | Add-Content -LiteralPath (Join-Path $Out 'errors.txt') }
        Start-Sleep -Seconds 2
        $App.Refresh()
    }
} finally {
    foreach ($Name in $Names) { [Environment]::SetEnvironmentVariable($Name,$Saved[$Name],'Process') }
    SaveLogs 'after'
    'Diagnostic environment was process-local. No automatic notes, forced exit, installer or settings changes.' |
        Set-Content -LiteralPath (Join-Path $Out 'scope.txt') -Encoding UTF8
    $Archive = $Out + '.zip'
    Compress-Archive -LiteralPath $Out -DestinationPath $Archive
    Write-Host ('Evidence: ' + $Archive) -ForegroundColor Yellow
    Write-Host 'If FengYin is still running, exit normally before returning to the installed version.'
}
