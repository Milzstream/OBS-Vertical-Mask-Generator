#ifndef MyAppName
#define MyAppName "Source Cutout"
#endif
#ifndef MyAppVersion
#define MyAppVersion "0.7.4"
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
#define OutputName "source-cutout-0.7.4-windows-x64"
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
RestartApplications=no
MinVersion=10.0
SetupLogging=yes
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
Type: files; Name: "{app}\onnxruntime.dll"
Type: files; Name: "{app}\DirectML.dll"
Type: filesandordirs; Name: "{app}\data\models"
Type: filesandordirs; Name: "{pf}\obs-studio\data\obs-plugins\{#PluginName}"
Type: filesandordirs; Name: "{commonappdata}\obs-studio\plugins\{#PluginName}\bin"

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[UninstallDelete]
Type: filesandordirs; Name: "{app}"

[Run]
Filename: "{code:OBSLaunchPath}"; Description: "Launch OBS Studio"; Flags: shellexec nowait postinstall skipifsilent; Check: ShouldOfferOBSLaunch

[Code]
var
  OBSExePath: String;
  ClosedOBS: Boolean;

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

function OBSLaunchPath(Param: String): String;
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
  Result := OBSLaunchPath('') <> '';
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

function TaskkillExe: String;
begin
  Result := ExpandConstant('{sys}\taskkill.exe');
end;

function RunTaskkill(const Params: String): Integer;
var
  Code: Integer;
begin
  if not Exec(TaskkillExe, Params, '', SW_HIDE, ewWaitUntilTerminated, Code) then
    Code := -1;
  Log('Source Cutout: taskkill ' + Params + ' exit ' + IntToStr(Code));
  Result := Code;
end;

procedure StartTaskkill(const Params: String);
var
  Code: Integer;
begin
  if Exec(ExpandConstant('{cmd}'), '/C start "" /B "' + TaskkillExe + '" ' + Params, '', SW_HIDE, ewWaitUntilTerminated, Code) then
    Log('Source Cutout: started taskkill ' + Params)
  else
    Log('Source Cutout: could not start taskkill ' + Params);
end;

function WaitUntilOBSExits(const Milliseconds: Integer): Boolean;
var
  Remaining: Integer;
begin
  Remaining := Milliseconds;
  while IsOBSRunning and (Remaining > 0) do
  begin
    Sleep(250);
    Remaining := Remaining - 250;
  end;
  Result := not IsOBSRunning;
end;

function CloseRunningOBS: Boolean;
begin
  if not IsOBSRunning then
  begin
    Result := True;
    Exit;
  end;
  Log('Source Cutout: closing OBS');
  Sleep(2000);
  if IsOBSRunning then
  begin
    StartTaskkill('/IM obs64.exe');
    StartTaskkill('/IM obs32.exe');
    if not WaitUntilOBSExits(4000) then
    begin
      RunTaskkill('/F /IM obs64.exe');
      RunTaskkill('/F /IM obs32.exe');
      WaitUntilOBSExits(2000);
    end;
  end;
  Result := not IsOBSRunning;
  ClosedOBS := Result;
  if Result then
    Log('Source Cutout: OBS closed')
  else
    Log('Source Cutout: OBS still running');
end;

function InitializeSetup: Boolean;
begin
  Result := True;
  OBSExePath := QueryProcessPath('obs64.exe');
  if OBSExePath = '' then
    OBSExePath := QueryProcessPath('obs32.exe');
  if WizardSilent then
  begin
    if IsOBSRunning and not CloseRunningOBS then
      Result := False;
    Exit;
  end;
  while IsOBSRunning do
  begin
    if MsgBox('OBS Studio is running. Close OBS, then click Retry to install Source Cutout.', mbInformation, MB_RETRYCANCEL) = IDCANCEL then
    begin
      Result := False;
      Exit;
    end;
  end;
end;

function DeleteRunSentinels(const Dir: String): Integer;
var
  FindRec: TFindRec;
  Path: String;
begin
  Result := 0;
  if not DirExists(Dir) then
    Exit;
  if not FindFirst(Dir + '\run_*', FindRec) then
    Exit;
  try
    repeat
      if (FindRec.Attributes and FILE_ATTRIBUTE_DIRECTORY) = 0 then
      begin
        Path := Dir + '\' + FindRec.Name;
        if DeleteFile(Path) then
          Result := Result + 1
        else
          Log('Source Cutout: could not delete ' + Path);
      end;
    until not FindNext(FindRec);
  finally
    FindClose(FindRec);
  end;
end;

procedure ClearCrashSentinel(const ExePath: String);
var
  Removed: Integer;
  AppData: String;
  Root: String;
begin
  Removed := 0;
  if ExePath <> '' then
  begin
    Root := ExtractFileDir(ExtractFileDir(ExtractFileDir(ExePath)));
    Removed := Removed + DeleteRunSentinels(Root + '\config\obs-studio\.sentinel');
  end;
  AppData := GetEnv('APPDATA');
  if AppData <> '' then
    Removed := Removed + DeleteRunSentinels(AppData + '\obs-studio\.sentinel');
  if Removed > 0 then
    Log('Source Cutout: cleared crash sentinel');
end;

procedure DeinitializeSetup;
var
  Code: Integer;
  Path: String;
begin
  if (not WizardSilent) or (not ClosedOBS) then
    Exit;
  Path := OBSLaunchPath('');
  if Path = '' then
  begin
    Log('Source Cutout: OBS path unknown, not relaunching');
    Exit;
  end;
  ClearCrashSentinel(Path);
  if ShellExec('', Path, '', ExtractFileDir(Path), SW_SHOWNORMAL, ewNoWait, Code) then
    Log('Source Cutout: relaunched OBS')
  else
    Log('Source Cutout: ShellExec failed ' + IntToStr(Code));
end;
