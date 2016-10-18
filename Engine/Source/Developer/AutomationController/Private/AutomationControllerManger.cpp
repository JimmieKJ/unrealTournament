// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AutomationControllerPrivatePCH.h"

#if WITH_EDITOR
#include "MessageLog.h"
#endif

namespace AutomationControllerConstants
{
	const FString HistoryConfigSectionName = TEXT("AutomationController.History");
}

void FAutomationControllerManager::RequestAvailableWorkers(const FGuid& SessionId)
{
	//invalidate previous tests
	++ExecutionCount;
	DeviceClusterManager.Reset();

	ControllerResetDelegate.Broadcast();

	// Don't allow reports to be exported
	bTestResultsAvailable = false;

	//store off active session ID to reject messages that come in from different sessions
	ActiveSessionId = SessionId;

	//TODO AUTOMATION - include change list, game, etc, or remove when launcher is integrated
	int32 ChangelistNumber = 10000;
	FString ProcessName = TEXT("instance_name");

	MessageEndpoint->Publish(new FAutomationWorkerFindWorkers(ChangelistNumber, FApp::GetGameName(), ProcessName, SessionId), EMessageScope::Network);

	// Reset the check test timers
	LastTimeUpdateTicked = FPlatformTime::Seconds();
	CheckTestTimer = 0.f;
}

void FAutomationControllerManager::RequestTests()
{
	//invalidate incoming results
	ExecutionCount++;
	//reset the number of responses we have received
	RefreshTestResponses = 0;

	ReportManager.Empty();

	for (int32 ClusterIndex = 0; ClusterIndex < DeviceClusterManager.GetNumClusters(); ++ClusterIndex)
	{
		int32 DevicesInCluster = DeviceClusterManager.GetNumDevicesInCluster(ClusterIndex);
		if (DevicesInCluster > 0)
		{
			FMessageAddress MessageAddress = DeviceClusterManager.GetDeviceMessageAddress(ClusterIndex, 0);

			ResetIntermediateTestData();

			//issue tests on appropriate platforms
			MessageEndpoint->Send(new FAutomationWorkerRequestTests(bDeveloperDirectoryIncluded, RequestedTestFlags), MessageAddress);
		}
	}
}


void FAutomationControllerManager::RunTests( const bool bInIsLocalSession )
{
	ExecutionCount++;
	CurrentTestPass = 0;
	ReportManager.SetCurrentTestPass(CurrentTestPass);
	ClusterDistributionMask = 0;
	bTestResultsAvailable = false;
	TestRunningArray.Empty();
	bIsLocalSession = bInIsLocalSession;

	// Reset the check test timers
	LastTimeUpdateTicked = FPlatformTime::Seconds();
	CheckTestTimer = 0.f;

#if WITH_EDITOR
	FMessageLog AutomationTestingLog("AutomationTestingLog");
	FString NewPageName = FString::Printf(TEXT("-----Test Run %d----"), ExecutionCount);
	FText NewPageNameText = FText::FromString(*NewPageName);
	AutomationTestingLog.Open();
	AutomationTestingLog.NewPage(NewPageNameText);
	AutomationTestingLog.Info(NewPageNameText);
#endif
	//reset all tests
	ReportManager.ResetForExecution(NumTestPasses);

	for (int32 ClusterIndex = 0; ClusterIndex < DeviceClusterManager.GetNumClusters(); ++ClusterIndex)
	{
		//enable each device cluster
		ClusterDistributionMask |= (1<<ClusterIndex);

		//for each device in this cluster
		for (int32 DeviceIndex = 0; DeviceIndex < DeviceClusterManager.GetNumDevicesInCluster(ClusterIndex); ++DeviceIndex)
		{
			//mark the device as idle
			DeviceClusterManager.SetTest(ClusterIndex, DeviceIndex, NULL);

			// Send command to reset tests (delete local files, etc)
			FMessageAddress MessageAddress = DeviceClusterManager.GetDeviceMessageAddress(ClusterIndex, DeviceIndex);
			MessageEndpoint->Send(new FAutomationWorkerResetTests(), MessageAddress);
		}
	}

	// Inform the UI we are running tests
	if (ClusterDistributionMask != 0)
	{
		SetControllerStatus( EAutomationControllerModuleState::Running );
	}
}


