// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneCapture.h"
#include "LevelCapture.generated.h"

UCLASS()
class MOVIESCENECAPTURE_API ULevelCapture : public UMovieSceneCapture
{
public:
	GENERATED_BODY()

	/** Specify a prerequisite actor that must be set up before we start capturing */
	void SetPrerequisiteActor(AActor* Prereq);

	virtual void Initialize(TWeakPtr<FSceneViewport> InViewport) override;
	virtual void Tick(float DeltaSeconds) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	
private:

	/** Specify a prerequisite actor that must be set up before we start capturing */
	UPROPERTY()
	TLazyObjectPtr<AActor> PrerequisiteActor;

	/** Copy of the ID from PrerequisiteActor. Required because JSON serialization exports the path of the object, rather that its GUID */
	UPROPERTY()
	FGuid PrerequisiteActorId;
};