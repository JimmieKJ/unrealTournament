// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTGameEngine.h"
#include "UTAnalytics.h"

UUTGameEngine::UUTGameEngine(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	bFirstRun = true;
	ReadEULACaption = NSLOCTEXT("UTGameEngine", "ReadEULACaption", "READ ME FIRST");
	ReadEULAText = NSLOCTEXT("UTGameEngine", "ReadEULAText", "EULA TEXT");
	GameNetworkVersion = 3008004;

	CurrentMaxTickRate = 100.f;
	// Running average delta time, initial value at 100 FPS so fast machines don't have to creep up
	// to a good frame rate due to code limiting upward "mobility".
	RunningAverageDeltaTime = 1 / 100.f;
}


void UUTGameEngine::Init(IEngineLoop* InEngineLoop)
{
	// @TODO FIXMESTEVE temp hack for network compatibility code
	GEngineNetVersion = GameNetworkVersion;
	UE_LOG(UT, Warning, TEXT("************************************Set Net Version %d"), GEngineNetVersion);

	if (bFirstRun)
	{
#if PLATFORM_LINUX
		// Don't write code like this, don't copy paste this anywhere. Will get removed with Engine 4.5
		// hacked until we get an engine with FLinuxPlatformMisc::MessageBoxExt
		EAppReturnType::Type LINUXMessageBoxExt(EAppMsgType::Type MsgType, const TCHAR* Text, const TCHAR* Caption);
		if (LINUXMessageBoxExt(EAppMsgType::YesNo, *ReadEULAText.ToString(), *ReadEULACaption.ToString()) != EAppReturnType::Yes)
		{
			FPlatformMisc::RequestExit(true);
			return;
		}
#else
		if (FPlatformMisc::MessageBoxExt(EAppMsgType::YesNo, *ReadEULAText.ToString(), *ReadEULACaption.ToString()) != EAppReturnType::Yes)
		{
			FPlatformMisc::RequestExit(true);
			return;
		}
#endif
		bFirstRun = false;
		SaveConfig();
		GConfig->Flush(false);
	}

	FUTAnalytics::Initialize();
	Super::Init(InEngineLoop);
}

void UUTGameEngine::PreExit()
{
	Super::PreExit();
	FUTAnalytics::Shutdown();
}

// @TODO FIXMESTEVE - we want open to be relative like it used to be
bool UUTGameEngine::HandleOpenCommand(const TCHAR* Cmd, FOutputDevice& Ar, UWorld *InWorld)
{
	return HandleTravelCommand(Cmd, Ar, InWorld);
}

bool UUTGameEngine::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Out)
{
	if (FParse::Command(&Cmd, TEXT("START")))
	{
		FWorldContext &WorldContext = GetWorldContextFromWorldChecked(InWorld);
		FURL TestURL(&WorldContext.LastURL, Cmd, TRAVEL_Absolute);
		// make sure the file exists if we are opening a local file
		if (TestURL.IsLocalInternal() && !MakeSureMapNameIsValid(TestURL.Map))
		{
			Out.Logf(TEXT("ERROR: The map '%s' does not exist."), *TestURL.Map);
			return true;
		}
		else
		{
			SetClientTravel(InWorld, Cmd, TRAVEL_Absolute);
			return true;
		}
	}
	else
	{
		return Super::Exec(InWorld, Cmd, Out);
	}
}

void UUTGameEngine::Tick(float DeltaSeconds, bool bIdleMode)
{
	// HACK: make sure our default URL options are in all travel URLs since FURL code to do this was removed
	for (int32 WorldIdx = 0; WorldIdx < WorldList.Num(); ++WorldIdx)
	{
		FWorldContext& Context = WorldList[WorldIdx];
		if (!Context.TravelURL.IsEmpty())
		{
			FURL DefaultURL;
			DefaultURL.LoadURLConfig(TEXT("DefaultPlayer"), GGameIni);
			FURL NewURL(&DefaultURL, *Context.TravelURL, TRAVEL_Absolute);
			for (int32 i = 0; i < DefaultURL.Op.Num(); i++)
			{
				FString OpKey;
				DefaultURL.Op[i].Split(TEXT("="), &OpKey, NULL);
				if (!NewURL.HasOption(*OpKey))
				{
					new(NewURL.Op) FString(DefaultURL.Op[i]);
				}
			}
			Context.TravelURL = NewURL.ToString();
		}
	}

	SmoothFrameRate(DeltaSeconds);

	Super::Tick(DeltaSeconds, bIdleMode);
}

