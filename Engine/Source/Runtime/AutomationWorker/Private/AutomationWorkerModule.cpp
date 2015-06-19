// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AutomationWorkerPrivatePCH.h"
#include "AssetRegistryModule.h"


#define LOCTEXT_NAMESPACE "AutomationTest"

IMPLEMENT_MODULE(FAutomationWorkerModule, AutomationWorker);


/* IModuleInterface interface
 *****************************************************************************/

void FAutomationWorkerModule::StartupModule()
{
	Initialize();

	FAutomationTestFramework::GetInstance().PreTestingEvent.AddRaw(this, &FAutomationWorkerModule::HandlePreTestingEvent);
	FAutomationTestFramework::GetInstance().PostTestingEvent.AddRaw(this, &FAutomationWorkerModule::HandlePostTestingEvent);
}

void FAutomationWorkerModule::ShutdownModule()
{
	FAutomationTestFramework::GetInstance().PreTestingEvent.RemoveAll(this);
	FAutomationTestFramework::GetInstance().PostTestingEvent.RemoveAll(this);
}

bool FAutomationWorkerModule::SupportsDynamicReloading()
{
	return true;
}


/* IAutomationWorkerModule interface
 *****************************************************************************/

void FAutomationWorkerModule::Tick()
{
	//execute latent commands from the previous frame.  Gives the rest of the engine a turn to tick before closing the test
	bool bAllLatentCommandsComplete  = ExecuteLatentCommands();
	if (bAllLatentCommandsComplete)
	{
		//if we were running the latent commands as a result of executing a network command, report that we are now done
		if (bExecutingNetworkCommandResults)
		{
			ReportNetworkCommandComplete();
			bExecutingNetworkCommandResults = false;
		}

		//if the controller has requested the next network command be executed
		if (bExecuteNextNetworkCommand)
		{
			//execute network commands if there are any queued up and our role is appropriate
			bool bAllNetworkCommandsComplete = ExecuteNetworkCommands();
			if (bAllNetworkCommandsComplete)
			{
				ReportTestComplete();
			}

			//we've now executed a network command which may have enqueued further latent actions
			bExecutingNetworkCommandResults = true;

			//do not execute anything else until expressly told to by the controller
			bExecuteNextNetworkCommand = false;
		}
	}

	if (MessageEndpoint.IsValid())
	{
		MessageEndpoint->ProcessInbox();
	}

	// Run any of the automation commands that was obtained during initialization.
	//RunDeferredAutomationCommands();
}


/* ISessionManager implementation
 *****************************************************************************/

bool FAutomationWorkerModule::ExecuteLatentCommands()
{
	bool bAllLatentCommandsComplete = false;
	
	if (GIsAutomationTesting)
	{
		// Ensure that latent automation commands have time to execute
		bAllLatentCommandsComplete = FAutomationTestFramework::GetInstance().ExecuteLatentCommands();
	}
	
	return bAllLatentCommandsComplete;
}


bool FAutomationWorkerModule::ExecuteNetworkCommands()
{
	bool bAllLatentCommandsComplete = false;
	
	if (GIsAutomationTesting)
	{
		// Ensure that latent automation commands have time to execute
		bAllLatentCommandsComplete = FAutomationTestFramework::GetInstance().ExecuteNetworkCommands();
	}

	return bAllLatentCommandsComplete;
}


