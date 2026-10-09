#ifndef SourceDir
  #error "必须通过 /DSourceDir=... 指定已安装的程序目录"
#endif
#ifndef OutputDir
  #define OutputDir "."
#endif
#ifndef AppVersion
  #define AppVersion "1.1.7"
#endif
#ifndef ChineseMessages
  #define ChineseMessages "compiler:Languages\ChineseSimplified.isl"
#endif

[Setup]
AppId={{BA041761-FF1B-4F84-B055-AE70EC8C883B}
AppName=风吟
AppVersion={#AppVersion}
VersionInfoVersion={#AppVersion}.0
AppPublisher=风吟
DefaultDirName={autopf}\风吟
DefaultGroupName=风吟
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputDir={#OutputDir}
OutputBaseFilename=风吟-{#AppVersion}-Windows-x64
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
SetupLogging=yes
InfoBeforeFile={#SourceDir}\AudioComponents.txt
CloseApplications=yes
RestartApplications=no
AlwaysRestart=yes
UninstallDisplayName=风吟

[Languages]
Name: "chinesesimp"; MessagesFile: "{#ChineseMessages}"

[Messages]
FinishedRestartMessage=风吟已安装完成。音频驱动需要重启电脑后才能正常使用。请保存其他工作，然后重启；重启后再打开风吟。

[Tasks]
Name: "desktopicon"; Description: "在桌面创建快捷方式"; GroupDescription: "快捷方式："; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Excludes: "MicrosoftEdgeWebView2RuntimeInstallerX64.exe"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\MicrosoftEdgeWebView2RuntimeInstallerX64.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall

[Icons]
Name: "{group}\风吟"; Filename: "{app}\FengYin.exe"
Name: "{group}\安装 ASIO4ALL 低延迟组件"; Filename: "{app}\components\ASIO4ALL\ASIO4ALL_2_22.exe"
Name: "{group}\安装 VB-CABLE 网页声音组件"; Filename: "{app}\components\VB-CABLE\VBCABLE_Setup_x64.exe"
Name: "{group}\安装 Fresh Air 音色效果器"; Filename: "{app}\components\Fresh Air\Setup Fresh Air v1.0.8.exe"
Name: "{autodesktop}\风吟"; Filename: "{app}\FengYin.exe"; Tasks: desktopicon

[Run]
Filename: "{tmp}\MicrosoftEdgeWebView2RuntimeInstallerX64.exe"; Parameters: "/silent /install"; StatusMsg: "正在安装视频与精美界面离线运行组件…"; Flags: waituntilterminated

[Registry]
Root: HKLM; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "FengYinAudioRecovery"; ValueData: """{app}\FengYinAudioWatchdog.exe"" --login-recovery"; Flags: uninsdeletevalue

[Code]

var
  VBCableInstalledThisRun: Boolean;
  AudioDriverRestartRequired: Boolean;

procedure MarkAudioDriverRestart();
begin
  AudioDriverRestartRequired := True;
end;

function NeedsASIO4ALL(): Boolean;
var
  Names: TArrayOfString;
  I: Integer;
  ClassId, Server: String;
begin
  Result := True;
  if RegGetSubkeyNames(HKLM64, 'SOFTWARE\ASIO', Names) then
    for I := 0 to GetArrayLength(Names) - 1 do
      if Pos('ASIO4ALL', Uppercase(Names[I])) > 0 then
        if RegQueryStringValue(HKLM64, 'SOFTWARE\ASIO\' + Names[I], 'CLSID', ClassId) then
          if RegQueryStringValue(HKLM64, 'SOFTWARE\Classes\CLSID\' + ClassId + '\InprocServer32', '', Server) then
            if FileExists(RemoveQuotes(Server)) then
            begin
              Result := False;
              Exit;
            end;
end;

function NeedsVBCable(): Boolean;
var
  ResultCode: Integer;
  Arguments: String;
begin
  { A service registry key can survive device removal. Check a present PnP
    device instead; code 14 means installed but awaiting a restart. }
  Arguments := '-NoProfile -NonInteractive -Command "try { ' +
    '$d = Get-CimInstance Win32_PnPEntity -ErrorAction Stop | Where-Object { ' +
    '$_.Present -eq $true -and $_.Service -in @(''VBAudioVACMME'',''VBAudioVACWDM'') ' +
    '-and $_.ConfigManagerErrorCode -in @(0,14) }; ' +
    'if ($d) { exit 0 } else { exit 1 } } catch { exit 2 }"';
  Result := True;
  if Exec(ExpandConstant('{sys}\WindowsPowerShell\v1.0\powershell.exe'),
          Arguments, '', SW_HIDE, ewWaitUntilTerminated, ResultCode) then
  begin
    Log('VB-CABLE present-device check returned ' + IntToStr(ResultCode));
    Result := ResultCode <> 0;
  end;
end;

procedure ExplainVBCableInstall();
begin
  MsgBox('接下来安装随包 VB-CABLE。新安装请点击 Install Driver；若原厂窗口只有 Remove Driver，表示需要先移除旧版，再按后续提示重新安装。若要求重启，请保存工作后重启，再运行本安装包完成安装。不要仅关闭窗口。', mbInformation, MB_OK);
end;

procedure VerifyVBCableInstalled();
begin
  if NeedsVBCable() then
    RaiseException('VB-CABLE 未完成安装。请在原厂窗口点击 Install Driver，不要直接关闭窗口。');
  VBCableInstalledThisRun := True;
end;

function HasFreshAirBinary(Path: String): Boolean;
begin
  { VST3 can be a flat DLL or a bundle. A directory alone is not proof of
    installation: require the Windows x64 module inside it. }
  Result := FileExists(Path)
    or FileExists(Path + '\Contents\x86_64-win\Fresh Air.vst3');
end;

function HasFreshAir(): Boolean;
begin
  Result := HasFreshAirBinary(ExpandConstant('{commoncf64}\VST3\Fresh Air.vst3'))
    or HasFreshAirBinary(ExpandConstant('{commoncf64}\VST3\Slate Digital\Fresh Air.vst3'));
end;

function NeedsFreshAir(): Boolean;
begin
  Result := not HasFreshAir();
end;

procedure VerifyFreshAirInstalled();
var
  ResultCode: Integer;
begin
  if NeedsFreshAir() then
  begin
    Log('Fresh Air silent install did not produce a discoverable x64 VST3; opening original installer.');
    MsgBox('Fresh Air 自动安装未完成。接下来将打开原厂安装向导，请勾选 64 位 VST3 组件并使用默认安装位置。风吟的其他功能不受影响。', mbInformation, MB_OK);
    if not Exec(ExpandConstant('{app}\components\Fresh Air\Setup Fresh Air v1.0.8.exe'),
      ExpandConstant('/NORESTART /LOG="{app}\components\Fresh Air\install-retry.log"'),
      '', SW_SHOWNORMAL, ewWaitUntilTerminated, ResultCode) then
      Log('Fresh Air installer could not start: ' + IntToStr(ResultCode));
    if NeedsFreshAir() then
      MsgBox('Fresh Air 尚未安装成功，次中萨-气包音的效果链暂不可用。可从开始菜单运行“安装 Fresh Air 音色效果器”重试。安装日志保存在风吟安装目录的 components\Fresh Air 文件夹。', mbError, MB_OK)
    else
      Log('Fresh Air x64 VST3 verified after interactive installation.');
  end;
end;

function NeedRestart(): Boolean;
begin
  Result := True;
end;

procedure RunRequiredComponent(FileName, Arguments, WorkingDirectory: String);
var
  ResultCode: Integer;
begin
  if not Exec(ExpandConstant(FileName), ExpandConstant(Arguments), ExpandConstant(WorkingDirectory),
      SW_SHOWNORMAL, ewWaitUntilTerminated, ResultCode) then
    RaiseException('依赖安装程序无法启动：' + FileName);
  Log('Required component ' + FileName + ' exit=' + IntToStr(ResultCode));
  if (ResultCode <> 0) and (ResultCode <> 3010) then
    RaiseException('依赖安装未完成或已取消：' + FileName + '，返回码 ' + IntToStr(ResultCode));
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  ResultCode: Integer;
begin
  if CurStep <> ssPostInstall then Exit;
  { Capture the interactive user's defaults, not the elevated administrator's. }
  if not ExecAsOriginalUser(ExpandConstant('{app}\FengYinAudioWatchdog.exe'), '--snapshot-install', '',
      SW_HIDE, ewWaitUntilTerminated, ResultCode) then
    RaiseException('无法保存安装前的声音输出，请关闭风吟后重试。');
  if ResultCode <> 0 then RaiseException('声音输出仍被占用或无法保存，请关闭风吟后重试。');
  MarkAudioDriverRestart();
  RunRequiredComponent('{app}\components\ASIO4ALL\ASIO4ALL_2_22.exe', '', '');
  if NeedsASIO4ALL() then RaiseException('ASIO4ALL 安装未完成，请完成原厂安装向导。');
  ExplainVBCableInstall();
  RunRequiredComponent('{app}\components\VB-CABLE\VBCABLE_Setup_x64.exe', '', '{app}\components\VB-CABLE');
  if NeedsVBCable() then
  begin
    MsgBox('尚未检测到可用的 VB-CABLE。如果刚才原厂程序执行了 Remove Driver，必须重新完成 Install Driver。接下来再次打开安装器；若提示必须先重启，请重启后重新运行风吟安装包。', mbInformation, MB_OK);
    RunRequiredComponent('{app}\components\VB-CABLE\VBCABLE_Setup_x64.exe', '', '{app}\components\VB-CABLE');
  end;
  VerifyVBCableInstalled();
  RunRequiredComponent('{app}\components\Fresh Air\Setup Fresh Air v1.0.8.exe',
    '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /SP- /TYPE=full /LOG="{app}\components\Fresh Air\install.log"', '');
  VerifyFreshAirInstalled();
  if NeedsFreshAir() then RaiseException('Fresh Air 安装未完成，不能完成风吟依赖安装。');
  if not SaveStringToFile(ExpandConstant('{app}\dependencies-generation.txt'),
      GetDateTimeString('yyyymmddhhnnss', '', ''), False) then
    RaiseException('无法记录依赖安装结果。');
end;

function CanLaunchNow(): Boolean;
begin
  Result := not NeedRestart() and not NeedsASIO4ALL() and not NeedsVBCable();
end;
