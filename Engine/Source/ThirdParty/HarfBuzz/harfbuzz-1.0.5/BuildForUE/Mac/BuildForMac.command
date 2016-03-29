#!/bin/sh
# Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

CUR_PATH=$(dirname "$0")
PATH_TO_CMAKE_FILE="$CUR_PATH/../"

# Temporary build directories (used as working directories when running CMake)
MAKE_PATH="$CUR_PATH/../../Mac/Build"

rm -rf $MAKE_PATH
mkdir -p $MAKE_PATH
cd $MAKE_PATH
echo "Generating HarfBuzz makefile..."
cmake -G "Unix Makefiles" $PATH_TO_CMAKE_FILE
echo "Building HarfBuzz..."
make harfbuzz
cd $PATH_TO_CMAKE_FILE
rm -rf $MAKE_PATH
