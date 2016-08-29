// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "ModuleManager.h"


DEFINE_LOG_CATEGORY_STATIC(LogAutomationTest, Warning, All);

FString FAutomationTestInfo::GetTestAsString() const
{
	return FString::Printf(TEXT("%s,%s,%i,%d,%s,%s,%i,%s,%s"), *DisplayName, *TestName, TestFlags, NumParticipantsRequired, *TestParameter, *SourceFile, SourceFileLine, *AssetPath, *OpenCommand);
}

void FAutomationTestInfo::ParseStringInfo(const FString& InTestInfo)
{
	//Split New Test Name into string array
	TArray<FString> Pieces;
	InTestInfo.ParseIntoArray(Pieces, TEXT(","), false);

	// We should always have at least 3 parameters
	check(Pieces.Num() >= 3);

	DisplayName = Pieces[0];
	TestName = Pieces[1];
	TestFlags = uint8(FCString::Atoi(*Pieces[2]));

	NumParticipantsRequired = FCString::Atoi(*Pieces[3]);

	// Optional Parameters
	if ( Pieces.Num() >= 5 )
	{
		TestParameter = Pieces[4];
	}

	if ( Pieces.Num() >= 6 )
	{
		SourceFile = Pieces[5];
	}

	if ( Pieces.Num() >= 7 )
	{
		SourceFileLine = FCString::Atoi(*Pieces[6]);
	}

	if ( Pieces.Num() >= 8 )
	{
		AssetPath = Pieces[7];
	}

	if ( Pieces.Num() >= 9 )
	{
		OpenCommand = Pieces[8];
	}
}


void FAutomationTestFramework::FAutomationTestFeedbackContext::Serialize( const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category )
{
	if (FAutomationTestFramework::GetInstance().CachedContext)
	{
		FAutomationTestFramework::GetInstance().CachedContext->Serialize(V, Verbosity, Category);
	}
	//ignore 
	if (!IsRunningCommandlet() && (Verbosity == ELogVerbosity::SetColor))
	{
		return;
	}
	// Ensure there's a valid unit test associated with the context
	if ( CurTest )
	{
		// Warnings
		if ( Verbosity == ELogVerbosity::Warning )
		{
			// If warnings should be treated as errors, log the warnings as such in the current unit test
			if ( TreatWarningsAsErrors )
			{
				CurTest->AddError(FString(V), 2);
			}
			else
			{
				CurTest->AddWarning( FString( V ) );
			}
		}
		// Errors
		else if ( Verbosity == ELogVerbosity::Error )
		{
			CurTest->AddError(FString(V), 2);
		}
		// Log items
		else
		{
			// IMPORTANT NOTE: This code will never be called in a build with NO_LOGGING defined, which means pretty much
			// any Test or Shipping config build.  If you're trying to use the automation test framework for performance
			// data capture in a Test config, you'll want to call the AddAnalyticsItemToCurrentTest() function instead of
			// using this log interception stuff.

			FString LogString = FString(V);
			FString AnalyticsString = TEXT("AUTOMATIONANALYTICS");
			if (LogString.StartsWith(*AnalyticsString))
			{
				//Remove "analytics" from the string
				LogString = LogString.Right(LogString.Len() - (AnalyticsString.Len() + 1));

				CurTest->AddAnalyticsItem(LogString);
			}
			else
			{
				CurTest->AddLogItem(LogString);
			}
		}
	}
}


FAutomationTestFramework& FAutomationTestFramework::GetInstance()
{
	static FAutomationTestFramework Framework;
	return Framework;
}


bool FAutomationTestFramework::RegisterAutomationTest( const FString& InTestNameToRegister, class FAutomationTestBase* InTestToRegister )
{
	const bool bAlreadyRegistered = AutomationTestClassNameToInstanceMap.Contains( InTestNameToRegister );
	if ( !bAlreadyRegistered )
	{
		AutomationTestClassNameToInstanceMap.Add( InTestNameToRegister, InTestToRegister );
	}
	return !bAlreadyRegistered;
}


