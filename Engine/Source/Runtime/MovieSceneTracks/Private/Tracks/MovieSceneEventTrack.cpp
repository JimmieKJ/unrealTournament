// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneEventSection.h"
#include "MovieSceneEventTrack.h"
#include "MovieSceneEventTrackInstance.h"


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


void UMovieSceneEventTrack::TriggerEvents(float Position, float LastPosition)
{
	if ((Sections.Num() == 0) || (Position == LastPosition))
	{
		return;
	}

	bool Backwards = Position < LastPosition;

	if ((!Backwards && !bFireEventsWhenForwards) ||
		(Backwards && !bFireEventsWhenBackwards))
	{
		return;
	}

	// get the level script actor
	if ((GEngine == nullptr) || (GEngine->GameViewport == nullptr))
	{
		return;
	}

	UWorld* World = GEngine->GameViewport->GetWorld();

	if (World == nullptr)
	{
		return;
	}

	ALevelScriptActor* LevelScriptActor = World->GetLevelScriptActor();

	if (LevelScriptActor == nullptr)
	{
		return;
	}

	TArray<UMovieSceneSection*> TraversedSections = MovieSceneHelpers::GetTraversedSections(Sections, Position, LastPosition);
	for (auto EventSection : TraversedSections)
	{
		CastChecked<UMovieSceneEventSection>(EventSection)->TriggerEvents(LevelScriptActor, Position, LastPosition);
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
