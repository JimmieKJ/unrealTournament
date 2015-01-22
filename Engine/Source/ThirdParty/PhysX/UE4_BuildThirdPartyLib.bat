REM PhysX
setlocal

REM PhysX root is always next to UE4, since that's how p4 is structured, and we know that we are always in UE4\Engine\Source\ThirdParty\PhysX
REM This batch file wouldn't work otherwise
set PhysXRoot=..\NotForLicensees\PhysX

REM Go to the branch
pushd %PhysXRoot%
call UE4_BuildThirdPartyLib.bat %THIRD_PARTY_CHANGELIST% -submit -config debug -config profile -platformPhysx vc11win32;Win32 -platformPhysx vc11win64;x64 -platformPhysx vc12win32;Win32 -platformPhysx vc12win64;x64 -platformPhysx vc10ps4;ORBIS -platformAPEX vc11win32-PhysX_3.3;Win32 -platformAPEX vc11win64-PhysX_3.3;x64 -platformAPEX vc12win32-PhysX_3.3;Win32 -platformAPEX vc12win64-PhysX_3.3;x64 -platformAPEX vc12ps4-PhysX_3.3;ORBIS
call UE4_BuildThirdPartyLib.bat %THIRD_PARTY_CHANGELIST% -submit -config debug -config profile -2012 -platformPhysx vc11xboxone;Durango -platformAPEX vc11xboxone-PhysX_3.3;Durango
popd