void FAutomationWorkerModule::Initialize()
{
	if (FPlatformProcess::SupportsMultithreading())
	{
		// initialize messaging
		MessageEndpoint = FMessageEndpoint::Builder("FAutomationWorkerModule")
			.Handling<FAutomationWorkerFindWorkers>(this, &FAutomationWorkerModule::HandleFindWorkersMessage)
			.Handling<FAutomationWorkerNextNetworkCommandReply>(this, &FAutomationWorkerModule::HandleNextNetworkCommandReplyMessage)
			.Handling<FAutomationWorkerPing>(this, &FAutomationWorkerModule::HandlePingMessage)
			.Handling<FAutomationWorkerResetTests>(this, &FAutomationWorkerModule::HandleResetTests)
			.Handling<FAutomationWorkerRequestTests>(this, &FAutomationWorkerModule::HandleRequestTestsMessage)
			.Handling<FAutomationWorkerRunTests>(this, &FAutomationWorkerModule::HandleRunTestsMessage)
			.WithInbox();

		if (MessageEndpoint.IsValid())
		{
			MessageEndpoint->Subscribe<FAutomationWorkerFindWorkers>();
		}

		bExecuteNextNetworkCommand = true;		
	}
	else
	{
		bExecuteNextNetworkCommand = false;
	}
	ExecutionCount = INDEX_NONE;
	bExecutingNetworkCommandResults = false;

#if !(UE_BUILD_SHIPPING)
	//Obtain any command line tests commands that use '-automationtests='.
	FString AutomationCmds;
	if (FParse::Value(FCommandLine::Get(), TEXT("AutomationTests="), AutomationCmds, false))
	{
		new(DeferredAutomationCommands) FString(FString(TEXT("Automation CommandLineTests ")) + AutomationCmds);
	}

	//If the asset registry is loading assets then we'll wait for it to stop before running our automation tests.
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	if (AssetRegistryModule.Get().IsLoadingAssets())
	{
		AssetRegistryModule.Get().OnFilesLoaded().AddRaw(this, &FAutomationWorkerModule::RunDeferredAutomationCommands);
	}
	else
	{
		//If the registry is not loading then we'll just go ahead and run our tests.
		RunDeferredAutomationCommands();
	}
#endif // !(UE_BUILD_SHIPPING)
}

void FAutomationWorkerModule::ReportNetworkCommandComplete()
{
	if (GIsAutomationTesting)
	{
		MessageEndpoint->Send(new FAutomationWorkerRequestNextNetworkCommand(ExecutionCount), TestRequesterAddress);
		if (StopTestEvent.IsBound())
		{
			// this is a local test; the message to continue will never arrive, so lets not wait for it
			bExecuteNextNetworkCommand = true;
		}
	}
}


/**
 *	Takes a large transport array and splits it into pieces of a desired size and returns the portion of this which is requested
 *
 * @param FullTransportArray The whole series of data
 * @param CurrentChunkIndex The The chunk we are requesting
 * @param NumToSend The maximum number of bytes we should be splitting into.
 * @return The section of the transport array which matches our index requested
 */
TArray< uint8 > GetTransportSection( const TArray< uint8 >& FullTransportArray, const int32 NumToSend, const int32 RequestedChunkIndex )
{
	TArray< uint8 > TransportArray = FullTransportArray;

	if( NumToSend > 0 )
	{
		int32 NumToRemoveFromStart = RequestedChunkIndex * NumToSend;
		if( NumToRemoveFromStart > 0 )
		{
			TransportArray.RemoveAt( 0, NumToRemoveFromStart );
		}

		int32 NumToRemoveFromEnd = FullTransportArray.Num() - NumToRemoveFromStart - NumToSend;
		if( NumToRemoveFromEnd > 0 )
		{
			TransportArray.RemoveAt( TransportArray.Num()-NumToRemoveFromEnd, NumToRemoveFromEnd );
		}
	}
	else
	{
		TransportArray.Empty();
	}

	return TransportArray;
}


void FAutomationWorkerModule::ReportTestComplete()
{
	if (GIsAutomationTesting)
	{
		//see if there are any more network commands left to execute
		bool bAllLatentCommandsComplete = FAutomationTestFramework::GetInstance().ExecuteLatentCommands();

		//structure to track error/warning/log messages
		FAutomationTestExecutionInfo ExecutionInfo;

		bool bSuccess = FAutomationTestFramework::GetInstance().StopTest(ExecutionInfo);

		if (StopTestEvent.IsBound())
		{
			StopTestEvent.Execute(bSuccess, TestName, ExecutionInfo);
		}
		else
		{
			// send the results to the controller
			FAutomationWorkerRunTestsReply* Message = new FAutomationWorkerRunTestsReply();

			Message->TestName = TestName;
			Message->ExecutionCount = ExecutionCount;
			Message->Success = bSuccess;
			Message->Duration = ExecutionInfo.Duration;
			Message->Errors = ExecutionInfo.Errors;
			Message->Warnings = ExecutionInfo.Warnings;
			Message->Logs = ExecutionInfo.LogItems;

			MessageEndpoint->Send(Message, TestRequesterAddress);
		}


		// reset local state
		TestRequesterAddress.Invalidate();
		ExecutionCount = INDEX_NONE;
		TestName.Empty();
		StopTestEvent.Unbind();
	}
}


