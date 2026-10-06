#ifndef SourceDir
  #error "必须通过 /DSourceDir=... 指定已安装的程序目录"
#endif
#ifndef OutputDir
  #define OutputDir "."
#endif
#ifndef AppVersion
  #define AppVersion "1.1.0"
#endif
#ifndef ChineseMessages
  #define ChineseMessages "compiler:Languages\ChineseSimplified.isl"
#endif
#ifndef HasAudioDriver
  #define HasAudioDriver 0
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
UninstallDisplayName=风吟

[Languages]
Name: "chinesesimp"; MessagesFile: "{#ChineseMessages}"

[Tasks]
Name: "desktopicon"; Description: "在桌面创建快捷方式"; GroupDescription: "快捷方式："; Flags: unchecked
Name: "vbcable"; Description: "安装 VB-CABLE 基础版（VB-Audio 捐赠软件，安装后需重启）"; GroupDescription: "低延迟组件："; Check: NeedsVBCable

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Excludes: "MicrosoftEdgeWebview2Setup.exe"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\MicrosoftEdgeWebview2Setup.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall

[Icons]
Name: "{group}\风吟"; Filename: "{app}\FengYin.exe"
Name: "{autodesktop}\风吟"; Filename: "{app}\FengYin.exe"; Tasks: desktopicon

[Run]
Filename: "{tmp}\MicrosoftEdgeWebview2Setup.exe"; Parameters: "/silent /install"; StatusMsg: "正在检查视频与精美界面运行组件…"; Flags: waituntilterminated
Filename: "{app}\components\VB-CABLE\VBCABLE_Setup_x64.exe"; WorkingDir: "{app}\components\VB-CABLE"; Tasks: vbcable; Check: NeedsVBCable; StatusMsg: "请在 VB-CABLE 原厂窗口中点击 Install Driver，完成后重启电脑"; Flags: waituntilterminated
Filename: "{app}\FengYin.exe"; Description: "启动风吟"; Check: CanLaunchNow; Flags: nowait postinstall skipifsilent

[Code]
var
  DriverNeedsRestart: Boolean;

function NeedsVBCable(): Boolean;
begin
  Result := not RegKeyExists(HKLM, 'SYSTEM\CurrentControlSet\Services\VBAudioVACMME')
    and not RegKeyExists(HKLM, 'SYSTEM\CurrentControlSet\Services\VBAudioVACWDM');
end;

function NeedRestart(): Boolean;
begin
  Result := DriverNeedsRestart or WizardIsTaskSelected('vbcable');
end;

function CanLaunchNow(): Boolean;
begin
  Result := not WizardIsTaskSelected('vbcable');
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  ResultCode: Integer;
begin
  #if HasAudioDriver
  if CurStep = ssPostInstall then
  begin
    if not Exec(ExpandConstant('{app}\FengYinDriverSetup.exe'),
      '--install "' + ExpandConstant('{app}\driver\FengYinAudio.inf') + '"',
      '', SW_HIDE, ewWaitUntilTerminated, ResultCode) then
      RaiseException('无法启动风吟音频驱动安装程序。');
    if (ResultCode <> 0) and (ResultCode <> 3010) then
      RaiseException(Format('风吟音频驱动安装失败（错误码 %d），本次安装已停止。', [ResultCode]));
    if ResultCode = 3010 then
      DriverNeedsRestart := True;
  end;
  #endif
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  ResultCode: Integer;
begin
  #if HasAudioDriver
  if CurUninstallStep = usUninstall then
  begin
    if not Exec(ExpandConstant('{app}\FengYinDriverSetup.exe'), '--uninstall', '',
      SW_HIDE, ewWaitUntilTerminated, ResultCode) then
      RaiseException('无法启动风吟音频驱动卸载程序。');
    if (ResultCode <> 0) and (ResultCode <> 3010) then
      RaiseException(Format('为保护系统声音，风吟没有删除虚拟音频设备（错误码 %d）。请先重启电脑，再重新卸载。', [ResultCode]));
    if ResultCode = 3010 then
      DriverNeedsRestart := True;
  end;
  #endif
end;
