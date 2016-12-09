// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AutomationBlueprintFunctionLibrary.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "EngineGlobals.h"
#include "UnrealClient.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Texture.h"
#include "Engine/GameViewportClient.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Tests/AutomationCommon.h"
#include "Logging/MessageLog.h"
#include "TakeScreenshotAfterTimeLatentAction.h"
#include "HighResScreenshot.h"
#include "Slate/SceneViewport.h"
#include "Tests/AutomationTestSettings.h"
#include "Slate/WidgetRenderer.h"
#include "DelayAction.h"
#include "Widgets/SViewport.h"
#include "Framework/Application/SlateApplication.h"

#define LOCTEXT_NAMESPACE "Automation"

DEFINE_LOG_CATEGORY_STATIC(BlueprintAssertion, Error, Error)
DEFINE_LOG_CATEGORY_STATIC(AutomationFunctionLibrary, Log, Log)

static TAutoConsoleVariable<int32> CVarAutomationScreenshotResolutionWidth(
	TEXT("AutomationScreenshotResolutionWidth"),
	0,
	TEXT("The width of automation screenshots."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarAutomationScreenshotResolutionHeight(
	TEXT("AutomationScreenshotResolutionHeight"),
	0,
	TEXT("The height of automation screenshots."),
	ECVF_Default);

FAutomationScreenshotOptions::FAutomationScreenshotOptions()
	: Resolution(ForceInit)
	, Delay(0.2f)
	, bDisableNoisyRenderingFeatures(false)
	, VisualizeBuffer(NAME_None)
	, Tolerance(EComparisonTolerance::Low)
	, ToleranceAmount()
	, MaximumLocalError(0.10f)
	, MaximumGlobalError(0.02f)
	, bIgnoreAntiAliasing(true)
	, bIgnoreColors(false)
{
}

void FAutomationScreenshotOptions::SetToleranceAmounts(EComparisonTolerance InTolerance)
{
	switch ( InTolerance )
	{
	case EComparisonTolerance::Zero:
		ToleranceAmount = FComparisonToleranceAmount(0, 0, 0, 0, 0, 255);
		break;
	case EComparisonTolerance::Low:
		ToleranceAmount = FComparisonToleranceAmount(16, 16, 16, 16, 16, 240);
		break;
	case EComparisonTolerance::Medium:
		ToleranceAmount = FComparisonToleranceAmount(24, 24, 24, 24, 24, 220);
		break;
	case EComparisonTolerance::High:
		ToleranceAmount = FComparisonToleranceAmount(32, 32, 32, 32, 64, 96);
		break;
	}
}

#if (WITH_DEV_AUTOMATION_TESTS || WITH_PERF_AUTOMATION_TESTS)

class FAutomationScreenshotTaker
{
	FString	Name;
	FAutomationScreenshotOptions Options;

	int32 OriginalPostProcessing;
	int32 OriginalMotionBlur;
	int32 OriginalSSRQuality;
	int32 OriginalEyeAdaptation;
	int32 OriginalContactShadows;

public:
	FAutomationScreenshotTaker(const FString& InName, FAutomationScreenshotOptions InOptions)
		: Name(InName)
		, Options(InOptions)
	{
		GEngine->GameViewport->OnScreenshotCaptured().AddRaw(this, &FAutomationScreenshotTaker::GrabScreenShot);

		check(IsInGameThread());

		IConsoleVariable* PostProcessingQuality = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PostProcessAAQuality"));
		IConsoleVariable* MotionBlur = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MotionBlurQuality"));
		IConsoleVariable* ScreenSpaceReflections = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SSR.Quality"));
		IConsoleVariable* EyeAdapation = IConsoleManager::Get().FindConsoleVariable(TEXT("r.EyeAdaptationQuality"));
		IConsoleVariable* ContactShadows = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ContactShadows"));

		OriginalPostProcessing = PostProcessingQuality->GetInt();
		OriginalMotionBlur = MotionBlur->GetInt();
		OriginalSSRQuality = ScreenSpaceReflections->GetInt();
		OriginalEyeAdaptation = EyeAdapation->GetInt();
		OriginalContactShadows = ContactShadows->GetInt();

		if ( Options.bDisableNoisyRenderingFeatures )
		{
			PostProcessingQuality->Set(1);
			MotionBlur->Set(0);
			ScreenSpaceReflections->Set(0);
			EyeAdapation->Set(0);
			ContactShadows->Set(0);
		}
	}

	~FAutomationScreenshotTaker()
	{
		check(IsInGameThread());

		if ( Options.bDisableNoisyRenderingFeatures )
		{
			IConsoleVariable* PostProcessingQuality = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PostProcessAAQuality"));
			IConsoleVariable* MotionBlur = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MotionBlurQuality"));
			IConsoleVariable* ScreenSpaceReflections = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SSR.Quality"));
			IConsoleVariable* EyeAdapation = IConsoleManager::Get().FindConsoleVariable(TEXT("r.EyeAdaptationQuality"));
			IConsoleVariable* ContactShadows = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ContactShadows"));

			PostProcessingQuality->Set(OriginalPostProcessing);
			MotionBlur->Set(OriginalMotionBlur);
			ScreenSpaceReflections->Set(OriginalSSRQuality);
			EyeAdapation->Set(OriginalEyeAdaptation);
			ContactShadows->Set(OriginalContactShadows);
		}

		GEngine->GameViewport->OnScreenshotCaptured().RemoveAll(this);

		FAutomationTestFramework::Get().NotifyScreenshotTakenAndCompared();
	}

	void GrabScreenShot(int32 InSizeX, int32 InSizeY, const TArray<FColor>& InImageData)
	{
		check(IsInGameThread());

		FAutomationScreenshotData Data = AutomationCommon::BuildScreenshotData(GWorld->GetName(), Name, InSizeX, InSizeY);

		// Copy the relevant data into the metadata for the screenshot.
		Data.bHasComparisonRules = true;
		Data.ToleranceRed = Options.ToleranceAmount.Red;
		Data.ToleranceGreen = Options.ToleranceAmount.Green;
		Data.ToleranceBlue = Options.ToleranceAmount.Blue;
		Data.ToleranceAlpha = Options.ToleranceAmount.Alpha;
		Data.ToleranceMinBrightness = Options.ToleranceAmount.MinBrightness;
		Data.ToleranceMaxBrightness = Options.ToleranceAmount.MaxBrightness;
		Data.bIgnoreAntiAliasing = Options.bIgnoreAntiAliasing;
		Data.bIgnoreColors = Options.bIgnoreColors;
		Data.MaximumLocalError = Options.MaximumLocalError;
		Data.MaximumGlobalError = Options.MaximumGlobalError;

		FAutomationTestFramework::Get().OnScreenshotCaptured().ExecuteIfBound(InImageData, Data);

		UE_LOG(AutomationFunctionLibrary, Log, TEXT("Screenshot captured as %s"), *Data.Path);

		if ( GIsAutomationTesting )
		{
			FAutomationTestFramework::Get().OnScreenshotCompared.AddRaw(this, &FAutomationScreenshotTaker::OnComparisonComplete);
		}
		else
		{
			delete this;
		}
	}

	void OnComparisonComplete(bool bWasNew, bool bWasSimilar)
	{
		FAutomationTestFramework::Get().OnScreenshotCompared.RemoveAll(this);

		if ( bWasNew )
		{
			UE_LOG(AutomationFunctionLibrary, Warning, TEXT("New Screenshot was discovered!  Please add a ground truth version of it."));
		}
		else
		{
			if ( bWasSimilar )
			{
				UE_LOG(AutomationFunctionLibrary, Log, TEXT("Screenshot was similar!"));
			}
			else
			{
				UE_LOG(AutomationFunctionLibrary, Error, TEXT("Screenshot test failed, screenshots were different!"));
			}
		}

		delete this;
	}
};

#endif

UAutomationBlueprintFunctionLibrary::UAutomationBlueprintFunctionLibrary(const class FObjectInitializer& Initializer)
	: Super(Initializer)
{
}

bool UAutomationBlueprintFunctionLibrary::TakeAutomationScreenshotInternal(const FString& Name, FAutomationScreenshotOptions Options)
{
	if ( !FAutomationTestFramework::Get().IsScreenshotAllowed() )
	{
		UE_LOG(AutomationFunctionLibrary, Log, TEXT("Attempted to capture screenshot (%s) but screenshots are not enabled."), *Name);
		return false;
	}

	// Fallback resolution if all else fails for screenshots.
	uint32 ResolutionX = 1280;
	uint32 ResolutionY = 720;

	// First get the default set for the project.
	UAutomationTestSettings const* AutomationTestSettings = GetDefault<UAutomationTestSettings>();
	if ( AutomationTestSettings->DefaultScreenshotResolution.GetMin() > 0 )
	{
		ResolutionX = (uint32)AutomationTestSettings->DefaultScreenshotResolution.X;
		ResolutionY = (uint32)AutomationTestSettings->DefaultScreenshotResolution.Y;
	}
	
	// If there's an override resolution, use that instead.
	if ( Options.Resolution.GetMin() > 0 )
	{
		ResolutionX = (uint32)Options.Resolution.X;
		ResolutionY = (uint32)Options.Resolution.Y;
	}
	else
	{
		// Failing to find an override, look for a platform override that may have been provided through the
		// device profiles setup, to configure the CVars for controlling the automation screenshot size.
		int32 OverrideWidth = CVarAutomationScreenshotResolutionWidth.GetValueOnGameThread();
		int32 OverrideHeight = CVarAutomationScreenshotResolutionHeight.GetValueOnGameThread();

		if ( OverrideWidth > 0 )
		{
			ResolutionX = (uint32)OverrideWidth;
		}

		if ( OverrideHeight > 0 )
		{
			ResolutionY = (uint32)OverrideHeight;
		}
	}

	// Force all mip maps to load before taking the screenshot.
	UTexture::ForceUpdateTextureStreaming();

#if (WITH_DEV_AUTOMATION_TESTS || WITH_PERF_AUTOMATION_TESTS)
	FAutomationScreenshotTaker* TempObject = new FAutomationScreenshotTaker(Name, Options);
#endif

    if ( FPlatformProperties::HasFixedResolution() )
    {
	    FScreenshotRequest::RequestScreenshot(false);
	    return true;
    }
	else
	{
	    FHighResScreenshotConfig& Config = GetHighResScreenshotConfig();

	    if ( Config.SetResolution(ResolutionX, ResolutionY, 1.0f) )
	    {
			if ( !GEngine->GameViewport->GetGameViewport()->TakeHighResScreenShot() )
			{
				// If we failed to take the screenshot, we're going to need to cleanup the automation screenshot taker.
#if (WITH_DEV_AUTOMATION_TESTS || WITH_PERF_AUTOMATION_TESTS)
				delete TempObject;
#endif 
				return false;
			}

			return true;
		}
	}

	return false;
}

void UAutomationBlueprintFunctionLibrary::TakeAutomationScreenshot(UObject* WorldContextObject, FLatentActionInfo LatentInfo, const FString& Name, const FAutomationScreenshotOptions& Options)
{
	if ( GIsAutomationTesting )
	{
		if ( UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject) )
		{
			FLatentActionManager& LatentActionManager = World->GetLatentActionManager();
			if ( LatentActionManager.FindExistingAction<FTakeScreenshotAfterTimeLatentAction>(LatentInfo.CallbackTarget, LatentInfo.UUID) == nullptr )
			{
				LatentActionManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, new FTakeScreenshotAfterTimeLatentAction(LatentInfo, Name, Options));
			}
		}
	}
	else
	{
		UE_LOG(AutomationFunctionLibrary, Log, TEXT("Screenshot not captured - screenshots are only taken during automation tests"));
	}
}

