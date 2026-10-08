param([switch]$RestoreOnly, [string]$OriginalSid)
$ErrorActionPreference = 'Stop'
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
if (!$OriginalSid) { $OriginalSid = $identity.User.Value }
if ($identity.User.Value -ne $OriginalSid) { throw 'Run with the same Windows account, not another administrator account.' }
$admin = (New-Object Security.Principal.WindowsPrincipal($identity)).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (!$admin) {
    $script = $PSCommandPath.Replace("'", "''")
    $mode = if ($RestoreOnly) { ' -RestoreOnly' } else { '' }
    $command = "& '$script' -OriginalSid '$OriginalSid'$mode"
    $encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($command))
    Start-Process powershell.exe -Verb RunAs -Wait -ArgumentList "-NoProfile -ExecutionPolicy Bypass -EncodedCommand $encoded"
    exit
}
$mutex = New-Object Threading.Mutex($false, 'Global\FengYinCrashCapture')
$locked = $false
$base = $null
$key = $null
$backup = Join-Path $PSScriptRoot 'wer-backup.clixml'
$subkey = 'SOFTWARE\Microsoft\Windows\Windows Error Reporting\LocalDumps\FengYinAudioEngine.exe'
$names = @('DumpFolder', 'DumpType', 'DumpCount')
function Restore-Wer {
    if (!(Test-Path -LiteralPath $backup)) { return }
    $saved = Import-Clixml -LiteralPath $backup
    $target = $base.CreateSubKey($subkey)
    try {
        foreach ($name in $names) {
            $value = $saved.Values | Where-Object { $_.Name -eq $name }
            if (@($value).Count -ne 1) { throw 'Invalid backup; do not delete it. Contact support.' }
            if ($value.Exists) { $target.SetValue($name, $value.Value, [Microsoft.Win32.RegistryValueKind]$value.Kind) }
            else { $target.DeleteValue($name, $false) }
        }
        $empty = ($target.ValueCount -eq 0 -and $target.SubKeyCount -eq 0)
    } finally { $target.Dispose() }
    if (!$saved.KeyExisted -and $empty) { $base.DeleteSubKey($subkey, $false) }
    Remove-Item -LiteralPath $backup
    Write-Host 'Temporary crash-capture settings restored.' -ForegroundColor Green
}
try {
    try { $locked = $mutex.WaitOne(0) } catch [Threading.AbandonedMutexException] { $locked = $true }
    if (!$locked) { throw 'A capture is already running. Finish or close that window first.' }
    $base = [Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::LocalMachine, [Microsoft.Win32.RegistryView]::Registry64)
    if ($RestoreOnly) { Restore-Wer; return }
    if (Test-Path -LiteralPath $backup) { Restore-Wer }
    Write-Host 'This captures FengYinAudioEngine crashes only. No drivers are changed.'
    Write-Host 'A full dump contains process memory and may include private data. Nothing is uploaded.'
    $answer = Read-Host 'Press Enter to start, or type Q to cancel'
    if ($answer -eq 'Q') { return }
    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $out = Join-Path $PSScriptRoot ("FengYin-Crash-" + $stamp + '-' + [Guid]::NewGuid().ToString('N').Substring(0,6))
    $dump = Join-Path $out 'dumps'
    New-Item -ItemType Directory -Path $dump -Force | Out-Null
    # Explicitly grant the launching user access even after UAC elevation.
    $acl = Get-Acl -LiteralPath $dump
    $rule = New-Object Security.AccessControl.FileSystemAccessRule($identity.User, 'Modify', 'ContainerInherit,ObjectInherit', 'None', 'Allow')
    $acl.AddAccessRule($rule)
    Set-Acl -LiteralPath $dump -AclObject $acl
    $old = $base.OpenSubKey($subkey)
    $values = @()
    foreach ($name in $names) {
        $exists = $null -ne $old -and $old.GetValueNames() -contains $name
        $values += [pscustomobject]@{Name=$name;Exists=$exists;Value=$(if($exists){$old.GetValue($name,$null,[Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)});Kind=$(if($exists){[int]$old.GetValueKind($name)}else{1})}
    }
    [pscustomobject]@{KeyExisted=($null -ne $old);Values=$values} | Export-Clixml -LiteralPath $backup
    if ($old) { $old.Dispose() }
    $key = $base.CreateSubKey($subkey)
    $key.SetValue('DumpFolder', $dump, [Microsoft.Win32.RegistryValueKind]::ExpandString)
    $key.SetValue('DumpType', 2, [Microsoft.Win32.RegistryValueKind]::DWord)
    $key.SetValue('DumpCount', 2, [Microsoft.Win32.RegistryValueKind]::DWord)
    $key.Dispose(); $key = $null
    Write-Host 'READY: Open the installed FengYin. Select its ASIO4ALL low-latency mode ONCE.' -ForegroundColor Cyan
    Read-Host 'After the error appears, return here and press Enter' | Out-Null
    # Do not package a dump while WER still has it open.
    $deadline = (Get-Date).AddSeconds(90)
    $ready = $false
    do {
        $files = @(Get-ChildItem -LiteralPath $dump -Filter '*.dmp')
        $ready = $files.Count -gt 0
        foreach ($file in $files) {
            try { $stream = [IO.File]::Open($file.FullName, 'Open', 'Read', 'None'); $stream.Dispose(); if ($file.Length -eq 0) { $ready=$false } }
            catch { $ready = $false }
        }
        if (!$ready) { Start-Sleep -Seconds 2 }
    } while (!$ready -and (Get-Date) -lt $deadline)
    Restore-Wer
    if (!$ready) { 'No complete dump captured. This is NOT proof the crash is fixed.' | Set-Content (Join-Path $out 'NO-COMPLETE-DUMP.txt') }
    & (Join-Path $PSScriptRoot 'read-only\Collect-Audio.ps1') -OutputRoot $out
    Get-Process -Name '*FengYin*' -ErrorAction SilentlyContinue | ForEach-Object {
        try { $_.Modules | Select-Object FileName,@{n='Version';e={$_.FileVersionInfo.FileVersion}} } catch { $_.Exception.Message }
    } | Out-String -Width 300 | Set-Content (Join-Path $out 'live-modules.txt')
    # ZipArchive supports large dumps; never delete the uncompressed evidence.
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = $out + '.zip'
    [IO.Compression.ZipFile]::CreateFromDirectory($out, $zip, [IO.Compression.CompressionLevel]::Fastest, $true)
    Write-Host "DONE. Send this file: $zip" -ForegroundColor Green
    if (!$ready) { Write-Warning 'Dump missing or incomplete. Send the ZIP anyway; do not repeat installation.' }
} catch {
    Write-Host $_.Exception.Message -ForegroundColor Red
} finally {
    if ($key) { $key.Dispose() }
    if ($locked -and $base) {
        try { Restore-Wer } catch { Write-Warning 'Restore failed. Keep wer-backup.clixml and run Restore.cmd.' }
        $base.Dispose()
    }
    if ($locked) { $mutex.ReleaseMutex() }
    $mutex.Dispose()
    Read-Host 'Press Enter to close' | Out-Null
}
