// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LevelSequencePlayer.h"
#include "LevelSequenceActor.generated.h"

class ULevelSequenceBurnIn;

UCLASS(Blueprintable)
class LEVELSEQUENCE_API ULevelSequenceBurnInInitSettings : public UObject
{
	GENERATED_BODY()
};

UCLASS()
class LEVELSEQUENCE_API ULevelSequenceBurnInOptions : public UObject
{
public:

	GENERATED_BODY()
	ULevelSequenceBurnInOptions(const FObjectInitializer& Init);

	/** Ensure the settings object is up-to-date */
	void ResetSettings();

public:

	UPROPERTY(EditAnywhere, Category="General")
	bool bUseBurnIn;

	UPROPERTY(EditAnywhere, Category="General", meta=(EditCondition=bUseBurnIn, MetaClass="LevelSequenceBurnIn"))
	FStringClassReference BurnInClass;

	UPROPERTY(EditAnywhere, Category="General", meta=(EditCondition=bUseBurnIn, EditInline))
	ULevelSequenceBurnInInitSettings* Settings;

protected:

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif //WITH_EDITOR
};

/**
 * Actor responsible for controlling a specific level sequence in the world.
 */
UCLASS(hideCategories=(Rendering, Physics, LOD, Activation))
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="General", meta=(EditInline))
	ULevelSequenceBurnInOptions* BurnInOptions;

public:

	/**
	 * Get the level sequence being played by this actor.
	 *
	 * @param Whether to load the sequence object if it is not already in memory.
	 * @return Level sequence, or nullptr if not assigned or if it cannot be loaded.
	 * @see SetSequence
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	ULevelSequence* GetSequence(bool Load = false) const;

	/**
	 * Set the level sequence being played by this actor.
	 *
	 * @param InSequence The sequence object to set.
	 * @see GetSequence
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void SetSequence(ULevelSequence* InSequence);

	/** Refresh this actor's burn in */
	void RefreshBurnIn();

public:

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

#if WITH_EDITOR
	virtual bool GetReferencedContentObjects(TArray<UObject*>& Objects) const override;
#endif //WITH_EDITOR

	void InitializePlayer();

private:
	/** Burn-in widget */
	UPROPERTY()
	ULevelSequenceBurnIn* BurnInInstance;
};
