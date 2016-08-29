// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "SlateBasics.h"
#include "AutomationCommon.h"
#include "ImageUtils.h"
#include "ShaderCompiler.h"		// GShaderCompilingManager
#include "GameFramework/GameMode.h"

#if WITH_EDITOR
#include "FileHelpers.h"
#endif

#include "Matinee/MatineeActor.h"

#if (WITH_DEV_AUTOMATION_TESTS || WITH_PERF_AUTOMATION_TESTS)

DEFINE_LOG_CATEGORY_STATIC(LogEngineAutomationLatentCommand, Log, All);
DEFINE_LOG_CATEGORY(LogEditorAutomationTests);
DEFINE_LOG_CATEGORY(LogEngineAutomationTests);

//declare static variable
FOnEditorAutomationMapLoad AutomationCommon::OnEditorAutomationMapLoad;

///////////////////////////////////////////////////////////////////////
// Common Latent commands

namespace AutomationCommon
{
	/** These save a PNG and get sent over the network */
	static void SaveWindowAsScreenshot(TSharedRef<SWindow> Window, const FString& FileName)
	{
		TSharedRef<SWidget> WindowRef = Window;

		TArray<FColor> OutImageData;
		FIntVector OutImageSize;
		if (FSlateApplication::Get().TakeScreenshot(WindowRef, OutImageData, OutImageSize))
		{
			FAutomationTestFramework::GetInstance().OnScreenshotCaptured().ExecuteIfBound(OutImageSize.X, OutImageSize.Y, OutImageData, FileName);
		}
	}

	// @todo this is a temporary solution. Once we know how to get test's hands on a proper world
	// this function should be redone/removed
	UWorld* GetAnyGameWorld()
	{
		UWorld* TestWorld = nullptr;
		const TIndirectArray<FWorldContext>& WorldContexts = GEngine->GetWorldContexts();
		for ( const FWorldContext& Context : WorldContexts )
		{
			if ( ( ( Context.WorldType == EWorldType::PIE ) || ( Context.WorldType == EWorldType::Game ) ) && ( Context.World() != NULL ) )
			{
				TestWorld = Context.World();
				break;
			}
		}

		return TestWorld;
	}
}

bool AutomationOpenMap(const FString& MapName)
{
	bool bCanProceed = true;
	FString OutString = TEXT("");
#if WITH_EDITOR
	if (GIsEditor && AutomationCommon::OnEditorAutomationMapLoad.IsBound())
	{
		AutomationCommon::OnEditorAutomationMapLoad.Broadcast(MapName, &OutString);
	}
	else
#endif
	{
		UWorld* TestWorld = AutomationCommon::GetAnyGameWorld();

		if ( TestWorld->GetMapName() != MapName )
		{
			FString OpenCommand = FString::Printf(TEXT("Open %s"), *MapName);
			GEngine->Exec(TestWorld, *OpenCommand);
		}

		//Wait for map to load - need a better way to determine if loaded
		ADD_LATENT_AUTOMATION_COMMAND(FWaitForMapToLoadCommand());
	}

	return (OutString.IsEmpty());
}


bool FWaitLatentCommand::Update()
{
	float NewTime = FPlatformTime::Seconds();
	if (NewTime - StartTime >= Duration)
	{
		return true;
	}
	return false;
}

bool FTakeActiveEditorScreenshotCommand::Update()
{
	AutomationCommon::SaveWindowAsScreenshot(FSlateApplication::Get().GetActiveTopLevelWindow().ToSharedRef(),ScreenshotName);
	return true;
}

bool FTakeEditorScreenshotCommand::Update()
{
	AutomationCommon::SaveWindowAsScreenshot(ScreenshotParameters.CurrentWindow.ToSharedRef(), ScreenshotParameters.ScreenshotName);
	return true;
}

bool FLoadGameMapCommand::Update()
{
	check(GEngine->GetWorldContexts().Num() == 1);
	check(GEngine->GetWorldContexts()[0].WorldType == EWorldType::Game);

	UE_LOG(LogEngineAutomationTests, Log, TEXT("Loading Map Now. '%s'"), *MapName);
	GEngine->Exec(GEngine->GetWorldContexts()[0].World(), *FString::Printf(TEXT("Open %s"), *MapName));
	return true;
}

bool FExitGameCommand::Update()
{
	UWorld* TestWorld = AutomationCommon::GetAnyGameWorld();

	if ( APlayerController* TargetPC = UGameplayStatics::GetPlayerController(TestWorld, 0) )
	{
		TargetPC->ConsoleCommand(TEXT("Exit"), true);
	}

	return true;
}

bool FRequestExitCommand::Update()
{
	GIsRequestingExit = true;
	return true;
}

bool FWaitForMapToLoadCommand::Update()
{
	//TODO Automation we need a better way to know when the map finished loading.

	//TODO - Is there a better way to see if the map is loaded?  Are Actors Initialized isn't right in Fortnite...
	UWorld* TestWorld = AutomationCommon::GetAnyGameWorld();

	if ( TestWorld && TestWorld->AreActorsInitialized() )
	{
		AGameMode* GameMode = TestWorld->GetAuthGameMode();
		if ( GameMode && GameMode->HasMatchStarted() )
		{
			return true;
		}
	}

	return false;
}