void FAutomationControllerManager::StopTests()
{
	bTestResultsAvailable = false;
	ClusterDistributionMask = 0;

	ReportManager.StopRunningTests();

	// Inform the UI we have stopped running tests
	if (DeviceClusterManager.HasActiveDevice())
	{
		SetControllerStatus( EAutomationControllerModuleState::Ready );
	}
	else
	{
		SetControllerStatus( EAutomationControllerModuleState::Disabled );
	}

	TestRunningArray.Empty();
}


void FAutomationControllerManager::Init()
{
	AutomationTestState = EAutomationControllerModuleState::Disabled;
	bTestResultsAvailable = false;
	bScreenshotsEnabled = true;
	bRequestFullScreenScreenshots = false;
	bSendAnalytics = FParse::Param(FCommandLine::Get(), TEXT("SendAutomationAnalytics"));

	// Update the ini with the settings
	bTrackHistory = false;
	GConfig->GetBool(*AutomationControllerConstants::HistoryConfigSectionName, TEXT("bTrackHistory"), bTrackHistory, GEngineIni);

	// Default num of items to track
	NumberOfHistoryItemsTracked = 5;
	GConfig->GetInt(*AutomationControllerConstants::HistoryConfigSectionName, TEXT("NumberOfHistoryItemsTracked"), NumberOfHistoryItemsTracked, GEngineIni);
}


void FAutomationControllerManager::RequestLoadAsset( const FString& InAssetName )
{
	MessageEndpoint->Publish(new FAssetEditorRequestOpenAsset(InAssetName), EMessageScope::Process);
}


void FAutomationControllerManager::Tick()
{
	ProcessAvailableTasks();
}


void FAutomationControllerManager::ProcessAvailableTasks()
{
	// Distribute tasks
	if( ClusterDistributionMask != 0)
	{
		// For each device cluster
		for( int32 ClusterIndex = 0; ClusterIndex < DeviceClusterManager.GetNumClusters(); ++ClusterIndex )
		{
			bool bAllTestsComplete = true;

			// If any of the devices were valid
			if( ( ClusterDistributionMask & ( 1<< ClusterIndex ) ) && DeviceClusterManager.GetNumDevicesInCluster( ClusterIndex ) > 0 )
			{
				ExecuteNextTask( ClusterIndex, bAllTestsComplete );
			}

			//if we're all done running our tests
			if ( bAllTestsComplete )
			{
				//we don't need to test this cluster anymore
				ClusterDistributionMask &= ~( 1<<ClusterIndex );

				if ( ClusterDistributionMask == 0 )
				{
					ProcessResults();
					//Notify the graphical layout we are done processing results.
					TestsCompleteDelegate.Broadcast();
				}
			}
		}
	}

	if (bIsLocalSession == false)
	{
		// Update the test status for timeouts if this is not a local session
		UpdateTests();
	}
}


