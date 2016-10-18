// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneSequence.h"
#include "MovieSceneCinematicShotSection.h"
#include "MovieSceneCinematicShotTrack.h"
#include "MovieSceneCinematicShotTrackInstance.h"
#include "MovieSceneCommonHelpers.h"


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


TSharedPtr<IMovieSceneTrackInstance> UMovieSceneCinematicShotTrack::CreateInstance()
{
	return MakeShareable(new FMovieSceneCinematicShotTrackInstance(*this)); 
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
