// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Interface for AutomationController modules.
 */
class IAutomationControllerModule
	: public IModuleInterface
{
public:

	/**
	 * Gets the automation controller.
	 *
	 * @return a reference to the automation controller.
	 */
	virtual IAutomationControllerManagerRef GetAutomationController( ) = 0;

	/** Init message bus usage. */
	virtual void Init() = 0;

	/** Tick function that will execute enabled tests for different device clusters. */
	virtual void Tick() = 0;

	static IAutomationControllerModule& Get()
	{
		return FModuleManager::GetModuleChecked<IAutomationControllerModule>("AutomationController");
	}
};
