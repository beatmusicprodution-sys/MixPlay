#define AppVer "0.1.0"
[Setup]
AppName=Mix Play
AppVersion={#AppVer}
AppPublisher=Beat Music
DefaultDirName={autopf}\Mix Play
OutputDir=..\dist
OutputBaseFilename=MixPlay-Windows-Setup
ArchitecturesInstallIn64BitMode=x64
ArchitecturesAllowed=x64
PrivilegesRequired=admin
Compression=lzma
[Files]
Source: "..\build\MixPlay_artefacts\Release\VST3\Mix Play.vst3\*"; DestDir: "{commoncf64}\VST3\Mix Play.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\build\MixPlay_artefacts\Release\Standalone\Mix Play.exe"; DestDir: "{app}"; Flags: ignoreversion
[Icons]
Name: "{autoprograms}\Mix Play"; Filename: "{app}\Mix Play.exe"
