// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CrashDebugHelper.h"

class CRASHDEBUGHELPER_API FCrashDebugHelperModule : public IModuleInterface
{
public:
	virtual void StartupModule();
	virtual void ShutdownModule();

	/** Gets the crash debug helper singleton or returns NULL */
	virtual ICrashDebugHelper* Get();

private:
	class ICrashDebugHelper* CrashDebugHelper;
};