bool FAutomationTestFramework::UnregisterAutomationTest( const FString& InTestNameToUnregister )
{
	const bool bRegistered = AutomationTestClassNameToInstanceMap.Contains( InTestNameToUnregister );
	if ( bRegistered )
	{
		AutomationTestClassNameToInstanceMap.Remove( InTestNameToUnregister );
	}
	return bRegistered;
}


void FAutomationTestFramework::EnqueueLatentCommand(TSharedPtr<IAutomationLatentCommand> NewCommand)
{
	//ensure latent commands are never used within smoke tests - will only catch when smokes are exclusively requested
	check((RequestedTestFilter & EAutomationTestFlags::FilterMask) != EAutomationTestFlags::SmokeFilter);

	//ensure we are currently "running a test"
	check(GIsAutomationTesting);

	LatentCommands.Enqueue(NewCommand);
}


/**
 * Enqueues a network command for execution in accordance with this workers role
 *
 * @param NewCommand - The new command to enqueue for network execution
 */
void FAutomationTestFramework::EnqueueNetworkCommand(TSharedPtr<IAutomationNetworkCommand> NewCommand)
{
	//ensure latent commands are never used within smoke tests
	check((RequestedTestFilter & EAutomationTestFlags::FilterMask) != EAutomationTestFlags::SmokeFilter);

	//ensure we are currently "running a test"
	check(GIsAutomationTesting);

	NetworkCommands.Enqueue(NewCommand);
}


bool FAutomationTestFramework::ContainsTest( const FString& InTestName ) const
{
	return AutomationTestClassNameToInstanceMap.Contains( InTestName );
}


bool FAutomationTestFramework::RunSmokeTests()
{
	bool bAllSuccessful = true;

	uint32 PreviousRequestedTestFilter = RequestedTestFilter;
	//so extra log spam isn't generated
	RequestedTestFilter = EAutomationTestFlags::SmokeFilter;
	
	// Skip running on cooked platforms like mobile
	//@todo - better determination of whether to run than requires cooked data
	// Ensure there isn't another slow task in progress when trying to run unit tests
	const bool bRequiresCookedData = FPlatformProperties::RequiresCookedData();
	if ((!bRequiresCookedData && !GIsSlowTask && !GIsPlayInEditorWorld && !FPlatformProperties::IsProgram()) || bForceSmokeTests)
	{
		TArray<FAutomationTestInfo> TestInfo;

		GetValidTestNames( TestInfo );

		if ( TestInfo.Num() > 0 )
		{
			double SmokeTestStartTime = FPlatformTime::Seconds();

			// Output the results of running the automation tests
			TMap<FString, FAutomationTestExecutionInfo> OutExecutionInfoMap;

			// Run each valid test
			FScopedSlowTask SlowTask(TestInfo.Num());

			for ( int TestIndex = 0; TestIndex < TestInfo.Num(); ++TestIndex )
			{
				SlowTask.EnterProgressFrame(1);
				if (TestInfo[TestIndex].GetTestFlags() & EAutomationTestFlags::SmokeFilter )
				{
					FString TestCommand = TestInfo[TestIndex].GetTestName();
					FAutomationTestExecutionInfo& CurExecutionInfo = OutExecutionInfoMap.Add( TestCommand, FAutomationTestExecutionInfo() );

					int32 RoleIndex = 0;  //always default to "local" role index.  Only used for multi-participant tests
					StartTestByName( TestCommand, RoleIndex );
					const bool CurTestSuccessful = StopTest(CurExecutionInfo);

					bAllSuccessful = bAllSuccessful && CurTestSuccessful;
				}
			}

			double EndTime = FPlatformTime::Seconds();
			double TimeForTest = static_cast<float>(EndTime - SmokeTestStartTime);
			if (TimeForTest > 2.0f)
			{
				//force a failure if a smoke test takes too long
				UE_LOG(LogAutomationTest, Warning, TEXT("Smoke tests took > 2s to run: %.2fs"), (float)TimeForTest);
			}

			FAutomationTestFramework::DumpAutomationTestExecutionInfo( OutExecutionInfoMap );
		}
	}
	else if( bRequiresCookedData )
	{
		UE_LOG( LogAutomationTest, Log, TEXT( "Skipping unit tests for the cooked build." ) );
	}
	else if (!FPlatformProperties::IsProgram())
	{
		UE_LOG(LogAutomationTest, Error, TEXT("Skipping unit tests.") );
		bAllSuccessful = false;
	}

	//revert to allowing all logs
	RequestedTestFilter = PreviousRequestedTestFilter;

	return bAllSuccessful;
}


