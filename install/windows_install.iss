#define MyAppExeName MyAppName + ".exe"
#define MyAppAssocName MyAppName + " File"
#define MyAppAssocExt ".lisp"
#define MyAppAssocExt2 ".lsp"
#define MyAppAssocKey StringChange(MyAppAssocName, " ", "") + "File"

[Setup]
AppId={{EA965AB6-E6B2-49A5-9C05-807FE5783FC9}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
DefaultDirName={autopf}\{#MyAppName}
UninstallDisplayIcon={app}\{#MyAppExeName}
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
ChangesAssociations=yes
DisableProgramGroupPage=yes
OutputDir={#OutputDir}
OutputBaseFilename={#MyAppName}_{#MyAppVersion}
SetupIconFile={#IconPath}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
DisableDirPage=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"

[Registry]
; Регистрация расширений (только если выбрана задача fileassoc)
Root: HKCR; Subkey: "Software\Classes\{#MyAppAssocExt}"; ValueType: string; ValueName: ""; ValueData: "{#MyAppAssocKey}"; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKCR; Subkey: "Software\Classes\{#MyAppAssocExt2}"; ValueType: string; ValueName: ""; ValueData: "{#MyAppAssocKey}"; Flags: uninsdeletevalue; Tasks: fileassoc

Root: HKCR; Subkey: "Software\Classes\{#MyAppAssocKey}"; ValueType: string; ValueName: ""; ValueData: "{#MyAppAssocName}"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKCR; Subkey: "Software\Classes\{#MyAppAssocKey}\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKCR; Subkey: "Software\Classes\{#MyAppAssocKey}\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

Root: HKCR; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".lisp"; ValueData: ""; Tasks: fileassoc
Root: HKCR; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".lsp"; ValueData: ""; Tasks: fileassoc

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; Flags: unchecked
Name: "fileassoc"; Description: "{cm:FileAssocDescription}"; Flags: checkedonce

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[CustomMessages]
english.FileAssocDescription=Associate .lisp/.lsp files with {#MyAppName}
russian.FileAssocDescription=Ассоциировать файлы .lisp/.lsp с {#MyAppName}

english.FileAssocGroup=File associations:
russian.FileAssocGroup=Ассоциации файлов:
