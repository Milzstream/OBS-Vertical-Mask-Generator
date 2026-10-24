#ifndef MyAppName
#define MyAppName "Source Cutout"
#endif
#ifndef MyAppVersion
#define MyAppVersion "0.5.3"
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
#define OutputName "source-cutout-0.5.3-windows-x64"
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
RestartApplications=yes
MinVersion=10.0
VersionInfoVersion={#MyAppVersion}
VersionInfoCompany={#MyAppPublisher}
VersionInfoDescription={#MyAppName} OBS plugin

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[InstallDelete]
Type: files; Name: "{pf}\obs-studio\obs-plugins\64bit\{#PluginName}.dll"
Type: files; Name: "{pf}\obs-studio\obs-plugins\64bit\{#PluginName}.pdb"
Type: files; Name: "{pf}\obs-studio\obs-plugins\64bit\onnxruntime.dll"
Type: files; Name: "{pf}\obs-studio\obs-plugins\64bit\DirectML.dll"
Type: filesandordirs; Name: "{pf}\obs-studio\data\obs-plugins\{#PluginName}"
Type: filesandordirs; Name: "{commonappdata}\obs-studio\plugins\{#PluginName}\bin"

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[UninstallDelete]
Type: filesandordirs; Name: "{app}"

[Run]
Filename: "{code:OBSLaunchPath}"; Description: "Launch OBS Studio"; Flags: nowait postinstall skipifsilent; Check: ShouldOfferOBSLaunch

[Code]
var
  OBSExePath: String;

function QueryProcessPath(const Name: String): String;
var
  Locator: Variant;
  Service: Variant;
  ResultSet: Variant;
  Item: Variant;
begin
  Result := '';
  try
    Locator := CreateOleObject('WbemScripting.SWbemLocator');
    Service := Locator.ConnectServer('.', 'root\CIMV2');
    ResultSet := Service.ExecQuery('SELECT ExecutablePath FROM Win32_Process WHERE Name="' + Name + '"');
    if ResultSet.Count > 0 then
    begin
      Item := ResultSet.ItemIndex(0);
      Result := Item.ExecutablePath;
    end;
  except
    Result := '';
  end;
end;

function OBSLaunchPath: String;
begin
  if (OBSExePath <> '') and FileExists(OBSExePath) then
    Result := OBSExePath
  else
  begin
    Result := ExpandConstant('{autopf}\obs-studio\bin\64bit\obs64.exe');
    if not FileExists(Result) then
      Result := '';
  end;
end;

function ShouldOfferOBSLaunch: Boolean;
begin
  Result := OBSLaunchPath <> '';
end;

function IsOBSRunning: Boolean;
var
  Locator: Variant;
  Service: Variant;
  ResultSet: Variant;
begin
  Result := False;
  try
    Locator := CreateOleObject('WBEMScripting.SWbemLocator');
    Service := Locator.ConnectServer('.', 'root\CIMV2');
    ResultSet := Service.ExecQuery('SELECT ProcessId FROM Win32_Process WHERE Name="obs64.exe" OR Name="obs32.exe"');
    Result := ResultSet.Count > 0;
  except
    Result := False;
  end;
end;

function InitializeSetup: Boolean;
var
  Tries: Integer;
begin
  Result := True;
  OBSExePath := QueryProcessPath('obs64.exe');
  if OBSExePath = '' then
    OBSExePath := QueryProcessPath('obs32.exe');
  Tries := 0;
  while IsOBSRunning do
  begin
    if WizardSilent then
    begin
      if Tries >= 45 then
      begin
        Result := False;
        Exit;
      end;
      Sleep(1000);
      Tries := Tries + 1;
    end
    else if MsgBox('OBS Studio is running. Close OBS, then click Retry to install Source Cutout.', mbInformation, MB_RETRYCANCEL) = IDCANCEL then
    begin
      Result := False;
      Exit;
    end;
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  ResultCode: Integer;
  Path: String;
begin
  if (CurStep <> ssPostInstall) or (not WizardSilent) then
    Exit;
  Path := OBSLaunchPath;
  if Path <> '' then
    Exec(Path, '', ExtractFilePath(Path), SW_SHOWNORMAL, ewNoWait, ResultCode);
end;
