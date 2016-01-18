// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneShotSection.h"
#include "MovieSceneShotTrack.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneShotTrackInstance.h"


#define LOCTEXT_NAMESPACE "MovieSceneShotTrack"


/* UMovieSceneShotTrack interface
 *****************************************************************************/

void UMovieSceneShotTrack::AddNewShot(FGuid CameraHandle, float StartTime, const FText& ShotName, int32 ShotNumber)
{
	Modify();

	FName UniqueShotName = MakeUniqueObjectName(this, UMovieSceneShotSection::StaticClass(), *ShotName.ToString());
	UMovieSceneShotSection* NewSection = NewObject<UMovieSceneShotSection>(this, UniqueShotName, RF_Transactional);
	{
		NewSection->SetStartTime(StartTime);
		NewSection->SetEndTime(FindEndTimeForShot(StartTime));
		NewSection->SetCameraGuid(CameraHandle);
		NewSection->SetShotNameAndNumber(ShotName , ShotNumber);
	}

	Sections.Add(NewSection);

	// When a new shot is added, sort all shots to ensure they are in the correct order
	SortShots();

	// Once shots are sorted fixup the surrounding shots to fix any gaps
	FixupSurroundingShots(*NewSection, false);
}


/* UMovieSceneTrack interface
 *****************************************************************************/

void UMovieSceneShotTrack::AddSection(UMovieSceneSection& Section)
{
	if (Section.IsA<UMovieSceneShotSection>())
	{
		Sections.Add(&Section);
	}
}


TSharedPtr<IMovieSceneTrackInstance> UMovieSceneShotTrack::CreateInstance()
{
	return MakeShareable(new FMovieSceneShotTrackInstance(*this)); 
}


UMovieSceneSection* UMovieSceneShotTrack::CreateNewSection()
{
	return NewObject<UMovieSceneShotSection>(this, NAME_None, RF_Transactional);
}


const TArray<UMovieSceneSection*>& UMovieSceneShotTrack::GetAllSections() const
{
	return Sections;
}


void UMovieSceneShotTrack::RemoveSection(UMovieSceneSection& Section)
{
	Sections.Remove(&Section);
	FixupSurroundingShots(Section, true);
	SortShots();

	// @todo Sequencer: The movie scene owned by the section is now abandoned.  Should we offer to delete it?  
}


#if WITH_EDITORONLY_DATA
FText UMovieSceneShotTrack::GetDisplayName() const
{
	return LOCTEXT("TrackName", "Shots");
}
#endif


#if WITH_EDITOR
void UMovieSceneShotTrack::OnSectionMoved(UMovieSceneSection& Section)
{
	FixupSurroundingShots(Section, false);
}
#endif


void UMovieSceneShotTrack::SortShots()
{
	Sections.Sort([](const UMovieSceneSection& A, const UMovieSceneSection& B)
		{
			return A.GetStartTime() < B.GetStartTime();
		}
	);
}


void UMovieSceneShotTrack::FixupSurroundingShots(UMovieSceneSection& Section, bool bDelete)
{
	// Find the previous section and extend it to take the place of the section being deleted
	int32 SectionIndex = INDEX_NONE;

	if (Sections.Find(&Section, SectionIndex))
	{
		int32 PrevSectionIndex = SectionIndex - 1;
		if( Sections.IsValidIndex( PrevSectionIndex ) )
		{
			// Extend the previous section
			Sections[PrevSectionIndex]->SetEndTime( bDelete ? Section.GetEndTime() : Section.GetStartTime() );
		}

		if( !bDelete )
		{
			int32 NextSectionIndex = SectionIndex + 1;
			if(Sections.IsValidIndex(NextSectionIndex))
			{
				// Shift the next shot's start time so that it starts when the new shot ends
				Sections[NextSectionIndex]->SetStartTime(Section.GetEndTime());
			}
		}
	}

	SortShots();
}


float UMovieSceneShotTrack::FindEndTimeForShot( float StartTime )
{
	float EndTime = 0;
	bool bFoundEndTime = false;

	for( UMovieSceneSection* Section : Sections )
	{
		if( Section->GetStartTime() >= StartTime )
		{
			EndTime = Section->GetStartTime();
			bFoundEndTime = true;
			break;
		}
	}

	if( !bFoundEndTime )
	{
		UMovieScene* OwnerScene = GetTypedOuter<UMovieScene>();

		// End time should just end where the movie scene ends.  Ensure it is at least the same as start time (this should only happen when the movie scene has an initial time range smaller than the start time
		EndTime = FMath::Max( OwnerScene->GetPlaybackRange().GetUpperBoundValue(), StartTime );
	}

			
	if( StartTime == EndTime )
	{
		// Give the shot a reasonable length of time to start out with.  A 0 time shot is not usable
		EndTime = StartTime + .5f;
	}

	return EndTime;
}


#undef LOCTEXT_NAMESPACE