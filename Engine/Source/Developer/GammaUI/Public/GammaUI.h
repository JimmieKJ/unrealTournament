// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "ModuleInterface.h"
#include "SlateBasics.h"
#include "EditorStyle.h"

class FGammaUI : public IModuleInterface
{

public:

	// IModuleInterface implementation.
	virtual void StartupModule();
	virtual void ShutdownModule();

	/** @return a pointer to the Gamma UI panel; invalid pointer if one has already been allocated */
	virtual TSharedPtr< class SWidget > GetGammaUIPanel();
};
