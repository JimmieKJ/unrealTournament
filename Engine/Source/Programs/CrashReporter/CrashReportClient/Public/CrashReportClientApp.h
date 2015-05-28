// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#define CRASH_REPORT_UNATTENDED_ONLY			PLATFORM_LINUX

// Pre-compiled header includes
#include "Core.h"
#if !CRASH_REPORT_UNATTENDED_ONLY
	#include "SlateBasics.h"
	#include "StandaloneRenderer.h"
#endif // CRASH_REPORT_UNATTENDED_ONLY

DECLARE_LOG_CATEGORY_EXTERN(CrashReportClientLog, Log, All)

// Helper macros
#define STRINGIZE_IMPL(STR) #STR
#define STRINGIZE(STR) STRINGIZE_IMPL(STR)

#define FILE_LINE_STRING TEXT(__FILE__) TEXT("(") TEXT(STRINGIZE(__LINE__)) TEXT(")")

/** This writes and error to the log rather than calling 'check': don't want the crash reporter crashing */
void CrashReportClientCheck(bool bCondition, const TCHAR* Location);
#define CRASHREPORTCLIENT_CHECK(COND) CrashReportClientCheck(COND, FILE_LINE_STRING)

/**
 * Run the crash report client app
 */
void RunCrashReportClient(const TCHAR* Commandline);
