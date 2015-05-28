// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"


/** Whether we should generate crash reports even if the debugger is attached. */
extern CORE_API bool GAlwaysReportCrash;

/** Whether to use ClientReportClient rather than AutoReporter. */
extern CORE_API bool GUseCrashReportClient;

extern CORE_API TCHAR MiniDumpFilenameW[1024];

// @TODO yrx 2014-09-11 Move to PlatformExceptionHandling
#if PLATFORM_WINDOWS
// @TODO yrx 2014-10-09 These methods are specific to windows, remove from here.
extern CORE_API int32 ReportCrash( LPEXCEPTION_POINTERS ExceptionInfo );
extern CORE_API void NewReportEnsure( const TCHAR* ErrorMessage );
#elif PLATFORM_XBOXONE
// @TODO yrx 2014-10-09 Should be move to another file
extern CORE_API int32 ReportCrash( int ExceptionCode, LPEXCEPTION_POINTERS ExceptionInfo );
extern CORE_API void NewReportEnsure( const TCHAR* ErrorMessage );
#elif PLATFORM_MAC
// @TODO yrx 2014-10-09 Should be move to another file
extern CORE_API int32 ReportCrash( ucontext_t *Context, int32 Signal, struct __siginfo* Info );
extern CORE_API void NewReportEnsure( const TCHAR* ErrorMessage );
#elif PLATFORM_LINUX
extern CORE_API int32 ReportCrash( ucontext_t *Context, int32 Signal, siginfo_t* Info );
extern CORE_API void NewReportEnsure( const TCHAR* ErrorMessage );
#endif