void FAutomationControllerManager::ExecuteNextTask( int32 ClusterIndex, OUT bool& bAllTestsCompleted )
{
	bool bTestThatRequiresMultiplePraticipantsHadEnoughParticipants = false;
	TArray< IAutomationReportPtr > TestsRunThisPass;

	// For each device in this cluster
	int32 NumDevicesInCluster = DeviceClusterManager.GetNumDevicesInCluster( ClusterIndex );
	for( int32 DeviceIndex = 0; DeviceIndex < NumDevicesInCluster; ++DeviceIndex )
	{
		// If this device is idle
		if( !DeviceClusterManager.GetTest( ClusterIndex, DeviceIndex ).IsValid() && DeviceClusterManager.DeviceEnabled( ClusterIndex, DeviceIndex ) )
		{
			// Get the next test that should be worked on
			TSharedPtr< IAutomationReport > NextTest = ReportManager.GetNextReportToExecute( bAllTestsCompleted, ClusterIndex,CurrentTestPass, NumDevicesInCluster );
			if( NextTest.IsValid() )
			{
				// Get the status of the test
				EAutomationState::Type TestState = NextTest->GetState( ClusterIndex,CurrentTestPass );
				if( TestState == EAutomationState::NotRun )
				{
					// Reserve this device for the test
					DeviceClusterManager.SetTest( ClusterIndex, DeviceIndex, NextTest );
					TestsRunThisPass.Add( NextTest );

					// If we now have enough devices reserved for the test, run it!
					TArray<FMessageAddress> DeviceAddresses = DeviceClusterManager.GetDevicesReservedForTest( ClusterIndex, NextTest );
					if( DeviceAddresses.Num() == NextTest->GetNumParticipantsRequired() )
					{
						// Send it to each device
						for (int32 AddressIndex = 0; AddressIndex < DeviceAddresses.Num(); ++AddressIndex)
						{
							FAutomationTestResults TestResults;

							GLog->Logf(ELogVerbosity::Display, TEXT("Running Automation: '%s' (Class Name: '%s')"), *TestsRunThisPass[AddressIndex]->GetDisplayName(), *TestsRunThisPass[AddressIndex]->GetCommand());
							TestResults.State = EAutomationState::InProcess;

							TestResults.GameInstance = DeviceClusterManager.GetClusterDeviceName( ClusterIndex, DeviceIndex );
							NextTest->SetResults( ClusterIndex,CurrentTestPass, TestResults );
							NextTest->ResetNetworkCommandResponses();

							// Mark the device as busy
							FMessageAddress DeviceAddress = DeviceAddresses[AddressIndex];

							// Send the test to the device for execution!
							MessageEndpoint->Send(new FAutomationWorkerRunTests(ExecutionCount, AddressIndex, NextTest->GetCommand(), NextTest->GetDisplayName(), bScreenshotsEnabled, bRequestFullScreenScreenshots, bSendAnalytics), DeviceAddress);

							// Add a test so we can check later if the device is still active
							TestRunningArray.Add( FTestRunningInfo( DeviceAddress ) );
						}
					}
				}
			}
		}
		else
		{
			// At least one device is still working
			bAllTestsCompleted = false;
		}
	}

	// Ensure any tests we have attempted to run on this pass had enough participants to successfully run.
	for( int32 TestIndex = 0; TestIndex < TestsRunThisPass.Num(); TestIndex++ )
	{
		IAutomationReportPtr CurrentTest = TestsRunThisPass[ TestIndex ];

		if( CurrentTest->GetNumDevicesRunningTest() != CurrentTest->GetNumParticipantsRequired() )
		{
			if( GetNumDevicesInCluster( ClusterIndex ) < CurrentTest->GetNumParticipantsRequired()  )
			{
				float EmptyDuration = 0.0f;
				TArray<FString> EmptyStringArray;
				TArray<FString> AutomationsWarnings;
				AutomationsWarnings.Add( FString::Printf( TEXT( "Needed %d devices to participate, Only had %d available." ), CurrentTest->GetNumParticipantsRequired(), DeviceClusterManager.GetNumDevicesInCluster( ClusterIndex ) ) );

				FAutomationTestResults TestResults;
				TestResults.State = EAutomationState::NotEnoughParticipants;
				TestResults.GameInstance = DeviceClusterManager.GetClusterDeviceName(ClusterIndex, 0);
				TestResults.Warnings.Append( AutomationsWarnings );

				CurrentTest->SetResults( ClusterIndex,CurrentTestPass, TestResults );
				DeviceClusterManager.ResetAllDevicesRunningTest( ClusterIndex, CurrentTest );
			}
		}
	}

	//Check to see if we finished a pass
	if( bAllTestsCompleted && CurrentTestPass < NumTestPasses - 1 )
	{
		CurrentTestPass++;
		ReportManager.SetCurrentTestPass(CurrentTestPass);
		bAllTestsCompleted = false;
	}
}


