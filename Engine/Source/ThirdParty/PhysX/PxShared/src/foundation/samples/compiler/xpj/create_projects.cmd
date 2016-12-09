@echo off

set XPJ="../../../../../../../../BuildTools/xpj/1/win32/xpj4.exe"

set PX_SHARED=%~p0
:pxsharedloop
if exist %PX_SHARED%\src\compiler\xpj goto :havepxshared
set PX_SHARED=%PX_SHARED%..\
goto :pxsharedloop

:havepxshared

set WINDOWS_CODEGEN_DEBUG=/WX
set WINDOWS_CODEGEN_RELEASE=/WX

set ANDROID_HOME=
set ANT_HOME=
set ANDROID_NDK_ROOT=
set NDK_ROOT=
set NDKROOT=
set JAVA_HOME=

set NSIGHT_TEGRA_PROJECT_REVISION=4

set OSX_ARCH=-arch i386

@echo Creating Visual Stduio 2012 Solution and Project Files
%XPJ% -v 1 -t VC11 -p WIN32 -x TestFoundation.xpj
%XPJ% -v 1 -t VC11 -p WIN64 -x TestFoundation.xpj

@echo Creating Make files for Linux
%XPJ% -v 1 -t make -p linux32 -x TestFoundation.xpj
%XPJ% -v 1 -t make -p linux64 -x TestFoundation.xpj

@echo Creating Make files for Android-VC11 integration
%XPJ% -v 1 -t VC11 -p android -x TestFoundation.xpj

goto cleanExit

:pauseExit
pause

:cleanExit

