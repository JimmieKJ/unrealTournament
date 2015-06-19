// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


// This file is included in some resource files, which issue a warning:
//
// warning RC4011: identifier truncated to 'PLATFORM_CAN_SUPPORT_EDITORONLY'
//
// due to limitations of resource compiler. The only thing needed from this file
// for resource compilation is PREPROCESSOR_TO_STRING macro at the end, so we take
// rest of code out for resource compilation.
#ifndef RC_INVOKED

#define LOCALIZED_SEEKFREE_SUFFIX	TEXT("_LOC")
#define PLAYWORLD_PACKAGE_PREFIX TEXT("UEDPIE")
#define PLAYWORLD_CONSOLE_BASE_PACKAGE_PREFIX TEXT("UED")

#ifndef WITH_EDITORONLY_DATA
	#if !PLATFORM_CAN_SUPPORT_EDITORONLY_DATA || UE_SERVER || PLATFORM_IOS
		#define WITH_EDITORONLY_DATA	0
	#else
		#define WITH_EDITORONLY_DATA	1
	#endif
#endif

/** This controls if metadata for compiled in classes is unpacked and setup at boot time. Meta data is not normally used except by the editor. **/
#define WITH_METADATA (WITH_EDITORONLY_DATA && WITH_EDITOR)

// Set up optimization control macros, now that we have both the build settings and the platform macros
#define PRAGMA_DISABLE_OPTIMIZATION		PRAGMA_DISABLE_OPTIMIZATION_ACTUAL
#if UE_BUILD_DEBUG
	#define PRAGMA_ENABLE_OPTIMIZATION  PRAGMA_DISABLE_OPTIMIZATION_ACTUAL
#else
	#define PRAGMA_ENABLE_OPTIMIZATION  PRAGMA_ENABLE_OPTIMIZATION_ACTUAL
#endif

#if UE_BUILD_DEBUG
	#define FORCEINLINE_DEBUGGABLE FORCEINLINE_DEBUGGABLE_ACTUAL
#else
	#define FORCEINLINE_DEBUGGABLE FORCEINLINE
#endif


#if STATS
	#define CLOCK_CYCLES(Timer)   {Timer -= FPlatformTime::Cycles();}
	#define UNCLOCK_CYCLES(Timer) {Timer += FPlatformTime::Cycles();}
#else
	#define CLOCK_CYCLES(Timer)
	#define UNCLOCK_CYCLES(Timer)
#endif

#define SHUTDOWN_IF_EXIT_REQUESTED
#define RETURN_IF_EXIT_REQUESTED
#define RETURN_VAL_IF_EXIT_REQUESTED(x)

#if CHECK_PUREVIRTUALS
#define PURE_VIRTUAL(func,extra) =0;
#else
#define PURE_VIRTUAL(func,extra) { LowLevelFatalError(TEXT("Pure virtual not implemented (%s)"), TEXT(#func)); extra }
#endif


// Code analysis features
#ifndef USING_CODE_ANALYSIS
	#define USING_CODE_ANALYSIS 0
#endif

#if USING_CODE_ANALYSIS
	#if !defined( CA_IN ) || !defined( CA_OUT ) || !defined( CA_READ_ONLY ) || !defined( CA_WRITE_ONLY ) || !defined( CA_VALID_POINTER ) || !defined( CA_CHECK_RETVAL ) || !defined( CA_NO_RETURN ) || !defined( CA_SUPPRESS ) || !defined( CA_ASSUME )
		#error Code analysis macros are not configured correctly for this platform
	#endif
#else
	// Just to be safe, define all of the code analysis macros to empty macros
	#define CA_IN 
	#define CA_OUT
	#define CA_READ_ONLY
	#define CA_WRITE_ONLY
	#define CA_VALID_POINTER
	#define CA_CHECK_RETVAL
	#define CA_NO_RETURN
	#define CA_SUPPRESS( WarningNumber )
	#define CA_ASSUME( Expr )
#endif

#ifndef USING_SIGNED_CONTENT
	#define USING_SIGNED_CONTENT 0
#endif

enum {INDEX_NONE	= -1				};
enum {UNICODE_BOM   = 0xfeff			};

enum EForceInit 
{
	ForceInit,
	ForceInitToZero
};
enum ENoInit {NoInit};

// Handle type to stably track users on a specific platform
typedef int32 FPlatformUserId;
const FPlatformUserId PLATFORMUSERID_NONE = INDEX_NONE;
#endif // RC_INVOKED

// helpers to turn an preprocessor token into a real string (see UBT_COMPILED_PLATFORM)
#define PREPROCESSOR_TO_STRING_INNER(x) #x
#define PREPROCESSOR_TO_STRING(x) PREPROCESSOR_TO_STRING_INNER(x)

#define PREPROCESSOR_JOIN_INNER(x, y) x##y
#define PREPROCESSOR_JOIN(x, y) PREPROCESSOR_JOIN_INNER(x, y)

// When passed to pragma message will result in clickable warning in VS
#define WARNING_LOCATION(Line) __FILE__ "(" PREPROCESSOR_TO_STRING(Line) ")"