void FAutomationControllerManager::Startup()
{
	MessageEndpoint = FMessageEndpoint::Builder("FAutomationControllerModule")
		.Handling<FAutomationWorkerFindWorkersResponse>(this, &FAutomationControllerManager::HandleFindWorkersResponseMessage)
		.Handling<FAutomationWorkerPong>(this, &FAutomationControllerManager::HandlePongMessage)
		.Handling<FAutomationWorkerRequestNextNetworkCommand>(this, &FAutomationControllerManager::HandleRequestNextNetworkCommandMessage)
		.Handling<FAutomationWorkerRequestTestsReply>(this, &FAutomationControllerManager::HandleRequestTestsReplyMessage)
		.Handling<FAutomationWorkerRequestTestsReplyComplete>(this, &FAutomationControllerManager::HandleRequestTestsReplyCompleteMessage)
		.Handling<FAutomationWorkerRunTestsReply>(this, &FAutomationControllerManager::HandleRunTestsReplyMessage)
		.Handling<FAutomationWorkerScreenImage>(this, &FAutomationControllerManager::HandleReceivedScreenShot)
		.Handling<FAutomationWorkerWorkerOffline>(this, &FAutomationControllerManager::HandleWorkerOfflineMessage);

	if (MessageEndpoint.IsValid())
	{
		MessageEndpoint->Subscribe<FAutomationWorkerWorkerOffline>();
	}

	ClusterDistributionMask = 0;
	ExecutionCount = 0;
	bDeveloperDirectoryIncluded = false;
	RequestedTestFlags = EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::PerfFilter;

	NumOfTestsToReceive = 0;
	NumTestPasses = 1;

	//Default to machine name
	DeviceGroupFlags = 0;
	ToggleDeviceGroupFlag(EAutomationDeviceGroupTypes::MachineName);
}


void FAutomationControllerManager::Shutdown()
{
	MessageEndpoint.Reset();
	ShutdownDelegate.Broadcast();
	RemoveCallbacks();
}


void FAutomationControllerManager::RemoveCallbacks()
{
	ShutdownDelegate.Clear();
	TestsAvailableDelegate.Clear();
	TestsRefreshedDelegate.Clear();
	TestsCompleteDelegate.Clear();
}


void FAutomationControllerManager::SetTestNames( const FMessageAddress& AutomationWorkerAddress )
{
	int32 DeviceClusterIndex = INDEX_NONE;
	int32 DeviceIndex = INDEX_NONE;

	// Find the device that requested these tests
	if( DeviceClusterManager.FindDevice( AutomationWorkerAddress, DeviceClusterIndex, DeviceIndex ) )
	{
		// Sort tests by display name
		struct FCompareAutomationTestInfo
		{
			FORCEINLINE bool operator()(const FAutomationTestInfo& A,const FAutomationTestInfo& B) const
			{
				return A.GetDisplayName() < B.GetDisplayName();
			}
		};

		TestInfo.Sort(FCompareAutomationTestInfo());

		// Add each test to the collection
		for( int32 TestIndex = 0; TestIndex < TestInfo.Num(); ++TestIndex )
		{
			// Ensure our test exists. If not, add it
			ReportManager.EnsureReportExists( TestInfo[TestIndex], DeviceClusterIndex, NumTestPasses );
		}

		// Clear any intermediate data we had associated with the tests whilst building the full list of tests
		ResetIntermediateTestData();
	}
	else
	{
		//todo automation - make sure to report error if the device was not discovered correctly
	}

	// Note the response
	RefreshTestResponses++;

	// If we have received all the responses we expect to
	if (RefreshTestResponses == DeviceClusterManager.GetNumClusters())
	{
		TestsRefreshedDelegate.Broadcast();

		// Update the tests with tracking details
		ReportManager.TrackHistory(bTrackHistory, NumberOfHistoryItemsTracked);
	}
}


