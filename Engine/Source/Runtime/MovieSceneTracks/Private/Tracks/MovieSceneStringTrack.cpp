// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneStringSection.h"
#include "MovieSceneStringTrack.h"
#include "MovieSceneStringTrackInstance.h"


#define LOCTEXT_NAMESPACE "MovieSceneStringTrack"


/* UMovieSceneStringTrack interface
 *****************************************************************************/

bool UMovieSceneStringTrack::AddKeyToSection(float Time, const FString& String, FKeyParams KeyParams)
{
	UMovieSceneSection* TargetSection = MovieSceneHelpers::FindNearestSectionAtTime(Sections, Time);

	if (TargetSection == nullptr)
	{
		TargetSection = CreateNewSection();
		TargetSection->SetStartTime(Time);
		TargetSection->SetEndTime(Time);

		Sections.Add(TargetSection);
	}

	UMovieSceneStringSection* StringSection = Cast<UMovieSceneStringSection>(TargetSection);

	if (StringSection == nullptr)
	{
		return false;
	}

	StringSection->AddKey(Time, String, EMovieSceneKeyInterpolation::Auto);

	return true;
}


bool UMovieSceneStringTrack::Eval(float Position, float LastPostion, FString& OutString) const
{
	const UMovieSceneSection* Section = MovieSceneHelpers::FindNearestSectionAtTime(Sections, Position);

	if (Section)
	{
		if (!Section->IsInfinite())
		{
			Position = FMath::Clamp(Position, Section->GetStartTime(), Section->GetEndTime());
		}

		OutString = CastChecked<UMovieSceneStringSection>(Section)->Eval(Position, OutString);
	}

	return Section != nullptr;
}


/* UMovieSceneTrack interface
 *****************************************************************************/

void UMovieSceneStringTrack::AddSection(UMovieSceneSection& Section)
{
	Sections.Add(&Section);
}


TSharedPtr<IMovieSceneTrackInstance> UMovieSceneStringTrack::CreateInstance()
{
	return MakeShareable(new FMovieSceneStringTrackInstance(*this));
}


UMovieSceneSection* UMovieSceneStringTrack::CreateNewSection()
{
	return NewObject<UMovieSceneSection>(this, UMovieSceneStringSection::StaticClass(), NAME_None, RF_Transactional);
}


const TArray<UMovieSceneSection*>& UMovieSceneStringTrack::GetAllSections() const
{
	return Sections;
}


TRange<float> UMovieSceneStringTrack::GetSectionBoundaries() const
{
	TRange<float> SectionBoundaries = TRange<float>::Empty();

	for (auto& Section : Sections)
	{
		SectionBoundaries = TRange<float>::Hull(SectionBoundaries, Section->GetRange());
	}

	return SectionBoundaries;
}


bool UMovieSceneStringTrack::HasSection(const UMovieSceneSection& Section) const
{
	return Sections.Contains(&Section);
}


bool UMovieSceneStringTrack::IsEmpty() const
{
	return (Sections.Num() == 0);
}


void UMovieSceneStringTrack::RemoveAllAnimationData()
{
	Sections.Empty();
}


void UMovieSceneStringTrack::RemoveSection(UMovieSceneSection& Section)
{
	Sections.Remove(&Section);
}


#if WITH_EDITORONLY_DATA

FText UMovieSceneStringTrack::GetDefaultDisplayName() const
{ 
	return LOCTEXT("TrackName", "Strings"); 
}

#endif


#undef LOCTEXT_NAMESPACE
