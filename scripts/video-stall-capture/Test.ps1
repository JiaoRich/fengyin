$ErrorActionPreference='Stop'
foreach($file in @('Capture.ps1','Test.ps1','NativeCommands.ps1')) {
    $tokens=$null;$errors=$null
    [System.Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot $file),[ref]$tokens,[ref]$errors) | Out-Null
    if($errors.Count){throw ($errors | Out-String)}
}
. (Join-Path $PSScriptRoot 'NativeCommands.ps1')
$nativeOut=Join-Path $pwd 'native-command-smoke'
New-Item -ItemType Directory -Path $nativeOut | Out-Null
foreach($code in @(0,7,0,19,0)) {
    $result=Invoke-CapturedNative (Join-Path $env:SystemRoot 'System32/cmd.exe') ('/d /c echo stdout & echo stderr 1>&2 & exit '+$code) $nativeOut ('exit-'+$code+'-'+[guid]::NewGuid().ToString('N')) 10
    if($result.ExitCode -ne $code -or $result.Output -notmatch 'stdout' -or $result.Error -notmatch 'stderr') {throw 'Native exit code/output regression'}
}
$timedOut=$false
try {Invoke-CapturedNative (Join-Path $env:SystemRoot 'System32/WindowsPowerShell/v1.0/powershell.exe') '-NoProfile -Command Start-Sleep -Seconds 10' $nativeOut 'timeout' 1 | Out-Null}
catch {if($_.Exception.Message -match 'timed out'){$timedOut=$true}else{throw}}
if(-not $timedOut){throw 'Native command timeout was not enforced'}
$own='FengYinVideo-'+[guid]::NewGuid().ToString('N')
$names=@(Get-OwnedStallSessionNames ('WPR_initiated_'+$own+'_WPR System Collector WPR_initiated_'+$own+'_WPR Event Collector WPR_initiated_Unrelated_WPR System Collector FengYinVideo-not-a-guid'))
if($names.Count -ne 1 -or $names[0] -ne $own){throw 'Collector session ownership filter failed'}
Write-Host 'PASS exact native exit codes including nonzero, redirected output/error, timeout, strict session ownership'
Add-Type -Path (Join-Path $PSScriptRoot 'WindowSnapshot.cs')
Add-Type -AssemblyName System.Windows.Forms
$form=New-Object System.Windows.Forms.Form
$panel=New-Object System.Windows.Forms.Panel
$form.Controls.Add($panel)
$panel.SetBounds(10,10,120,80)
$form.SetBounds(100,100,500,400)
$form.Show()
[System.Windows.Forms.Application]::DoEvents()
try {
    $panel.Enabled=$false
    $snapshot=[FengYin.StallCapture.Windows]::Take([uint32]$PID)
    $root=@($snapshot.Items | Where-Object Handle -eq ('0x'+$form.Handle.ToInt64().ToString('x')))
    $child=@($snapshot.Items | Where-Object Handle -eq ('0x'+$panel.Handle.ToInt64().ToString('x')))
    if($root.Count -ne 1 -or -not $root[0].InfoRead -or -not $root[0].Visible -or -not $root[0].Enabled){throw 'Visible enabled root not captured'}
    if($child.Count -ne 1 -or $child[0].Enabled -or -not $child[0].Visible){throw 'Disabled child not captured'}
    if(-not @($snapshot.Threads | Where-Object Read).Count){throw 'GUI thread state unavailable'}
    if(($root[0].Bounds.Right-$root[0].Bounds.Left) -ne $form.Width){throw 'Window rectangle mismatch'}
    $panel.Enabled=$true
    $again=[FengYin.StallCapture.Windows]::Take([uint32]$PID)
    if(-not @($again.Items | Where-Object { $_.Handle -eq $child[0].Handle -and $_.Enabled }).Count){throw 'Enabled transition not captured'}
    $form.Hide()
    $hidden=[FengYin.StallCapture.Windows]::Take([uint32]$PID)
    if(@($hidden.Items | Where-Object { $_.Handle -eq $root[0].Handle -and $_.Visible }).Count){throw 'Hidden transition not captured'}
    $snapshot | ConvertTo-Json -Depth 8 | Set-Content 'window-smoke.json' -Encoding UTF8
    Write-Host 'PASS actual Win32 visible/enabled/hidden transitions, rectangles, GUI thread state, JSON serialization'
} finally {$form.Dispose()}

