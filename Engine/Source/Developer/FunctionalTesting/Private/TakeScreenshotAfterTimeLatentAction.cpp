// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "FunctionalTestingPrivatePCH.h"
#include "AutomationBlueprintFunctionLibrary.h"
#include "TakeScreenshotAfterTimeLatentAction.h"

FTakeScreenshotAfterTimeLatentAction::FTakeScreenshotAfterTimeLatentAction(const FLatentActionInfo& LatentInfo, const FString& InScreenshotName, float Seconds)
	: ExecutionFunction(LatentInfo.ExecutionFunction)
	, OutputLink(LatentInfo.Linkage)
	, CallbackTarget(LatentInfo.CallbackTarget)
	, ScreenshotName(InScreenshotName)
	, SecondsRemaining(Seconds)
	, IssuedScreenshotCapture(false)
	, TakenScreenshot(false)
{
}

FTakeScreenshotAfterTimeLatentAction::~FTakeScreenshotAfterTimeLatentAction()
{
}

void FTakeScreenshotAfterTimeLatentAction::UpdateOperation(FLatentResponse& Response)
{
	if ( !TakenScreenshot )
	{
		if ( !IssuedScreenshotCapture )
		{
			SecondsRemaining -= Response.ElapsedTime();
			if ( SecondsRemaining <= 0.0f )
			{
				UAutomationBlueprintFunctionLibrary::TakeAutomationScreenshotInternal(ScreenshotName);
				IssuedScreenshotCapture = true;
			}
		}
		else
		{
			TakenScreenshot = true;
		}
	}
	else
	{
		Response.FinishAndTriggerIf(true, ExecutionFunction, OutputLink, CallbackTarget);
	}
}

#if WITH_EDITOR
FString FTakeScreenshotAfterTimeLatentAction::GetDescription() const
{
	return FString::Printf(TEXT("Take screenshot named %s after %f seconds"), *ScreenshotName, SecondsRemaining);
}
#endif