void FAutomationTestFramework::ResetTests()
{
	bool bEnsureExists = false;
	bool bDeleteEntireTree = true;
	//make sure all transient files are deleted successfully
	IFileManager::Get().DeleteDirectory(*FPaths::AutomationTransientDir(), bEnsureExists, bDeleteEntireTree);
}


void FAutomationTestFramework::StartTestByName( const FString& InTestToRun, const int32 InRoleIndex )
{
	if (GIsAutomationTesting)
	{
		while(!LatentCommands.IsEmpty())
		{
			TSharedPtr<IAutomationLatentCommand> TempCommand;
			LatentCommands.Dequeue(TempCommand);
		}
		while(!NetworkCommands.IsEmpty())
		{
			TSharedPtr<IAutomationNetworkCommand> TempCommand;
			NetworkCommands.Dequeue(TempCommand);
		}
		FAutomationTestExecutionInfo TempExecutionInfo;
		StopTest(TempExecutionInfo);
	}

	FString TestName;
	FString Params;
	if (!InTestToRun.Split(TEXT(" "), &TestName, &Params, ESearchCase::CaseSensitive))
	{
		TestName = InTestToRun;
	}

	NetworkRoleIndex = InRoleIndex;

	// Ensure there isn't another slow task in progress when trying to run unit tests
	if ( !GIsSlowTask && !GIsPlayInEditorWorld )
	{
		// Ensure the test exists in the framework and is valid to run
		if ( ContainsTest( TestName ) )
		{
			// Make any setting changes that have to occur to support unit testing
			PrepForAutomationTests();

			InternalStartTest( InTestToRun );
		}
		else
		{
			UE_LOG(LogAutomationTest, Error, TEXT("Test %s does not exist and could not be run."), *InTestToRun);
		}
	}
	else
	{
		UE_LOG(LogAutomationTest, Error, TEXT("Test %s is too slow and could not be run."), *InTestToRun);
	}
}


bool FAutomationTestFramework::StopTest( FAutomationTestExecutionInfo& OutExecutionInfo )
{
	check(GIsAutomationTesting);
	
	bool bSuccessful = InternalStopTest(OutExecutionInfo);

	// Restore any changed settings now that unit testing has completed
	ConcludeAutomationTests();

	return bSuccessful;
}


bool FAutomationTestFramework::ExecuteLatentCommands()
{
	check(GIsAutomationTesting);

	bool bHadAnyLatentCommands = !LatentCommands.IsEmpty();
	while (!LatentCommands.IsEmpty())
	{
		//get the next command to execute
		TSharedPtr<IAutomationLatentCommand> NextCommand;
		LatentCommands.Peek(NextCommand);

		bool bComplete = NextCommand->InternalUpdate();
		if (bComplete)
		{
			//all done.  remove from the queue
			LatentCommands.Dequeue(NextCommand);
		}
		else
		{
			break;
		}
	}
	//need more processing on the next frame
	if (bHadAnyLatentCommands)
	{
		return false;
	}

	return true;
}


