// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AutomationWorkerPrivatePCH.h"

#if WITH_EDITOR
#include "AssetRegistryModule.h"
#endif

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
	bSendAnalytics = false;
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
			for ( auto& Error : ExecutionInfo.Errors )
			{
				Message->Errors.Add(FAutomationWorkerEvent(Error));
			}
			Message->Warnings = ExecutionInfo.Warnings;
			Message->Logs = ExecutionInfo.LogItems;

			// sending though the endpoint will free Message memory, so analytics need to be sent first
			if (bSendAnalytics)
			{
				if (!FAutomationAnalytics::IsInitialized())
				{
					FAutomationAnalytics::Initialize();
				}
				FAutomationAnalytics::FireEvent_AutomationTestResults(Message, BeautifiedTestName);
				SendAnalyticsEvents(ExecutionInfo.AnalyticsItems);
			}

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
	MessageEndpoint->Send(new FAutomationWorkerRequestTestsReplyComplete(), ControllerAddress);
}


/* FAutomationWorkerModule callbacks
 *****************************************************************************/

void FAutomationWorkerModule::HandleFindWorkersMessage(const FAutomationWorkerFindWorkers& Message, const IMessageContextRef& Context)
{
	// Set the Instance name to be the same as the session browser. This information should be shared at some point
	if ((Message.SessionId == FApp::GetSessionId()) && (Message.Changelist == 10000))
	{
		TestRequesterAddress = Context->GetSender();

#if WITH_EDITOR
		//If the asset registry is loading assets then we'll wait for it to stop before running our automation tests.
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		if (AssetRegistryModule.Get().IsLoadingAssets())
		{
			if (!AssetRegistryModule.Get().OnFilesLoaded().IsBoundToObject(this))
			{
				AssetRegistryModule.Get().OnFilesLoaded().AddRaw(this, &FAutomationWorkerModule::SendWorkerFound);
				GLog->Logf(ELogVerbosity::Log, TEXT("...Forcing Asset Registry Load For Automation"));
			}
		}
		else
#endif
		{
			//If the registry is not loading then we'll just go ahead and run our tests.
			SendWorkerFound();
		}
	}
}


void FAutomationWorkerModule::SendWorkerFound()
{
	FAutomationWorkerFindWorkersResponse* Response = new FAutomationWorkerFindWorkersResponse();

	FString OSMajorVersionString, OSSubVersionString;
	FPlatformMisc::GetOSVersions(OSMajorVersionString, OSSubVersionString);

	FString OSVersionString = OSMajorVersionString + TEXT(" ") + OSSubVersionString;
	FString CPUModelString = FPlatformMisc::GetCPUBrand().Trim();

	Response->DeviceName = FPlatformProcess::ComputerName();
	Response->InstanceName = FString::Printf(TEXT("%s-%i"), FPlatformProcess::ComputerName(), FPlatformProcess::GetCurrentProcessId());
	Response->Platform = FPlatformProperties::PlatformName();
	Response->SessionId = FApp::GetSessionId();
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

	MessageEndpoint->Send(Response, TestRequesterAddress);
	TestRequesterAddress.Invalidate();
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
	FAutomationTestFramework::GetInstance().SetRequestedTestFilter(Message.RequestedTestFlags);
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
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(ScreenShotName), bTree);
			FFileHelper::SaveArrayToFile(CompressedBitmap, *ScreenShotName);
		}
	}

}
#endif


void FAutomationWorkerModule::HandleRunTestsMessage( const FAutomationWorkerRunTests& Message, const IMessageContextRef& Context )
{
	ExecutionCount = Message.ExecutionCount;
	TestName = Message.TestName;
	BeautifiedTestName = Message.BeautifiedTestName;
	bSendAnalytics = Message.bSendAnalytics;
	TestRequesterAddress = Context->GetSender();
	FAutomationTestFramework::GetInstance().SetScreenshotOptions(Message.bScreenshotsEnabled, Message.bUseFullSizeScreenShots);

	// Always allow the first network command to execute
	bExecuteNextNetworkCommand = true;

	// We are not executing network command sub-commands right now
	bExecutingNetworkCommandResults = false;

	FAutomationTestFramework::GetInstance().StartTestByName(Message.TestName, Message.RoleIndex);
}


//dispatches analytics events to the data collector
void FAutomationWorkerModule::SendAnalyticsEvents(TArray<FString>& InAnalyticsItems)
{
	for (int32 i = 0; i < InAnalyticsItems.Num(); ++i)
	{
		FString EventString = InAnalyticsItems[i];
		if( EventString.EndsWith( TEXT( ",PERF" ) ) )
		{
			// Chop the ",PERF" off the end
			EventString = EventString.Left( EventString.Len() - 5 );

			FAutomationPerformanceSnapshot PerfSnapshot;
			PerfSnapshot.FromCommaDelimitedString( EventString );
			
			RecordPerformanceAnalytics( PerfSnapshot );
		}
	}
}




void FAutomationWorkerModule::RecordPerformanceAnalytics( const FAutomationPerformanceSnapshot& PerfSnapshot )
{
	// @todo: Pass in additional performance capture data from incoming FAutomationPerformanceSnapshot!

	FAutomationAnalytics::FireEvent_FPSCapture(PerfSnapshot);
}

#undef LOCTEXT_NAMESPACE
