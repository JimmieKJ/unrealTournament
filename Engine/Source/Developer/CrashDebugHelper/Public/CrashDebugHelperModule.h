// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "ModuleManager.h"
#include "CrashDebugHelper.h"

class FCrashDebugHelperModule : public IModuleInterface
{
public:
	virtual void StartupModule();
	virtual void ShutdownModule();

	/** Gets the crash debug helper singleton or returns NULL */
	CRASHDEBUGHELPER_API virtual ICrashDebugHelper* Get();

private:
	class ICrashDebugHelper* CrashDebugHelper;
};
