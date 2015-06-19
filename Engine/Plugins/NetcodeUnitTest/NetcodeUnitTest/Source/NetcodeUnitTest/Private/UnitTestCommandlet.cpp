// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "NetcodeUnitTestPCH.h"
#include "NUTUtil.h"
#include "NUTUtilNet.h"

#include "UnitTestCommandlet.h"
#include "UnitTestManager.h"

#include "SLogDialog.h"


// @todo JohnB: Fix StandaloneRenderer support for static builds
#if !IS_MONOLITHIC
#include "StandaloneRenderer.h"
#endif

#include "Engine/GameInstance.h"

#include "UnrealClient.h"
#include "SlateBasics.h"


// @todo JohnB: If you later end up doing test client instances that are unit tested against client netcode
//				(same way as you do server instances at the moment, for testing server netcode),
//				then this commandlet code is probably an excellent base for setting up minimal standalone clients like that,
//				as it's intended to strip-down running unit tests, from a minimal client itself


// Enable access to the private UGameEngine.GameInstance value, using the GET_PRIVATE macro
IMPLEMENT_GET_PRIVATE_VAR(UGameEngine, GameInstance, UGameInstance*);

/** The exit-confirmation dialog, displayed to the user when all unit tests are complete */
static TSharedPtr<SWindow> ConfirmDialog=NULL;

/** Whether or not exiting the commandlet has been confirmed by user input yet */
static bool bConfirmedExit=false;


/**
 * UUnitTestCommandlet
 */
UUnitTestCommandlet::UUnitTestCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Start off with GIsClient set to false, so we don't init viewport etc., but turn it on later for netcode
	IsClient = false;

	IsServer = false;
	IsEditor = false;

	LogToConsole = true;
	ShowErrorCount = true;
}