void UAutomationBlueprintFunctionLibrary::TakeAutomationScreenshotAtCamera(UObject* WorldContextObject, FLatentActionInfo LatentInfo, ACameraActor* Camera, const FString& NameOverride, const FAutomationScreenshotOptions& Options)
{
	if ( Camera == nullptr )
	{
		FMessageLog("PIE").Error(LOCTEXT("CameraRequired", "A camera is required to TakeAutomationScreenshotAtCamera"));
		return;
	}

	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(WorldContextObject, 0);

	if ( PlayerController == nullptr )
	{
		FMessageLog("PIE").Error(LOCTEXT("PlayerRequired", "A player controller is required to TakeAutomationScreenshotAtCamera"));
		return;
	}

	// Move the player, then queue up a screenshot.
	// We need to delay before the screenshot so that the motion blur has time to stop.
	PlayerController->SetViewTarget(Camera, FViewTargetTransitionParams());
	FString ScreenshotName = Camera->GetName();

	if ( !NameOverride.IsEmpty() )
	{
		ScreenshotName = NameOverride;
	}

	if ( UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject) )
	{
		ScreenshotName = FString::Printf(TEXT("%s_%s"), *World->GetName(), *ScreenshotName);

		FLatentActionManager& LatentActionManager = World->GetLatentActionManager();
		if ( LatentActionManager.FindExistingAction<FTakeScreenshotAfterTimeLatentAction>(LatentInfo.CallbackTarget, LatentInfo.UUID) == nullptr )
		{
			LatentActionManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, new FTakeScreenshotAfterTimeLatentAction(LatentInfo, ScreenshotName, Options));
		}
	}
}

