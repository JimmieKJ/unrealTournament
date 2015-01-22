// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/** States for running the automation process */
namespace EAutomationTestState
{
	enum Type
	{
		Idle,			// Automation process is not running
		FindWorkers,	// Find workers to run the tests
		RequestTests,	// Find the tests that can be run on the workers
		MonitorTests,	// Monitor the test progress to see when they are complete
		GenerateReport,	// Generate the report
		Complete		// The process is finished
	};
};


/**
 * Implements a helper class that runs the automation tests
 */
class FAutomatedTestManager	
	: public IAutomatedTestManager
{
public:

	/**
	 * Constructor.
	 *
	 * @param SessionID The session ID used to select the session in the automation controller.
	 */
	FAutomatedTestManager( const FGuid SessionID );

	/**
	 * Destructor.
	 */
	~FAutomatedTestManager();

public:

	// IAutomatedTestManager interface

	virtual bool IsTestingComplete() override;
	virtual void RunTests() override;
	virtual void Tick( float DeltaTime ) override;

protected:

	/**
	 * Finds some automation workers on the discovered game instances.
	 *
	 * @param DeltaTime Time since last update. Used to add a delay before we send the request.
	 */
	void FindWorkers( float DeltaTime );

	/**
	 * Generates the automation report, and finish the testing process.
	 */
	void GenerateReport();

	/**
	 * Monitors the testing process to see if we have finished.
	 */
	void MonitorTests();

private:

	/**
	 * Callback function from the automation controller to let us know there are tests available.
	 */
	void HandleRefreshTestCallback();

private:

	/** The automation controller running the tests */
	IAutomationControllerManagerPtr AutomationController;

	/** The automation controller module running the tests */
	IAutomationControllerModule* AutomationControllerModule;

	/** The current state of the automation process */
	EAutomationTestState::Type AutomationTestState;

	/** Delay used before finding workers on game instances. Just to ensure they have started up */
	float DelayTimer;

	/** Session ID to run the tests on */
	FGuid SessionID;
};
