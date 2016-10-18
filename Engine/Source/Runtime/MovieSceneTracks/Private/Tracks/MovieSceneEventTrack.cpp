// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneEventSection.h"
#include "MovieSceneEventTrack.h"
#include "MovieSceneEventTrackInstance.h"


#define LOCTEXT_NAMESPACE "MovieSceneEventTrack"

/* UMovieSceneEventTrack interface
 *****************************************************************************/

bool UMovieSceneEventTrack::AddKeyToSection(float Time, FName EventName, FKeyParams KeyParams)
{
	UMovieSceneSection* TargetSection = MovieSceneHelpers::FindNearestSectionAtTime(Sections, Time);

	if (TargetSection == nullptr)
	{
		TargetSection = CreateNewSection();
		TargetSection->SetStartTime(Time);
		TargetSection->SetEndTime(Time);

		Sections.Add(TargetSection);
	}

	UMovieSceneEventSection* EventSection = Cast<UMovieSceneEventSection>(TargetSection);

	if (EventSection == nullptr)
	{
		return false;
	}

	EventSection->AddKey(Time, EventName, KeyParams);

	return true;
}


void UMovieSceneEventTrack::TriggerEvents(float Position, float LastPosition, IMovieScenePlayer& Player)
{
	if ((Sections.Num() == 0) || (Position == LastPosition))
	{
		return;
	}

	// Don't allow events to fire when playback is in a stopped state. This can occur when stopping 
	// playback and returning the current position to the start of playback. It's not desireable to have 
	// all the events from the last playback position to the start of playback be fired.
	if (Player.GetPlaybackStatus() == EMovieScenePlayerStatus::Stopped)
	{
		return;
	}

	bool Backwards = Position < LastPosition;

	if ((!Backwards && !bFireEventsWhenForwards) ||
		(Backwards && !bFireEventsWhenBackwards))
	{
		return;
	}

	TArray<UMovieSceneSection*> TraversedSections = MovieSceneHelpers::GetTraversedSections(Sections, Position, LastPosition);
	for (auto EventSection : TraversedSections)
	{
		CastChecked<UMovieSceneEventSection>(EventSection)->TriggerEvents(Position, LastPosition, Player);
	}
}


/* UMovieSceneTrack interface
 *****************************************************************************/

void UMovieSceneEventTrack::AddSection(UMovieSceneSection& Section)
{
	Sections.Add(&Section);
}


TSharedPtr<IMovieSceneTrackInstance> UMovieSceneEventTrack::CreateInstance()
{
	return MakeShareable(new FMovieSceneEventTrackInstance(*this));
}


UMovieSceneSection* UMovieSceneEventTrack::CreateNewSection()
{
	return NewObject<UMovieSceneSection>(this, UMovieSceneEventSection::StaticClass(), NAME_None, RF_Transactional);
}


const TArray<UMovieSceneSection*>& UMovieSceneEventTrack::GetAllSections() const
{
	return Sections;
}


TRange<float> UMovieSceneEventTrack::GetSectionBoundaries() const
{
	TRange<float> SectionBoundaries = TRange<float>::Empty();

	for (auto& Section : Sections)
	{
		SectionBoundaries = TRange<float>::Hull(SectionBoundaries, Section->GetRange());
	}

	return SectionBoundaries;
}


bool UMovieSceneEventTrack::HasSection(const UMovieSceneSection& Section) const
{
	return Sections.Contains(&Section);
}


bool UMovieSceneEventTrack::IsEmpty() const
{
	return (Sections.Num() == 0);
}


void UMovieSceneEventTrack::RemoveAllAnimationData()
{
	Sections.Empty();
}


void UMovieSceneEventTrack::RemoveSection(UMovieSceneSection& Section)
{
	Sections.Remove(&Section);
}

#if WITH_EDITORONLY_DATA

FText UMovieSceneEventTrack::GetDefaultDisplayName() const
{ 
	return LOCTEXT("TrackName", "Events"); 
}

#endif


#undef LOCTEXT_NAMESPACE
