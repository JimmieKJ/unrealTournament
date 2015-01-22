// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LauncherAutomatedServicePrivatePCH.h"


FAutomatedTestManager::FAutomatedTestManager( FGuid InSessionID ) :
	SessionID( InSessionID )
{
	// Load the automation controller
	AutomationControllerModule = &FModuleManager::LoadModuleChecked<IAutomationControllerModule>( "AutomationController" );
	AutomationController = AutomationControllerModule->GetAutomationController();

	AutomationController->Init();

	AutomationTestState = EAutomationTestState::Idle;
}


FAutomatedTestManager::~FAutomatedTestManager()
{
	AutomationControllerModule->ShutdownModule();
}


bool FAutomatedTestManager::IsTestingComplete()
{
	// Check if we are in the completed state 
	return AutomationTestState == EAutomationTestState::Complete;
}


void FAutomatedTestManager::RunTests(  )
{
	// Register for the callback that tells us there are tests available
	AutomationController->OnTestsRefreshed().BindRaw(this, &FAutomatedTestManager::HandleRefreshTestCallback);

	// Set state to FindWorkers to kick off the testing process
	AutomationTestState = EAutomationTestState::FindWorkers;
	DelayTimer = 0.0f;
}


void FAutomatedTestManager::Tick( float DeltaTime )
{
	// Update the automation controller to keep it running
	AutomationController->Tick();

	// Update the automation process
	switch( AutomationTestState )
	{
	case EAutomationTestState::FindWorkers :
		{
			FindWorkers( DeltaTime );
			break;
		}
	case EAutomationTestState::MonitorTests :
		{
			MonitorTests();
			break;
		}
	case EAutomationTestState::GenerateReport :
		{
			GenerateReport();
			break;
		}
	case EAutomationTestState::Complete :
	case EAutomationTestState::Idle:
	default:
		{
			// Do nothing while we are waiting for something
			break;
		}
	}
}


void FAutomatedTestManager::HandleRefreshTestCallback()
{
	// We have found some workers
	// Create a filter to add to the automation controller, otherwise we don't get any reports
	AutomationController->SetFilter( MakeShareable( new AutomationFilterCollection() ) );

	// Start the tests
	AutomationController->RunTests();

	// Set state to monitoring to check for test completion
	AutomationTestState = EAutomationTestState::MonitorTests;
}


void FAutomatedTestManager::FindWorkers( float DeltaTime )
{
	// Time to delay requesting workers - ensure they are up and running
	static const float DelayTime = 5.0f;

	DelayTimer += DeltaTime;

	if ( DelayTimer > DelayTime )
	{
		// Request the workers
		AutomationController->RequestAvailableWorkers( SessionID );
		AutomationTestState = EAutomationTestState::Idle;
	}
}


void FAutomatedTestManager::MonitorTests()
{
	if ( AutomationController->CheckTestResultsAvailable() )
	{
		// We have finished the testing, and results are available
		AutomationTestState = EAutomationTestState::GenerateReport;
	}
}


void FAutomatedTestManager::GenerateReport()
{
	// Filter the report to get all available reports
	uint32 FileExportTypeMask = 0;
	EFileExportType::SetFlag( FileExportTypeMask, EFileExportType::FET_All );

	// Generate the report
	AutomationController->ExportReport( FileExportTypeMask );

	// Set state to complete
	AutomationTestState = EAutomationTestState::Complete;
}