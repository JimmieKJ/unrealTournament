// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleInterface.h"


/**
 * Interface for target platform modules.
 */
class ITargetPlatformModule
	: public IModuleInterface
{
public:

	/**
	 * Gets the module's target platform.
	 *
	 * @return The target platform.
	 */
	virtual ITargetPlatform* GetTargetPlatform() = 0;

public:

	/** Virtual destructor. */
	virtual ~ITargetPlatformModule() { }
};