bool FAutomationTestFramework::ExecuteNetworkCommands()
{
	check(GIsAutomationTesting);
	bool bHadAnyNetworkCommands = !NetworkCommands.IsEmpty();

	if( bHadAnyNetworkCommands )
	{
		// Get the next command to execute
		TSharedPtr<IAutomationNetworkCommand> NextCommand;
		NetworkCommands.Dequeue(NextCommand);
		if (NextCommand->GetRoleIndex() == NetworkRoleIndex)
		{
			NextCommand->Run();
		}
	}

	return !bHadAnyNetworkCommands;
}

void FAutomationTestFramework::LoadTestModules( )
{
	const bool bRunningEditor = GIsEditor && !IsRunningCommandlet();

	bool bRunningSmokeTests = ((RequestedTestFilter & EAutomationTestFlags::FilterMask) == EAutomationTestFlags::SmokeFilter);
	if( !bRunningSmokeTests )
	{
		TArray<FString> EngineTestModules;
		GConfig->GetArray( TEXT("/Script/Engine.AutomationTestSettings"), TEXT("EngineTestModules"), EngineTestModules, GEngineIni);
		//Load any engine level modules.
		for( int32 EngineModuleId = 0; EngineModuleId < EngineTestModules.Num(); ++EngineModuleId)
		{
			const FName ModuleName = FName(*EngineTestModules[EngineModuleId]);
			//Make sure that there is a name available.  This can happen if a name is left blank in the Engine.ini
			if (ModuleName == NAME_None || ModuleName == TEXT("None"))
			{
				UE_LOG(LogAutomationTest, Warning, TEXT("The automation test module ('%s') doesn't have a valid name."), *ModuleName.ToString());
				continue;
			}
			if (!FModuleManager::Get().IsModuleLoaded(ModuleName))
			{
				UE_LOG(LogAutomationTest, Log, TEXT("Loading automation test module: '%s'."), *ModuleName.ToString());
				FModuleManager::Get().LoadModule(ModuleName);
			}
		}
		//Load any editor modules.
		if( bRunningEditor )
		{
			TArray<FString> EditorTestModules;
			GConfig->GetArray( TEXT("/Script/Engine.AutomationTestSettings"), TEXT("EditorTestModules"), EditorTestModules, GEngineIni);
			for( int32 EditorModuleId = 0; EditorModuleId < EditorTestModules.Num(); ++EditorModuleId )
			{
				const FName ModuleName = FName(*EditorTestModules[EditorModuleId]);
				//Make sure that there is a name available.  This can happen if a name is left blank in the Engine.ini
				if (ModuleName == NAME_None || ModuleName == TEXT("None"))
				{
					UE_LOG(LogAutomationTest, Warning, TEXT("The automation test module ('%s') doesn't have a valid name."), *ModuleName.ToString());
					continue;
				}
				if (!FModuleManager::Get().IsModuleLoaded(ModuleName))
				{
					UE_LOG(LogAutomationTest, Log, TEXT("Loading automation test module: '%s'."), *ModuleName.ToString());
					FModuleManager::Get().LoadModule(ModuleName);
				}
			}
		}
	}
}


