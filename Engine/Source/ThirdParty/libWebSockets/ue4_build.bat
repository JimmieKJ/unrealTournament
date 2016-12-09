@ECHO OFF
goto DOC
Build script for Windows.
=========================

See https://libwebsockets.org for more information about this project.

This subdirectory contains the following:

* libwebsockets
	Pre-built static libraries and include headers for linking into engine code.
* libwebsockets_source
	The source for libWebsockets version 1.7-stable.

In order to keep this directory as pristine as possible, create the temporary build direcory outside the source dir instead of as a subdirectory as suggested in libwebsockets_source/README.build.md, but refer to that document if you need to modify any build options.

This library needs the OpenSSL library already provided for some platforms in ../OpenSSL. When adding new platforms, you may have to add OpenSSL binaries for that platform first.

:DOC
SETLOCAL
rem Execute in a new cmd shell so we can use exit to exit the batch file without closing the surrounding shell
IF NOT "%wrapped%"=="%~0" (
	SET wrapped=%~0
	%ComSpec% /s /c ""%~0" %*"
	GOTO :EOF
)
SET BASE_DIR=%~dp0%
cd %BASE_DIR%

CALL :DO_BUILD Win64 2013
CALL :DO_BUILD Win64 2015
CALL :DO_BUILD Win32 2013
CALL :DO_BUILD Win32 2015
exit /b

:DO_BUILD
@ECHO ON
SET PLATFORM_DIR=%1\%2
SET BUILD_DIR=Build\%PLATFORM_DIR%

SET LIB_SUBDIR=lib\%PLATFORM_DIR%
SET INCLUDE_SUBDIR=include\%PLATFORM_DIR%
SET INSTALL_PREFIX=%BASE_DIR%\libwebsockets
SET SOURCE_PREFIX=%BASE_DIR%\libwebsockets_src
SET OPENSSL_PREFIX=%BASE_DIR%\..\OpenSSL\1.0.2g
SET OPENSSL_INCLUDE=%OPENSSL_PREFIX%\include\%1\VC%2
SET OPENSSL_LIBDIR=%OPENSSL_PREFIX%\lib\%1\VC%2
SET OPENSSL_LIBS=%OPENSSL_LIBDIR%\libeay32.lib;%OPENSSL_LIBDIR%\ssleay32.lib

mkdir %BUILD_DIR%
mkdir %INSTALL_PREFIX%\%LIB_SUBDIR%
mkdir %INSTALL_PREFIX%\%INCLUDE_SUBDIR%

cd %BUILD_DIR%

IF %2 == 2013 (
	SET "CMAKE_GENERATOR=Visual Studio 12 2013"
	SET "DEVENV=c:\Program Files (x86)\Microsoft Visual Studio 12.0\Common7\IDE\devenv.exe"
) ELSE (
	SET "CMAKE_GENERATOR=Visual Studio 14 2015"
	SET "DEVENV=c:\Program Files (x86)\Microsoft Visual Studio 14.0\Common7\IDE\devenv.exe"
)

IF %1 == Win32 (
	SET OPT_ARCH=
) ELSE (
	SET "OPT_ARCH= %1"
)

cmake ^
	-G "%CMAKE_GENERATOR%%OPT_ARCH%" ^
	-DCMAKE_INSTALL_PREFIX:PATH=%INSTALL_PREFIX% ^
	-DLWS_INSTALL_LIB_DIR:PATH=%LIB_SUBDIR% ^
	-DLWS_OPENSSL_INCLUDE_DIRS:PATH=%OPENSSL_INCLUDE% ^
	-DLWS_OPENSSL_LIBRARIES:PATH=%OPENSSL_LIBS% ^
	-DLWS_SSL_CLIENT_USE_OS_CA_CERTS:BOOL=ON ^
	-DLWS_WITHOUT_TESTAPPS:BOOL=ON ^
	-DLWS_WITH_HTTP2:BOOL=ON ^
	-DLWS_WITH_SHARED:BOOL=OFF ^
	%SOURCE_PREFIX%
CALL :CHECK_ERRORLEVEL cmake

cd %BASE_DIR%
cmake --build %BUILD_DIR% --target install --config Debug
CALL :CHECK_ERRORLEVEL "install debug"
cd %INSTALL_PREFIX%\%LIB_SUBDIR%
ren websockets_static.lib websockets_static_d.lib
cd %BASE_DIR%
cmake --build %BUILD_DIR% --target install --config Release
CALL :CHECK_ERRORLEVEL "install release"

rem lws_config.h is platform specific
cd %INSTALL_PREFIX%\include
move /y lws_config.h %PLATFORM_DIR%
CALL :CHECK_ERRORLEVEL "move /y lws_config.h %PLATFORM_DIR%"

rem Don't know how to prevent cmake from installing these
cd %INSTALL_PREFIX%
rd /s /q cmake
cd %BASE_DIR%

exit /b

:CHECK_ERRORLEVEL
IF ERRORLEVEL 1 (
	echo Error: %1 returned a non zero exit status: %errorlevel%
	cd %BASE_DIR%
	exit %errorlevel%
)
exit /b
