REM libPNG
pushd libPNG-1.5.2\projects

	p4 edit %THIRD_PARTY_CHANGELIST% ..\pnglibconf.h
	p4 edit %THIRD_PARTY_CHANGELIST% ..\lib\...

	REM vs2013
	pushd vstudio12
	msbuild vstudio12.sln /target:Clean,libpng /p:Platform=Win32;Configuration="Release Library"
	msbuild vstudio12.sln /target:Clean,libpng /p:Platform=Win32;Configuration="Debug Library"
	msbuild vstudio12.sln /target:Clean,libpng /p:Platform=x64;Configuration="Release Library"
	msbuild vstudio12.sln /target:Clean,libpng /p:Platform=x64;Configuration="Debug Library"
	popd

	REM vs2015
	pushd vstudio14
	msbuild vstudio14.sln /target:Clean,libpng /p:Platform=Win32;Configuration="Release Library"
	msbuild vstudio14.sln /target:Clean,libpng /p:Platform=Win32;Configuration="Debug Library"
	msbuild vstudio14.sln /target:Clean,libpng /p:Platform=x64;Configuration="Release Library"
	msbuild vstudio14.sln /target:Clean,libpng /p:Platform=x64;Configuration="Debug Library"
	popd
	
	REM WinRT
	pushd WinRT
	msbuild WinRT.sln /target:Clean,libpng /p:Platform=x64;Configuration=Release
	msbuild WinRT.sln /target:Clean,libpng /p:Platform=x64;Configuration=Debug
	popd

	REM XboxOne
	pushd XboxOne
	msbuild libpng_XboxOne.sln /target:Clean,libpng /p:Platform=Durango;Configuration=Release
	msbuild libpng_XboxOne.sln /target:Clean,libpng /p:Platform=Durango;Configuration=Debug
	popd

	REM PS4
	pushd PS4
	msbuild libpng.sln /target:Clean,libpng /p:Platform=ORBIS;Configuration=Release
	msbuild libpng.sln /target:Clean,libpng /p:Platform=ORBIS;Configuration=Debug
	popd

	REM Missing Android, Linux, HTML5
popd

