#define MyAppName "Elite Pen"
#define MyAppVersion "1.7.0"
#define MyAppPublisher "Power Elite Studio"
#define MyAppExeName "Elite Pen.exe"

[Setup]
AppId={{A6182B1A-4B96-4E6C-AE4F-C3B56D952CEE}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
VersionInfoCompany={#MyAppPublisher}
VersionInfoDescription=Instalador de Elite Pen
VersionInfoProductName={#MyAppName}
VersionInfoProductVersion={#MyAppVersion}
VersionInfoVersion=1.7.0.0
DefaultDirName={localappdata}\Programs\Power Elite Studio\Elite Pen
DefaultGroupName=Power Elite Studio
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
OutputDir=..\dist\installer
OutputBaseFilename=Elite Pen Setup 1.7.0
SetupIconFile=..\resources\generated\elite_pen.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
LicenseFile=..\LICENSE.txt
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern dynamic
CloseApplications=yes
RestartApplications=no
AppMutex=Local\PowerEliteStudio.ElitePen.Singleton.v1
ChangesAssociations=no
ChangesEnvironment=no

[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"

[Tasks]
Name: "desktopicon"; Description: "Crear un acceso directo en el escritorio"; GroupDescription: "Accesos directos:"; Flags: unchecked

[Files]
Source: "..\dist\Elite Pen\Elite Pen.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSE.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\packaging\README_PORTABLE.txt"; DestDir: "{app}"; DestName: "LEEME.txt"; Flags: ignoreversion

[Icons]
Name: "{group}\Elite Pen"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\Elite Pen"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Abrir Elite Pen"; Flags: nowait postinstall skipifsilent
