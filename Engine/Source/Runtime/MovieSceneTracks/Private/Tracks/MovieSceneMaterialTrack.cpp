// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneParameterSection.h"
#include "MovieSceneMaterialTrack.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneMaterialTrackInstance.h"


UMovieSceneMaterialTrack::UMovieSceneMaterialTrack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	TrackTint = FColor(64,192,64,65);
#endif
}


UMovieSceneSection* UMovieSceneMaterialTrack::CreateNewSection()
{
	return NewObject<UMovieSceneParameterSection>(this, UMovieSceneParameterSection::StaticClass(), NAME_None, RF_Transactional);
}


void UMovieSceneMaterialTrack::RemoveAllAnimationData()
{
	Sections.Empty();
}


bool UMovieSceneMaterialTrack::HasSection(const UMovieSceneSection& Section) const
{
	return Sections.Contains(&Section);
}


void UMovieSceneMaterialTrack::AddSection(UMovieSceneSection& Section)
{
	Sections.Add(&Section);
}


void UMovieSceneMaterialTrack::RemoveSection(UMovieSceneSection& Section)
{
	Sections.Remove(&Section);
}


bool UMovieSceneMaterialTrack::IsEmpty() const
{
	return Sections.Num() == 0;
}


TRange<float> UMovieSceneMaterialTrack::GetSectionBoundaries() const
{
	TArray< TRange<float> > Bounds;

	for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
	{
		Bounds.Add(Sections[SectionIndex]->GetRange());
	}

	return TRange<float>::Hull(Bounds);
}


const TArray<UMovieSceneSection*>& UMovieSceneMaterialTrack::GetAllSections() const
{
	return Sections;
}


void UMovieSceneMaterialTrack::AddScalarParameterKey(FName ParameterName, float Time, float Value)
{
	UMovieSceneParameterSection* NearestSection = Cast<UMovieSceneParameterSection>(MovieSceneHelpers::FindNearestSectionAtTime(Sections, Time));
	if (NearestSection == nullptr)
	{
		NearestSection = Cast<UMovieSceneParameterSection>(CreateNewSection());
		NearestSection->SetStartTime(Time);
		NearestSection->SetEndTime(Time);
		Sections.Add(NearestSection);
	}
	if (NearestSection->TryModify())
	{
		NearestSection->AddScalarParameterKey(ParameterName, Time, Value);	
	}
}


void UMovieSceneMaterialTrack::AddColorParameterKey(FName ParameterName, float Time, FLinearColor Value)
{
	UMovieSceneParameterSection* NearestSection = Cast<UMovieSceneParameterSection>(MovieSceneHelpers::FindNearestSectionAtTime(Sections, Time));
	if (NearestSection == nullptr)
	{
		NearestSection = Cast<UMovieSceneParameterSection>(CreateNewSection());
		NearestSection->SetStartTime(Time);
		NearestSection->SetEndTime(Time);
		Sections.Add(NearestSection);
	}
	if (NearestSection->TryModify())
	{
		NearestSection->AddColorParameterKey(ParameterName, Time, Value);
	}
}


void UMovieSceneMaterialTrack::Eval(float Position, TArray<FScalarParameterNameAndValue>& OutScalarValues, TArray<FColorParameterNameAndValue>& OutColorValues) const
{
	TArray<FVectorParameterNameAndValue> OutVectorValues;
	for (UMovieSceneSection* Section : Sections)
	{
		if ( Section->IsActive() )
		{
			UMovieSceneParameterSection* ParameterSection = Cast<UMovieSceneParameterSection>( Section );
			ParameterSection->Eval( Position, OutScalarValues, OutVectorValues, OutColorValues );
		}
	}
}


UMovieSceneComponentMaterialTrack::UMovieSceneComponentMaterialTrack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
}


TSharedPtr<IMovieSceneTrackInstance> UMovieSceneComponentMaterialTrack::CreateInstance()
{
	return MakeShareable(new FMovieSceneComponentMaterialTrackInstance(*this));
}


#if WITH_EDITORONLY_DATA
FText UMovieSceneComponentMaterialTrack::GetDefaultDisplayName() const
{
	return FText::FromString(FString::Printf(TEXT("Material Element %i"), MaterialIndex));
}
#endif
