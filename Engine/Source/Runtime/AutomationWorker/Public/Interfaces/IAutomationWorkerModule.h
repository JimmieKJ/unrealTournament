// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Interface for AutomationWorker modules.
 */
class IAutomationWorkerModule
	: public IModuleInterface
{
public:
	/** Called whenever a test is stopped */
	DECLARE_DELEGATE_ThreeParams(FStopTestEvent, bool, FString, class FAutomationTestExecutionInfo const &);

	/**
	 * Ticks the automation worker module.
	 */
	virtual void Tick( ) = 0;
	/**
	 * Start a test
	 * @param	InTestToRun			Name of the test that should be run
	 * @param	InRoleIndex			Identifier for which worker in this group that should execute a command
	 * @param	InStopTestEvent		Delegate to fire when the command is finished
	 */
	virtual void RunTest(const FString& InTestToRun, const int32 InRoleIndex, FStopTestEvent const& InStopTestEvent) = 0;


protected:

	IAutomationWorkerModule() { }
};
