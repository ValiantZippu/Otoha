; ============================================================================
;  Otoha — Windows installer (Inno Setup 6)
;  Build the Release binary first (see scripts/release.sh / docs/release.md),
;  then compile this script with ISCC.exe to produce Otoha-<version>-Setup.exe.
;
;  Policies implemented here (#27-#31, #41, #52):
;    * installs ONLY application files into {autopf}\Otoha — never System32
;    * user settings live in %APPDATA%\Otoha and are PRESERVED on upgrade
;    * uninstall keeps user data by default; deleting it is an explicit opt-in
;    * no drivers / virtual audio components exist in this release, so no
;      driver privileges, signing, or audio-routing cleanup applies (#31)
; ============================================================================

#define MyAppName "Otoha"
#define MyAppVersion GetEnv("OTOHA_RELEASE_VERSION")
#if MyAppVersion == ""
  #define MyAppVersion "0.0.0-local"
#endif
; M16 #40: predictable artifact name, overridable via ISCC /DSetupExeName=...
#ifndef SetupExeName
  #define SetupExeName "Otoha-{#MyAppVersion}-Windows-x64"
#endif

[Setup]
AppId={{8C1B2E44-6D5A-4B7E-9A21-OTOHA-DESKTOP}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=..\..\release
OutputBaseFilename={#SetupExeName}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\Otoha.exe
; Signing is prepared but optional: set these via environment/ISCC command line,
; never in the repository (#33).
;SignTool=signtool $f

[Files]
; Built with: cmake --build build --config Release
Source: "..\build\Release\Otoha.exe"; DestDir: "{app}"; Flags: ignoreversion signonce
Source: "THIRD-PARTY-NOTICES.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\Otoha.exe"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\Otoha.exe"; \
  Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; \
  GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
; Startup registration is a user choice at install time AND toggleable in the
; app later (#32/#38). The app manages its own registry value when toggled.
Name: "startupentry"; Description: "Start Otoha when Windows starts"; \
  Flags: unchecked

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; \
  ValueType: string; ValueName: "Otoha"; ValueData: """{app}\Otoha.exe"""; \
  Flags: uninsdeletevalue; Tasks: startupentry

[Run]
Filename: "{app}\Otoha.exe"; Description: "{cm:LaunchProgram,{#MyAppName}}"; \
  Flags: nowait postinstall skipifsilent

[UninstallDelete]
; Nothing: application files are removed via [Files]/[Icons] uninstall entries;
; %APPDATA%\Otoha (settings, profiles, custom presets) survives unless the
; user explicitly opts in below.

[Code]
var
  DeleteDataCheck: TNewCheckBox;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep <> usPostUninstall then Exit;

  // Offer (never force) removal of user data (#30/#52).
  if MsgBox('Also delete your Otoha settings, presets and device profiles?' #13#10 +
            'Choosing No keeps them for a future reinstall.',
            mbConfirmation, MB_YESNO, IDNO) = IDYES then
    DelTree(ExpandConstant('{userappdata}\Otoha'), True, True, True);
end;
