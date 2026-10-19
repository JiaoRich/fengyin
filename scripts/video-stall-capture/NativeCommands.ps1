function Invoke-CapturedNative([string]$FilePath,[string]$Arguments,[string]$Directory,[string]$Label,[int]$Timeout=30) {
    # Own the process handle directly. Start-Process -RedirectStandard* may
    # yield a null ExitCode even after WaitForExit on Windows PowerShell 5.1.
    $process=New-Object System.Diagnostics.Process
    $process.StartInfo.FileName=$FilePath
    $process.StartInfo.Arguments=$Arguments
    $process.StartInfo.UseShellExecute=$false
    $process.StartInfo.CreateNoWindow=$true
    $process.StartInfo.RedirectStandardOutput=$true
    $process.StartInfo.RedirectStandardError=$true
    $outputTask=$null;$errorTask=$null
    try {
        if(-not $process.Start()){throw ('Cannot start '+$Label)}
        $outputTask=$process.StandardOutput.ReadToEndAsync()
        $errorTask=$process.StandardError.ReadToEndAsync()
        if(-not $process.WaitForExit($Timeout*1000)) {
            $process.Kill();$process.WaitForExit()
            throw ('Native command timed out: '+$Label)
        }
        $process.WaitForExit()
        $code=$process.ExitCode
        if($null -eq $code){throw ('Missing native exit code: '+$Label)}
        return [pscustomobject]@{ExitCode=[int]$code;Output=$outputTask.GetAwaiter().GetResult();Error=$errorTask.GetAwaiter().GetResult()}
    } finally {
        if($null -ne $outputTask){$outputTask.GetAwaiter().GetResult() | Set-Content -LiteralPath (Join-Path $Directory ($Label+'.txt')) -Encoding UTF8}
        if($null -ne $errorTask){$errorTask.GetAwaiter().GetResult() | Set-Content -LiteralPath (Join-Path $Directory ($Label+'-stderr.txt')) -Encoding UTF8}
        $process.Dispose()
    }
}

function Get-OwnedStallSessionNames([string]$Text) {
    # Only the exact private prefix emitted by this collector, never arbitrary WPR sessions.
    @([regex]::Matches($Text,'WPR_initiated_(FengYinVideo-[0-9a-fA-F]{32})_WPR (?:System|Event) Collector') |
        ForEach-Object {$_.Groups[1].Value} | Sort-Object -Unique)
}
