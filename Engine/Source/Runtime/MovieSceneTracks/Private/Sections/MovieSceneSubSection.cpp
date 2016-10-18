// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneSequence.h"
#include "MovieSceneSubSection.h"

TWeakObjectPtr<UMovieSceneSubSection> UMovieSceneSubSection::TheRecordingSection;

/* UMovieSceneSubSection structors
 *****************************************************************************/

UMovieSceneSubSection::UMovieSceneSubSection()
	: StartOffset(0.0f)
	, TimeScale(1.0f)
	, PrerollTime(0.0f)
{
}

void UMovieSceneSubSection::SetSequence(UMovieSceneSequence* Sequence)
{
	SubSequence = Sequence;

#if WITH_EDITOR
	OnSequenceChangedDelegate.ExecuteIfBound(SubSequence);
#endif
}

UMovieSceneSequence* UMovieSceneSubSection::GetSequence() const
{
	// when recording we need to act as if we have no sequence
	// the sequence is patched at the end of recording
	if(GetRecordingSection() == this)
	{
		return nullptr;
	}
	else
	{
		return SubSequence;
	}
}

UMovieSceneSubSection* UMovieSceneSubSection::GetRecordingSection()
{
	// check if the section is still valid and part of a track (i.e. it has not been deleted or GCed)
	if(TheRecordingSection.IsValid())
	{
		UMovieSceneTrack* TrackOuter = Cast<UMovieSceneTrack>(TheRecordingSection->GetOuter());
		if(TrackOuter)
		{
			if(TrackOuter->HasSection(*TheRecordingSection.Get()))
			{
				return TheRecordingSection.Get();
			}
		}
	}

	return nullptr;
}

void UMovieSceneSubSection::SetAsRecording(bool bRecord)
{
	if(bRecord)
	{
		TheRecordingSection = this;
	}
	else
	{
		TheRecordingSection = nullptr;
	}
}

bool UMovieSceneSubSection::IsSetAsRecording()
{
	return GetRecordingSection() != nullptr;
}

AActor* UMovieSceneSubSection::GetActorToRecord()
{
	UMovieSceneSubSection* RecordingSection = GetRecordingSection();
	if(RecordingSection)
	{
		return RecordingSection->ActorToRecord.Get();
	}

	return nullptr;
}

#if WITH_EDITOR
void UMovieSceneSubSection::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// recreate runtime instance when sequence is changed
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UMovieSceneSubSection, SubSequence))
	{
		OnSequenceChangedDelegate.ExecuteIfBound(SubSequence);
	}
}
#endif

UMovieSceneSection* UMovieSceneSubSection::SplitSection( float SplitTime )
{
	if ( !IsTimeWithinSection( SplitTime ) )
	{
		return nullptr;
	}

	float InitialStartTime = GetStartTime();
	float InitialStartOffset = StartOffset;
	float NewStartOffset = ( SplitTime - InitialStartTime ) / TimeScale;
	NewStartOffset += InitialStartOffset;

	// Ensure start offset is not less than 0
	NewStartOffset = FMath::Max( NewStartOffset, 0.f );

	UMovieSceneSubSection* NewSection = Cast<UMovieSceneSubSection>( UMovieSceneSection::SplitSection( SplitTime ) );
	if ( NewSection )
	{
		NewSection->StartOffset = NewStartOffset;
		return NewSection;
	}

	return nullptr;
}

void UMovieSceneSubSection::TrimSection( float TrimTime, bool bTrimLeft )
{
	if ( !IsTimeWithinSection( TrimTime ) )
	{
		return;
	}

	float InitialStartTime = GetStartTime();
	float InitialStartOffset = StartOffset;

	UMovieSceneSection::TrimSection( TrimTime, bTrimLeft );

	// If trimming off the left, set the offset of the shot
	if ( bTrimLeft )
	{
		float NewStartOffset = ( TrimTime - InitialStartTime ) / TimeScale;
		NewStartOffset += InitialStartOffset;

		// Ensure start offset is not less than 0
		StartOffset = FMath::Max( NewStartOffset, 0.f );
	}
}