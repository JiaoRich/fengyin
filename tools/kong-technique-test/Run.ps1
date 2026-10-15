param([string]$PluginPath='')
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
if(-not $PluginPath){
  $common=Join-Path ${env:CommonProgramFiles} 'VST3'
  $candidates=@(Get-ChildItem -LiteralPath $common -Recurse -ErrorAction SilentlyContinue | Where-Object { $_.Name -match '(?i)(qin|kong).*\.vst3$' -and $_.FullName -notmatch '(?i)\.vst3[\\/]' })
  if($candidates.Count -eq 1){$PluginPath=$candidates[0].FullName}
  else {
    Add-Type -AssemblyName System.Windows.Forms
    [System.Windows.Forms.MessageBox]::Show('Select the installed QinEngine VST3 file. No drivers or presets will be modified.','FengYin sampler test') | Out-Null
    $dialog=New-Object System.Windows.Forms.OpenFileDialog
    $dialog.InitialDirectory=$common;$dialog.Filter='VST3 plugin (*.vst3)|*.vst3|All files (*.*)|*.*'
    if($dialog.ShowDialog() -ne 'OK'){throw 'Plugin selection cancelled'}
    $PluginPath=$dialog.FileName
  }
}
if(-not (Test-Path -LiteralPath $PluginPath)){throw 'Qin plugin not found'}
$stamp=Get-Date -Format 'yyyyMMdd-HHmmss'
$out=Join-Path $root "Results-$stamp"
if((Get-Item -LiteralPath $root).PSDrive.Free -lt 2GB){throw 'Please leave at least 2 GB free disk space for recordings.'}
New-Item -ItemType Directory -Path $out | Out-Null
$exe=Join-Path $root 'FengYinKongTechniqueTest.exe'
$assets=@('erhu','matouqin','hulusi','xun','nanxiao','qudi','suona','sheng','pipa','liuqin','sanxian','guzheng')
$summary=@()
foreach($asset in $assets){
  $folder=Join-Path $out $asset;New-Item -ItemType Directory -Path $folder | Out-Null
  Write-Host "Testing $asset ... Please keep this window open."
  $arguments='"{0}" "{1}.kam" "{2}"' -f $PluginPath,$asset,$folder
  $p=Start-Process -FilePath $exe -ArgumentList $arguments -PassThru -RedirectStandardOutput (Join-Path $folder 'worker.log') -RedirectStandardError (Join-Path $folder 'error.log')
  $null=$p.Handle
  $finished=$p.WaitForExit(300000)
  $status='completed'
  if(-not $finished){Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue;$status='timeout'}
  else {$p.WaitForExit();if($p.ExitCode -ne 0){$status='failed'}}
  if($status -eq 'completed' -and -not (Test-Path (Join-Path $folder 'completed.txt'))){$status='incomplete'}
  $summary+= [pscustomobject]@{instrument=$asset;status=$status;exitCode=$(if($finished){$p.ExitCode}else{$null});capability='NOT_AUTOMATICALLY_CERTIFIED'}
  $summary | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 (Join-Path $out 'summary.json')
}
$zip="$out.zip"
Compress-Archive -LiteralPath $out -DestinationPath $zip
Write-Host "Finished. Send this ZIP for analysis: $zip"
Start-Process explorer.exe -ArgumentList ('/select,"{0}"' -f $zip)