int32 UUnitTestCommandlet::Main(const FString& Params)
{
	// @todo JohnB: Fix StandaloneRenderer support for static builds
#if IS_MONOLITHIC
	UE_LOG(LogUnitTest, Log, TEXT("NetcodeUnitTest commandlet not currently supported in static/monolithic builds"));

#else
	GIsRequestingExit = false;

	// @todo JohnB: Steam detection doesn't seem to work this early on, but does work further down the line;
	//				try to find a way, to detect it as early as possible

	// NetcodeUnitTest is not compatible with Steam; if Steam is running/detected, abort immediately
	// @todo JohnB: Add support for Steam
	if (NUTNet::IsSteamNetDriverAvailable())
	{
		UE_LOG(LogUnitTest, Log, TEXT("NetcodeUnitTest does not currently support Steam. Close Steam before running."));
		GIsRequestingExit = true;
	}

	if (!GIsRequestingExit)
	{
		GIsRunning = true;

		// Hack-set the engine GameViewport, so that setting GIsClient, doesn't cause an auto-exit
		// @todo JohnB: If you later remove the GIsClient setting code below, remove this as well
		if (GEngine->GameViewport == NULL)
		{
			UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);

			// GameInstace = GameEngine->GameInstance;
			UGameInstance* GameInstance = GET_PRIVATE(UGameEngine, GameEngine, GameInstance);

			if (GameEngine != NULL)
			{
				UGameViewportClient* NewViewport = NewObject<UGameViewportClient>(GameEngine);
				FWorldContext* CurContext = GameInstance->GetWorldContext();

				GameEngine->GameViewport = NewViewport;
				NewViewport->Init(*CurContext, GameInstance);

				// Set the internal FViewport, for the new game viewport, to avoid another bit of auto-exit code
				NewViewport->Viewport = new FDummyViewport(NewViewport);

				// Set the main engine world context game viewport, to match the newly created viewport, in order to prevent crashes
				CurContext->GameViewport = NewViewport;
			}
		}


		// Now, after init stages are done, enable GIsClient for netcode and such
		GIsClient = true;


		// Before running any unit tests, create a world object, which will contain the unit tests manager etc.
		//	(otherwise, when the last unit test world is cleaned up, the unit test manager stops functioning)
		UWorld* UnitTestWorld = NUTNet::CreateUnitTestWorld(false);


		const TCHAR* ParamsRef = *Params;
		FString UnitTestParam = TEXT("");
		FString UnitCmd = TEXT("UnitTest ");


		if (FParse::Value(ParamsRef, TEXT("UnitTest="), UnitTestParam))
		{
			UnitCmd += UnitTestParam;
		}
		else
		{
			UnitCmd += TEXT("all");
		}

		GEngine->Exec(UnitTestWorld, *UnitCmd);


		// NOTE: This main loop is partly based off of FileServerCommandlet
		while (GIsRunning && !GIsRequestingExit)
		{
			GEngine->UpdateTimeAndHandleMaxTickRate();
			GEngine->Tick(FApp::GetDeltaTime(), false);

			if (FSlateApplication::IsInitialized())
			{
				FSlateApplication::Get().PumpMessages();
				FSlateApplication::Get().Tick();
			}


			// Execute deferred commands
			for (int32 DeferredCommandsIndex=0; DeferredCommandsIndex<GEngine->DeferredCommands.Num(); DeferredCommandsIndex++)
			{
				GEngine->Exec(UnitTestWorld, *GEngine->DeferredCommands[DeferredCommandsIndex], *GLog);
			}

			GEngine->DeferredCommands.Empty();


			FPlatformProcess::Sleep(0);


			// When the unit tests complete, open an exit-confirmation window, and when that closes, exit the main loop
			// NOTE: This will not execute, if the last open slate window is closed (such as when clicking 'yes' to 'abort all' dialog);
			//			in that circumstance, GIsRequestingExit gets set, by the internal engine code
			if (GUnitTestManager == NULL || !GUnitTestManager->IsRunningUnitTests())
			{
				if (bConfirmedExit || FApp::IsUnattended())
				{
					// Wait until the status window is closed, if it is still open, before exiting
					if (GUnitTestManager == NULL || !GUnitTestManager->StatusWindow.IsValid())
					{
						GIsRequestingExit = true;
					}
				}
				else if (!ConfirmDialog.IsValid())
				{
					FText CompleteMsg = FText::FromString(TEXT("Unit test commandlet complete."));
					FText CompleteTitle = FText::FromString(TEXT("Unit tests complete"));

					FOnLogDialogResult DialogCallback = FOnLogDialogResult::CreateLambda(
						[&](const TSharedRef<SWindow>& DialogWindow, EAppReturnType::Type Result, bool bNoResult)
						{
							if (DialogWindow == ConfirmDialog)
							{
								bConfirmedExit = true;
							}
						});

					ConfirmDialog = OpenLogDialog_NonModal(EAppMsgType::Ok, CompleteMsg, CompleteTitle, DialogCallback);


					// If the 'abort all' dialog was open, close it now as it is redundant;
					// can't close it before opening above dialog though, otherwise no slate windows, triggers an early exit
					if (GUnitTestManager != NULL && GUnitTestManager->AbortAllDialog.IsValid())
					{
						GUnitTestManager->AbortAllDialog->RequestDestroyWindow();

						GUnitTestManager->AbortAllDialog.Reset();
					}
				}
			}
		}

		// Cleanup the unit test world
		NUTNet::MarkUnitTestWorldForCleanup(UnitTestWorld, true);


		GIsRunning = false;
	}
#endif

	return (GWarn->Errors.Num() == 0 ? 0 : 1);
}

void UUnitTestCommandlet::CreateCustomEngine(const FString& Params)
{
	// @todo JohnB: Fix StandaloneRenderer support for static builds
#if !IS_MONOLITHIC
	FSlateApplication::InitializeAsStandaloneApplication(GetStandardStandaloneRenderer());
#endif
}