void FAutomationWorkerModule::SendTests( const FMessageAddress& ControllerAddress )
{
	for( int32 TestIndex = 0; TestIndex < TestInfo.Num(); TestIndex++ )
	{
		MessageEndpoint->Send(new FAutomationWorkerRequestTestsReply(TestInfo[TestIndex].GetTestAsString(), TestInfo.Num()), ControllerAddress);
	}
}


/* FAutomationWorkerModule callbacks
 *****************************************************************************/

void FAutomationWorkerModule::HandleFindWorkersMessage( const FAutomationWorkerFindWorkers& Message, const IMessageContextRef& Context )
{
	// Set the Instance name to be the same as the session browser. This information should be shared at some point
	FString InstanceName = FString::Printf(TEXT("%s-%i"), FPlatformProcess::ComputerName(), FPlatformProcess::GetCurrentProcessId());

	if ((Message.SessionId == FApp::GetSessionId()) && (Message.Changelist == 10000))
	{
		FAutomationWorkerFindWorkersResponse* Response = new FAutomationWorkerFindWorkersResponse();

		FString OSMajorVersionString, OSSubVersionString;
		FPlatformMisc::GetOSVersions( OSMajorVersionString, OSSubVersionString );

		FString OSVersionString = OSMajorVersionString + TEXT(" ") + OSSubVersionString;
		FString CPUModelString = FPlatformMisc::GetCPUBrand().Trim();

		Response->DeviceName = FPlatformProcess::ComputerName();
		Response->InstanceName = InstanceName;
		Response->Platform = FPlatformProperties::PlatformName();
		Response->SessionId = Message.SessionId;
		Response->OSVersionName = OSVersionString;
		Response->ModelName = FPlatformMisc::GetDefaultDeviceProfileName();
		Response->GPUName = FPlatformMisc::GetPrimaryGPUBrand();
		Response->CPUModelName = CPUModelString;
		Response->RAMInGB = FPlatformMemory::GetPhysicalGBRam();
#if WITH_ENGINE
		Response->RenderModeName = AutomationCommon::GetRenderDetailsString();
#else
		Response->RenderModeName = TEXT("Unknown");
#endif

		MessageEndpoint->Send(Response, Context->GetSender());
	}
}


void FAutomationWorkerModule::HandleNextNetworkCommandReplyMessage( const FAutomationWorkerNextNetworkCommandReply& Message, const IMessageContextRef& Context )
{
	// Allow the next command to execute
	bExecuteNextNetworkCommand = true;

	// We should never be executing sub-commands of a network command when we're waiting for a cue for the next network command
	check(bExecutingNetworkCommandResults == false);
}


void FAutomationWorkerModule::HandlePingMessage( const FAutomationWorkerPing& Message, const IMessageContextRef& Context )
{
	MessageEndpoint->Send(new FAutomationWorkerPong(), Context->GetSender());
}


void FAutomationWorkerModule::HandleResetTests( const FAutomationWorkerResetTests& Message, const IMessageContextRef& Context )
{
	FAutomationTestFramework::GetInstance().ResetTests();
}


void FAutomationWorkerModule::HandleRequestTestsMessage( const FAutomationWorkerRequestTests& Message, const IMessageContextRef& Context )
{
	FAutomationTestFramework::GetInstance().LoadTestModules();
	FAutomationTestFramework::GetInstance().SetDeveloperDirectoryIncluded(Message.DeveloperDirectoryIncluded);
	FAutomationTestFramework::GetInstance().SetVisualCommandletFilter(Message.VisualCommandletFilterOn);
	FAutomationTestFramework::GetInstance().GetValidTestNames( TestInfo );

	SendTests(Context->GetSender());
}


void FAutomationWorkerModule::HandlePreTestingEvent()
{
#if WITH_ENGINE
	if (!GIsEditor && GEngine->GameViewport)
	{
		GEngine->GameViewport->OnScreenshotCaptured().AddRaw(this, &FAutomationWorkerModule::HandleScreenShotCaptured);
	}
	//Register the editor screen shot callback
	FAutomationTestFramework::GetInstance().OnScreenshotCaptured().BindRaw(this, &FAutomationWorkerModule::HandleScreenShotCapturedWithName);
#endif
}


void FAutomationWorkerModule::HandlePostTestingEvent()
{
#if WITH_ENGINE
	if (!GIsEditor && GEngine->GameViewport)
	{
		GEngine->GameViewport->OnScreenshotCaptured().RemoveAll(this);
	}
	//Register the editor screen shot callback
	FAutomationTestFramework::GetInstance().OnScreenshotCaptured().BindRaw(this, &FAutomationWorkerModule::HandleScreenShotCapturedWithName);
#endif
}


