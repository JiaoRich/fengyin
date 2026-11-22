function Write-JsonFile($Value,[string]$Path) {
    ConvertTo-Json -InputObject $Value -Depth 12 | Set-Content -LiteralPath $Path -Encoding UTF8
}
function Add-CaptureError([string]$Folder,[string]$Stage,[string]$Message) {
    [pscustomobject]@{time=(Get-Date).ToString('o');stage=$Stage;message=$Message} |
        ConvertTo-Json -Compress | Add-Content -LiteralPath (Join-Path $Folder 'errors.jsonl') -Encoding UTF8
}
function Invoke-CaptureWorker([string]$Script,[string]$Spec,[string]$Folder,[string]$Label,[int]$Seconds=45,[switch]$DumpWorker) {
    $ps=Join-Path $env:SystemRoot 'System32/WindowsPowerShell/v1.0/powershell.exe'
    # EncodedCommand uses literal PowerShell strings; cmd.exe never parses paths.
    $command="& '"+$Script.Replace("'","''")+"' -SpecPath '"+$Spec.Replace("'","''")+"'"
    $encoded=[Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($command))
    $child=New-Object Diagnostics.Process
    $child.StartInfo.FileName=$ps
    $child.StartInfo.Arguments='-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -EncodedCommand '+$encoded
    $child.StartInfo.UseShellExecute=$false;$child.StartInfo.CreateNoWindow=$true
    $child.StartInfo.RedirectStandardOutput=$true;$child.StartInfo.RedirectStandardError=$true
    try {
        if(-not $child.Start()){throw 'Cannot start diagnostic worker'}
        $stdout=$child.StandardOutput.ReadToEndAsync();$stderr=$child.StandardError.ReadToEndAsync()
        if(-not $child.WaitForExit($Seconds*1000)) {
            if($DumpWorker) {
                # Never forcibly terminate DbgHelp while it may hold target thread state.
                Add-CaptureError $Folder $Label 'Dump worker exceeded outer deadline; not killed. Incomplete dump excluded.'
                return 124
            }
            $child.Kill();$null=$child.WaitForExit(5000)
            Add-CaptureError $Folder $Label 'Read-only context worker timed out; only this owned worker stopped.'
            return 124
        }
        $child.WaitForExit()
        $stdout.GetAwaiter().GetResult() | Set-Content -LiteralPath (Join-Path $Folder ($Label+'-stdout.txt')) -Encoding UTF8
        $stderr.GetAwaiter().GetResult() | Set-Content -LiteralPath (Join-Path $Folder ($Label+'-stderr.txt')) -Encoding UTF8
        return $child.ExitCode
    } finally {$child.Dispose()}
}
function Read-LogTail([string]$File,[int]$MaximumBytes=1048576) {
    $s=New-Object IO.FileStream($File,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite)
    try {
        $count=[int][Math]::Min($s.Length,$MaximumBytes);$offset=$s.Length-$count
        $null=$s.Seek($offset,[IO.SeekOrigin]::Begin);$buffer=New-Object byte[] $count
        $used=0;while($used -lt $count){$n=$s.Read($buffer,$used,$count-$used);if($n -le 0){break};$used+=$n}
        $text=[Text.Encoding]::UTF8.GetString($buffer,0,$used)
        if($offset -gt 0){$nl=$text.IndexOf("`n");if($nl -ge 0){$text=$text.Substring($nl+1)}}
        @($text -split "`r?`n" | Select-Object -Last 2500 | ForEach-Object {
            if($_ -match '(?i)license.?key|activation.?code|authorization\s*[:=]|bearer\s|access.?token|api.?key|secret|password|签名密钥|激活码') {'[Sensitive-looking log line omitted]'} else {$_}
        }) -join "`n"
    } finally {$s.Dispose()}
}
function Select-RelatedWebViews($Rows,$Roots) {
    $found=@{}
    foreach($root in $Roots){$found[[int]$root.Id]=[long]$root.StartFileTime}
    for($depth=0;$depth -lt 8;$depth++) {
        foreach($row in $Rows) {
            if($row.Name -ine 'msedgewebview2.exe'){continue}
            $id=[int]$row.ProcessId;$parent=[int]$row.ParentProcessId
            if($found.ContainsKey($parent) -and [long]$row.StartFileTime -ge $found[$parent]){$found[$id]=[long]$row.StartFileTime}
        }
    }
    @($Rows | Where-Object {$_.Name -ieq 'msedgewebview2.exe' -and $found.ContainsKey([int]$_.ProcessId)})
}