void FAutomationControllerManager::ProcessResults()
{
	bHasErrors = false;
	bHasWarning = false;
	bHasLogs = false;

	TArray< TSharedPtr< IAutomationReport > >& TestReports = GetReports();

	if (TestReports.Num())
	{
		bTestResultsAvailable = true;

		for (int32 Index = 0; Index < TestReports.Num(); Index++)
		{
			CheckChildResult(TestReports[Index ]);
		}
	}

	SetControllerStatus( EAutomationControllerModuleState::Ready );
}


void FAutomationControllerManager::CheckChildResult(TSharedPtr<IAutomationReport> InReport)
{
	TArray<TSharedPtr<IAutomationReport> >& ChildReports = InReport->GetChildReports();

	if (ChildReports.Num() > 0)
	{
		for (int32 Index = 0; Index < ChildReports.Num(); Index++)
		{
			CheckChildResult(ChildReports[Index]);
		}
	}
	else if ((bHasErrors && bHasWarning && bHasLogs ) == false && InReport->IsEnabled())
	{
		for (int32 ClusterIndex = 0; ClusterIndex < GetNumDeviceClusters(); ++ClusterIndex)
		{
			FAutomationTestResults TestResults = InReport->GetResults( ClusterIndex,CurrentTestPass );

			if (TestResults.Errors.Num())
			{
				bHasErrors = true;
			}
			if (TestResults.Warnings.Num())
			{
				bHasWarning = true;
			}
			if (TestResults.Logs.Num())
			{
				bHasLogs = true;
			}
		}
	}
}


void FAutomationControllerManager::SetControllerStatus( EAutomationControllerModuleState::Type InAutomationTestState )
{
	if (InAutomationTestState != AutomationTestState)
	{
		// Inform the UI if the test state has changed
		AutomationTestState = InAutomationTestState;
		TestsAvailableDelegate.Broadcast(AutomationTestState);
	}
}


void FAutomationControllerManager::RemoveTestRunning( const FMessageAddress& TestAddressToRemove )
{
	for ( int32 Index = 0; Index < TestRunningArray.Num(); Index++ )
	{
		if ( TestRunningArray[ Index ].OwnerMessageAddress == TestAddressToRemove )
		{
			TestRunningArray.RemoveAt( Index );
			break;
		}
	}
}


void FAutomationControllerManager::AddPingResult( const FMessageAddress& ResponderAddress )
{
	for ( int32 Index = 0; Index < TestRunningArray.Num(); Index++ )
	{
		if ( TestRunningArray[ Index ].OwnerMessageAddress == ResponderAddress )
		{
			TestRunningArray[ Index ].LastPingTime = 0;
			break;
		}
	}
}


