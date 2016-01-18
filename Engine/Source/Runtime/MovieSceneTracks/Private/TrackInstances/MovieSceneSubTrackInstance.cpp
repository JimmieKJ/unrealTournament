// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneSubTrack.h"
#include "MovieSceneSubTrackInstance.h"
#include "MovieSceneSequenceInstance.h"
#include "MovieSceneSubSection.h"
#include "IMovieScenePlayer.h"


/* FMovieSceneSubTrackInstance structors
 *****************************************************************************/

FMovieSceneSubTrackInstance::FMovieSceneSubTrackInstance(UMovieSceneSubTrack& InTrack)
	: SubTrack(&InTrack)
{ }


/* IMovieSceneTrackInstance interface
 *****************************************************************************/

void FMovieSceneSubTrackInstance::ClearInstance(IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	for (const auto& Pair : SequenceInstancesBySection)
	{
		Player.RemoveMovieSceneInstance(*Pair.Key.Get(), Pair.Value.ToSharedRef());
	}
}


void FMovieSceneSubTrackInstance::RefreshInstance(const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	TSet<UMovieSceneSection*> FoundSections;
	const TArray<UMovieSceneSection*>& AllSections = SubTrack->GetAllSections();

	// create instances for sections that need one
	for (const auto Section : AllSections)
	{
		UMovieSceneSubSection* SubSection = CastChecked<UMovieSceneSubSection>(Section);

		// If the section doesn't have a valid movie scene or no longer has one 
		// (e.g user deleted it) then skip adding an instance for it
		if (SubSection->GetSequence() == nullptr)
		{
			continue;
		}

		FoundSections.Add(Section);

		// create an instance for the section
		TSharedPtr<FMovieSceneSequenceInstance> Instance = SequenceInstancesBySection.FindRef(SubSection);

		if (!Instance.IsValid())
		{
			Instance = MakeShareable(new FMovieSceneSequenceInstance(*SubSection->GetSequence()));
			SequenceInstancesBySection.Add(SubSection, Instance.ToSharedRef());
		}

		Player.AddOrUpdateMovieSceneInstance(*SubSection, Instance.ToSharedRef());
		Instance->RefreshInstance(Player);
	}

	// remove sections that no longer exist
	TMap<TWeakObjectPtr<UMovieSceneSubSection>, TSharedPtr<FMovieSceneSequenceInstance>>::TIterator It = SequenceInstancesBySection.CreateIterator();

	for(; It; ++It)
	{
		if (!FoundSections.Contains(It.Key().Get()))
		{
			Player.RemoveMovieSceneInstance(*It.Key().Get(), It.Value().ToSharedRef());
			It.RemoveCurrent();
		}
	}
}


void FMovieSceneSubTrackInstance::RestoreState(const TArray<UObject*>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	for (const auto Section : SubTrack->GetAllSections())
	{
		UMovieSceneSubSection* SubSection = CastChecked<UMovieSceneSubSection>(Section);
		TSharedPtr<FMovieSceneSequenceInstance> Instance = SequenceInstancesBySection.FindRef(SubSection);

		if (Instance.IsValid())
		{
			Instance->RestoreState(Player);
		}
	}
}


void FMovieSceneSubTrackInstance::SaveState(const TArray<UObject*>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	for (const auto Section : SubTrack->GetAllSections())
	{
		UMovieSceneSubSection* SubSection = CastChecked<UMovieSceneSubSection>(Section);
		TSharedPtr<FMovieSceneSequenceInstance> Instance = SequenceInstancesBySection.FindRef(SubSection);

		if (Instance.IsValid())
		{
			Instance->SaveState(Player);
		}
	}
}


void FMovieSceneSubTrackInstance::Update(float Position, float LastPosition, const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance, EMovieSceneUpdatePass UpdatePass) 
{
	const TArray<UMovieSceneSection*>& AllSections = SubTrack->GetAllSections();
	TArray<UMovieSceneSection*> TraversedSections = MovieSceneHelpers::GetTraversedSections(AllSections, Position, LastPosition);

	for (const auto Section : TraversedSections)
	{
		// skip sections with invalid time scale
		UMovieSceneSubSection* SubSection = CastChecked<UMovieSceneSubSection>(Section);

		if (SubSection->TimeScale == 0.0f)
		{
			continue;
		}

		// skip sections without valid instances
		TSharedPtr<FMovieSceneSequenceInstance> Instance = SequenceInstancesBySection.FindRef(SubSection);

		if (!Instance.IsValid())
		{
			continue;
		}

		// calculate section's local time
		const float InstanceOffset = SubSection->StartOffset + Instance->GetTimeRange().GetLowerBoundValue();
		const float InstanceLastPosition = InstanceOffset + (LastPosition - SubSection->GetStartTime()) / SubSection->TimeScale;
		const float InstancePosition = InstanceOffset + (Position - SubSection->GetStartTime()) / SubSection->TimeScale;

		// update section
		Instance->Update(InstancePosition, InstanceLastPosition, Player);
	}
}