void UAutomationBlueprintFunctionLibrary::TakeAutomationScreenshotOfUI(UObject* WorldContextObject, FLatentActionInfo LatentInfo, const FString& Name, const FAutomationScreenshotOptions& Options)
{
	if ( !FAutomationTestFramework::Get().IsScreenshotAllowed() )
	{
		UE_LOG(AutomationFunctionLibrary, Log, TEXT("Attempted to capture screenshot (%s) but screenshots are not enabled."), *Name);

		if ( UWorld* World = WorldContextObject->GetWorld() )
		{
			FLatentActionManager& LatentActionManager = World->GetLatentActionManager();
			if ( LatentActionManager.FindExistingAction<FDelayAction>(LatentInfo.CallbackTarget, LatentInfo.UUID) == nullptr )
			{
				LatentActionManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, new FDelayAction(0, LatentInfo));
			}
		}

		return;
	}

	// Force all mip maps to load before taking the screenshot.
	UTexture::ForceUpdateTextureStreaming();

	if ( UWorld* World = WorldContextObject->GetWorld() )
	{
		if ( UGameViewportClient* GameViewport = WorldContextObject->GetWorld()->GetGameViewport() )
		{
			TSharedPtr<SViewport> Viewport = GameViewport->GetGameViewportWidget();
			if ( Viewport.IsValid() )
			{
				TArray<FColor> OutColorData;
				FIntVector OutSize;
				if ( FSlateApplication::Get().TakeScreenshot(Viewport.ToSharedRef(), OutColorData, OutSize) )
				{
#if (WITH_DEV_AUTOMATION_TESTS || WITH_PERF_AUTOMATION_TESTS)
					FAutomationScreenshotTaker* TempObject = new FAutomationScreenshotTaker(Name, Options);

					FAutomationScreenshotData Data = AutomationCommon::BuildScreenshotData(GWorld->GetName(), Name, OutSize.X, OutSize.Y);

					// Copy the relevant data into the metadata for the screenshot.
					Data.bHasComparisonRules = true;
					Data.ToleranceRed = Options.ToleranceAmount.Red;
					Data.ToleranceGreen = Options.ToleranceAmount.Green;
					Data.ToleranceBlue = Options.ToleranceAmount.Blue;
					Data.ToleranceAlpha = Options.ToleranceAmount.Alpha;
					Data.ToleranceMinBrightness = Options.ToleranceAmount.MinBrightness;
					Data.ToleranceMaxBrightness = Options.ToleranceAmount.MaxBrightness;
					Data.bIgnoreAntiAliasing = Options.bIgnoreAntiAliasing;
					Data.bIgnoreColors = Options.bIgnoreColors;
					Data.MaximumLocalError = Options.MaximumLocalError;
					Data.MaximumGlobalError = Options.MaximumGlobalError;

					GEngine->GameViewport->OnScreenshotCaptured().Broadcast(OutSize.X, OutSize.Y, OutColorData);
#endif
				}

				FLatentActionManager& LatentActionManager = World->GetLatentActionManager();
				if ( LatentActionManager.FindExistingAction<FTakeScreenshotAfterTimeLatentAction>(LatentInfo.CallbackTarget, LatentInfo.UUID) == nullptr )
				{
					LatentActionManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, new FWaitForScreenshotComparisonLatentAction(LatentInfo));
				}
			}
		}
	}
}

