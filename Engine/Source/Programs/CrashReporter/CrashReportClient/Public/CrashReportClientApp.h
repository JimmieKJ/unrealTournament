// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#define CRASH_REPORT_UNATTENDED_ONLY			PLATFORM_LINUX

// Pre-compiled header includes
#include "Core.h"
#if !CRASH_REPORT_UNATTENDED_ONLY
	#include "SlateBasics.h"
	#include "StandaloneRenderer.h"
#endif // CRASH_REPORT_UNATTENDED_ONLY

#include "CrashReportClientConfig.h"

DECLARE_LOG_CATEGORY_EXTERN(CrashReportClientLog, Log, All)

/**
 * Run the crash report client app
 */
void RunCrashReportClient(const TCHAR* Commandline);
