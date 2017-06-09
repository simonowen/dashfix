; Inno Setup script for DashFix installer

#define MyAppName "DashFix"
#define MyAppVersion "1.1"
#define MyAppPublisher "Simon Owen"
#define MyAppURL "https://github.com/simonowen/dashfix"
#define MyAppExeName "DashFix.exe"

[Setup]
; NOTE: The value of AppId uniquely identifies this application.
; Do not use the same AppId value in installers for other applications.
; (To generate a new GUID, click Tools | Generate GUID inside the IDE.)
AppId={{E693D1E7-CF41-48CE-8E10-8A7FB6D6AE03}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={pf}\{#MyAppName}
DefaultGroupName={#MyAppName}
UninstallDisplayName={#MyAppName}
DisableProgramGroupPage=auto
InfoBeforeFile=License.txt
OutputDir=.
OutputBaseFilename=DashFixSetup_{#StringChange(MyAppVersion, '.', '')}
Compression=lzma
SolidCompression=yes
SignTool=signtool $f
SignedUninstaller=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Messages]
SetupAppTitle=Setup {#MyAppName}
SetupWindowTitle=Setup - {#MyAppName} v{#MyAppVersion}

[Files]
Source: "Release\DashFix.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "Release\inject.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "ReadMe.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "License.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{commonprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{commonprograms}\{#MyAppName} Website"; Filename: "https://github.com/simonowen/dashfix"

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "{#MyAppName}"; ValueData: """{app}\{#MyAppExeName}"" --startup"; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\SimonOwen"; Flags: uninsdeletekeyifempty
Root: HKCU; Subkey: "Software\SimonOwen\DashFix"; Flags: uninsdeletekey

[UninstallRun]
Filename: "taskkill.exe"; Parameters: "/f /im ""{#MyAppExeName}"""; Flags: runhidden

[Code]

function GetAppPid(const ExeName : string): Integer;
var
    WbemLocator: Variant;
    WbemServices: Variant;
    WbemObjectSet: Variant;
begin
    WbemLocator := CreateOleObject('WBEMScripting.SWBEMLocator');
    WbemServices := WbemLocator.ConnectServer('.', 'root\CIMV2');
    WbemObjectSet := WbemServices.ExecQuery(Format('SELECT ProcessId FROM Win32_Process Where Name="%s"',[ExeName]));
    if WbemObjectSet.Count > 0 then
      Result := WbemObjectSet.ItemIndex(0).ProcessId
    else
      Result := 0;
end;

function CloseSteamApps(const Operation: String): Boolean;
var
  Response: Integer;
begin
  Result := True

  while Result and ((GetAppPid('Steam.exe') <> 0) or (GetAppPid('vrdashboard.exe') <> 0)) do
  begin
    Response := MsgBox('Please close Steam and SteamVR to ' + Operation + ' {#MyAppName}.', mbInformation, MB_OKCANCEL);
    if Response = IDCANCEL then
      Result := False
  end;
end;

procedure CloseApplication(const ExeName: String);
var
  ResultCode: Integer;
begin
  Exec(ExpandConstant('taskkill.exe'), '/f /im ' + '"' + ExeName + '"', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
end;


function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := ''

  if GetAppPid('DashFix.exe') <> 0 then
  begin
    if not CloseSteamApps('upgrade') then
      Result := 'Steam or SteamVR are still running.'
    else
      CloseApplication('{#MyAppExeName}')
  end;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
var
  ResultCode: Integer;
begin
  Result := True;

  if (CurPageID = wpFinished) and ((not WizardForm.YesRadio.Visible) or 
    (not WizardForm.YesRadio.Checked))
  then
    ExecAsOriginalUser(ExpandConstant('{app}\{#MyAppExeName}'), '', '', 
      SW_SHOWNORMAL, ewNoWait, ResultCode);
end;

function InitializeUninstall(): Boolean;
begin
  Result := CloseSteamApps('uninstall')
end;
