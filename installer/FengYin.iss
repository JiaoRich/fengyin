#ifndef SourceDir
  #error "必须通过 /DSourceDir=... 指定已安装的程序目录"
#endif
#ifndef OutputDir
  #define OutputDir "."
#endif
#ifndef AppVersion
  #define AppVersion "1.1.1"
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
UninstallDisplayName=风吟

[Languages]
Name: "chinesesimp"; MessagesFile: "{#ChineseMessages}"

[Tasks]
Name: "asio4all"; Description: "安装 ASIO4ALL 64位低延迟组件（请完成原厂安装向导）"; GroupDescription: "低延迟组件："; Check: NeedsASIO4ALL
Name: "desktopicon"; Description: "在桌面创建快捷方式"; GroupDescription: "快捷方式："; Flags: unchecked
Name: "freshair"; Description: "安装 Fresh Air 1.0.8 音色效果器（次中萨-气包音必需）"; GroupDescription: "音色组件："; Check: NeedsFreshAir

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
Filename: "{app}\components\ASIO4ALL\ASIO4ALL_2_22.exe"; Tasks: asio4all; Check: NeedsASIO4ALL; StatusMsg: "请完成 ASIO4ALL 原厂安装向导"; Flags: waituntilterminated
Filename: "{tmp}\MicrosoftEdgeWebView2RuntimeInstallerX64.exe"; Parameters: "/silent /install"; StatusMsg: "正在安装视频与精美界面离线运行组件…"; Flags: waituntilterminated
Filename: "{app}\components\VB-CABLE\VBCABLE_Setup_x64.exe"; WorkingDir: "{app}\components\VB-CABLE"; Check: NeedsVBCable; BeforeInstall: ExplainVBCableInstall; AfterInstall: VerifyVBCableInstalled; StatusMsg: "正在安装 VB-CABLE 网页声音组件…"; Flags: waituntilterminated
Filename: "{app}\components\Fresh Air\Setup Fresh Air v1.0.8.exe"; Parameters: "/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /SP-"; Tasks: freshair; Check: NeedsFreshAir; AfterInstall: VerifyFreshAirInstalled; StatusMsg: "正在安装 Fresh Air 1.0.8 音色效果器…"; Flags: waituntilterminated
Filename: "{app}\FengYin.exe"; Description: "启动风吟"; Check: CanLaunchNow; Flags: nowait postinstall skipifsilent

[Code]

var
  VBCableInstalledThisRun: Boolean;

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
  MsgBox('风吟低延迟模式需要 VB-CABLE。接下来的原厂窗口中请点击 Install Driver，等待安装完成后再关闭窗口。', mbInformation, MB_OK);
end;

procedure VerifyVBCableInstalled();
begin
  if NeedsVBCable() then
    RaiseException('VB-CABLE 未完成安装。请在原厂窗口点击 Install Driver，不要直接关闭窗口。');
  VBCableInstalledThisRun := True;
end;

function HasFreshAir(): Boolean;
begin
  Result := FileExists(ExpandConstant('{commoncf64}\VST3\Fresh Air.vst3'))
    or FileExists(ExpandConstant('{commoncf64}\VST3\Slate Digital\Fresh Air.vst3'));
end;

function NeedsFreshAir(): Boolean;
begin
  Result := not HasFreshAir();
end;

procedure VerifyFreshAirInstalled();
begin
  if NeedsFreshAir() then
    RaiseException('Fresh Air 1.0.8 安装未完成：未找到 Slate Digital\Fresh Air.vst3。');
end;

function NeedRestart(): Boolean;
begin
  Result := VBCableInstalledThisRun;
end;

function CanLaunchNow(): Boolean;
begin
  Result := not VBCableInstalledThisRun and not NeedsASIO4ALL() and not NeedsVBCable();
end;