#if WITH_ENGINE
void FAutomationWorkerModule::HandleScreenShotCaptured(int32 Width, int32 Height, const TArray<FColor>& Bitmap)
{
	FString Filename = FScreenshotRequest::GetFilename();

	HandleScreenShotCapturedWithName(Width, Height, Bitmap, Filename);
}

void FAutomationWorkerModule::HandleScreenShotCapturedWithName(int32 Width, int32 Height, const TArray<FColor>& Bitmap, const FString& ScreenShotName)
{
	if( FAutomationTestFramework::GetInstance().IsScreenshotAllowed() )
	{
		int32 NewHeight = Height;
		int32 NewWidth = Width;

		TArray<FColor> ScaledBitmap;

		if( FAutomationTestFramework::GetInstance().ShouldUseFullSizeScreenshots() )
		{
			ScaledBitmap = Bitmap;
			//Clear the alpha channel before saving
			for ( int32 Index = 0; Index < Width*Height; Index++ )
			{
				ScaledBitmap[Index].A = 255;
			}
		}
		else
		{
			//Set the thumbnail size
			NewHeight = 128;
			NewWidth = 256;

			// Create and save the thumbnail
			FImageUtils::CropAndScaleImage(Width, Height, NewWidth, NewHeight, Bitmap, ScaledBitmap);
		}


		TArray<uint8> CompressedBitmap;
		FImageUtils::CompressImageArray(NewWidth, NewHeight, ScaledBitmap, CompressedBitmap);

		// Send the screen shot if we have a target
		if( TestRequesterAddress.IsValid() )
		{
			FAutomationWorkerScreenImage* Message = new FAutomationWorkerScreenImage();

			FString SFilename = ScreenShotName;
			Message->ScreenShotName = SFilename;
			Message->ScreenImage = CompressedBitmap;
			MessageEndpoint->Send(Message, TestRequesterAddress);
		}
		else
		{
			//Save locally
			const bool bTree = true;
			const FString FileName = FPaths::RootDir() + ScreenShotName;
			IFileManager::Get().MakeDirectory( *FPaths::GetPath(FileName), bTree );
			FFileHelper::SaveArrayToFile( CompressedBitmap, *FileName );
		}
	}

}
#endif


void FAutomationWorkerModule::HandleRunTestsMessage( const FAutomationWorkerRunTests& Message, const IMessageContextRef& Context )
{
	ExecutionCount = Message.ExecutionCount;
	TestName = Message.TestName;
	TestRequesterAddress = Context->GetSender();
	FAutomationTestFramework::GetInstance().SetScreenshotOptions(Message.bScreenshotsEnabled, Message.bUseFullSizeScreenShots);

	// Always allow the first network command to execute
	bExecuteNextNetworkCommand = true;

	// We are not executing network command sub-commands right now
	bExecutingNetworkCommandResults = false;

	FAutomationTestFramework::GetInstance().StartTestByName(Message.TestName, Message.RoleIndex);
}


void FAutomationWorkerModule::RunTest(const FString& InTestToRun, const int32 InRoleIndex, FStopTestEvent const& InStopTestEvent)
{
	TestName = InTestToRun;

	StopTestEvent = InStopTestEvent;
	// Always allow the first network command to execute
	bExecuteNextNetworkCommand = true;

	// We are not executing network command sub-commands right now
	bExecutingNetworkCommandResults = false;

	FAutomationTestFramework::GetInstance().StartTestByName(InTestToRun, InRoleIndex);
}

void FAutomationWorkerModule::RunDeferredAutomationCommands()
{
	// Get the loaded asset registry module.
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	
	// Don't run any of the deferred commands if the registry is still loading otherwise not all possible tests will be found and ran.
	if (!AssetRegistryModule.Get().IsLoadingAssets())
	{
		// Execute all currently queued deferred commands (allows commands to be queued up for next frame).
		const int32 DeferredCommandsCount = DeferredAutomationCommands.Num();
		for (int32 DeferredCommandsIndex = 0; DeferredCommandsIndex < DeferredCommandsCount; DeferredCommandsIndex++)
		{
			{
				GEngine->Exec(NULL, *DeferredAutomationCommands[DeferredCommandsIndex], *GLog);
			}
		}

		// Once all of the commands have executed then we remove them from the array.
		DeferredAutomationCommands.RemoveAt(0, DeferredCommandsCount);
	}
}