void FAutomationTestFramework::GetValidTestNames( TArray<FAutomationTestInfo>& TestInfo ) const
{
	TestInfo.Empty();

	// Determine required application type (Editor, Game, or Commandlet)
	const bool bRunningEditor = GIsEditor && !IsRunningCommandlet();
	const bool bRunningGame = !GIsEditor || IsRunningGame();
	const bool bRunningCommandlet = IsRunningCommandlet();

	//application flags
	uint32 ApplicationSupportFlags = 0;
	if ( bRunningEditor )
	{
		ApplicationSupportFlags |= EAutomationTestFlags::EditorContext;
	}
	if ( bRunningGame )
	{
		ApplicationSupportFlags |= EAutomationTestFlags::ClientContext;
	}
	if ( bRunningCommandlet )
	{
		ApplicationSupportFlags |= EAutomationTestFlags::CommandletContext;
	}

	//Feature support - assume valid RHI until told otherwise
	uint32 FeatureSupportFlags = EAutomationTestFlags::FeatureMask;
	// @todo: Handle this correctly. GIsUsingNullRHI is defined at Engine-level, so it can't be used directly here in Core.
	// For now, assume Null RHI is only used for commandlets, servers, and when the command line specifies to use it.
	if (FPlatformProperties::SupportsWindowedMode())
	{
		bool bUsingNullRHI = FParse::Param( FCommandLine::Get(), TEXT("nullrhi") ) || IsRunningCommandlet() || IsRunningDedicatedServer();
		if (bUsingNullRHI)
		{
			FeatureSupportFlags &= (~EAutomationTestFlags::NonNullRHI);
		}
	}
	if (FApp::IsUnattended())
	{
		FeatureSupportFlags &= (~EAutomationTestFlags::RequiresUser);
	}

	for ( TMap<FString, FAutomationTestBase*>::TConstIterator TestIter( AutomationTestClassNameToInstanceMap ); TestIter; ++TestIter )
	{
		const FAutomationTestBase* CurTest = TestIter.Value();
		check( CurTest );

		uint32 CurTestFlags = CurTest->GetTestFlags();

		//filter out full tests when running smoke tests
		const bool bPassesFilterRequirement = ((CurTestFlags & RequestedTestFilter) != 0);

		//Application Tests
		uint32 CurTestApplicationFlags = (CurTestFlags & EAutomationTestFlags::ApplicationContextMask);
		const bool bPassesApplicationRequirements = (CurTestApplicationFlags == 0) || (CurTestApplicationFlags & ApplicationSupportFlags);
		
		//Feature Tests
		uint32 CurTestFeatureFlags = (CurTestFlags & EAutomationTestFlags::FeatureMask);
		const bool bPassesFeatureRequirements = (CurTestFeatureFlags == 0) || (CurTestFeatureFlags & FeatureSupportFlags);

		const bool bEnabled = (CurTestFlags & EAutomationTestFlags::Disabled) == 0;

		if (bEnabled && bPassesApplicationRequirements && bPassesFeatureRequirements && bPassesFilterRequirement)
		{
			CurTest->GenerateTestNames(TestInfo);
		}
	}
}


bool FAutomationTestFramework::ShouldTestContent(const FString& Path) const
{
	static TArray<FString> TestLevelFolders;
	if ( TestLevelFolders.Num() == 0 )
	{
		GConfig->GetArray( TEXT("/Script/Engine.AutomationTestSettings"), TEXT("TestLevelFolders"), TestLevelFolders, GEngineIni);
	}

	bool bMatchingDirectory = false;
	for ( const FString& Folder : TestLevelFolders )
	{
		const FString PatternToCheck = FString::Printf(TEXT("/%s/"), *Folder);
		if ( Path.Contains(*PatternToCheck) )
		{
			bMatchingDirectory = true;
		}
	}
	if (bMatchingDirectory)
	{
		return true;
	}

	FString DevelopersPath = FPaths::GameDevelopersDir().LeftChop(1);
	return bDeveloperDirectoryIncluded || !Path.StartsWith(DevelopersPath);
}


void FAutomationTestFramework::SetDeveloperDirectoryIncluded(const bool bInDeveloperDirectoryIncluded)
{
	bDeveloperDirectoryIncluded = bInDeveloperDirectoryIncluded;
}


void FAutomationTestFramework::SetRequestedTestFilter(const uint32 InRequestedTestFlags)
{
	RequestedTestFilter = InRequestedTestFlags;
}


FOnTestScreenshotCaptured& FAutomationTestFramework::OnScreenshotCaptured()
{
	return TestScreenshotCapturedDelegate;
}


