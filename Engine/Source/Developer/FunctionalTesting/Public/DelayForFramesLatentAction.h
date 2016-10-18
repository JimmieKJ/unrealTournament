// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

class FDelayForFramesLatentAction : public FPendingLatentAction
{
public:
	FDelayForFramesLatentAction(const FLatentActionInfo& LatentInfo, int32 NumFrames);
	virtual ~FDelayForFramesLatentAction();

	virtual void UpdateOperation(FLatentResponse& Response) override;

#if WITH_EDITOR
	// Returns a human readable description of the latent operation's current state
	virtual FString GetDescription() const override;
#endif

private:
	FName ExecutionFunction;
	int32 OutputLink;
	FWeakObjectPtr CallbackTarget;
	int32 FramesRemaining;
};
