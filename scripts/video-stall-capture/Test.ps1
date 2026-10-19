$ErrorActionPreference='Stop'
foreach($file in @('Capture.ps1','Test.ps1')) {
    $tokens=$null;$errors=$null
    [System.Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot $file),[ref]$tokens,[ref]$errors) | Out-Null
    if($errors.Count){throw ($errors | Out-String)}
}
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
    & $wpr -status collectors -details -instancename $session
    if($LASTEXITCODE){throw 'WPR status failed'}
    & $wpr -stop (Join-Path $pwd 'cpu-smoke.etl') -skipPdbGen -compress -instancename $session
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
