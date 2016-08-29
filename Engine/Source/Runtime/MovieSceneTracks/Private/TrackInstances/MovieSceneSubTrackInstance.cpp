// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneSubTrack.h"
#include "MovieSceneSubTrackInstance.h"
#include "MovieSceneSequenceInstance.h"
#include "MovieSceneSubSection.h"
#include "MovieSceneCinematicShotTrack.h"
#include "MovieSceneSequence.h"
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


void FMovieSceneSubTrackInstance::RefreshInstance(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	TSet<UMovieSceneSection*> FoundSections;
	const TArray<UMovieSceneSection*>& AllSections = SubTrack->GetAllSections();

	// create instances for sections that need one
	for (const auto Section : AllSections)
	{
		UMovieSceneSubSection* SubSection = CastChecked<UMovieSceneSubSection>(Section);
		
		FoundSections.Add(Section);

		// create an instance for the section
		TSharedPtr<FMovieSceneSequenceInstance> Instance = SequenceInstancesBySection.FindRef(SubSection);

		if (!Instance.IsValid())
		{
			UMovieSceneSequence* Sequence = SubSection->GetSequence();
			if(Sequence != nullptr)
			{
				Instance = MakeShareable(new FMovieSceneSequenceInstance(*Sequence));
			}
			else
			{
				Instance = MakeShareable(new FMovieSceneSequenceInstance(*SubTrack));
			}
			SequenceInstancesBySection.Add(SubSection, Instance.ToSharedRef());
		}

		Player.AddOrUpdateMovieSceneInstance(*SubSection, Instance.ToSharedRef());
		Instance->RefreshInstance(Player);

#if WITH_EDITOR
		SubSection->OnSequenceChanged() = FOnSequenceChanged::CreateSP(Instance.Get(), &FMovieSceneSequenceInstance::HandleSequenceSectionChanged);
#endif
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


void FMovieSceneSubTrackInstance::RestoreState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
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


void FMovieSceneSubTrackInstance::SaveState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
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


TArray<UMovieSceneSection*> FMovieSceneSubTrackInstance::GetAllTraversedSectionsWithPreroll( const TArray<UMovieSceneSection*>& Sections, float CurrentTime, float PreviousTime )
{
	TArray<UMovieSceneSection*> TraversedSections;

	bool bPlayingBackwards = CurrentTime - PreviousTime < 0.0f;
	float MaxTime = bPlayingBackwards ? PreviousTime : CurrentTime;
	float MinTime = bPlayingBackwards ? CurrentTime : PreviousTime;

	TRange<float> TraversedRange(MinTime, TRangeBound<float>::Inclusive(MaxTime));

	for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
	{
		UMovieSceneSection* Section = Sections[SectionIndex];

		float PrerollTime = 0.0f;
		UMovieSceneSubSection* SubSection = CastChecked<UMovieSceneSubSection>(Section);
		if (SubSection != nullptr)
		{
			PrerollTime = SubSection->PrerollTime;
		}

		if ((Section->GetStartTime()-PrerollTime == CurrentTime) || TraversedRange.Overlaps(TRange<float>(Section->GetRange().GetLowerBoundValue()-PrerollTime, Section->GetRange().GetUpperBoundValue())))
		{
			TraversedSections.Add(Section);
		}
	}

	return TraversedSections;
}

bool FMovieSceneSubTrackInstance::ShouldEvaluateIfOverlapping(const TArray<UMovieSceneSection*>& TraversedSections, UMovieSceneSection* Section) const
{
	// Check with this shot's exclusive upper bound for when shots are adjacent to each other but on different rows.
	TRange<float> ThisSectionWithExclusiveUpper = TRange<float>(Section->GetRange().GetLowerBoundValue(), Section->GetRange().GetUpperBoundValue());

	const bool bShouldRemove = TraversedSections.ContainsByPredicate([=](UMovieSceneSection* OtherSection){
		if (Section->GetRowIndex() == OtherSection->GetRowIndex() &&
			ThisSectionWithExclusiveUpper.Overlaps(OtherSection->GetRange()) &&
			Section->GetOverlapPriority() < OtherSection->GetOverlapPriority())
		{
			return true;
		}
		return false;
	});

	return bShouldRemove;
}

TArray<UMovieSceneSection*> FMovieSceneSubTrackInstance::GetTraversedSectionsWithPreroll( const TArray<UMovieSceneSection*>& Sections, float CurrentTime, float PreviousTime )
{
	TArray<UMovieSceneSection*> TraversedSections = GetAllTraversedSectionsWithPreroll(Sections, CurrentTime, PreviousTime);

	// Remove any overlaps that are underneath another
	for (int32 RemoveAt = 0; RemoveAt < TraversedSections.Num(); )
	{
		UMovieSceneSection* Section = TraversedSections[RemoveAt];
		
		const bool bShouldRemove = ShouldEvaluateIfOverlapping(TraversedSections, Section);
		
		if (bShouldRemove)
		{
			TraversedSections.RemoveAt(RemoveAt, 1, false);
		}
		else
		{
			++RemoveAt;
		}
	}

	return TraversedSections;
}


void FMovieSceneSubTrackInstance::Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) 
{
	const TArray<UMovieSceneSection*>& AllSections = SubTrack->GetAllSections();

	// Evaluate only the sections that intersect with the update time and not including the last position
	float CurrentTime = UpdateData.Position;
	float PreviousTime = CurrentTime;

	TArray<UMovieSceneSection*> TraversedSections = GetTraversedSectionsWithPreroll(AllSections, CurrentTime, PreviousTime);
	TArray<TWeakObjectPtr<UMovieSceneSubSection>>& LastTraversedSections = UpdatePassToLastTraversedSectionsMap.FindOrAdd(UpdateData.UpdatePass);

	for ( TWeakObjectPtr<UMovieSceneSubSection> LastSubSection : LastTraversedSections )
	{
		if (LastSubSection.IsValid())
		{
			if ( TraversedSections.Contains( LastSubSection.Get() ) == false )
			{
				UpdateSection( UpdateData, Player, LastSubSection.Get(), true );
			}
		}
	}
	LastTraversedSections.Empty();

	for (const auto Section : TraversedSections)
	{
		UMovieSceneSubSection* SubSection = CastChecked<UMovieSceneSubSection>( Section );
		UpdateSection(UpdateData, Player, SubSection, false);
		LastTraversedSections.Add( TWeakObjectPtr<UMovieSceneSubSection>( SubSection ) );
	}
}

void FMovieSceneSubTrackInstance::UpdateSection( EMovieSceneUpdateData& UpdateData, class IMovieScenePlayer& Player, UMovieSceneSubSection* SubSection, bool bSectionWasDeactivated )
{
	if (SubSection->TimeScale == 0.0f)
	{
		return;
	}

	// skip sections without valid instances
	TSharedPtr<FMovieSceneSequenceInstance> Instance = SequenceInstancesBySection.FindRef(SubSection);

	if (!Instance.IsValid())
	{
		return;
	}

	// calculate section's local time
	const float InstanceOffset = SubSection->StartOffset + Instance->GetTimeRange().GetLowerBoundValue() - SubSection->PrerollTime;
	float InstanceLastPosition = InstanceOffset + (UpdateData.LastPosition - (SubSection->GetStartTime()- SubSection->PrerollTime)) / SubSection->TimeScale;
	float InstancePosition = InstanceOffset + (UpdateData.Position - (SubSection->GetStartTime()- SubSection->PrerollTime)) / SubSection->TimeScale;

	UMovieSceneSequence* SubSequence = SubSection->GetSequence();
	if (SubSequence)
	{
		UMovieScene* SubMovieScene = SubSequence->GetMovieScene();
		if (SubMovieScene && SubMovieScene->GetForceFixedFrameIntervalPlayback() && SubMovieScene->GetFixedFrameInterval() > 0 )
		{
			float FixedFrameInterval = ( SubMovieScene->GetFixedFrameInterval() / SubSection->TimeScale );
			InstancePosition = UMovieScene::CalculateFixedFrameTime( InstancePosition, FixedFrameInterval );
			InstanceLastPosition = UMovieScene::CalculateFixedFrameTime( InstanceLastPosition, FixedFrameInterval );
		}
	}

	EMovieSceneUpdateData SubUpdateData(InstancePosition, InstanceLastPosition);
	SubUpdateData.bJumpCut = UpdateData.LastPosition < SubSection->GetStartTime() || UpdateData.LastPosition > SubSection->GetEndTime();
	SubUpdateData.UpdatePass = UpdateData.UpdatePass;
	SubUpdateData.bPreroll = UpdateData.Position < SubSection->GetStartTime();
	SubUpdateData.bSubSceneDeactivate = bSectionWasDeactivated;
	SubUpdateData.bUpdateCameras = SubTrack->GetClass() == UMovieSceneCinematicShotTrack::StaticClass();

	// update sub sections

	if (SubUpdateData.UpdatePass == MSUP_PreUpdate)
	{
		Instance->PreUpdate(Player);
	}
		
	Instance->UpdatePassSingle(SubUpdateData, Player);

	if (SubUpdateData.UpdatePass == MSUP_PostUpdate)
	{
		Instance->PostUpdate(Player);
	}
}
