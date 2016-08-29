// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

class FTakeScreenshotAfterTimeLatentAction : public FPendingLatentAction
{
public:
	FTakeScreenshotAfterTimeLatentAction(const FLatentActionInfo& LatentInfo, const FString& InScreenshotName, float Seconds);
	virtual ~FTakeScreenshotAfterTimeLatentAction();

	virtual void UpdateOperation(FLatentResponse& Response) override;

#if WITH_EDITOR
	// Returns a human readable description of the latent operation's current state
	virtual FString GetDescription() const override;
#endif

private:
	FName ExecutionFunction;
	int32 OutputLink;
	FWeakObjectPtr CallbackTarget;
	FString ScreenshotName;
	float SecondsRemaining;
	bool IssuedScreenshotCapture;
	bool TakenScreenshot;
};
