// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AllowWinRTPLatformTypes.h: Defines for allowing the use of WinRT types.
=============================================================================*/

#ifndef WINRT_PLATFORM_TYPES_GUARD
	#define WINRT_PLATFORM_TYPES_GUARD
#else
	#error Nesting AllowWinRTPlatformTypes.h is not allowed!
#endif

#define INT ::INT
#define UINT ::UINT
#define DWORD ::DWORD
#define FLOAT ::FLOAT

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
