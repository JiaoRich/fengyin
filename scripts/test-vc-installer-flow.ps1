$ErrorActionPreference='Stop'
# Execute the production Pascal wrapper in Inno, substituting only external
# process/UI dependencies. No drivers, app files or shared runtimes are changed.
$source=Get-Content installer/FengYin.iss -Raw -Encoding UTF8
$procedure=[regex]::Match($source,'(?s)procedure PrepareVCRuntime\(\);.*?(?=procedure MarkAudioDriverRestart)').Value
if(-not $procedure){throw 'Production prerequisite wrapper missing'}
$procedure=$procedure -replace '\bExec\(', 'MockExec(' -replace '\bMsgBox\(', 'MockMsgBox('
$folder=Join-Path $env:TEMP ('fy-vc-inno-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $folder | Out-Null
$result=Join-Path $folder 'result.txt'
$escaped=$result.Replace("'","''")
$script=@"
[Setup]
AppName=FengYin VC prerequisite flow test
AppVersion=1
DefaultDirName={tmp}\FengYinVCFlow
CreateAppDir=no
Uninstallable=no
PrivilegesRequired=lowest
OutputDir=$folder
OutputBaseFilename=flow-test
[Code]
var CaseId, Calls: Integer;
function MockExec(const Filename, Params, WorkingDir: String; const ShowCmd: Integer; const Wait: TExecWait; var Code: Integer): Boolean;
begin
  Calls := Calls + 1; Result := True;
  if CaseId = 0 then Code := 0
  else if CaseId = 1 then Code := 3010
  else if CaseId = 2 then Code := 3011
  else if CaseId = 3 then begin if Calls = 1 then Code := 1602 else Code := 0; end
  else Code := 1618;
end;
function MockMsgBox(const Text: String; const Typ: TMsgBoxType; const Buttons: Integer): Integer;
begin
  if CaseId = 3 then Result := IDRETRY else Result := IDCANCEL;
end;
$procedure
procedure CurStepChanged(CurStep: TSetupStep);
var Failed, ExpectedFailure: Boolean;
begin
  if CurStep <> ssInstall then Exit;
  for CaseId := 0 to 4 do
  begin
    Calls := 0; Failed := False;
    try PrepareVCRuntime(); except Failed := True; end;
    ExpectedFailure := (CaseId = 2) or (CaseId = 4);
    if Failed <> ExpectedFailure then RaiseException('Unexpected prerequisite outcome');
    if (CaseId = 3) and (Calls <> 2) then RaiseException('Retry failed');
    if (CaseId <> 3) and (Calls <> 1) then RaiseException('Unexpected repeated process');
  end;
  if not SaveStringToFile('$escaped', '5 production Pascal flow scenarios passed', False) then RaiseException('Cannot save evidence');
end;
"@
$iss=Join-Path $folder 'flow.iss';Set-Content -LiteralPath $iss -Value $script -Encoding UTF8
$iscc=Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'
& $iscc $iss
if($LASTEXITCODE -ne 0){throw 'Production Pascal prerequisite wrapper failed compilation'}
$process=Start-Process (Join-Path $folder 'flow-test.exe') -ArgumentList '/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART' -Wait -PassThru
if($process.ExitCode -ne 0 -or -not (Test-Path $result)){throw 'Production prerequisite flow failed'}
New-Item -ItemType Directory -Force vc-runtime-test | Out-Null
Copy-Item -LiteralPath $result -Destination vc-runtime-test/production-inno-flow.txt -Force
Write-Host 'Production Inno prerequisite flow: success, restart, deferred restart, retry, cancel passed.'
