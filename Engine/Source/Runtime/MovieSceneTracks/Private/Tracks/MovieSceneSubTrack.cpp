// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneSequence.h"
#include "MovieSceneSubSection.h"
#include "MovieSceneSubTrack.h"
#include "MovieSceneSubTrackInstance.h"


#define LOCTEXT_NAMESPACE "MovieSceneSubTrack"


/* UMovieSceneSubTrack interface
 *****************************************************************************/

void UMovieSceneSubTrack::AddSequence(UMovieSceneSequence& Sequence, float Time)
{
	Modify();

	UMovieSceneSubSection* NewSection = CastChecked<UMovieSceneSubSection>(CreateNewSection());
	{
		NewSection->SetSequence(Sequence);
		NewSection->SetStartTime(Time);
		NewSection->SetEndTime(Time + Sequence.GetMovieScene()->GetPlaybackRange().Size<float>());
	}

	Sections.Add(NewSection);
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


#undef LOCTEXT_NAMESPACE
