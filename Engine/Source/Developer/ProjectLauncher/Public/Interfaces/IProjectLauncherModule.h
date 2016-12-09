// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

/**
 * Interface for launcher UI modules.
 */
class IProjectLauncherModule
	: public IModuleInterface
{
public:

	/**
	 * Virtual destructor.
	 */
	virtual ~IProjectLauncherModule( ) { }
};
