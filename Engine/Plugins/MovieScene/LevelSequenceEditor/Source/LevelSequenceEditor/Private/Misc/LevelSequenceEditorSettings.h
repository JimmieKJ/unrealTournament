// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LevelSequenceEditorSettings.generated.h"


class UMovieSceneTrack;


USTRUCT()
struct FLevelSequencePropertyTrackSettings
{
	GENERATED_BODY()

	/** Optional ActorComponent tag (when keying a component property). */
	UPROPERTY(config, EditAnywhere, Category=PropertyTrack)
	FString ComponentPath;

	/** Path to the keyed property within the Actor or ActorComponent. */
	UPROPERTY(config, EditAnywhere, Category=PropertyTrack)
	FString PropertyPath;
};


USTRUCT()
struct FLevelSequenceTrackSettings
{
	GENERATED_BODY()

	/** The Actor class to create movie scene tracks for. */
	UPROPERTY(config, noclear, EditAnywhere, Category=TrackSettings, meta=(MetaClass="Actor"))
	FStringClassReference MatchingActorClass;

	/** List of movie scene track classes to be added automatically. */
	UPROPERTY(config, noclear, EditAnywhere, Category=TrackSettings, meta=(MetaClass="MovieSceneTrack"))
	TArray<FStringClassReference> DefaultTracks;

	/** List of property names for which movie scene tracks will be created automatically. */
	UPROPERTY(config, EditAnywhere, Category=TrackSettings)
	TArray<FLevelSequencePropertyTrackSettings> DefaultPropertyTracks;
};


/**
 * Level Sequence Editor settings.
 */
UCLASS(config=EditorPerProjectUserSettings)
class ULevelSequenceEditorSettings
	: public UObject
{
	GENERATED_BODY()

public:

	/** Specifies class properties for which movie scene tracks will be created automatically. */
	UPROPERTY(config, EditAnywhere, Category=Tracks)
	TArray<FLevelSequenceTrackSettings> TrackSettings;
};