void FAutomationControllerManager::UpdateTests( )
{
	static const float CheckTestInterval = 1.0f;
	static const float GameInstanceLostTimer = 200.0f;

	CheckTestTimer += FPlatformTime::Seconds() - LastTimeUpdateTicked;
	LastTimeUpdateTicked = FPlatformTime::Seconds();
	if ( CheckTestTimer > CheckTestInterval )
	{
		for ( int32 Index = 0; Index < TestRunningArray.Num(); Index++ )
		{
			TestRunningArray[ Index ].LastPingTime += CheckTestTimer;

			if ( TestRunningArray[ Index ].LastPingTime > GameInstanceLostTimer )
			{
				// Find the game session instance info
				int32 ClusterIndex;
				int32 DeviceIndex;
				verify( DeviceClusterManager.FindDevice( TestRunningArray[ Index ].OwnerMessageAddress, ClusterIndex, DeviceIndex ) );
				//verify this device thought it was busy
				TSharedPtr <IAutomationReport> Report = DeviceClusterManager.GetTest(ClusterIndex, DeviceIndex);
				check (Report.IsValid());
				// A dummy array used to report the result

				TArray<FString> EmptyStringArray;
				TArray<FString> ErrorStringArray;
				ErrorStringArray.Add( FString( TEXT( "Failed" ) ) );
				bHasErrors = true;
				GLog->Logf(ELogVerbosity::Display, TEXT("Timeout hit. Nooooooo."));

				FAutomationTestResults TestResults;
				TestResults.State = EAutomationState::Fail;
				TestResults.GameInstance = DeviceClusterManager.GetClusterDeviceName(ClusterIndex, DeviceIndex);

				// Set the results
				Report->SetResults(ClusterIndex,CurrentTestPass, TestResults );
				bTestResultsAvailable = true;

				// Disable the device in the cluster so it is not used again
				DeviceClusterManager.DisableDevice( ClusterIndex, DeviceIndex );

				// Remove the running test
				TestRunningArray.RemoveAt( Index-- );

				// If there are no more devices, set the module state to disabled
				if ( DeviceClusterManager.HasActiveDevice() == false )
				{
					GLog->Logf(ELogVerbosity::Display, TEXT("Module disabled"));
					SetControllerStatus( EAutomationControllerModuleState::Disabled );
					ClusterDistributionMask = 0;
				}
				else
				{
					GLog->Logf(ELogVerbosity::Display, TEXT("Module not disabled. Keep looking."));
					// Remove the cluster from the mask if there are no active devices left
					if ( DeviceClusterManager.GetNumActiveDevicesInCluster( ClusterIndex ) == 0 )
					{
						ClusterDistributionMask &= ~( 1<<ClusterIndex );
					}
					if ( TestRunningArray.Num() == 0  )
					{
						SetControllerStatus( EAutomationControllerModuleState::Ready );
					}
				}
			}
			else
			{
				MessageEndpoint->Send(new FAutomationWorkerPing(), TestRunningArray[Index].OwnerMessageAddress);
			}
		}
		CheckTestTimer = 0.f;
	}
}


const bool FAutomationControllerManager::ExportReport( uint32 FileExportTypeMask )
{
	return ReportManager.ExportReport( FileExportTypeMask, GetNumDeviceClusters() );
}


bool FAutomationControllerManager::IsTestRunnable( IAutomationReportPtr InReport ) const
{
	bool bIsRunnable = false;

	for( int32 ClusterIndex = 0; ClusterIndex < GetNumDeviceClusters(); ++ClusterIndex )
	{
		if( InReport->IsSupported( ClusterIndex ) )
		{
			if( GetNumDevicesInCluster( ClusterIndex ) >= InReport->GetNumParticipantsRequired() )
			{
				bIsRunnable = true;
				break;
			}
		}
	}

	return bIsRunnable;
}

/* FAutomationControllerModule callbacks
 *****************************************************************************/

void FAutomationControllerManager::HandleFindWorkersResponseMessage( const FAutomationWorkerFindWorkersResponse& Message, const IMessageContextRef& Context )
{
	if (Message.SessionId == ActiveSessionId)
	{
		DeviceClusterManager.AddDeviceFromMessage(Context->GetSender(), Message, DeviceGroupFlags);
	}

	RequestTests();

	SetControllerStatus( EAutomationControllerModuleState::Ready );
}


void FAutomationControllerManager::HandlePongMessage( const FAutomationWorkerPong& Message, const IMessageContextRef& Context )
{
	AddPingResult(Context->GetSender());
}

void FAutomationControllerManager::HandleReceivedScreenShot( const FAutomationWorkerScreenImage& Message, const IMessageContextRef& Context )
{
	// Forward the screen shot on to listeners
	FAutomationWorkerScreenImage* ImageMessage = new FAutomationWorkerScreenImage( Message );
	MessageEndpoint->Publish(ImageMessage, EMessageScope::Network);

	// Save the screen shot locally (assuming that the controller manager is running on PC/Mac)
	const bool bTree = true;
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Message.ScreenShotName), bTree);
	FFileHelper::SaveArrayToFile(Message.ScreenImage, *Message.ScreenShotName);
}

