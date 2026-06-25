; Inno Setup script for ClipStream.
; Compile from the project root:  ISCC.exe installer\ClipStream.iss
; SourceDir is set to the project root so all paths below are root-relative.

#define MyAppName "ClipStream"
#define MyAppVersion "0.2.0"
#define MyAppPublisher "Saqlain Abbas"
#define MyAppURL "https://github.com/Razee4315/clipstream"
#define MyAppExeName "ClipStream.exe"

[Setup]
AppId={{8E2C7A41-3B5D-4E0A-9C8F-CL1PSTR3AM01}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}/issues
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
; Per-user install — no admin prompt.
PrivilegesRequired=lowest
SourceDir=..
OutputDir=installer\output
OutputBaseFilename=ClipStream-Setup-{#MyAppVersion}
SetupIconFile=resources\icon.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
; Close a running instance before installing/updating.
CloseApplications=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional icons:"; Flags: unchecked
Name: "startupicon"; Description: "Start {#MyAppName} automatically at login"; GroupDescription: "Startup:"

[Files]
Source: "dist\ClipStream\*"; DestDir: "{app}"; Flags: recursesubdirs ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; \
    ValueName: "{#MyAppName}"; ValueData: """{app}\{#MyAppExeName}"""; \
    Tasks: startupicon; Flags: uninsdeletevalue

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch {#MyAppName}"; Flags: nowait postinstall skipifsilent