#if PLATFORM_LINUX

#include "SDL.h"

EAppReturnType::Type LINUXMessageBoxExt(EAppMsgType::Type MsgType, const TCHAR* Text, const TCHAR* Caption)
{
	int NumberOfButtons = 0;

	if (SDL_WasInit(SDL_INIT_VIDEO) == 0)
	{
		SDL_InitSubSystem(SDL_INIT_VIDEO);
	}

	SDL_MessageBoxButtonData *Buttons = nullptr;

	switch (MsgType)
	{
	case EAppMsgType::Ok:
		NumberOfButtons = 1;
		Buttons = new SDL_MessageBoxButtonData[NumberOfButtons];
		Buttons[0].flags = SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
		Buttons[0].text = "Ok";
		Buttons[0].buttonid = EAppReturnType::Ok;
		break;

	case EAppMsgType::YesNo:
		NumberOfButtons = 2;
		Buttons = new SDL_MessageBoxButtonData[NumberOfButtons];
		Buttons[0].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		Buttons[0].text = "Yes";
		Buttons[0].buttonid = EAppReturnType::Yes;
		Buttons[1].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		Buttons[1].text = "No";
		Buttons[1].buttonid = EAppReturnType::No;
		break;

	case EAppMsgType::OkCancel:
		NumberOfButtons = 2;
		Buttons = new SDL_MessageBoxButtonData[NumberOfButtons];
		Buttons[0].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		Buttons[0].text = "Ok";
		Buttons[0].buttonid = EAppReturnType::Ok;
		Buttons[1].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		Buttons[1].text = "Cancel";
		Buttons[1].buttonid = EAppReturnType::Cancel;
		break;

	case EAppMsgType::YesNoCancel:
		NumberOfButtons = 3;
		Buttons = new SDL_MessageBoxButtonData[NumberOfButtons];
		Buttons[0].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		Buttons[0].text = "Yes";
		Buttons[0].buttonid = EAppReturnType::Yes;
		Buttons[1].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		Buttons[1].text = "No";
		Buttons[1].buttonid = EAppReturnType::No;
		Buttons[2].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		Buttons[2].text = "Cancel";
		Buttons[2].buttonid = EAppReturnType::Cancel;
		break;

	case EAppMsgType::CancelRetryContinue:
		NumberOfButtons = 3;
		Buttons = new SDL_MessageBoxButtonData[NumberOfButtons];
		Buttons[0].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		Buttons[0].text = "Continue";
		Buttons[0].buttonid = EAppReturnType::Continue;
		Buttons[1].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		Buttons[1].text = "Retry";
		Buttons[1].buttonid = EAppReturnType::Retry;
		Buttons[2].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		Buttons[2].text = "Cancel";
		Buttons[2].buttonid = EAppReturnType::Cancel;
		break;

	case EAppMsgType::YesNoYesAllNoAll:
		NumberOfButtons = 4;
		Buttons = new SDL_MessageBoxButtonData[NumberOfButtons];
		Buttons[0].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		Buttons[0].text = "Yes";
		Buttons[0].buttonid = EAppReturnType::Yes;
		Buttons[1].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		Buttons[1].text = "No";
		Buttons[1].buttonid = EAppReturnType::No;
		Buttons[2].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		Buttons[2].text = "Yes to all";
		Buttons[2].buttonid = EAppReturnType::YesAll;
		Buttons[3].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		Buttons[3].text = "No to all";
		Buttons[3].buttonid = EAppReturnType::NoAll;
		break;

	case EAppMsgType::YesNoYesAllNoAllCancel:
		NumberOfButtons = 5;
		Buttons = new SDL_MessageBoxButtonData[NumberOfButtons];
		Buttons[0].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		Buttons[0].text = "Yes";
		Buttons[0].buttonid = EAppReturnType::Yes;
		Buttons[1].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		Buttons[1].text = "No";
		Buttons[1].buttonid = EAppReturnType::No;
		Buttons[2].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		Buttons[2].text = "Yes to all";
		Buttons[2].buttonid = EAppReturnType::YesAll;
		Buttons[3].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		Buttons[3].text = "No to all";
		Buttons[3].buttonid = EAppReturnType::NoAll;
		Buttons[4].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		Buttons[4].text = "Cancel";
		Buttons[4].buttonid = EAppReturnType::Cancel;
		break;

	case EAppMsgType::YesNoYesAll:
		NumberOfButtons = 3;
		Buttons = new SDL_MessageBoxButtonData[NumberOfButtons];
		Buttons[0].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		Buttons[0].text = "Yes";
		Buttons[0].buttonid = EAppReturnType::Yes;
		Buttons[1].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		Buttons[1].text = "No";
		Buttons[1].buttonid = EAppReturnType::No;
		Buttons[2].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		Buttons[2].text = "Yes to all";
		Buttons[2].buttonid = EAppReturnType::YesAll;
		break;
	}

	SDL_MessageBoxData MessageBoxData =
	{
		SDL_MESSAGEBOX_INFORMATION,
		NULL, // No parent window
		TCHAR_TO_UTF8(Caption),
		TCHAR_TO_UTF8(Text),
		NumberOfButtons,
		Buttons,
		NULL // Default color scheme
	};

	int ButtonPressed = -1;
	if (SDL_ShowMessageBox(&MessageBoxData, &ButtonPressed) == -1)
	{
		UE_LOG(LogInit, Fatal, TEXT("Error Presenting MessageBox: %s\n"), ANSI_TO_TCHAR(SDL_GetError()));
		// unreachable
		return EAppReturnType::Cancel;
	}

	delete[] Buttons;

	return ButtonPressed == -1 ? EAppReturnType::Cancel : static_cast<EAppReturnType::Type>(ButtonPressed);
}
#endif