void FAutomationControllerManager::HandleRequestNextNetworkCommandMessage( const FAutomationWorkerRequestNextNetworkCommand& Message, const IMessageContextRef& Context )
{
	// Harvest iteration of running the tests this result came from (stops stale results from being committed to subsequent runs)
	if (Message.ExecutionCount == ExecutionCount)
	{
		// Find the device id for the address
		int32 ClusterIndex;
		int32 DeviceIndex;

		verify(DeviceClusterManager.FindDevice(Context->GetSender(), ClusterIndex, DeviceIndex));

		// Verify this device thought it was busy
		TSharedPtr <IAutomationReport> Report = DeviceClusterManager.GetTest(ClusterIndex, DeviceIndex);
		check (Report.IsValid());

		// Increment network command responses
		bool bAllResponsesReceived = Report->IncrementNetworkCommandResponses();

		// Test if we've accumulated all responses AND this was the result for the round of test running AND we're still running tests
		if (bAllResponsesReceived && (ClusterDistributionMask & (1<<ClusterIndex))) 
		{
			// Reset the counter
			Report->ResetNetworkCommandResponses();

			// For every device in this networked test
			TArray<FMessageAddress> DeviceAddresses = DeviceClusterManager.GetDevicesReservedForTest(ClusterIndex, Report);
			check (DeviceAddresses.Num() == Report->GetNumParticipantsRequired());

			// Send it to each device
			for (int32 AddressIndex = 0; AddressIndex < DeviceAddresses.Num(); ++AddressIndex)
			{
				//send "next command message" to worker
				MessageEndpoint->Send(new FAutomationWorkerNextNetworkCommandReply(), DeviceAddresses[AddressIndex]);
			}
		}
	}
}


void FAutomationControllerManager::HandleRequestTestsReplyMessage(const FAutomationWorkerRequestTestsReply& Message, const IMessageContextRef& Context)
{
	FAutomationTestInfo NewTest(Message.TestInfo);
	TestInfo.Add(NewTest);
}


void FAutomationControllerManager::HandleRequestTestsReplyCompleteMessage(const FAutomationWorkerRequestTestsReplyComplete& Message, const IMessageContextRef& Context)
{
	SetTestNames(Context->GetSender());
}


void FAutomationControllerManager::HandleRunTestsReplyMessage( const FAutomationWorkerRunTestsReply& Message, const IMessageContextRef& Context )
{
	// If we should commit these results
	if (Message.ExecutionCount == ExecutionCount)
	{
		FAutomationTestResults TestResults;

		TestResults.State = Message.Success ? EAutomationState::Success : EAutomationState::Fail;
		TestResults.Duration = Message.Duration;

		// Mark device as back on the market
		int32 ClusterIndex;
		int32 DeviceIndex;

		verify(DeviceClusterManager.FindDevice(Context->GetSender(), ClusterIndex, DeviceIndex));

		TestResults.GameInstance = DeviceClusterManager.GetClusterDeviceName(ClusterIndex, DeviceIndex);
		for ( auto& Error : Message.Errors )
		{
			TestResults.Errors.Add(Error.ToAutomationEvent());
		}
		TestResults.Logs = Message.Logs;
		TestResults.Warnings = Message.Warnings;

		// Verify this device thought it was busy
		TSharedPtr <IAutomationReport> Report = DeviceClusterManager.GetTest(ClusterIndex, DeviceIndex);
		check(Report.IsValid());

		Report->SetResults(ClusterIndex,CurrentTestPass, TestResults);

#if WITH_EDITOR
		FMessageLog AutomationTestingLog("AutomationTestingLog");
		AutomationTestingLog.Open();
#endif

		for ( TArray<FAutomationEvent>::TConstIterator ErrorIter(TestResults.Errors); ErrorIter; ++ErrorIter )
		{
			// 	FAutomationTestFramework::GetInstance().LogTestMessage(**ErrorIter, ELogVerbosity::Error);
			GLog->Logf(ELogVerbosity::Error, TEXT("%s"), *( *ErrorIter ).ToString());
#if WITH_EDITOR
			AutomationTestingLog.Error(FText::FromString(( *ErrorIter ).ToString()));
#endif
		}
		for (TArray<FString>::TConstIterator WarningIter(Message.Warnings); WarningIter; ++WarningIter)
		{
			GLog->Logf(ELogVerbosity::Warning, TEXT("%s"), **WarningIter);
#if WITH_EDITOR
			AutomationTestingLog.Warning(FText::FromString(*WarningIter));
#endif
		}
		for (TArray<FString>::TConstIterator LogItemIter(Message.Logs); LogItemIter; ++LogItemIter)
		{
			GLog->Logf(ELogVerbosity::Log, TEXT("%s"), **LogItemIter);
#if WITH_EDITOR
			AutomationTestingLog.Info(FText::FromString(*LogItemIter));
#endif
		}

		if (TestResults.State == EAutomationState::Success)
		{
			FString SuccessString = FString::Printf(TEXT("...Automation Test Succeeded (%s)"), *Report->GetDisplayName());
			GLog->Logf(ELogVerbosity::Log, *SuccessString);
#if WITH_EDITOR
			AutomationTestingLog.Info(FText::FromString(*SuccessString));
#endif
		}
		else
		{
			FString FailureString = FString::Printf(TEXT("...Automation Test Failed (%s)"), *Report->GetDisplayName());
			GLog->Logf(ELogVerbosity::Log, *FailureString);
#if WITH_EDITOR
			AutomationTestingLog.Error(FText::FromString(*FailureString));
#endif
			//FAutomationTestFramework::GetInstance().Lo
		}

		// const bool TestSucceeded = (TestResults.State == EAutomationState::Success);
		//FAutomationTestFramework::GetInstance().LogEndTestMessage(Report->GetDisplayName(), TestSucceeded);

		// Device is now good to go
		DeviceClusterManager.SetTest(ClusterIndex, DeviceIndex, NULL);
	}

	// Remove the running test
	RemoveTestRunning(Context->GetSender());
}


