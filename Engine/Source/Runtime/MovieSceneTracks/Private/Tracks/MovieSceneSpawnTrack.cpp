// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneSpawnTrack.h"
#include "MovieSceneSpawnTrackInstance.h"
#include "MovieSceneBoolSection.h"


#define LOCTEXT_NAMESPACE "MovieSceneSpawnTrack"


/* UMovieSceneSpawnTrack structors
 *****************************************************************************/

UMovieSceneSpawnTrack::UMovieSceneSpawnTrack(const FObjectInitializer& Obj)
	: Super(Obj)
{
#if WITH_EDITORONLY_DATA
	TrackTint = FColor(43, 43, 155, 65);
#endif
}


/* UMovieSceneSpawnTrack interface
 *****************************************************************************/

bool UMovieSceneSpawnTrack::Eval(float Position, float LastPostion, bool& bOutSpawned) const
{
	const UMovieSceneBoolSection* Section = Cast<UMovieSceneBoolSection>(MovieSceneHelpers::FindNearestSectionAtTime(Sections, Position));
	if (!Section)
	{
		return false;
	}

	if (!Section->IsInfinite())
	{
		Position = FMath::Clamp(Position, Section->GetStartTime(), Section->GetEndTime());
	}

	bOutSpawned = Section->Eval(Position);
	return true;
}


/* UMovieSceneTrack interface
 *****************************************************************************/

UMovieSceneSection* UMovieSceneSpawnTrack::CreateNewSection()
{
	UMovieSceneBoolSection* Section = NewObject<UMovieSceneBoolSection>(this, NAME_None, RF_Transactional);
	Section->GetCurve().SetDefaultValue(true);
	return Section;
}


TSharedPtr<IMovieSceneTrackInstance> UMovieSceneSpawnTrack::CreateInstance()
{
	return MakeShareable(new FMovieSceneSpawnTrackInstance(*this));
}


bool UMovieSceneSpawnTrack::HasSection(const UMovieSceneSection& Section) const
{
	return Sections.ContainsByPredicate([&](const UMovieSceneSection* In){ return In == &Section; });
}


void UMovieSceneSpawnTrack::AddSection(UMovieSceneSection& Section)
{
	Sections.Add(&Section);
}


void UMovieSceneSpawnTrack::RemoveSection(UMovieSceneSection& Section)
{
	Sections.RemoveAll([&](const UMovieSceneSection* In){ return In == &Section; });
}


bool UMovieSceneSpawnTrack::IsEmpty() const
{
	return Sections.Num() == 0;
}


TRange<float> UMovieSceneSpawnTrack::GetSectionBoundaries() const
{
	return TRange<float>::All();
}


const TArray<UMovieSceneSection*>& UMovieSceneSpawnTrack::GetAllSections() const
{
	return Sections;
}

#if WITH_EDITORONLY_DATA

ECookOptimizationFlags UMovieSceneSpawnTrack::GetCookOptimizationFlags() const
{
	// Since the spawn track denotes the lifetime of a spawnable, if the object is never spawned, we can remove the entire object
	for (UMovieSceneSection* Section : Sections)
	{
		UMovieSceneBoolSection* BoolSection = CastChecked<UMovieSceneBoolSection>(Section);
		if (!BoolSection->IsActive())
		{
			continue;
		}
		else if (BoolSection->GetCurve().GetNumKeys() == 0 && BoolSection->GetCurve().GetDefaultValue())
		{
			return ECookOptimizationFlags::None;
		}
		else for (auto It = BoolSection->GetCurve().GetKeyIterator(); It; ++It)
		{
			if (It->Value)
			{
				return ECookOptimizationFlags::None;
			}
		}
	}

	return ECookOptimizationFlags::RemoveObject;
}

#endif

#if WITH_EDITORONLY_DATA

FText UMovieSceneSpawnTrack::GetDisplayName() const
{
	return LOCTEXT("TrackName", "Spawned");
}

#endif


#undef LOCTEXT_NAMESPACE
