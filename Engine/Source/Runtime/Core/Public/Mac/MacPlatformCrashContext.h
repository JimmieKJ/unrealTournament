// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Apple/ApplePlatformCrashContext.h"

struct CORE_API FMacCrashContext : public FApplePlatformCrashContext
{
	/** Mimics Windows WER format */
	void GenerateWindowsErrorReport(char const* WERPath) const;

	/** Creates (fake so far) minidump */
	void GenerateMinidump(char const* Path) const;

	/** Generates information for crash reporter */
	void GenerateCrashInfoAndLaunchReporter() const;
};

typedef FMacCrashContext FPlatformCrashContext;
