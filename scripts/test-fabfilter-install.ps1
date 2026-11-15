$ErrorActionPreference='Stop'
$Directory=Join-Path (Get-Location) 'fabfilter-install-test'
./scripts/prepare-fabfilter.ps1 -Destination $Directory
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File ./installer/Install-FabFilter.ps1 -PackageDirectory $Directory -EvidenceFile "$Directory/first.json"
if ($LASTEXITCODE -ne 0) {throw 'FabFilter actual installation failed'}
$First=Get-Content "$Directory/first.json" -Raw | ConvertFrom-Json
$Stamp=(Get-Item -LiteralPath $First.plugin).LastWriteTimeUtc
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File ./installer/Install-FabFilter.ps1 -PackageDirectory $Directory -EvidenceFile "$Directory/repeat.json"
if ($LASTEXITCODE -ne 0) {throw 'FabFilter repeated dependency check failed'}
$Repeat=Get-Content "$Directory/repeat.json" -Raw | ConvertFrom-Json
if ($Repeat.action -ne 'existing-compatible-plugin' -or (Get-Item -LiteralPath $First.plugin).LastWriteTimeUtc -ne $Stamp) {throw 'Compatible existing FabFilter plugin was overwritten'}