static TAutoConsoleVariable<int32> CVarUnsteadyFPS(
	TEXT("ut.UnsteadyFPS"), 0,
	TEXT("Causes FPS to bounce around randomly in the 85-120 range."));

static TAutoConsoleVariable<int32> CVarSmoothFrameRate(
	TEXT("ut.SmoothFrameRate"), 1,
	TEXT("Enable frame rate smoothing."));

float UUTGameEngine::GetMaxTickRate(float DeltaTime, bool bAllowFrameRateSmoothing)
{
	float MaxTickRate = 0;

	// Don't smooth here if we're a dedicated server
	if (IsRunningDedicatedServer())
	{
		return Super::GetMaxTickRate(DeltaTime, bAllowFrameRateSmoothing);
	}
		
	if (bSmoothFrameRate && bAllowFrameRateSmoothing && CVarSmoothFrameRate.GetValueOnGameThread())
	{
		MaxTickRate = CurrentMaxTickRate;
		
		// Make sure that we don't try to smooth below a certain minimum
		MaxTickRate = FMath::Max(FrameRateMinimum, MaxTickRate);
	}

	if (CVarUnsteadyFPS.GetValueOnGameThread())
	{
		static float LastMaxTickRate = 85.f;
		float RandDelta = FMath::FRandRange(-5.f, 5.f);
		MaxTickRate = FMath::Clamp(LastMaxTickRate + RandDelta, 85.f, 120.f);
		LastMaxTickRate = MaxTickRate;
	}

	// Hard cap at frame rate cap
	if (FrameRateCap > 0)
	{
		MaxTickRate = FMath::Min(FrameRateCap, MaxTickRate);
	}
	
	return MaxTickRate;
}

void UUTGameEngine::SmoothFrameRate(float DeltaTime)
{
	// Adjust the maximum tick rate dynamically
	// As maximum tick rate is met consistently, shorten the time period to try to reach equilibrium faster
	if (1.f / DeltaTime < CurrentMaxTickRate * 0.9f)
	{
		MissedFrames++;
		MadeFrames = 0;
		MadeFramesStreak = 0;
		// Miss enough frames during this sample period, apply MissedFramePenalty
		if (MissedFrames > MissedFrameThreshold)
		{
			CurrentMaxTickRate *= MissedFramePenalty;
			MissedFrames = 0;
			MadeFrames = 0;
			//UE_LOG(UT, Log, TEXT("Missed framerate %f"), CurrentMaxTickRate);
		}
	}
	else
	{
		MadeFrames++;
		if (MadeFrames >= CurrentMaxTickRate * FMath::Max(MadeFrameMinimumThreshold, MadeFrameStartingThreshold - MadeFramesStreak))
		{
			// We made framerate enough times in a row, creep max back up
			MadeFramesStreak++;
			CurrentMaxTickRate += MadeFrameBonus;
			MadeFrames = 0;
			MissedFrames = 0;
			//UE_LOG(UT, Log, TEXT("Made framerate %f"), CurrentMaxTickRate);
		}
	}
}