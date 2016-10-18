// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "FunctionalTestingPrivatePCH.h"

#include "AutomationCommon.h"
#include "AutomationTest.h"
#include "DelayForFramesLatentAction.h"
#include "TakeScreenshotAfterTimeLatentAction.h"
#include "Engine/LatentActionManager.h"
#include "SlateBasics.h"

#include "AutomationBlueprintFunctionLibrary.h"

DEFINE_LOG_CATEGORY_STATIC(BlueprintAssertion, Error, Error)
DEFINE_LOG_CATEGORY_STATIC(AutomationFunctionLibrary, Log, Log)

UAutomationBlueprintFunctionLibrary::UAutomationBlueprintFunctionLibrary(const class FObjectInitializer& Initializer) : Super(Initializer)
{
}

void UAutomationBlueprintFunctionLibrary::TakeAutomationScreenshotInternal(const FString& Name)
{
	// Force all mip maps to load before taking the screenshot.
	UTexture::ForceUpdateTextureStreaming();

	const bool bAddFilenameSuffix = true;
	FScreenshotRequest::RequestScreenshot(Name, false, bAddFilenameSuffix);
}

void UAutomationBlueprintFunctionLibrary::TakeAutomationScreenshot(UObject* WorldContextObject, FLatentActionInfo LatentInfo, const FString& Name)
{
	if (GIsAutomationTesting)
	{
		if(!FAutomationTestFramework::GetInstance().IsScreenshotAllowed())
		{
			UE_LOG(AutomationFunctionLibrary, Error, TEXT("Attempted to capture screenshot (%s) but screenshots are not enabled. If you're seeing this on TeamCity, please make sure the name of your test contains '_VisualTest'"), *Name);
			return;
		}

		if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject))
		{
			FLatentActionManager& LatentActionManager = World->GetLatentActionManager();
			if (LatentActionManager.FindExistingAction<FTakeScreenshotAfterTimeLatentAction>(LatentInfo.CallbackTarget, LatentInfo.UUID) == nullptr)
			{
				LatentActionManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, new FTakeScreenshotAfterTimeLatentAction(LatentInfo, Name, 0.0f));
			}
		}
	}
	else
	{
		UE_LOG(AutomationFunctionLibrary, Log, TEXT("Screenshot not captured - screenshots are only taken during automation tests"));
	}
}

void UAutomationBlueprintFunctionLibrary::TakeAutomationScreenshotAtCamera(UObject* WorldContextObject, FLatentActionInfo LatentInfo, ACameraActor* Camera, const FString& NameOverride, float DelayBeforeScreenshotSeconds)
{
	if ( Camera == nullptr )
	{
		UE_LOG(AutomationFunctionLibrary, Error, TEXT("A camera is required to TakeAutomationScreenshotAtCamera"));
		return;
	}

	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(WorldContextObject, 0);

	if ( PlayerController == nullptr )
	{
		UE_LOG(AutomationFunctionLibrary, Error, TEXT("A player controller is required to TakeAutomationScreenshotAtCamera"));
		return;
	}

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(WorldContextObject, 0);
	FTransform CameraTransform = Camera->GetTransform();

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
			LatentActionManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, new FTakeScreenshotAfterTimeLatentAction(LatentInfo, ScreenshotName, DelayBeforeScreenshotSeconds));
		}
	}
}

void UAutomationBlueprintFunctionLibrary::BeginPerformanceCapture()
{
    //::BeginPerformanceCapture();
}

void UAutomationBlueprintFunctionLibrary::EndPerformanceCapture()
{
    //::EndPerformanceCapture();
}