void FAutomationTestFramework::SetScreenshotOptions( const bool bInScreenshotsEnabled, const bool bInUseFullSizeScreenshots )
{
	bScreenshotsEnabled = bInScreenshotsEnabled;
	bUseFullSizeScreenShots = bInUseFullSizeScreenshots;
}


bool FAutomationTestFramework::IsScreenshotAllowed() const
{
	return bScreenshotsEnabled;
}


bool FAutomationTestFramework::ShouldUseFullSizeScreenshots() const
{
	return bUseFullSizeScreenShots;
}


void FAutomationTestFramework::PrepForAutomationTests()
{
	check(!GIsAutomationTesting);

	// Fire off callback signifying that unit testing is about to begin. This allows
	// other systems to prepare themselves as necessary without the unit testing framework having to know
	// about them.
	PreTestingEvent.Broadcast();

	// Cache the contents of GWarn, as unit testing is going to forcibly replace GWarn with a specialized feedback context
	// designed for unit testing
	CachedContext = GWarn;
	AutomationTestFeedbackContext.TreatWarningsAsErrors = GWarn->TreatWarningsAsErrors;
	GWarn = &AutomationTestFeedbackContext;

	// Mark that unit testing has begun
	GIsAutomationTesting = true;
}


void FAutomationTestFramework::ConcludeAutomationTests()
{
	check(GIsAutomationTesting);
	
	// Mark that unit testing is over
	GIsAutomationTesting = false;

	GWarn = CachedContext;
	CachedContext = NULL;

	// Fire off callback signifying that unit testing has concluded.
	PostTestingEvent.Broadcast();
}


/**
 * Helper method to dump the contents of the provided test name to execution info map to the provided feedback context
 *
 * @param	InContext		Context to dump the execution info to
 * @param	InInfoToDump	Execution info that should be dumped to the provided feedback context
 */
void FAutomationTestFramework::DumpAutomationTestExecutionInfo( const TMap<FString, FAutomationTestExecutionInfo>& InInfoToDump )
{
	const FString SuccessMessage = NSLOCTEXT("UnrealEd", "AutomationTest_Success", "Success").ToString();
	const FString FailMessage = NSLOCTEXT("UnrealEd", "AutomationTest_Fail", "Fail").ToString();
	for ( TMap<FString, FAutomationTestExecutionInfo>::TConstIterator MapIter(InInfoToDump); MapIter; ++MapIter )
	{
		const FString& CurTestName = MapIter.Key();
		const FAutomationTestExecutionInfo& CurExecutionInfo = MapIter.Value();

		UE_LOG(LogAutomationTest, Log, TEXT("%s: %s"), *CurTestName, CurExecutionInfo.bSuccessful ? *SuccessMessage : *FailMessage);

		if ( CurExecutionInfo.Errors.Num() > 0 )
		{
			SET_WARN_COLOR(COLOR_RED);
			CLEAR_WARN_COLOR();
			for ( TArray<FAutomationEvent>::TConstIterator ErrorIter( CurExecutionInfo.Errors ); ErrorIter; ++ErrorIter )
			{
				UE_LOG(LogAutomationTest, Error, TEXT("%s"), *(*ErrorIter).Message);
			}
		}

		if ( CurExecutionInfo.Warnings.Num() > 0 )
		{
			SET_WARN_COLOR(COLOR_YELLOW);
			CLEAR_WARN_COLOR();
			for ( TArray<FString>::TConstIterator WarningIter( CurExecutionInfo.Warnings ); WarningIter; ++WarningIter )
			{
				UE_LOG(LogAutomationTest, Warning, TEXT("%s"), **WarningIter );
			}
		}

		if ( CurExecutionInfo.LogItems.Num() > 0 )
		{
			//InContext->Logf( *FString::Printf( TEXT("%s"), *NSLOCTEXT("UnrealEd", "AutomationTest_LogItems", "Log Items").ToString() ) );
			for ( TArray<FString>::TConstIterator LogItemIter( CurExecutionInfo.LogItems ); LogItemIter; ++LogItemIter )
			{
				UE_LOG(LogAutomationTest, Log, TEXT("%s"), **LogItemIter );
			}
		}
		//InContext->Logf( TEXT("") );
	}
}