//void UAutomationBlueprintFunctionLibrary::BeginPerformanceCapture()
//{
//    //::BeginPerformanceCapture();
//}
//
//void UAutomationBlueprintFunctionLibrary::EndPerformanceCapture()
//{
//    //::EndPerformanceCapture();
//}

bool UAutomationBlueprintFunctionLibrary::AreAutomatedTestsRunning()
{
	return GIsAutomationTesting;
}

FAutomationScreenshotOptions UAutomationBlueprintFunctionLibrary::GetDefaultScreenshotOptionsForGameplay(EComparisonTolerance Tolerance)
{
	FAutomationScreenshotOptions Options;
	Options.Tolerance = Tolerance;
	Options.bDisableNoisyRenderingFeatures = true;
	Options.bIgnoreAntiAliasing = true;
	Options.SetToleranceAmounts(Tolerance);

	return Options;
}

FAutomationScreenshotOptions UAutomationBlueprintFunctionLibrary::GetDefaultScreenshotOptionsForRendering(EComparisonTolerance Tolerance)
{
	FAutomationScreenshotOptions Options;
	Options.Tolerance = Tolerance;
	Options.bDisableNoisyRenderingFeatures = false;
	Options.bIgnoreAntiAliasing = true;
	Options.SetToleranceAmounts(Tolerance);

	return Options;
}

#undef LOCTEXT_NAMESPACE
