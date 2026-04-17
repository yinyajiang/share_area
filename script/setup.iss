#ifndef MyAppVersion
  #define MyAppVersion "1.0.0"
#endif

[Setup]
AppName=ShareArea
AppVersion={#MyAppVersion}
AppPublisher=ShareArea
AppPublisherURL=https://github.com/yajiang/share_area
AppSupportURL=https://github.com/yajiang/share_area/issues
DefaultDirName={autopf}\ShareArea
DefaultGroupName=ShareArea
UninstallDisplayName=ShareArea
UninstallDisplayIcon={app}\ShareArea.exe
LicenseFile=..\LICENSE
OutputBaseFilename=ShareArea_Setup_{#MyAppVersion}
Compression=lzma2/ultra64
SolidCompression=yes
OutputDir=../output
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
SetupIconFile=..\resources\icons\icon.ico
DisableProgramGroupPage=yes

[Languages]
Name: "chinesesimplified"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "..\build\Release\ShareArea.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\Release\*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\Release\platforms\*"; DestDir: "{app}\platforms"; Flags: ignoreversion recursesubdirs
Source: "..\build\Release\styles\*"; DestDir: "{app}\styles"; Flags: ignoreversion recursesubdirs
Source: "..\build\Release\imageformats\*"; DestDir: "{app}\imageformats"; Flags: ignoreversion recursesubdirs
Source: "..\build\Release\iconengines\*"; DestDir: "{app}\iconengines"; Flags: ignoreversion recursesubdirs
Source: "..\build\Release\translations\*"; DestDir: "{app}\translations"; Flags: ignoreversion recursesubdirs
Source: "..\build\Release\generic\*"; DestDir: "{app}\generic"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist
Source: "..\build\Release\networkinformation\*"; DestDir: "{app}\networkinformation"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist
Source: "..\build\Release\tls\*"; DestDir: "{app}\tls"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist

[Icons]
Name: "{group}\ShareArea"; Filename: "{app}\ShareArea.exe"
Name: "{group}\{cm:UninstallProgram,ShareArea}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\ShareArea"; Filename: "{app}\ShareArea.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\ShareArea.exe"; Description: "{cm:LaunchProgram,ShareArea}"; Flags: nowait postinstall skipifsilent
