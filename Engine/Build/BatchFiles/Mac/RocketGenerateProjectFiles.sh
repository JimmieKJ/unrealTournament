#!/bin/sh

echo
echo Setting up Unreal Engine 4 project files...
echo

# this is located inside an extra 'Mac' path unlike the Windows variant.

if [ ! -d ../../../Source ]; then
 echo GenerateRocketProjectFiles ERROR: This script file does not appear to be located inside the Engine/Build/BatchFiles/Mac directory.
 exit
fi

source SetupMono.sh "`dirname "$0"`"

# pass all parameters to UBT
echo 
mono ../../../../Engine/Binaries/DotNET/UnrealBuildTool.exe -XcodeProjectFile -rocket "$@"
