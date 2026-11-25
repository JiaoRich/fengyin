function Save-Json($Value,[string]$Path) {
    $tmp=$Path+'.new'
    [IO.File]::WriteAllText($tmp,(ConvertTo-Json -InputObject $Value -Depth 16),[Text.UTF8Encoding]::new($true))
    if([IO.File]::Exists($Path)){[IO.File]::Delete($Path)}
    [IO.File]::Move($tmp,$Path)
}
function Add-Record([string]$Path,$Value) {
    $line=(ConvertTo-Json -InputObject $Value -Compress -Depth 12)+[Environment]::NewLine
    for($attempt=0;$attempt -lt 4;$attempt++){
        try {[IO.File]::AppendAllText($Path,$line,[Text.UTF8Encoding]::new($false));return}
        catch [IO.IOException] {if($attempt -eq 3){throw};Start-Sleep -Milliseconds 25}
    }
}
function Record-Error([string]$Folder,[string]$Stage,$ErrorValue) {
    Add-Record (Join-Path $Folder 'errors.jsonl') ([ordered]@{Time=(Get-Date).ToString('o');Stage=$Stage;Error=[string]$ErrorValue})
}
function Start-OwnedProcess([string]$Script,[string]$Spec,[string]$Folder,[string]$Label,[int]$Deadline) {
    $exe=Join-Path $env:SystemRoot 'System32/WindowsPowerShell/v1.0/powershell.exe'
    $command="& '"+$Script.Replace("'","''")+"' -SpecPath '"+$Spec.Replace("'","''")+"'"
    $command+='; if($null -ne $LASTEXITCODE){exit $LASTEXITCODE}'
    $p=New-Object Diagnostics.Process
    $p.StartInfo.FileName=$exe
    $p.StartInfo.Arguments='-NoLogo -NoProfile -NonInteractive -MTA -ExecutionPolicy Bypass -EncodedCommand '+[Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($command))
    $p.StartInfo.UseShellExecute=$false;$p.StartInfo.CreateNoWindow=$true
    $p.StartInfo.RedirectStandardOutput=$true;$p.StartInfo.RedirectStandardError=$true
    if(-not $p.Start()){throw 'Cannot start owned capture worker'}
    [pscustomobject]@{Process=$p;Clock=[Diagnostics.Stopwatch]::StartNew();Deadline=$Deadline;Folder=$Folder;Label=$Label;
        StdOut=$p.StandardOutput.ReadToEndAsync();StdErr=$p.StandardError.ReadToEndAsync();Finished=$false;ExitCode=$null}
}
function Poll-OwnedProcess($Job) {
    if($Job.Finished){return $true}
    if(-not $Job.Process.HasExited -and $Job.Clock.Elapsed.TotalSeconds -lt $Job.Deadline){return $false}
    if(-not $Job.Process.HasExited){
        $Job.Process.Kill();$null=$Job.Process.WaitForExit(3000);$Job.ExitCode=124
        Record-Error $Job.Folder $Job.Label 'Owned read-only worker timeout. Only this worker was stopped.'
    } else {$Job.ExitCode=$Job.Process.ExitCode}
    foreach($pair in @(@('stdout',$Job.StdOut),@('stderr',$Job.StdErr))) {
        if($pair[1].IsCompleted){
            $text=$pair[1].GetAwaiter().GetResult()
            if($text){[IO.File]::WriteAllText((Join-Path $Job.Folder ($Job.Label+'-'+$pair[0]+'.txt')),$text,[Text.UTF8Encoding]::new($true))}
        }
    }
    if($Job.ExitCode -ne 0){Record-Error $Job.Folder $Job.Label ('ExitCode='+$Job.ExitCode)}
    $Job.Process.Dispose();$Job.Finished=$true;return $true
}
function Get-FengYinProcesses {
    @(foreach($p in @(Get-Process -Name 'FengYin','FengYinDJ','FengYinAudioEngine' -ErrorAction SilentlyContinue)) {
        try {
            [pscustomobject]@{Id=$p.Id;Name=$p.ProcessName;Started=$p.StartTime.ToString('o');Path=$p.Path;Version=$p.MainModule.FileVersionInfo.FileVersion;
                Responding=$p.Responding;CpuSeconds=$p.TotalProcessorTime.TotalSeconds;WorkingSet=$p.WorkingSet64}
        } catch {[pscustomobject]@{Id=$p.Id;Name=$p.ProcessName;Error=$_.Exception.Message}}
    })
}
function Get-LogFiles([string[]]$Roots) {
    @(foreach($root in $Roots){if(Test-Path -LiteralPath $root){
        Get-ChildItem -LiteralPath $root -File -ErrorAction SilentlyContinue |
            Where-Object {$_.Name -match '^plugin-host(\.log.*|.*\.log)$|^(bluetooth|ble)[-_.].*\.log$'} |
            Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 6
    }})
}
function Read-BleIncrement([string]$File,[hashtable]$States,[string]$Destination,[double]$Elapsed) {
    # Shared read; never lock the host's file. Keep a 64-byte anchor to detect truncate/rewrite even at identical size.
    $stream=[IO.FileStream]::new($File,[IO.FileMode]::Open,[IO.FileAccess]::Read,([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
    try {
        $length=$stream.Length;$created=[IO.File]::GetCreationTimeUtc($File).Ticks
        $head=New-Object byte[] ([int][Math]::Min(64,$length));$null=$stream.Read($head,0,$head.Length)
        $old=$States[$File];$reset=($null -eq $old -or $old.Created -ne $created -or $length -lt $old.Offset)
        if(-not $reset -and $old.Head.Length){
            $prefix=[Convert]::ToBase64String($head,0,[Math]::Min($head.Length,$old.Head.Length))
            if($prefix -ne [Convert]::ToBase64String($old.Head)){$reset=$true}
        }
        if(-not $reset -and $old.Anchor.Length){
            $null=$stream.Seek($old.Offset-$old.Anchor.Length,[IO.SeekOrigin]::Begin)
            $anchor=New-Object byte[] $old.Anchor.Length;$n=$stream.Read($anchor,0,$anchor.Length)
            if($n -ne $anchor.Length -or [Convert]::ToBase64String($anchor) -ne [Convert]::ToBase64String($old.Anchor)){$reset=$true}
        }
        $offset=if($reset){[Math]::Max(0,$length-524288)}else{$old.Offset}
        if($length-$offset -gt 524288){$offset=$length-524288;$reset=$true}
        $null=$stream.Seek($offset,[IO.SeekOrigin]::Begin)
        $buffer=New-Object byte[] ([int]($length-$offset));$used=0
        while($used -lt $buffer.Length){$n=$stream.Read($buffer,$used,$buffer.Length-$used);if($n -eq 0){break};$used+=$n}
        $text=[Text.Encoding]::UTF8.GetString($buffer,0,$used)
        if($reset -and $offset -gt 0){$nl=$text.IndexOf("`n");$text=if($nl -ge 0){$text.Substring($nl+1)}else{''}}
        if(-not $reset -and $old.Pending){$text=$old.Pending+$text}
        if($reset){Add-Record $Destination ([ordered]@{ObservedAt=(Get-Date).ToString('o');Elapsed=$Elapsed;File=$File;Kind='initial-or-file-reset';Historical=$true;SkippedBytes=$offset})}
        $last=$text.LastIndexOf("`n");$pending=$text
        if($last -ge 0){
            $pending=$text.Substring($last+1)
            foreach($line in ($text.Substring(0,$last) -split "`r?`n")){
                if($line -notmatch '(?i)Bluetooth MIDI:|BLE discovery|BLE_DIAG|Bluetooth.*(connect|disconnect|pair|service)|MIDI.*(device|input|connect|disconnect)'){continue}
                $safe=if($line -match '(?i)password|secret|api.?key|license.?key|activation|access.?token|bearer\s|激活码|配对密钥'){'[sensitive-looking line omitted]'}else{$line.Substring(0,[Math]::Min(6000,$line.Length))}
                Add-Record $Destination ([ordered]@{ObservedAt=(Get-Date).ToString('o');Elapsed=$Elapsed;File=$File;Historical=[bool]$reset;Line=$safe})
            }
        }
        if($pending.Length -gt 16000){$pending=''}
        $end=$offset+$used;$anchor=New-Object byte[] ([int][Math]::Min(64,$end));$null=$stream.Seek($end-$anchor.Length,[IO.SeekOrigin]::Begin)
        $null=$stream.Read($anchor,0,$anchor.Length)
        $States[$File]=@{Offset=$end;Created=$created;Anchor=$anchor;Head=$head;Pending=$pending}
    } finally {$stream.Dispose()}
}
function Read-BluetoothHistory([string]$File) {
    $s=[Xml.XmlReaderSettings]::new();$s.DtdProcessing=[Xml.DtdProcessing]::Prohibit;$s.XmlResolver=$null;$s.MaxCharactersInDocument=2097152
    $reader=[Xml.XmlReader]::Create($File,$s)
    try {
        $doc=[Xml.XmlDocument]::new();$doc.XmlResolver=$null;$doc.Load($reader)
        foreach($v in $doc.SelectNodes('/PROPERTIES/VALUE[@name="history"]')) {
            foreach($row in @($v.GetAttribute('val') | ConvertFrom-Json)) {
                [pscustomobject]@{Id=[string]$row.id;Name=[string]$row.name}
            }
        }
    } finally {$reader.Dispose()}
}
function Write-EvidenceZip([string]$Folder) {
    Add-Type -AssemblyName System.IO.Compression
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip=$Folder+'.zip';$archive=[IO.Compression.ZipFile]::Open($zip,[IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach($f in Get-ChildItem -LiteralPath $Folder -File | Where-Object {Test-EvidenceName $_.Name}){
            $null=[IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive,$f.FullName,$f.Name,[IO.Compression.CompressionLevel]::Fastest)
        }
    } finally {$archive.Dispose()}
    return $zip
}
function Test-EvidenceName([string]$Name) {
    return $Name -notmatch '(^command-|^session-input\.json$|^finish\.flag$|^status\.json$|-request\.json$|\.new$)'
}
