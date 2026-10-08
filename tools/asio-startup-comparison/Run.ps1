$ErrorActionPreference='Stop'
try {
    if(Get-Process -Name 'FengYin','FengYinAudioEngine','FengYinAudioWatchdog' -ErrorAction SilentlyContinue) { throw 'Close FengYin first, then run again. No processes will be stopped for you.' }
    $out=Join-Path $PSScriptRoot ('Results-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
    New-Item -ItemType Directory -Path $out | Out-Null
    Write-Host 'Four isolated silent startup tests. No installer, drivers, Windows default device or FengYin settings will be changed.'
    Write-Host 'The configure cases apply the requested Realtek-only ASIO output policy, like build 152.'
    $results=@()
    foreach($mode in @('preserve','read','configure','configure-reset')) {
        Write-Host "Testing $mode ..."
        $p=Start-Process -FilePath (Join-Path $PSScriptRoot 'FengYinAsioComparison.exe') -ArgumentList "$mode $mode.log" -WorkingDirectory $out -PassThru
        $null=$p.Handle
        if(!$p.WaitForExit(20000)) { $p.Kill(); $p.WaitForExit(); $status='TIMEOUT (test process only terminated)' }
        else { $status=[string]$p.ExitCode }
        $results += "$mode : $status"
        $results | Set-Content (Join-Path $out 'results.txt')
        Start-Sleep -Seconds 2
    }
    Compress-Archive -LiteralPath $out -DestinationPath ($out+'.zip')
    Write-Host ('Send: '+$out+'.zip') -ForegroundColor Green
} catch {Write-Host $_.Exception.Message -ForegroundColor Red}
Read-Host 'Press Enter to close' | Out-Null