void FAutomationControllerManager::HandleWorkerOfflineMessage( const FAutomationWorkerWorkerOffline& Message, const IMessageContextRef& Context )
{
	FMessageAddress DeviceMessageAddress = Context->GetSender();
	DeviceClusterManager.Remove(DeviceMessageAddress);
}

bool FAutomationControllerManager::IsDeviceGroupFlagSet( EAutomationDeviceGroupTypes::Type InDeviceGroup ) const
{
	const uint32 FlagMask = 1 << InDeviceGroup;
	return (DeviceGroupFlags & FlagMask) > 0;
}

void FAutomationControllerManager::ToggleDeviceGroupFlag( EAutomationDeviceGroupTypes::Type InDeviceGroup )
{
	const uint32 FlagMask = 1 << InDeviceGroup;
	DeviceGroupFlags = DeviceGroupFlags ^ FlagMask;
}

void FAutomationControllerManager::UpdateDeviceGroups( )
{
	DeviceClusterManager.ReGroupDevices( DeviceGroupFlags );

	// Update the reports in case the number of clusters changed
	int32 NumOfClusters = DeviceClusterManager.GetNumClusters();
	ReportManager.ClustersUpdated(NumOfClusters);
}

void FAutomationControllerManager::TrackReportHistory(const bool bShouldTrack, const int32 NumReportsToTrack)
{
	bTrackHistory = bShouldTrack;
	NumberOfHistoryItemsTracked = NumReportsToTrack;

	// Update the ini with the settings
	GConfig->SetBool(*AutomationControllerConstants::HistoryConfigSectionName, TEXT("bTrackHistory"), bTrackHistory, GEngineIni);
	GConfig->SetInt(*AutomationControllerConstants::HistoryConfigSectionName, TEXT("NumberOfHistoryItemsTracked"), NumberOfHistoryItemsTracked, GEngineIni);

	ReportManager.TrackHistory(bTrackHistory, NumberOfHistoryItemsTracked);
}

const bool FAutomationControllerManager::IsTrackingHistory() const
{
	return bTrackHistory;
}

const int32 FAutomationControllerManager::GetNumberHistoryItemsTracking() const
{
	return NumberOfHistoryItemsTracked;
}