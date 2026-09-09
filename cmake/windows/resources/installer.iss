#ifndef MyAppName
#define MyAppName "HUD Mask"
#endif
#ifndef MyAppVersion
#define MyAppVersion "0.1.1"
#endif
#ifndef MyAppPublisher
#define MyAppPublisher "Milzstream"
#endif
#ifndef MyAppURL
#define MyAppURL "https://github.com/Milzstream/OBS-Vertical-Mask-Generator"
#endif
#ifndef PluginName
#define PluginName "vertical-hud-mask"
#endif
#ifndef SourceDir
#define SourceDir "..\..\..\release\RelWithDebInfo\vertical-hud-mask"
#endif
#ifndef OutputDir
#define OutputDir "..\..\..\release"
#endif
#ifndef OutputName
#define OutputName "vertical-hud-mask-0.1.1-windows-x64"
#endif
#ifndef LicenseFile
#define LicenseFile "..\..\..\LICENSE"
#endif

[Setup]
AppId={{8F3A2C91-4B6E-4D17-9A5C-1E8B7F0D4C22}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}/releases
DefaultDirName={commonappdata}\obs-studio\plugins\{#PluginName}
DisableDirPage=yes
DisableProgramGroupPage=yes
LicenseFile={#LicenseFile}
OutputDir={#OutputDir}
OutputBaseFilename={#OutputName}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog
UninstallDisplayName={#MyAppName} (OBS Plugin)
CloseApplications=yes
CloseApplicationsFilter=obs64.exe,obs32.exe
MinVersion=10.0
VersionInfoVersion={#MyAppVersion}
VersionInfoCompany={#MyAppPublisher}
VersionInfoDescription={#MyAppName} OBS plugin

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[UninstallDelete]
Type: filesandordirs; Name: "{app}"
