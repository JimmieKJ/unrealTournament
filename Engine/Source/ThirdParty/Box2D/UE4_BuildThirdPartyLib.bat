REM libPNG

pushd Box2D_v2.3.1
	REM Compiling Box2D (PC batch file)

	REM vs2015
	pushd Build\vs2015
	p4 edit %THIRD_PARTY_CHANGELIST% bin\...
	msbuild Box2D.sln /target:Clean,Box2D /p:Platform=Win32;Configuration="Release"
	msbuild Box2D.sln /target:Clean,Box2D /p:Platform=Win32;Configuration="Debug"
	msbuild Box2D.sln /target:Clean,Box2D /p:Platform=x64;Configuration="Release"
	msbuild Box2D.sln /target:Clean,Box2D /p:Platform=x64;Configuration="Debug"
	popd

	REM Box2D PC is still missing Android, Linux, HTML5, XboxOne, PS4
popd
