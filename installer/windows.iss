; Butterfly Compressor Windows installer (Inno Setup 6)
#define AppVer GetEnv("BFLY_VERSION")
#if AppVer == ""
  #define AppVer "0.1.0"
#endif

[Setup]
AppId={{B7E2A1C4-3D5F-4A8B-9C1E-2F4B6D8A0C3E}
AppName=Butterfly Compressor
AppVersion={#AppVer}
AppPublisher=iNi
DefaultDirName={commonpf64}\iNi\Butterfly Compressor
DefaultGroupName=iNi
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputDir=..\installer-out
OutputBaseFilename=ButterflyCompressor-Windows-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=Butterfly Compressor

[Types]
Name: "full"; Description: "Full installation"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plugin"; Types: full custom; Flags: fixed
Name: "standalone"; Description: "Standalone application"; Types: full

[Files]
Source: "..\build\ButterflyCompressor_artefacts\Release\VST3\Butterfly Compressor.vst3\*"; DestDir: "{commoncf64}\VST3\Butterfly Compressor.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\build\ButterflyCompressor_artefacts\Release\Standalone\Butterfly Compressor.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\MANUAL.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Butterfly Compressor"; Filename: "{app}\Butterfly Compressor.exe"; Components: standalone
Name: "{group}\Uninstall Butterfly Compressor"; Filename: "{uninstallexe}"

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\Butterfly Compressor.vst3"
