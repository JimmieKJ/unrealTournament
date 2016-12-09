// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Tracks/MovieSceneCinematicShotTrack.h"
#include "MovieSceneSequence.h"
#include "MovieSceneCommonHelpers.h"
#include "Sections/MovieSceneCinematicShotSection.h"
#include "Compilation/MovieSceneCompilerRules.h"


#define LOCTEXT_NAMESPACE "MovieSceneCinematicShotTrack"


/* UMovieSceneSubTrack interface
 *****************************************************************************/
UMovieSceneCinematicShotTrack::UMovieSceneCinematicShotTrack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	TrackTint = FColor(0, 0, 0, 127);
#endif
}

UMovieSceneSubSection* UMovieSceneCinematicShotTrack::AddSequence(UMovieSceneSequence* Sequence, float StartTime, float Duration, const bool& bInsertSequence)
{
	UMovieSceneSubSection* NewSection = UMovieSceneSubTrack::AddSequence(Sequence, StartTime, Duration, bInsertSequence);

	UMovieSceneCinematicShotSection* NewShotSection = Cast<UMovieSceneCinematicShotSection>(NewSection);

#if WITH_EDITOR

	if (Sequence != nullptr)
	{
		NewShotSection->SetShotDisplayName(Sequence->GetDisplayName());	
	}

#endif

	// When a new sequence is added, sort all sequences to ensure they are in the correct order
	MovieSceneHelpers::SortConsecutiveSections(Sections);

	// Once sequences are sorted fixup the surrounding sequences to fix any gaps
	//MovieSceneHelpers::FixupConsecutiveSections(Sections, *NewSection, false);

	return NewSection;
}

/* UMovieSceneTrack interface
 *****************************************************************************/

void UMovieSceneCinematicShotTrack::AddSection(UMovieSceneSection& Section)
{
	if (Section.IsA<UMovieSceneCinematicShotSection>())
	{
		Sections.Add(&Section);
	}
}


UMovieSceneSection* UMovieSceneCinematicShotTrack::CreateNewSection()
{
	return NewObject<UMovieSceneCinematicShotSection>(this, NAME_None, RF_Transactional);
}

void UMovieSceneCinematicShotTrack::RemoveSection(UMovieSceneSection& Section)
{
	Sections.Remove(&Section);
	//MovieSceneHelpers::FixupConsecutiveSections(Sections, Section, true);
	MovieSceneHelpers::SortConsecutiveSections(Sections);

	// @todo Sequencer: The movie scene owned by the section is now abandoned.  Should we offer to delete it?  
}

bool UMovieSceneCinematicShotTrack::SupportsMultipleRows() const
{
	return true;
}

TInlineValue<FMovieSceneSegmentCompilerRules> UMovieSceneCinematicShotTrack::GetTrackCompilerRules() const
{
	// Apply a high pass filter to overlapping sections such that only the highest row in a track wins
	struct FCinematicShotTrackCompilerRules : FMovieSceneSegmentCompilerRules
	{
		virtual void BlendSegment(FMovieSceneSegment& Segment, const TArrayView<const FMovieSceneSectionData>& SourceData) const
		{
			MovieSceneSegmentCompiler::BlendSegmentHighPass(Segment, SourceData);
		}
	};
	return FCinematicShotTrackCompilerRules();
}

TInlineValue<FMovieSceneSegmentCompilerRules> UMovieSceneCinematicShotTrack::GetRowCompilerRules() const
{
	class FCinematicRowRules : public FMovieSceneSegmentCompilerRules
	{
		virtual void BlendSegment(FMovieSceneSegment& Segment, const TArrayView<const FMovieSceneSectionData>& SourceData) const override
		{
			// Sort everything by priority, then latest start time wins
			if (Segment.Impls.Num() <= 1)
			{
				return;
			}

			Segment.Impls.Sort(
				[&](const FSectionEvaluationData& A, const FSectionEvaluationData& B)
				{
					const FMovieSceneSectionData& DataA = SourceData[A.ImplIndex];
					const FMovieSceneSectionData& DataB = SourceData[B.ImplIndex];

					if (DataA.Priority == DataB.Priority)
					{
						return TRangeBound<float>::MaxLower(DataA.Bounds.GetLowerBound(), DataB.Bounds.GetLowerBound()) == DataA.Bounds.GetLowerBound();
					}
					return DataA.Priority > DataB.Priority;
				}
			);

			Segment.Impls.RemoveAt(1, Segment.Impls.Num() - 1, true);
		}
	};

	return FCinematicRowRules();
}

#if WITH_EDITOR
void UMovieSceneCinematicShotTrack::OnSectionMoved(UMovieSceneSection& Section)
{
	//MovieSceneHelpers::FixupConsecutiveSections(Sections, Section, false);
}
#endif

#if WITH_EDITORONLY_DATA
FText UMovieSceneCinematicShotTrack::GetDefaultDisplayName() const
{
	return LOCTEXT("TrackName", "Shots");
}
#endif

#undef LOCTEXT_NAMESPACE
