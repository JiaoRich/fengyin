$ErrorActionPreference = 'Stop'
# Execute the production expression in the actual Inno Setup Pascal runtime.
# This harness installs no files or drivers and does not restart Windows.
$source = Get-Content installer/FengYin.iss -Raw -Encoding UTF8
$expression = [regex]::Match($source, "GetDateTimeString\('yyyymmddhhnnss',\s*#0,\s*#0\)").Value
if (-not $expression) { throw 'Production generation expression missing' }
$root = Join-Path $env:TEMP ('fengyin-marker-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $root | Out-Null
$result = Join-Path $root 'result.txt'
$script = @"
[Setup]
AppName=FengYin Marker Test
AppVersion=1
DefaultDirName={tmp}\FengYinMarker
CreateAppDir=no
Uninstallable=no
PrivilegesRequired=lowest
OutputDir=$root
OutputBaseFilename=marker-test
[Code]
function InitializeSetup(): Boolean;
begin
  if not SaveStringToFile('$result', $expression, False) then
    RaiseException('Marker write failed');
  Result := False;
end;
"@
$iss = Join-Path $root 'marker.iss'
Set-Content $iss $script -Encoding UTF8
$iscc = Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'
& $iscc $iss
if ($LASTEXITCODE) { throw 'Marker harness compilation failed' }
Start-Process (Join-Path $root 'marker-test.exe') -ArgumentList '/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART' -Wait
if (-not (Test-Path $result)) { throw 'Marker runtime failed to create output' }
$value = (Get-Content $result -Raw).Trim()
if ($value -notmatch '^\d{14}$') { throw "Invalid generation marker: $value" }
Write-Host "Production installer timestamp executed successfully: $value"
