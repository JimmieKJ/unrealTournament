pushd FreeType2-2.6\Builds

	REM FreeType2-2.6

	p4 edit %THIRD_PARTY_CHANGELIST% ..\lib\Win32\...
	p4 edit %THIRD_PARTY_CHANGELIST% ..\lib\Win64\...	
	p4 edit %THIRD_PARTY_CHANGELIST% ..\lib\PS4\...	
	p4 edit %THIRD_PARTY_CHANGELIST% ..\lib\Linux\...
	p4 edit %THIRD_PARTY_CHANGELIST% ..\lib\XboxOne\...

	REM vs2012
	pushd win32\vc2012
	msbuild freetype.sln /target:Clean,freetype /p:Platform=Win32;Configuration="Release"
	msbuild freetype.sln /target:Clean,freetype /p:Platform=Win64;Configuration="Release"
	popd
	
	REM vs2013
	pushd win32\vc2013
	msbuild freetype.sln /target:Clean,freetype /p:Platform=Win32;Configuration="Release"
	msbuild freetype.sln /target:Clean,freetype /p:Platform=Win64;Configuration="Release"
	popd
	
	REM PS4
	pushd PS4
	msbuild freetype.sln /target:Clean,freetype /p:Platform=ORBIS;Configuration="Debug"
	msbuild freetype.sln /target:Clean,freetype /p:Platform=ORBIS;Configuration="Release"
	popd

	rem Linux
	pushd Linux
	call crossbuild.cmd
	popd

	REM XboxOne
	pushd XboxOne
	msbuild freetype.sln /target:Clean,freetype /p:Platform=Durango;Configuration="Debug"
	msbuild freetype.sln /target:Clean,freetype /p:Platform=Durango;Configuration="Release"
	popd

	REM Missing Android, HTML5, WinRT
popd
