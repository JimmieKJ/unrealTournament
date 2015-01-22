// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#if !CRASH_REPORT_UNATTENDED_ONLY

#include "SlateBasics.h"
#include "SlateStyle.h"

/** Slate styles for the crash report client app */
class FCrashReportClientStyle
{
public:
	/**
	 * Set up specific styles for the crash report client app
	 */
	static void Initialize();

	/**
	 * Tidy up on shut-down
	 */
	static void Shutdown();

	/*
	 * Access to singleton style object
	 */ 
	static const ISlateStyle& Get();

private:
	static TSharedRef< FSlateStyleSet > Create();

	/** Singleton style object */
	static TSharedPtr<FSlateStyleSet> StyleSet;
};

#endif // !CRASH_REPORT_UNATTENDED_ONLY
