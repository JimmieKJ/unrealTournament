// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlateBasics.h"
#include "EditorStyle.h"
#include "ModuleInterface.h"


/**
 * Slate toolbox module public interface
 */
class FToolboxModule : public IModuleInterface
{

public:
	virtual void StartupModule();
	virtual void ShutdownModule();
};
