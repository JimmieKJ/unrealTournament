// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneLevelVisibilitySection.h"
#include "MovieSceneLevelVisibilityTrack.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneLevelVisibilityTrackInstance.h"


#define LOCTEXT_NAMESPACE "MovieSceneLevelVisibilityTrack"

UMovieSceneLevelVisibilityTrack::UMovieSceneLevelVisibilityTrack( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
}


TSharedPtr<IMovieSceneTrackInstance> UMovieSceneLevelVisibilityTrack::CreateInstance()
{
	return MakeShareable( new FMovieSceneLevelVisibilityTrackInstance( *this ) ); 
}


bool UMovieSceneLevelVisibilityTrack::IsEmpty() const
{
	return Sections.Num() == 0;
}


void UMovieSceneLevelVisibilityTrack::AddSection(UMovieSceneSection& Section)
{
	Sections.Add(&Section);
}


void UMovieSceneLevelVisibilityTrack::RemoveSection( UMovieSceneSection& Section )
{
	Sections.Remove(&Section);
}


UMovieSceneSection* UMovieSceneLevelVisibilityTrack::CreateNewSection()
{
	return NewObject<UMovieSceneLevelVisibilitySection>(this, UMovieSceneLevelVisibilitySection::StaticClass(), NAME_None, RF_Transactional);
}


const TArray<UMovieSceneSection*>& UMovieSceneLevelVisibilityTrack::GetAllSections() const
{
	return Sections;
}


TRange<float> UMovieSceneLevelVisibilityTrack::GetSectionBoundaries() const
{
	TArray< TRange<float> > Bounds;

	for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
	{
		Bounds.Add(Sections[SectionIndex]->GetRange());
	}

	return TRange<float>::Hull(Bounds);
}


bool UMovieSceneLevelVisibilityTrack::HasSection(const UMovieSceneSection& Section) const
{
	return Sections.Contains(&Section);
}


#if WITH_EDITORONLY_DATA
FText UMovieSceneLevelVisibilityTrack::GetDefaultDisplayName() const
{
	return LOCTEXT("DisplayName", "Level Visibility");
}
#endif

#undef LOCTEXT_NAMESPACE
