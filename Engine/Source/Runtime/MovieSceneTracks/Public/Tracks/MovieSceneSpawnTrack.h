// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneTrack.h"
#include "MovieSceneSpawnTrack.generated.h"


class IMovieSceneTrackInstance;
class UMovieSceneSection;


/**
 * Handles when a spawnable should be spawned and destroyed
 */
UCLASS(MinimalAPI)
class UMovieSceneSpawnTrack
	: public UMovieSceneTrack
{
public:

	UMovieSceneSpawnTrack(const FObjectInitializer& Obj);
	GENERATED_BODY()

public:

	/** Evaluate whether the controlled object should currently be spawned or not */
	bool Eval(float Position, float LastPostion, bool& bOutSpawned) const;

	/** Get the object identifier that this spawn track controls */
	const FGuid& GetObjectId() const
	{
		return ObjectGuid;
	}

	/** Set the object identifier that this spawn track controls */
	void SetObjectId(const FGuid& InGuid)
	{
		ObjectGuid = InGuid;
	}

public:

	// UMovieSceneTrack interface

	virtual UMovieSceneSection* CreateNewSection() override;
	virtual TSharedPtr<IMovieSceneTrackInstance> CreateInstance() override;
	virtual bool HasSection(const UMovieSceneSection& Section) const override;
	virtual void AddSection(UMovieSceneSection& Section) override;
	virtual void RemoveSection(UMovieSceneSection& Section) override;
	virtual bool IsEmpty() const override;
	virtual TRange<float> GetSectionBoundaries() const override;
	virtual const TArray<UMovieSceneSection*>& GetAllSections() const override;
#if WITH_EDITOR
	virtual ECookOptimizationFlags GetCookOptimizationFlags() const override;
#endif

#if WITH_EDITORONLY_DATA
	virtual FText GetDisplayName() const override;
#endif

protected:

	/** All the sections in this track */
	UPROPERTY()
	TArray<UMovieSceneSection*> Sections;

	/** The guid relating to the object we are to spawn and destroy */
	UPROPERTY()
	FGuid ObjectGuid;
};