// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IAutomationControllerModule.h"

/**
 * Implements the AutomationController module.
 */
class FAutomationControllerModule
	: public IAutomationControllerModule
{
public:

	// IAutomationControllerModule interface

	virtual IAutomationControllerManagerRef GetAutomationController( ) override;
	virtual void Init() override;
	virtual void Tick() override;

public:

	// IModuleInterface interface

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	virtual bool SupportsDynamicReloading() override;

private:

	/** Holds the automation controller singleton. */
	static IAutomationControllerManagerPtr AutomationControllerSingleton;
};
