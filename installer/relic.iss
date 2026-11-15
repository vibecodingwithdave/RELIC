; Windows installer for Relic, built with Inno Setup (https://jrsoftware.org/isinfo.php).
; The GitHub Actions workflow runs:  ISCC /DAppVersion=<version> installer\relic.iss
; It expects the Windows build in build\Relic_artefacts\Release (VST3 and Standalone).

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif

[Setup]
AppId={{F6AD80C4-EEB5-4516-B8A8-9EB21C06F94F}
AppName=Relic
AppVersion={#AppVersion}
AppVerName=Relic {#AppVersion}
AppPublisher=McBABE! Labs
AppPublisherURL=https://github.com/vibecodingwithdave/RELIC
AppSupportURL=https://github.com/vibecodingwithdave/RELIC/issues
AppUpdatesURL=https://github.com/vibecodingwithdave/RELIC/releases
DefaultDirName={autopf}\Relic
DefaultGroupName=Relic
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; The plugin goes in the shared VST3 folder under Program Files, which needs administrator rights.
PrivilegesRequired=admin
CloseApplications=yes
OutputDir=..\installer-out
OutputBaseFilename=Relic-Setup-{#AppVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
LicenseFile=..\LICENSE
InfoBeforeFile=welcome.txt
UninstallDisplayIcon={app}\Relic.exe

[Types]
Name: "full"; Description: "Plugin and standalone app"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "vst3"; Description: "Relic plugin (VST3), for Ardour and other music software"; Types: full custom
Name: "app"; Description: "Relic standalone app"; Types: full custom

[Files]
Source: "..\build\Relic_artefacts\Release\VST3\Relic.vst3\*"; DestDir: "{commoncf64}\VST3\Relic.vst3"; Components: vst3; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "..\build\Relic_artefacts\Release\Standalone\Relic.exe"; DestDir: "{app}"; Components: app; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\NOTICE.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Relic"; Filename: "{app}\Relic.exe"; Components: app
Name: "{group}\Uninstall Relic"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\Relic.exe"; Description: "Start Relic"; Flags: nowait postinstall skipifsilent; Components: app
