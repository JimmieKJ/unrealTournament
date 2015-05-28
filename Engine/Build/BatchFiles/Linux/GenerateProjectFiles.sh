#!/bin/bash

SCRIPT_DIR=$(cd "$(dirname "$BASH_SOURCE")" ; pwd)

set -e

TOP_DIR=$(cd $SCRIPT_DIR/../../.. ; pwd)
cd ${TOP_DIR}

if [ ! -e Build/OneTimeSetupPerformed ]; then
	echo
	echo "******************************************************"
	echo "You have not run Setup.sh, the build will likely fail."
	echo "******************************************************"
fi

echo
echo Setting up Unreal Engine 4 project files...
echo

set -x
xbuild Source/Programs/UnrealBuildTool/UnrealBuildTool.csproj \
  /verbosity:quiet /nologo \
  /p:TargetFrameworkVersion=v4.0 \
  /p:Configuration="Development"

xbuild Source/Programs/AutomationTool/AutomationTool.csproj \
  /verbosity:quiet /nologo \
  /tv:4.0 \
  /p:TargetFrameworkVersion=v4.0 \
  /p:Platform="AnyCPU" \
  /p:Configuration="Development"

xbuild Source/Programs/AutomationTool/Scripts/AutomationScripts.Automation.csproj \
  /verbosity:quiet /nologo \
  /tv:4.0 \
  /p:TargetFrameworkVersion=v4.0 \
  /p:Configuration="Development"

xbuild Source/Programs/AutomationTool/Linux/Linux.Automation.csproj \
  /verbosity:quiet /nologo \
  /tv:4.0 \
  /p:TargetFrameworkVersion=v4.0 \
  /p:Configuration="Development"

xbuild Source/Programs/AutomationTool/Android/Android.Automation.csproj \
  /verbosity:quiet /nologo \
  /tv:4.0 \
  /p:TargetFrameworkVersion=v4.0 \
  /p:Configuration="Development"

xbuild Source/Programs/AutomationTool/HTML5/HTML5.Automation.csproj \
  /verbosity:quiet /nologo \
  /tv:4.0 \
  /p:TargetFrameworkVersion=v4.0 \
  /p:Configuration="Development"

# pass all parameters to UBT
mono Binaries/DotNET/UnrealBuildTool.exe -makefile -kdevelopfile -qmakefile -cmakefile "$@"
set +x
