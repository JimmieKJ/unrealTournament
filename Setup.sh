#!/bin/bash
# Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

set -e

cd "`dirname "$0"`"

if [ ! -f UnrealTournament/Binaries/DotNET/GitDependencies.exe ]; then
	echo "GitSetup ERROR: This script does not appear to be located \
       in the root UT directory and must be run from there."
	exit 1
fi 

# Setup the git hooks
if [ -d .git/hooks ]; then
	echo "Registering git hooks... (this will override existing ones!)"
	echo \#!/bin/sh >.git/hooks/post-checkout
	echo mono UnrealTournament/Binaries/DotNET/GitDependencies.exe >>.git/hooks/post-checkout
	chmod +x .git/hooks/post-checkout

	echo \#!/bin/sh >.git/hooks/post-merge
	echo mono UnrealTournament/Binaries/DotNET/GitDependencies.exe >>.git/hooks/post-merge
	chmod +x .git/hooks/post-merge
fi

# Download the binaries
set +e
mono UnrealTournament/Binaries/DotNET/GitDependencies.exe --prompt "$@"
set -e
