$ErrorActionPreference='Stop'
$Ast=[System.Management.Automation.Language.Parser]::ParseFile((Join-Path (Get-Location) 'installer/Install-FabFilter.ps1'),[ref]$null,[ref]$null)
$VersionFunction=$Ast.Find({param($Node) $Node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $Node.Name -eq 'Test-ProRVersion'},$true)
if (-not $VersionFunction) {throw 'Missing Pro-R version gate'}
. ([scriptblock]::Create($VersionFunction.Extent.Text))
foreach($Case in @(@('2.0.0.0',$false),@('2.0.5.0',$false),@('2.0.6.0',$true),@('2.0.6.25',$true),@('2.0.7.0',$true),@('1.0.0.0',$false))) {
    if ((Test-ProRVersion ([version]$Case[0])) -ne $Case[1]) {throw "Unexpected Pro-R prerequisite decision: $($Case[0])"}
}

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
