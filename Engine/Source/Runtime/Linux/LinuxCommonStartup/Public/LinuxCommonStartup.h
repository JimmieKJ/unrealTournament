// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"

/**
 * @brief CommonLinuxMain - executes common startup code for Linux programs/engine
 * @param argc - number of arguments in argv[]
 * @param argv - array of arguments
 * @param RealMain - the next main routine to call in chain
 * @return error code to return to the OS
 */
int CommonLinuxMain(int argc, char *argv[], int (*RealMain)(const TCHAR * CommandLine));
