// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <wctype.h>
#include <limits.h>

// Set up compiler pragmas, etc
#include "HTML5/HTML5PlatformCompilerSetup.h"

// Define TEXT early
#define TEXT_PASTE(x) L ## x
#define TEXT_HELPER(a,b)	a ## b
#define TEXT(s)				TEXT_HELPER(L, s)

// map the Windows functions (that UE4 unfortunately uses be default) to normal functions
#if PLATFORM_HTML5_WIN32

#include <intrin.h>

#else

#define _alloca alloca

#endif

struct RECT
{
	int32 left;
	int32 top;
	int32 right;
	int32 bottom;
};

#define OUT
#define IN