void FAutomationTestFramework::InternalStartTest( const FString& InTestToRun )
{
	FString TestName;
	if (!InTestToRun.Split(TEXT(" "), &TestName, &Parameters, ESearchCase::CaseSensitive))
	{
		TestName = InTestToRun;
	}

	if ( ContainsTest( TestName ) )
	{
		CurrentTest = *( AutomationTestClassNameToInstanceMap.Find( TestName ) );
		check( CurrentTest );

		// Clear any execution info from the test in case it has been run before
		CurrentTest->ClearExecutionInfo();

		// Associate the test that is about to be run with the special unit test feedback context
		AutomationTestFeedbackContext.SetCurrentAutomationTest( CurrentTest );

		StartTime = FPlatformTime::Seconds();

		//if non-
		uint32 NonSmokeTestFlags = (EAutomationTestFlags::FilterMask & (~EAutomationTestFlags::SmokeFilter));
		if (RequestedTestFilter & NonSmokeTestFlags)
		{
			UE_LOG(LogAutomationTest, Log, TEXT("%s %s is starting at %f"), *CurrentTest->GetBeautifiedTestName(), *Parameters, StartTime);
		}

		// Run the test!
		bTestSuccessful = CurrentTest->RunTest(Parameters);
	}
}


bool FAutomationTestFramework::InternalStopTest(FAutomationTestExecutionInfo& OutExecutionInfo)
{
	check(GIsAutomationTesting);
	check(LatentCommands.IsEmpty());

	double EndTime = FPlatformTime::Seconds();
	double TimeForTest = static_cast<float>(EndTime - StartTime);
	uint32 NonSmokeTestFlags = (EAutomationTestFlags::FilterMask & (~EAutomationTestFlags::SmokeFilter));
	if (RequestedTestFilter & NonSmokeTestFlags)
	{
		UE_LOG(LogAutomationTest, Log, TEXT("%s %s ran in %f"), *CurrentTest->GetBeautifiedTestName(), *Parameters, TimeForTest);
	}

	// Disassociate the test from the feedback context
	AutomationTestFeedbackContext.SetCurrentAutomationTest( NULL );

	// Determine if the test was successful based on two criteria:
	// 1) Did the test itself report success?
	// 2) Did any errors occur and were logged by the feedback context during execution?++----
	bTestSuccessful = bTestSuccessful && !CurrentTest->HasAnyErrors();

	// Set the success state of the test based on the above criteria
	CurrentTest->SetSuccessState( bTestSuccessful );

	// Fill out the provided execution info with the info from the test
	CurrentTest->GetExecutionInfo( OutExecutionInfo );

	//save off timing for the test
	OutExecutionInfo.Duration = TimeForTest;

	//release pointers to now-invalid data
	CurrentTest = NULL;

	return bTestSuccessful;
}


void FAutomationTestFramework::AddAnalyticsItemToCurrentTest( const FString& AnalyticsItem )
{
	if( CurrentTest != nullptr )
	{
		CurrentTest->AddAnalyticsItem( AnalyticsItem );
	}
	else
	{
		UE_LOG( LogAutomationTest, Warning, TEXT( "AddAnalyticsItemToCurrentTest() called when no automation test was actively running!" ) );
	}
}

FAutomationTestFramework::FAutomationTestFramework()
:	CachedContext( NULL )
,	RequestedTestFilter(EAutomationTestFlags::SmokeFilter)
,	StartTime(0.0f)
,	bTestSuccessful(false)
,	CurrentTest(NULL)
,	bDeveloperDirectoryIncluded(false)
,	bScreenshotsEnabled(true)
,	bUseFullSizeScreenShots(true)
,	NetworkRoleIndex(0)
,	bForceSmokeTests(false)
{ }


