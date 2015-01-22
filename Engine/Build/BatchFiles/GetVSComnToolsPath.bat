@ECHO off

SET VSComnToolsPath=
SET TmpPath=""

REM Non-express VS2013 on 64-bit machine.
FOR /f "tokens=2,*" %%A IN ('REG.exe query HKLM\Software\Wow6432Node\Microsoft\VisualStudio\%1.0 /v "InstallDir" 2^>Nul') DO (
	SET TmpPath="%%B\..\Tools"
)

REM Non-express VS2013 on 32-bit machine.
IF %TmpPath% == "" (
	FOR /f "tokens=2,*" %%A IN ('REG.exe query HKLM\Software\Microsoft\VisualStudio\%1.0 /v "InstallDir" 2^>Nul') DO (
		SET TmpPath="%%B\..\Tools"
	)
)

REM Express VS2013 on 64-bit machine.
IF %TmpPath% == "" (
	FOR /f "tokens=2,*" %%A IN ('REG.exe query HKLM\Software\Wow6432Node\Microsoft\WDExpress\%1.0 /v "InstallDir" 2^>Nul') DO (
		SET TmpPath="%%B\..\Tools"
	)
)

REM Express VS2013 on 32-bit machine.
IF %TmpPath% == "" (
	FOR /f "tokens=2,*" %%A IN ('REG.exe query HKLM\Software\Microsoft\WDExpress\%1.0 /v "InstallDir" 2^>Nul') DO (
		SET TmpPath="%%B\..\Tools"
	)
)

IF NOT %TmpPath% == "" (
	CALL :normalisePath %TmpPath%
)

GOTO :EOF

:normalisePath
SET VSComnToolsPath=%~f1
GOTO :EOF
