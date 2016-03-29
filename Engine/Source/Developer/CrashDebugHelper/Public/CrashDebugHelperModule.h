// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
	CRASHDEBUGHELPER_API ICrashDebugHelper* Get();

private:
	class ICrashDebugHelper* CrashDebugHelper;
};