/**
 * Implements a local controller to run tests and spew results, mostly used by automated testing.
 */
static struct FQueueTests
{
	struct FJob
	{
		FString Test;
		int32 RoleIndex;
		FOutputDevice* Ar;

		FJob(FString const& InTest, int32 InRoleIndex, FOutputDevice* InAr)
			: Test(InTest)
			, RoleIndex(InRoleIndex)
			, Ar(InAr)
		{
		}
	};

	int32 NumTestsRun;
	bool bTestInPogress;
	bool bTicking;
	TArray<FJob> Queue;

	FQueueTests()
		: NumTestsRun(0)
		, bTestInPogress(false)
		, bTicking(false)
	{ }

	void NewTest(FString const& Command, int32 RoleIndex = 0, FOutputDevice* Ar = GLog)
	{
		new (Queue) FJob(Command, RoleIndex, Ar);
		if (!bTicking)
		{
			FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FQueueTests::TickQueueTests), 0.1f);
			bTicking = true;
		}
	}

	bool TickQueueTests(float DeltaTime)
	{
		check(bTicking);
		if (!GIsAutomationTesting && !bTestInPogress && Queue.Num())
		{
			FJob& CurrentJob = Queue[0];
			TArray<FAutomationTestInfo> TestInfo;
			FAutomationTestFramework::GetInstance().GetValidTestNames( TestInfo );
			bool bRanIt = false;
			for ( int TestIndex = 0; TestIndex < TestInfo.Num(); ++TestIndex )
			{
				FString TestCommand = TestInfo[TestIndex].GetTestName();
				if (TestCommand == CurrentJob.Test)
				{
					CurrentJob.Ar->Logf(TEXT("Running: %s"), *CurrentJob.Test);
					IAutomationWorkerModule::FStopTestEvent Event;
					Event.BindRaw(this, &FQueueTests::ConsoleCommandTestComplete, CurrentJob.Ar);
					if (FModuleManager::Get().IsModuleLoaded(TEXT("AutomationWorker")))
					{
						FModuleManager::GetModuleChecked<IAutomationWorkerModule>("AutomationWorker").RunTest(CurrentJob.Test, CurrentJob.RoleIndex, Event);
						bTestInPogress = true;
						bRanIt = true;
					}
					break;
				}
			}
			if (!bRanIt)
			{
				CurrentJob.Ar->Logf(TEXT("ERROR: Failed to find test %s"), *CurrentJob.Test);
			}
			Queue.RemoveAt(0);
		}
		bTicking = !!Queue.Num();
		return bTicking;
	}

	void ConsoleCommandTestComplete(bool bSuccess, FString Test, FAutomationTestExecutionInfo const& Results, FOutputDevice* Ar)
	{
		for ( TArray<FString>::TConstIterator ErrorIter( Results.Errors ); ErrorIter; ++ErrorIter )
		{
			Ar->Logf(ELogVerbosity::Error, TEXT("%s"), **ErrorIter);
		}
		for ( TArray<FString>::TConstIterator WarningIter( Results.Warnings ); WarningIter; ++WarningIter )
		{
			Ar->Logf(ELogVerbosity::Warning, TEXT("%s"), **WarningIter );
		}
		for ( TArray<FString>::TConstIterator LogItemIter( Results.LogItems ); LogItemIter; ++LogItemIter )
		{
			Ar->Logf(ELogVerbosity::Log, TEXT("%s"), **LogItemIter );
		}
		if (bSuccess)
		{
			Ar->Logf(ELogVerbosity::Log, TEXT("...Automation Test Succeeded (%s)"), *Test);
		}
		else
		{
			Ar->Logf(ELogVerbosity::Log, TEXT("...Automation Test Failed (%s)"), *Test);
		}
		bTestInPogress = false;
		NumTestsRun++;
		if (!bTicking)
		{
			GLog->Logf(ELogVerbosity::Log, TEXT("...Automation Test Queue Empty %d tests performed."), NumTestsRun);
			NumTestsRun = 0;

			//Quit when we are done if we have the command line option
			const FString CommandLine(FCommandLine::Get());
			if (CommandLine.Contains(TEXT("-AutomationTests=")))
			{
				//Let the current callstack complete then quit
				if (GEngine->IsEditor())
				{
					GEngine->DeferredCommands.Add(TEXT("QUIT_EDITOR"));
				}
				else
				{
					GEngine->DeferredCommands.Add(TEXT("QUIT"));
				}
			}
		}
	}

} QueueTests;

