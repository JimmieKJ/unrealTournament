Set WShell = CreateObject("WScript.Shell")
UeSdksRoot = ""
UeSdksRoot = WShell.ExpandEnvironmentStrings("%UE_SDKS_ROOT%")
rem WScript.StdOut.WriteLine(UeSdksRoot)

rem Trip whitespace
Trim(UeSdksRoot)

rem trim the trailing "\" if it's there
rem WScript.StdOut.WriteLine(Right(UeSdksRoot, 1))
If Right(UeSdksRoot, 1) = "\" Then
	UeSdksRoot=Left(UeSdksRoot, Len(UeSdksRoot)-1)
End If

CurrentlyInstalledFilename=UeSdksRoot + "\HostWin64\Linux_x64\CurrentlyInstalled.txt"
rem WScript.StdOut.WriteLine(CurrentlyInstalledFilename)

Set CurrentlyInstalledFile = CreateObject("Scripting.FileSystemObject").OpenTextFile(CurrentlyInstalledFilename,1)
CurrentlyInstalled = CurrentlyInstalledFile.ReadLine()
CurrentlyInstalledFile.Close()

CurrentlyInstalled=Replace(CurrentlyInstalled, vbCrLf, "")
rem WScript.StdOut.WriteLine(CurrentlyInstalled)

CurrentToolchainPath=UeSdksRoot + "\HostWin64\Linux_x64\" + CurrentlyInstalled + "\toolchain\"
rem WScript.StdOut.WriteLine(CurrentToolchainPath)

CC_String="CC=" + CurrentToolchainPath + "bin\clang++.exe"
CPP_String="CPP=" + CurrentToolchainPath + "bin\clang++.exe"
AR_RC_String="AR_RC=" + CurrentToolchainPath + "bin\x86_64-unknown-linux-gnu-ar.exe rc"
RANLIB_String="RANLIB=" + CurrentToolchainPath + "bin\x86_64-unknown-linux-gnu-ranlib.exe"
CROSS_SYSROOT_String="CROSS_SYSROOT=--sysroot=" + CurrentToolchainPath

WScript.StdOut.WriteLine(CC_String)
WScript.StdOut.WriteLine(CPP_String)
WScript.StdOut.WriteLine(AR_RC_String)
WScript.StdOut.WriteLine(RANLIB_String)
WScript.StdOut.WriteLine(CROSS_SYSROOT_String)
