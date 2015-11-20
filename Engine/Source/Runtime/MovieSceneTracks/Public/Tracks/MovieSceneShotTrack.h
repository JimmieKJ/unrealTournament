// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneSubTrack.h"
#include "MovieSceneShotTrack.generated.h"


class IMovieSceneTrackInstance;
class UMovieSceneSection;


/**
 * Handles manipulation of shot properties in a movie scene.
 */
UCLASS(MinimalAPI)
class UMovieSceneShotTrack
	: public UMovieSceneTrack
{
	GENERATED_BODY()

public:

	/** 
	 * Adds a new shot at the specified time.
	 *	
	 * @param CameraHandle Handle to the camera that the shot switches to when active.
	 * @param TimeRange The range within this track's movie scene where the shot is initially placed.
	 * @param ShotName The display name of the shot.
	 * @param ShotNumber The number of the shot. This is used to assist with auto-generated shot names when new shots are added.
	 */
	MOVIESCENETRACKS_API void AddNewShot(FGuid CameraHandle, float Time, const FText& ShotName, int32 ShotNumber);

public:

	// UMovieSceneTrack interface

	virtual void AddSection(UMovieSceneSection& Section) override;
	virtual TSharedPtr<IMovieSceneTrackInstance> CreateInstance() override;
	virtual UMovieSceneSection* CreateNewSection() override;
	virtual const TArray<UMovieSceneSection*>& GetAllSections() const override;
	virtual void RemoveSection(UMovieSceneSection& Section) override;

#if WITH_EDITORONLY_DATA
	virtual FText GetDisplayName() const override;
#endif

#if WITH_EDITOR
	virtual void OnSectionMoved(UMovieSceneSection& Section) override;
#endif

protected:

	float FindEndTimeForShot(float StartTime);

	void FixupSurroundingShots(UMovieSceneSection& Section, bool bDelete);

	/** Sorts shots according to their start time */
	void SortShots();

private:

	/** All movie scene sections. */
	UPROPERTY()
	TArray<UMovieSceneSection*> Sections;
};
