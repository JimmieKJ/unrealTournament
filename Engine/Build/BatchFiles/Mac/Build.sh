#!/bin/sh

# This script gets called every time Xcode does a build or clean operation, even though it's called "Build.sh".
# Values for $ACTION: "" = building, "clean" = cleaning

# Setup Mono
source Engine/Build/BatchFiles/Mac/SetupMono.sh Engine/Build/BatchFiles/Mac

# override env if action is specified on command line 
if [ $1 == "clean" ] 
	then 
		ACTION="clean" 
fi 


echo Building UBT... 

xbuild /property:Configuration=Development  /nologo Engine/Source/Programs/UnrealBuildTool/UnrealBuildTool.csproj| grep -i error  

case $ACTION in
	"")
		echo Building $1...

                Platform=""
                AdditionalFlags=""
		
		case $CLANG_STATIC_ANALYZER_MODE in
				"deep")
					AdditionalFlags+="-skipActionHistory"
					;;
				"shallow")
					AdditionalFlags+="-skipActionHistory"
					;;
				esac

		case $2 in 
			"iphoneos"|"IOS")
		        Platform="IOS"
				AdditionalFlags+=" -deploy -nocreatestub "
			;; 
  			"iphonesimulator")
		        	Platform="IOS"
		         	AdditionalFlags+=" -deploy -simulator -nocreatestub"
			;;
			"macosx")
					Platform="Mac"
			;;
			"HTML5")
				Platform="HTML5"
			;;
			*)
				Platform="$2"
				AdditionalFlags+=" -deploy " 
			;; 
		esac
		
		BuildTasks=$(defaults read com.apple.dt.Xcode IDEBuildOperationMaxNumberOfConcurrentCompileTasks)
		export NumUBTBuildTasks=$BuildTasks

		echo Running command : Engine/Binaries/DotNET/UnrealBuildTool.exe $1 $Platform $3 $AdditionalFlags "$4" 
		mono Engine/Binaries/DotNET/UnrealBuildTool.exe $1 $Platform $3 $AdditionalFlags "$4"
		;;
	"clean")
		echo "Cleaning $2 $3 $4..."

                Platform=""
                AdditionalFlags="-clean"

		case $3 in 
			"iphoneos"|"IOS")
	        	        Platform="IOS"
				AdditionalFlags+=" -nocreatestub"
			;;
			"iphonesimulator")
			        Platform="IOS"
		        	AdditionalFlags+=" -simulator"
				AdditionalFlags+=" -nocreatestub"
			;; 
			"macosx")
					Platform="Mac"
			;;
			"HTML5")
				Platform="HTML5"
			;;
			*) 
        			Platform="$3"
			;;
		
		esac
		echo Running command: mono Engine/Binaries/DotNET/UnrealBuildTool.exe $2 $Platform $4 $AdditionalFlags "$5"
		mono Engine/Binaries/DotNET/UnrealBuildTool.exe $2 $Platform $4 $AdditionalFlags "$5"
		;;
esac

ExitCode=$?
if [ $ExitCode -eq 254 ] || [ $ExitCode -eq 255 ]; then
	exit 0
else
	exit $ExitCode
fi
