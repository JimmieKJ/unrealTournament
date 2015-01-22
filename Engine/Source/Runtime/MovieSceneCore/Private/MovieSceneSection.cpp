// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCorePrivatePCH.h"
#include "MovieSceneSection.h"

UMovieSceneSection::UMovieSceneSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{

	StartTime = 0.0f;
	EndTime = 0.0f;
	RowIndex = 0;
}

const UMovieSceneSection* UMovieSceneSection::OverlapsWithSections(const TArray<UMovieSceneSection*>& Sections, int32 TrackDelta, float TimeDelta) const
{
	int32 NewTrackIndex = RowIndex + TrackDelta;
	TRange<float> NewSectionRange = TRange<float>(StartTime + TimeDelta, EndTime + TimeDelta);
	for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
	{
		const UMovieSceneSection* InSection = Sections[SectionIndex];
		if (this != InSection && InSection->GetRowIndex() == NewTrackIndex)
		{
			if (NewSectionRange.Overlaps(InSection->GetRange()))
			{
				return InSection;
			}
		}
	}
	return NULL;
}

void UMovieSceneSection::InitialPlacement(const TArray<UMovieSceneSection*>& Sections, float InStartTime, float InEndTime, bool bAllowMultipleRows)
{
	check(StartTime <= EndTime);

	StartTime = InStartTime;
	EndTime = InEndTime;

	RowIndex = 0;
	if (bAllowMultipleRows)
	{
		while (OverlapsWithSections(Sections) != NULL)
		{
			++RowIndex;
		}
	}
	else
	{
		for (;;)
		{
			const UMovieSceneSection* OverlappedSection = OverlapsWithSections(Sections);

			if (OverlappedSection == NULL) {break;}

			MoveSection(OverlappedSection->GetEndTime() - StartTime);
		}
	}
}

void UMovieSceneSection::AddKeyToCurve( FRichCurve& InCurve, float Time, float Value )
{
	if(IsTimeWithinSection(Time))
	{
		Modify();
		InCurve.UpdateOrAddKey(Time, Value);
	}
}