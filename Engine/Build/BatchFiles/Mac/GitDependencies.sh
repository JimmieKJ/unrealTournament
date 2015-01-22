#!/bin/sh
# Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

set -e

SCRIPT_PATH=$0
if [ -h $SCRIPT_PATH ]; then
	SCRIPT_PATH=$(dirname $SCRIPT_PATH)/$(readlink $SCRIPT_PATH)
fi

cd $(dirname $SCRIPT_PATH)

if [ ! -f ../../../Binaries/DotNET/GitDependencies.exe ]; then
	echo "Cannot find GitDependencies.exe. This script should be placed in Engine/Build/BatchFiles/Mac."
	exit 1
fi 

source SetupMono.sh "`pwd`"

cd ../../../..

mono Engine/Binaries/DotNET/GitDependencies.exe "$@"
