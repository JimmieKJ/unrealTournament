// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*================================================================================
	WinRTPlatform.h: Setup for the windows platform
==================================================================================*/

#pragma once

/** Define the WinRT platform to be the active one **/
#define PLATFORM_WINRT					1
#define PLATFORM_WINRT_ARM				1

/**
* Windows specific types
**/
struct FWinRTTypes : public FGenericPlatformTypes
{
	// defined in windefs.h, even though this is equivalent, the compiler doesn't think so
	typedef unsigned long	DWORD;
	typedef unsigned long	SIZE_T;
	typedef long			SSIZE_T;
};

typedef FWinRTTypes FPlatformTypes;

// Base defines, must define these for the platform, there are no defaults
#define PLATFORM_DESKTOP				0
#define PLATFORM_64BITS					0
#define PLATFORM_CAN_SUPPORT_EDITORONLY_DATA	0

// Base defines, defaults are commented out

#define PLATFORM_LITTLE_ENDIAN								1
#define PLATFORM_SUPPORTS_PRAGMA_PACK						1
//#define PLATFORM_ENABLE_VECTORINTRINSICS					0
#define PLATFORM_USE_LS_SPEC_FOR_WIDECHAR					0
#define PLATFORM_COMPILER_DISTINGUISHES_DWORD_AND_UINT		1
#define PLATFORM_COMPILER_HAS_GENERIC_KEYWORD				1
#define PLATFORM_HAS_BSD_TIME								0
#define PLATFORM_USE_PTHREADS								0
#define PLATFORM_MAX_FILEPATH_LENGTH						MAX_PATH
//@todo.WinRT: Texture streaming support...
#define PLATFORM_SUPPORTS_TEXTURE_STREAMING					0
#define PLATFORM_REQUIRES_FILESERVER						1
#define PLATFORM_SUPPORTS_MULTITHREADED_GC					0
#define PLATFORM_HAS_BSD_SOCKET_FEATURE_WINSOCKETS			1

// Function type macros.
#define VARARGS			__cdecl					/* Functions with variable arguments */
#define CDECL			__cdecl					/* Standard C function */
#define STDCALL			__stdcall				/* Standard calling convention */
#define FORCEINLINE		__forceinline			/* Force code to be inline */
#define FORCENOINLINE	__declspec(noinline)	/* Force code to NOT be inline */

// Hints compiler that expression is true; generally restricted to comparisons against constants
#define ASSUME(expr)	__assume(expr)

#define DECLARE_UINT64(x)	x

// Optimization macros (uses __pragma to enable inside a #define).
#define PRAGMA_DISABLE_OPTIMIZATION_ACTUAL __pragma(optimize("",off))
#define PRAGMA_ENABLE_OPTIMIZATION_ACTUAL  __pragma(optimize("",on))

// Backwater of the spec. All compilers support this except microsoft, and they will soon
#define TYPENAME_OUTSIDE_TEMPLATE

#pragma warning(disable : 4481) // nonstandard extension used: override specifier 'override'
#define ABSTRACT abstract

// Strings.
#define LINE_TERMINATOR		TEXT("\r\n")
#define LINE_TERMINATOR_ANSI "\r\n"

// Alignment.
//#define MS_ALIGN(n)
#define MS_ALIGN(n) __declspec(align(n))
//#define MS_ALIGN(n)		__align(n)
//#define GCC_PACK(n)		__attribute__ ((packed,aligned(n)))
//#define GCC_ALIGN(n)	__attribute__ ((aligned(n)))

// Pragmas
#define MSVC_PRAGMA(Pragma) __pragma(Pragma)

// DLL export and import definitions
#define DLLEXPORT __declspec(dllexport)
#define DLLIMPORT __declspec(dllimport)

// disable this now as it is annoying for generic platform implementations
#pragma warning(disable : 4100) // unreferenced formal parameter
