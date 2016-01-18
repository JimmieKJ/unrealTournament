// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCapturePCH.h"
#include "LevelCapture.h"

void ULevelCapture::SetPrerequisiteActor(AActor* InPrerequisiteActor)
{
	PrerequisiteActor = InPrerequisiteActor;
	PrerequisiteActorId = PrerequisiteActor.GetUniqueID().GetGuid();
}

void ULevelCapture::Initialize(TWeakPtr<FSceneViewport> InViewport)
{
	// Ensure the PrerequisiteActor is up to date with the PrerequisiteActorId, since this may have been deserialized from JSON
	PrerequisiteActor = FUniqueObjectGuid(PrerequisiteActorId);

	Super::Initialize(InViewport);
	CaptureStrategy = MakeShareable(new FFixedTimeStepCaptureStrategy(Settings.FrameRate));
}

void ULevelCapture::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!GWorld->HasBegunPlay())
	{
		return;
	}

	AActor* PrerequisiteActorPtr = PrerequisiteActor.Get();
	if (!PrerequisiteActorId.IsValid() || (PrerequisiteActorPtr && PrerequisiteActorPtr->HasActorBegunPlay()))
	{
		if (!bCapturing)
		{
			StartCapture();
		}

		PrepareForScreenshot();
	}
}

#if WITH_EDITOR
void ULevelCapture::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	PrerequisiteActorId = PrerequisiteActor.GetUniqueID().GetGuid();
}
#endif