///////////////////////////////////////////////////////////////////////
// Common Latent commands which are used across test type. I.e. Engine, Network, etc...


bool FPlayMatineeLatentCommand::Update()
{
	if (MatineeActor)
	{
		UE_LOG(LogEngineAutomationLatentCommand, Log, TEXT("Triggering the matinee named: '%s'"), *MatineeActor->GetName())

		//force this matinee to not be looping so it doesn't infinitely loop
		MatineeActor->bLooping = false;
		MatineeActor->Play();
	}
	return true;
}


bool FWaitForMatineeToCompleteLatentCommand::Update()
{
	bool bTestComplete = true;
	if (MatineeActor)
	{
		bTestComplete = !MatineeActor->bIsPlaying;
	}
	return bTestComplete;
}


bool FExecStringLatentCommand::Update()
{
	UE_LOG(LogEngineAutomationLatentCommand, Log, TEXT("Executing the console command: '%s'"), *ExecCommand);

	if (GEngine->GameViewport)
	{
		GEngine->GameViewport->Exec(NULL, *ExecCommand, *GLog);
	}
	else
	{
		GEngine->Exec(NULL, *ExecCommand);
	}
	return true;
}


bool FEngineWaitLatentCommand::Update()
{
	float NewTime = FPlatformTime::Seconds();
	if (NewTime - StartTime >= Duration)
	{
		return true;
	}
	return false;
}

ENGINE_API uint32 GStreamAllResourcesStillInFlight = -1;
bool FStreamAllResourcesLatentCommand::Update()
{
	float LocalStartTime = FPlatformTime::Seconds();

	GStreamAllResourcesStillInFlight = IStreamingManager::Get().StreamAllResources(Duration);

	float Time = FPlatformTime::Seconds();

	if(GStreamAllResourcesStillInFlight)
	{
		UE_LOG(LogEngineAutomationLatentCommand, Warning, TEXT("StreamAllResources() waited for %.2fs but %d resources are still in flight."), Time - LocalStartTime, GStreamAllResourcesStillInFlight);
	}
	else
	{
		UE_LOG(LogEngineAutomationLatentCommand, Log, TEXT("StreamAllResources() waited for %.2fs (max duration: %.2f)."), Time - LocalStartTime, Duration);
	}

	return true;
}

bool FEnqueuePerformanceCaptureCommands::Update()
{
	//for every matinee actor in the level
	for (TObjectIterator<AMatineeActor> It; It; ++It)
	{
		AMatineeActor* MatineeActor = *It;
		if (MatineeActor && MatineeActor->GetName().Contains(TEXT("Automation")))
		{
			//add latent action to execute this matinee
			ADD_LATENT_AUTOMATION_COMMAND(FPlayMatineeLatentCommand(MatineeActor));

			//add action to wait until matinee is complete
			ADD_LATENT_AUTOMATION_COMMAND(FWaitForMatineeToCompleteLatentCommand(MatineeActor));
		}
	}

	return true;
}


bool FMatineePerformanceCaptureCommand::Update()
{
	//for every matinee actor in the level
	for (TObjectIterator<AMatineeActor> It; It; ++It)
	{
		AMatineeActor* MatineeActor = *It;
		FString MatineeFOOName = MatineeActor->GetName();
		if (MatineeActor->GetName().Equals(MatineeName, ESearchCase::IgnoreCase))
		{


			//add latent action to execute this matinee
			ADD_LATENT_AUTOMATION_COMMAND(FPlayMatineeLatentCommand(MatineeActor));

			//Run the Stat FPS Chart command
			ADD_LATENT_AUTOMATION_COMMAND(FExecWorldStringLatentCommand(TEXT("StartFPSChart")));

			//add action to wait until matinee is complete
			ADD_LATENT_AUTOMATION_COMMAND(FWaitForMatineeToCompleteLatentCommand(MatineeActor));

			//Stop the Stat FPS Chart command
			ADD_LATENT_AUTOMATION_COMMAND(FExecWorldStringLatentCommand(TEXT("StopFPSChart")));
		}
		else
		{
			UE_LOG(LogEngineAutomationLatentCommand, Log, TEXT("'%s' is not the matinee name that is being searched for."), *MatineeActor->GetName())
		}
	}

	return true;

}


bool FExecWorldStringLatentCommand::Update()
{
	check(GEngine->GetWorldContexts().Num() == 1);
	check(GEngine->GetWorldContexts()[0].WorldType == EWorldType::Game);

	UE_LOG(LogEngineAutomationLatentCommand, Log, TEXT("Running Exec Command. '%s'"), *ExecCommand);
	GEngine->Exec(GEngine->GetWorldContexts()[0].World(), *ExecCommand);
	return true;
}


/**
* This will cause the test to wait for the shaders to finish compiling before moving on.
*/
bool FWaitForShadersToFinishCompilingInGame::Update()
{
	if (GShaderCompilingManager)
	{
		UE_LOG(LogEditorAutomationTests, Log, TEXT("Waiting for %i shaders to finish."), GShaderCompilingManager->GetNumRemainingJobs());
		GShaderCompilingManager->FinishAllCompilation();
		UE_LOG(LogEditorAutomationTests, Log, TEXT("Done waiting for shaders to finish."));
	}
	return true;
}
#endif //WITH_DEV_AUTOMATION_TESTS