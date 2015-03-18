; asttapi.iss
; install and setup the basics for asterisk TAPI driver


[Setup]
AppName=bv-tapi
AppVerName=bv-tapi 0.4
DefaultDirName={pf}\babblevoice
DefaultGroupName=babblevoice
Compression=lzma
SolidCompression=yes
PrivilegesRequired=admin
LicenseFile=license.rtf
OutputBaseFilename=bv-tapi
ArchitecturesAllowed=x86

[Files]
Source: "..\release\bv-tapi.tsp"; DestDir: "{sys}"; Flags: restartreplace
Source: "..\libs\sofia-sip-1.12.11\win32\pthread\pthreadVC2.dll"; DestDir: "{sys}"; Flags: restartreplace promptifolder
Source: "..\bv_tapi.xml"; DestDir: "{sys}"; Flags: promptifolder
Source: "vcredist_x86\vcredist_x86.exe"; DestDir: "{app}"; Flags: promptifolder
Source: "..\astloader\release\bv-loader.exe"; DestDir: "{app}"

[Icons]
Name: "{group}\Install the TAPI driver"; Filename: "{app}\bv-loader.exe"
Name: "{group}\Remove the TAPI driver"; Filename: "{app}\bv-loader.exe"; Parameters: "/remove"

[Run]
Filename: "{app}\bv-loader.exe"; Description: "Install The TAPI driver"; Flags: postinstall
Filename: "{app}\vcredist_x86.exe"; Description: "MSVC redist"; StatusMsg: "Installing MS VC++ components..."; Parameters: "/passive /norestart"

[UninstallRun]
Filename: "{app}\bv-loader.exe"; Parameters: "/remove"

[Registry]

