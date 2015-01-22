// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "ModuleInterface.h"
#include "SlateBasics.h"


/**
 * ModuleUI module interface
 */
class IModuleUIInterface
	: public IModuleInterface
{

public:

	/** @return a pointer to the module UI widget; invalid pointer if one has already been allocated */
	virtual TSharedRef< SWidget > GetModuleUIWidget() = 0;

private:

};
