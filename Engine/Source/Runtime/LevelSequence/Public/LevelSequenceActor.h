// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LevelSequencePlayer.h"
#include "LevelSequenceActor.generated.h"


/**
 * Actor responsible for controlling a specific level sequence in the world.
 */
UCLASS(Experimental, hideCategories=(Rendering, Physics, LOD, Activation))
class LEVELSEQUENCE_API ALevelSequenceActor
	: public AActor
{
public:

	GENERATED_BODY()

	/** Create and initialize a new instance. */
	ALevelSequenceActor(const FObjectInitializer& Init);

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Playback")
	bool bAutoPlay;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Playback", meta=(ShowOnlyInnerProperties))
	FLevelSequencePlaybackSettings PlaybackSettings;

	UPROPERTY(transient, BlueprintReadOnly, Category="Playback")
	ULevelSequencePlayer* SequencePlayer;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="General", meta=(AllowedClasses="LevelSequence"))
	FStringAssetReference LevelSequence;

public:

	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void SetSequence(ULevelSequence* InSequence);

public:

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

#if WITH_EDITOR
	virtual bool GetReferencedContentObjects(TArray<UObject*>& Objects) const override;
#endif //WITH_EDITOR

	void InitializePlayer();
};