bool GenerateTestNamesFromCommandLine(const TCHAR* InTestCommands, TArray<FString>& OutTestNames)
{
	//Split the test names up
	TArray<FString> TestList;
	const FString StringCommand = InTestCommands;
	StringCommand.ParseIntoArray(TestList, TEXT(","), true);

	//Get the list of valid names
	TArray<FAutomationTestInfo> TestInfo;
	FAutomationTestFramework::GetInstance().GetValidTestNames(TestInfo);

	for (int32 TestListIndex = 0; TestListIndex < TestList.Num(); ++TestListIndex)
	{
		const FString CleanTestName = TestList[TestListIndex].Trim();
		for (int TestIndex = 0; TestIndex < TestInfo.Num(); ++TestIndex)
		{
			if (TestInfo[TestIndex].GetDisplayName().StartsWith(CleanTestName))
			{
				OutTestNames.Add(TestInfo[TestIndex].GetTestName());
			}
		}
	}

	return OutTestNames.Num() > 0;
}

bool DirectAutomationCommand(const TCHAR* Cmd, FOutputDevice* Ar = GLog)
{
	bool bResult = false;
	if(FParse::Command(&Cmd,TEXT("automation")))
	{
		const TCHAR* TempCmd = Cmd;
		bResult = true;
		if( FParse::Command( &TempCmd, TEXT( "list" ) ) )
		{
			TArray<FAutomationTestInfo> TestInfo;
			FAutomationTestFramework::GetInstance().GetValidTestNames( TestInfo );
			for ( int TestIndex = 0; TestIndex < TestInfo.Num(); ++TestIndex )
			{
				FString TestCommand = TestInfo[TestIndex].GetTestName();
				Ar->Logf(TEXT("%s"), *TestCommand);
			}
		}
		else if( FParse::Command(&TempCmd,TEXT("run")) )
		{
			FString Test(TempCmd);
			Test = Test.Trim();
			QueueTests.NewTest(Test);
		}
		else if (FParse::Command(&TempCmd, TEXT("CommandLineTests")))
		{
			TArray<FString> TestNames;

			const bool bSkipScreenshots = FParse::Param(FCommandLine::Get(), TEXT("NoScreenshots"));
			const bool bFullSizeScreenshots = FParse::Param(FCommandLine::Get(), TEXT("FullSizeScreenshots"));
			FAutomationTestFramework::GetInstance().SetScreenshotOptions(!bSkipScreenshots, bFullSizeScreenshots);

			if (GenerateTestNamesFromCommandLine(TempCmd, TestNames))
			{
				//Get the number of times to loop
				int32 NumTestLoops = 1;
				FParse::Value(FCommandLine::Get(), TEXT("TestLoops="), NumTestLoops);

				for (int32 LoopIndex = 0; LoopIndex < NumTestLoops; ++LoopIndex)
				{
					for (int32 TestIndex = 0; TestIndex < TestNames.Num(); ++TestIndex)
					{
						QueueTests.NewTest(TestNames[TestIndex]);
					}
				}
			}
		}
		else if( FParse::Command(&TempCmd,TEXT("runall")) )
		{
			int32 Mod = 0;
			int32 Rem = 0;
			FParse::Value(Cmd, TEXT("MOD="), Mod);
			FParse::Value(Cmd, TEXT("REM="), Rem);
			TArray<FAutomationTestInfo> TestInfo;
			FAutomationTestFramework::GetInstance().GetValidTestNames( TestInfo );
			for ( int TestIndex = 0; TestIndex < TestInfo.Num(); ++TestIndex )
			{
				if (!Mod || TestIndex % Mod == Rem)
				{
					QueueTests.NewTest(TestInfo[TestIndex].GetTestName());
				}
			}
		}
		else
		{
			bResult = false;
		}
	}
	return bResult;
}




static class FAutomationTestCmd : private FSelfRegisteringExec
{
public:
	/** Console commands, see embeded usage statement **/
	virtual bool Exec( UWorld*, const TCHAR* Cmd, FOutputDevice& Ar) override
	{
		return DirectAutomationCommand(Cmd, &Ar);
	}
} AutomationTestCmd;


#undef LOCTEXT_NAMESPACE
