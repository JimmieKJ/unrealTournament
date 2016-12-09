// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformCrashContext.h"

struct CORE_API FWindowsPlatformCrashContext : public FGenericCrashContext
{
	FWindowsPlatformCrashContext() {}

	FWindowsPlatformCrashContext(bool bInIsEnsure)
	{
		bIsEnsure = bInIsEnsure;
	}

	virtual void AddPlatformSpecificProperties() override;
};

typedef FWindowsPlatformCrashContext FPlatformCrashContext;
