@echo off

REM We don't want to copy visualizers if the key lookup fails, as it may do with a build system running as SYSTEM
reg query "HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders" /v Personal 1>nul 2>&1
if errorlevel 0 (
	for /f "tokens=2,*" %%a in ('reg query "HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders" /v Personal ^| findstr Personal') do (
		set UE4_MyDocs=%%b

		pushd %~dp0
		call GetVSComnToolsPath 12
		popd
	
		if "!VsComnToolsPath!" == "" goto NoVisualStudio2013Environment
		attrib -R "!UE4_MyDocs!\Visual Studio 2013\Visualizers\UE4.natvis" 1>nul 2>nul
		copy /Y "%~dp0..\..\Extras\VisualStudioDebugging\UE4.natvis" "!UE4_MyDocs!\Visual Studio 2013\Visualizers\UE4.natvis" 1>nul 2>nul

	:NoVisualStudio2013Environment
		pushd %~dp0
		call GetVSComnToolsPath 11
		popd

		if "!VsComnToolsPath!" == "" goto NoVisualStudio2012Environment
		attrib -R "!UE4_MyDocs!\Visual Studio 2012\Visualizers\UE4.natvis" 1>nul 2>nul
		copy /Y "%~dp0..\..\Extras\VisualStudioDebugging\UE4.natvis" "!UE4_MyDocs!\Visual Studio 2012\Visualizers\UE4.natvis" 1>nul 2>nul

	:NoVisualStudio2012Environment
		if "%SCE_ORBIS_SDK_DIR%" == "" goto NoPS4Environment
		attrib -R "!UE4_MyDocs!\SCE\orbis-debugger\PS4UE4.natvis" 1>nul 2>nul
		copy /Y "%~dp0..\..\Extras\VisualStudioDebugging\PS4UE4.natvis" "!UE4_MyDocs!\SCE\orbis-debugger\PS4UE4.natvis" 1>nul 2>nul

	:NoPS4Environment
		set UE4_MyDocs=
	)
)
