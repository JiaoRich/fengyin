#ifndef SourceDir
  #error "必须通过 /DSourceDir=... 指定已安装的程序目录"
#endif
#ifndef OutputDir
  #define OutputDir "."
#endif
#ifndef AppVersion
  #define AppVersion "0.4.4"
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
CloseApplications=yes
RestartApplications=no
UninstallDisplayName=风吟

[Languages]
Name: "chinesesimp"; MessagesFile: "{#ChineseMessages}"

[Tasks]
Name: "desktopicon"; Description: "在桌面创建快捷方式"; GroupDescription: "快捷方式："; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Excludes: "MicrosoftEdgeWebview2Setup.exe"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\MicrosoftEdgeWebview2Setup.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall

[Icons]
Name: "{group}\风吟"; Filename: "{app}\FengYin.exe"
Name: "{autodesktop}\风吟"; Filename: "{app}\FengYin.exe"; Tasks: desktopicon

[Run]
Filename: "{tmp}\MicrosoftEdgeWebview2Setup.exe"; Parameters: "/silent /install"; StatusMsg: "正在检查视频与精美界面运行组件…"; Flags: waituntilterminated
Filename: "{app}\FengYin.exe"; Description: "启动风吟"; Flags: nowait postinstall skipifsilent
