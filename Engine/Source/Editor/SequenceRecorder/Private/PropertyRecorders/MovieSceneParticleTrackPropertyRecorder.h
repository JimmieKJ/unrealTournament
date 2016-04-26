// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMovieScenePropertyRecorder.h"
#include "MovieSceneParticleSection.h"
#include "MovieSceneParticleTrackPropertyRecorder.generated.h"

struct FMovieSceneParticleTrackPropertyRecorder;

UCLASS(MinimalAPI, NotBlueprintable)
class UMovieSceneParticleTrackPropertyRecorder : public UObject
{
	GENERATED_BODY()

public:
	UMovieSceneParticleTrackPropertyRecorder()
		: PropertyRecorder(nullptr)
	{
	}

	UFUNCTION()
	void OnTriggered(UParticleSystemComponent* Component, bool bActivating);

	FMovieSceneParticleTrackPropertyRecorder* PropertyRecorder;
};

struct FMovieSceneParticleTrackPropertyRecorder : public IMovieScenePropertyRecorder
{
public:
	friend class UMovieSceneParticleTrackPropertyRecorder;

	virtual ~FMovieSceneParticleTrackPropertyRecorder();

	virtual void CreateSection(UObject* InObjectToRecord, class UMovieScene* MovieScene, const FGuid& Guid, float Time, bool bRecord) override;
	virtual void FinalizeSection() override;
	virtual void Record(float CurrentTime) override;
	virtual void InvalidateObjectToRecord() override
	{
		SystemToRecord = nullptr;
	}

private:
	/** Object to record from */
	TLazyObjectPtr<class UParticleSystemComponent> SystemToRecord;

	/** Section to record to */
	TWeakObjectPtr<class UMovieSceneParticleSection> MovieSceneSection;

	/** Flag if we are actually recording or not */
	bool bRecording;

	bool bWasTriggered;

	EParticleKey::Type PreviousState;

	TWeakObjectPtr<UMovieSceneParticleTrackPropertyRecorder> DelegateProxy;
};
