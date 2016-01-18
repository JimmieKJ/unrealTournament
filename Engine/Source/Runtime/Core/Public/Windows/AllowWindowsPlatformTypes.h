// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#ifndef WINDOWS_PLATFORM_TYPES_GUARD
	#define WINDOWS_PLATFORM_TYPES_GUARD
#else
	#error Nesting AllowWindowsPLatformTypes.h is not allowed!
#endif

#pragma warning( push )
#pragma warning( disable : 4459 )

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
