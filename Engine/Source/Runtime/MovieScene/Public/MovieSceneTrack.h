// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EnumClassFlags.h"
#include "MovieSceneTrack.generated.h"


class UMovieSceneSection;
class FMovieSceneSequenceInstance;

/** Flags used to perform cook-time optimization of movie scene data */
enum class ECookOptimizationFlags
{
	/** Perform no cook optimization */
	None 			= 0,
	/** Remove this track since its of no consequence to runtime */
	RemoveTrack		= 1 << 0,
	/** Remove this track's object since its of no consequence to runtime */
	RemoveObject	= 1 << 1,
};
ENUM_CLASS_FLAGS(ECookOptimizationFlags)

/**
 * Base class for a track in a Movie Scene
 */
UCLASS(abstract, MinimalAPI)
class UMovieSceneTrack
	: public UObject
{
	GENERATED_BODY()

public:

	UMovieSceneTrack(const FObjectInitializer& InInitializer)
		: Super(InInitializer)
	{
#if WITH_EDITORONLY_DATA
		TrackTint = FColor(127, 127, 127, 0);
#endif
	}

	/**
	 * Creates a runtime instance of this class.
	 * 
	 * @param SequenceInstance The sequence instance to create this track instance for
	 * @return The created runtime instance. This must not be nullptr (note sharedptr not sharedref due to compatibility with uobjects not actually allowing pure virtuals)
	 */
	virtual TSharedPtr<class IMovieSceneTrackInstance> CreateInstance() PURE_VIRTUAL(UMovieSceneTrack::CreateInstance, check(false); return nullptr;);

	/**
	 * @return The name that makes this track unique from other track of the same class.
	 */
	virtual FName GetTrackName() const { return NAME_None; }

	/**
	 * @return Whether or not this track has any data in it.
	 */
	virtual bool IsEmpty() const PURE_VIRTUAL(UMovieSceneTrack::IsEmpty, return true;);

	/**
	 * Removes animation data.
	 */
	virtual void RemoveAllAnimationData() { }

	/**
	 * @return Whether or not this track supports multiple row indices.
	 */
	virtual bool SupportsMultipleRows() const
	{
		return false;
	}

	/**
	 * @return Whether or not this track's section bounds should be added to the play range
	 */
	virtual bool AddsSectionBoundsToPlayRange() const
	{
		return false;
	}

public:

	/**
	 * Add a section to this track.
	 *
	 * @param Section The section to add.
	 */
	virtual void AddSection(UMovieSceneSection& Section) PURE_VIRTUAL(UMovieSceneSection::AddSection,);

	/**
	 * Generates a new section suitable for use with this track.
	 *
	 * @return a new section suitable for use with this track.
	 */
	virtual class UMovieSceneSection* CreateNewSection() PURE_VIRTUAL(UMovieSceneTrack::CreateNewSection, return nullptr;);
	
	/**
	 * Called when all the sections of the track need to be retrieved.
	 * 
	 * @return List of all the sections in the track.
	 */
	virtual const TArray<UMovieSceneSection*>& GetAllSections() const PURE_VIRTUAL(UMovieSceneTrack::GetAllSections, static TArray<UMovieSceneSection*> Empty; return Empty;);

	/**
	 * Gets the section boundaries of this track.
	 * 
	 * @return The range of time boundaries.
	 */
	virtual TRange<float> GetSectionBoundaries() const PURE_VIRTUAL(UMovieSceneTrack::GetSectionBoundaries, return TRange<float>::Empty(););

	/**
	 * Checks to see if the section is in this track.
	 *
	 * @param Section The section to query for.
	 * @return True if the section is in this track.
	 */
	virtual bool HasSection(const UMovieSceneSection& Section) const PURE_VIRTUAL(UMovieSceneSection::HasSection, return false;);

	/**
	 * Removes a section from this track.
	 *
	 * @param Section The section to remove.
	 */
	virtual void RemoveSection(UMovieSceneSection& Section) PURE_VIRTUAL(UMovieSceneSection::RemoveSection,);

#if WITH_EDITOR

	/**
	 * Called when this track's movie scene is being cooked to determine if/how this track should be cooked.
	 * @return ECookOptimizationFlags detailing how to optimize this track
	 */
	virtual ECookOptimizationFlags GetCookOptimizationFlags() const { return ECookOptimizationFlags::None; }

#endif

#if WITH_EDITORONLY_DATA

	/**
	 * Get the track's display name.
	 *
	 * @return Display name text.
	 */
	virtual FText GetDisplayName() const PURE_VIRTUAL(UMovieSceneTrack::GetDisplayName, return FText::FromString(TEXT("Unnamed Track")););

	/**
	 * Get this track's color tint.
	 *
	 * @return Color Tint.
	 */
	const FColor& GetColorTint() const
	{
		return TrackTint;
	}

	/**
	 * Set this track's color tint.
	 *
	 * @param InTrackTint The color to tint this track.
	 */
	void SetColorTint(const FColor& InTrackTint)
	{
		TrackTint = InTrackTint;
	}

protected:

	/** This track's tint color */
	UPROPERTY(EditAnywhere, Category=General, DisplayName=Color)
	FColor TrackTint;

public:
#endif

#if WITH_EDITOR
	/**
	 * Called if the section is moved in Sequencer.
	 *
	 * @param Section The section that moved.
	 */
	virtual void OnSectionMoved(UMovieSceneSection& Section) { }
#endif
};
