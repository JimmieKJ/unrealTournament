
echo ************************* test build

echo *************** P4 settings
echo uebp_PORT=%uebp_PORT%
echo uebp_USER=%uebp_USER%
echo uebp_CLIENT=%uebp_CLIENT%
echo uebp_PASS=[redacted]
echo P4PORT=%P4PORT%
echo P4USER=%P4USER%
echo P4CLIENT=%P4CLIENT%
echo P4PASSWD=[redacted]

echo *************** builder settings
echo uebp_CL=%uebp_CL%
echo uebp_LOCAL_ROOT=%uebp_LOCAL_ROOT%
echo uebp_BuildRoot_P4=%uebp_BuildRoot_P4%
echo uebp_BuildRoot_Escaped=%uebp_BuildRoot_Escaped%
echo uebp_CLIENT_ROOT=%uebp_CLIENT_ROOT%

cd /D %uebp_LOCAL_ROOT%

"C:/Program Files (x86)/Microsoft SDKs/Windows/v7.0A/Bin/SignTool.exe" sign /a /n "Epic Games Inc." /t http://timestamp.verisign.com/scripts/timestamp.dll /q %uebp_LOCAL_ROOT%\Engine\Binaries\DotNET\AgentInterface.dll


if not ERRORLEVEL 1 GOTO Ok
echo BUILD FAILED: TestBuild.bat failed.
exit /B 1
:Ok

echo BUILD SUCCESSFUL
exit /B 0