// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneSequence.h"
#include "MovieSceneSubSection.h"
#include "MovieSceneSubTrack.h"
#include "MovieSceneSubTrackInstance.h"


#define LOCTEXT_NAMESPACE "MovieSceneSubTrack"


/* UMovieSceneSubTrack interface
 *****************************************************************************/
UMovieSceneSubTrack::UMovieSceneSubTrack( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	TrackTint = FColor(180, 0, 40, 65);
#endif
}

UMovieSceneSubSection* UMovieSceneSubTrack::AddSequence(UMovieSceneSequence* Sequence, float StartTime, float Duration, const bool& bInsertSequence)
{
	Modify();

	// If inserting, push all existing sections out to make space for the new one
	if (bInsertSequence)
	{
		// If there's a shot that starts at the same time as this new shot, push the shots forward to make space for this one
		bool bPushShotsForward = false;
		for (auto Section : Sections)
		{
			float SectionStartTime = Section->GetStartTime();
			if (FMath::IsNearlyEqual(SectionStartTime, StartTime))
			{
				bPushShotsForward = true;
				break;
			}
		}
		if (bPushShotsForward)
		{
			for (auto Section : Sections)
			{
				float SectionStartTime = Section->GetStartTime();
				if (SectionStartTime >= StartTime)
				{
					Section->SetStartTime(SectionStartTime + Duration);
					Section->SetEndTime(Section->GetEndTime() + Duration);
				}
			}
		}
		// Otherwise, see if there's a shot after the start time and clamp the duration to that next shot
		else
		{
			bool bFoundAfterShot = false;
			float MinDiff = FLT_MAX;
			for (auto Section : Sections)
			{
				float SectionStartTime = Section->GetStartTime();
				if (SectionStartTime > StartTime)
				{
					bFoundAfterShot = true;
					float Diff = SectionStartTime - StartTime;
					MinDiff = FMath::Min(MinDiff, Diff);
				}
			}

			if (MinDiff != FLT_MAX)
			{
				Duration = MinDiff;
			}
		}
	}
	// Otherwise, append this to the end of the existing sequences
	else
	{
		if (Sections.Num())
		{
			StartTime = Sections[0]->GetEndTime();

			for (const auto& Section : Sections)
			{
				StartTime = FMath::Max(StartTime, Section->GetEndTime());
			}
		}
	}

	UMovieSceneSubSection* NewSection = CastChecked<UMovieSceneSubSection>(CreateNewSection());
	{
		NewSection->SetSequence(Sequence);
		NewSection->SetStartTime(StartTime);
		NewSection->SetEndTime(StartTime + Duration);
	}

	Sections.Add(NewSection);

	return NewSection;
}

UMovieSceneSubSection* UMovieSceneSubTrack::AddSequenceToRecord()
{
	UMovieScene* MovieScene = CastChecked<UMovieScene>(GetOuter());
	TRange<float> PlaybackRange = MovieScene->GetPlaybackRange();

	int32 MaxRowIndex = 0;
	for (auto Section : Sections)
	{
		MaxRowIndex = FMath::Max(Section->GetRowIndex(), MaxRowIndex);
	}

	UMovieSceneSubSection* NewSection = CastChecked<UMovieSceneSubSection>(CreateNewSection());
	{
		NewSection->SetRowIndex(MaxRowIndex + 1);
		NewSection->SetAsRecording(true);
		NewSection->SetStartTime(PlaybackRange.GetLowerBoundValue());
		NewSection->SetEndTime(PlaybackRange.GetUpperBoundValue());
	}

	Sections.Add(NewSection);

	return NewSection;
}

bool UMovieSceneSubTrack::ContainsSequence(const UMovieSceneSequence& Sequence, bool Recursively) const
{
	for (const auto& Section : Sections)
	{
		const auto SubSection = CastChecked<UMovieSceneSubSection>(Section);

		// is the section referencing the sequence?
		const UMovieSceneSequence* SubSequence = SubSection->GetSequence();

		if (SubSequence == &Sequence)
		{
			return true;
		}

		if (!Recursively)
		{
			continue;
		}

		// does the section have sub-tracks referencing the sequence?
		const UMovieScene* SubMovieScene = SubSequence->GetMovieScene();

		if (SubMovieScene == nullptr)
		{
			continue;
		}

		UMovieSceneSubTrack* SubSubTrack = SubMovieScene->FindMasterTrack<UMovieSceneSubTrack>();

		if ((SubSubTrack != nullptr) && SubSubTrack->ContainsSequence(Sequence))
		{
			return true;
		}
	}

	return false;
}


/* UMovieSceneTrack interface
 *****************************************************************************/

void UMovieSceneSubTrack::AddSection(UMovieSceneSection& Section)
{
	if (Section.IsA<UMovieSceneSubSection>())
	{
		Sections.Add(&Section);
	}
}


TSharedPtr<IMovieSceneTrackInstance> UMovieSceneSubTrack::CreateInstance()
{
	return MakeShareable(new FMovieSceneSubTrackInstance(*this)); 
}


UMovieSceneSection* UMovieSceneSubTrack::CreateNewSection()
{
	return NewObject<UMovieSceneSubSection>(this, NAME_None, RF_Transactional);
}


const TArray<UMovieSceneSection*>& UMovieSceneSubTrack::GetAllSections() const 
{
	return Sections;
}


TRange<float> UMovieSceneSubTrack::GetSectionBoundaries() const
{
	TArray<TRange<float>> Bounds;

	for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
	{
		Bounds.Add(Sections[SectionIndex]->GetRange());
	}

	return TRange<float>::Hull(Bounds);
}


bool UMovieSceneSubTrack::HasSection(const UMovieSceneSection& Section) const
{
	return Sections.Contains(&Section);
}


bool UMovieSceneSubTrack::IsEmpty() const
{
	return Sections.Num() == 0;
}


void UMovieSceneSubTrack::RemoveAllAnimationData()
{
	Sections.Empty();
}


void UMovieSceneSubTrack::RemoveSection(UMovieSceneSection& Section)
{
	Sections.Remove(&Section);
}


bool UMovieSceneSubTrack::SupportsMultipleRows() const
{
	return true;
}

#if WITH_EDITORONLY_DATA
FText UMovieSceneSubTrack::GetDefaultDisplayName() const
{
	return LOCTEXT("TrackName", "Subscenes");
}
#endif

#undef LOCTEXT_NAMESPACE