FAutomationTestFramework::~FAutomationTestFramework()
{
	CachedContext = NULL;
	AutomationTestClassNameToInstanceMap.Empty();
}


FString FAutomationEvent::ToString() const
{
	FString ComplexString;

	if ( !Filename.IsEmpty() && LineNumber > 0 )
	{
		ComplexString += Filename;
		ComplexString += TEXT("(");
		ComplexString += FString::FromInt(LineNumber);
		ComplexString += TEXT("): ");
	}

	if ( !Context.IsEmpty() )
	{
		ComplexString += Context;
		ComplexString += TEXT(": ");
	}

	ComplexString += Message;

	return ComplexString;
}


void FAutomationTestBase::ClearExecutionInfo()
{
	ExecutionInfo.Clear();
}


void FAutomationTestBase::AddError(const FString& InError, int32 StackOffset)
{
	if( !bSuppressLogs )
	{
		TArray<FProgramCounterSymbolInfo> Stack = FPlatformStackWalk::GetStack(StackOffset + 1, 1);

		ExecutionInfo.Errors.Add(FAutomationEvent(InError, ExecutionInfo.Context, Stack[0].Filename, Stack[0].LineNumber));
	}
}


void FAutomationTestBase::AddWarning( const FString& InWarning )
{
	if( !bSuppressLogs )
	{
		ExecutionInfo.Warnings.Add( InWarning );
	}
}


void FAutomationTestBase::AddLogItem( const FString& InLogItem )
{
	if( !bSuppressLogs )
	{
		ExecutionInfo.LogItems.Add( InLogItem );
	}
}


void FAutomationTestBase::AddAnalyticsItem(const FString& InAnalyticsItem)
{
	ExecutionInfo.AnalyticsItems.Add(InAnalyticsItem);
}


bool FAutomationTestBase::HasAnyErrors() const
{
	return ExecutionInfo.Errors.Num() > 0;
}


void FAutomationTestBase::SetSuccessState( bool bSuccessful )
{
	ExecutionInfo.bSuccessful = bSuccessful;
}


void FAutomationTestBase::GetExecutionInfo( FAutomationTestExecutionInfo& OutInfo ) const
{
	OutInfo = ExecutionInfo;
}


void FAutomationTestBase::GenerateTestNames(TArray<FAutomationTestInfo>& TestInfo) const
{
	TArray<FString> BeautifiedNames;
	TArray<FString> ParameterNames;
	GetTests(BeautifiedNames, ParameterNames);

	FString BeautifiedTestName = GetBeautifiedTestName();

	for (int32 ParameterIndex = 0; ParameterIndex < ParameterNames.Num(); ++ParameterIndex)
	{
		FString CompleteBeautifiedNames = BeautifiedTestName;
		FString CompleteTestName = TestName;

		if (ParameterNames[ParameterIndex].Len())
		{
			CompleteBeautifiedNames = FString::Printf(TEXT("%s.%s"), *BeautifiedTestName, *BeautifiedNames[ParameterIndex]);;
			CompleteTestName = FString::Printf(TEXT("%s %s"), *TestName, *ParameterNames[ParameterIndex]);
		}

		// Add the test info to our collection
		FAutomationTestInfo NewTestInfo(
			CompleteBeautifiedNames,
			CompleteTestName,
			GetTestFlags(),
			GetRequiredDeviceNum(),
			ParameterNames[ParameterIndex],
			GetTestSourceFileName(),
			GetTestSourceFileLine(),
			GetTestAssetPath(ParameterNames[ParameterIndex]),
			GetTestOpenCommand(ParameterNames[ParameterIndex])
		);
		
		TestInfo.Add( NewTestInfo );
	}
}