$wpr=Join-Path $env:SystemRoot 'System32/wpr.exe'
if(-not (Test-Path -LiteralPath $wpr)){throw 'WPR unavailable: CPU collection was NOT tested'}
$session='FengYinVideoSmoke-'+[guid]::NewGuid().ToString('N')
$active=$false
try {
    & $wpr -start CPU -instancename $session
    if($LASTEXITCODE){throw 'WPR start failed'}
    $active=$true
    $clock=[Diagnostics.Stopwatch]::StartNew()
    while($clock.Elapsed.TotalSeconds -lt 5) { [Math]::Sqrt(12345) | Out-Null }
    & $wpr -status -instancename $session
    if($LASTEXITCODE){throw 'WPR status failed'}
    & $wpr -stop (Join-Path $pwd 'cpu-smoke.etl') 'FengYin collector smoke' -skipPdbGen -instancename $session
    if($LASTEXITCODE){throw 'WPR stop failed'}
    $active=$false
    if((Get-Item 'cpu-smoke.etl').Length -lt 4096){throw 'ETL missing/empty'}
    # Exercise the same UTF-8 JSONL and ZIP round trip used for evidence delivery.
    $folder=Join-Path $pwd 'collector-smoke'
    New-Item -ItemType Directory -Path $folder | Out-Null
    Copy-Item window-smoke.json $folder
    Copy-Item cpu-smoke.etl $folder
    Compress-Archive -LiteralPath $folder -DestinationPath 'collector-smoke.zip'
    Expand-Archive -LiteralPath 'collector-smoke.zip' -DestinationPath 'collector-smoke-expanded'
    $copy=Join-Path $pwd 'collector-smoke-expanded/collector-smoke/cpu-smoke.etl'
    if((Get-FileHash $copy).Hash -ne (Get-FileHash cpu-smoke.etl).Hash){throw 'ETL ZIP round trip changed bytes'}
    Write-Host 'PASS actual named WPR CPU start/status/stop, nonempty ETL and ZIP round trip'
} finally {
    if($active){& $wpr -cancel -instancename $session}
}

# Run the exact user-facing Capture.ps1 end-to-end, not a parallel hand-written WPR path.
$fixtureDir=Join-Path $pwd 'collector-fixture'
New-Item -ItemType Directory -Path $fixtureDir | Out-Null
$fixtureExe=Join-Path $fixtureDir 'FengYin.exe'
Add-Type -OutputAssembly $fixtureExe -OutputType WindowsApplication -ReferencedAssemblies System.Windows.Forms -TypeDefinition @'
using System;
using System.Windows.Forms;
public class CollectorFixture {
    [STAThread] public static void Main() {
        var window=new Form { Text="Synthetic collector fixture", Width=500, Height=400 };
        var timer=new Timer { Interval=90000 };
        timer.Tick+=(s,e)=>Application.Exit();timer.Start();Application.Run(window);
    }
}
'@
$fixture=[Diagnostics.Process]::Start($fixtureExe)
$orphan='FengYinVideo-'+[guid]::NewGuid().ToString('N')
$orphanStarted=$false
try {
    $start=Invoke-CapturedNative $wpr ('-start CPU -instancename '+$orphan) $nativeOut 'orphan-start' 30
    if($start.ExitCode -ne 0){throw 'Cannot create synthetic prior collector session'}
    $orphanStarted=$true
    # Match the customer's extracted folder spelling: spaces and parentheses.
    $copyRoot=Join-Path $pwd 'collector script (4)'
    New-Item -ItemType Directory -Path $copyRoot | Out-Null
    Get-ChildItem -LiteralPath $PSScriptRoot -File | Copy-Item -Destination $copyRoot
    $output=Join-Path $pwd 'exact entry output (4)'
    New-Item -ItemType Directory -Path $output | Out-Null
    $elapsed=[Diagnostics.Stopwatch]::StartNew()
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $copyRoot 'Capture.ps1') -NoPrompt -Seconds 30 -OutputBase $output
    if($LASTEXITCODE -ne 0){throw 'Exact user entry returned failure'}
    if($elapsed.Elapsed.TotalSeconds -lt 30){throw 'Capture ended before requested duration'}
    $bundle=@(Get-ChildItem -LiteralPath $output -Directory)
    if($bundle.Count -ne 1){throw 'Expected exactly one capture output directory'}
    foreach($name in @('session.json','cpu.etl','windows.jsonl','processes.csv','threads.csv','commands.jsonl','binaries.jsonl',($orphan+'.etl'))) {
        $file=Join-Path $bundle[0].FullName $name
        if(-not (Test-Path -LiteralPath $file) -or (Get-Item -LiteralPath $file).Length -eq 0){throw ('Exact entry missing evidence: '+$name)}
    }
    if(Test-Path (Join-Path $bundle[0].FullName 'capture-error.txt')){throw 'Exact entry wrote a capture error'}
    $metadata=Get-Content (Join-Path $bundle[0].FullName 'session.json') -Encoding UTF8 -Raw | ConvertFrom-Json
    $after=Invoke-CapturedNative (Join-Path $env:SystemRoot 'System32/logman.exe') 'query -ets' $nativeOut 'post-entry-inventory' 30
    $remaining=@(Get-OwnedStallSessionNames $after.Output)
    if($remaining -contains $metadata.Session -or $remaining -contains $orphan){throw 'User entry leaked an ETW session'}
    $orphanStarted=$false
    $zip=Get-Item ($bundle[0].FullName+'.zip')
    if($zip.Length -lt 4096){throw 'Exact entry result ZIP empty'}
    Copy-Item -LiteralPath $zip.FullName -Destination (Join-Path $pwd 'collector-exact-entry.zip')
    Write-Host 'PASS EXACT USER ENTRY: 30-second capture, prior-session recovery, window/process/thread data, CPU ETL, ZIP, no leaked session'
} finally {
    if($orphanStarted){& $wpr -cancel -instancename $orphan}
    if(-not $fixture.HasExited){$null=$fixture.CloseMainWindow();if(-not $fixture.WaitForExit(5000)){$fixture.Kill()}}
    $fixture.Dispose()
}
