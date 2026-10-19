[Setup]
AppId={{39ED6B1E-F72E-4E5B-80E4-647F742C7DB3}
AppName=RAMD Docs
AppVersion=1.0.0
AppPublisher=RAMD Studios
AppPublisherURL=https://ramd-store.rf.gd/
DefaultDirName={autopf}\RAMD Docs
DefaultGroupName=RAMD Docs
OutputDir=..\release-windows
OutputBaseFilename=ramd-docs-1.0.0-windows-x64-setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\ramd-docs.exe
CloseApplications=yes
[Files]
Source: "..\windows-dist\*"; DestDir: "{app}"; Excludes: "ui-check.exe"; Flags: ignoreversion recursesubdirs createallsubdirs
[Icons]
Name: "{group}\RAMD Docs"; Filename: "{app}\ramd-docs.exe"
Name: "{autodesktop}\RAMD Docs"; Filename: "{app}\ramd-docs.exe"; Tasks: desktopicon
[Tasks]
Name: desktopicon; Description: "Create a desktop shortcut"; Flags: unchecked
[Run]
Filename: "{app}\ramd-docs.exe"; Description: "Open RAMD Docs"; Flags: nowait postinstall skipifsilent
[Code]
function InitializeSetup(): Boolean;
var Code: Integer;
begin
  Result := FileExists(ExpandConstant('{pf64}\LibreOffice\program\mergedlo.dll')) or FileExists(ExpandConstant('{pf64}\LibreOffice\program\sofficeapp.dll'));
  if not Result then begin
    if MsgBox('RAMD Docs requires the 64-bit LibreOffice document engine. Install LibreOffice in its default location, then run this installer again. Open the official download page?', mbInformation, MB_YESNO) = IDYES then
      ShellExec('open', 'https://www.libreoffice.org/download/download-libreoffice/', '', '', SW_SHOWNORMAL, ewNoWait, Code);
  end;
end;
