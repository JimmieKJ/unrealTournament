// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneMaterialParameterSection.h"
#include "MovieSceneMaterialTrack.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneMaterialTrackInstance.h"

UMovieSceneMaterialTrack::UMovieSceneMaterialTrack( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
}

UMovieSceneSection* UMovieSceneMaterialTrack::CreateNewSection()
{
	return NewObject<UMovieSceneMaterialParameterSection>( this, UMovieSceneMaterialParameterSection::StaticClass(), NAME_None, RF_Transactional );
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

	for ( int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex )
	{
		Bounds.Add( Sections[SectionIndex]->GetRange() );
	}

	return TRange<float>::Hull( Bounds );
}

const TArray<UMovieSceneSection*>& UMovieSceneMaterialTrack::GetAllSections() const
{
	return Sections;
}

void UMovieSceneMaterialTrack::AddScalarParameterKey( FName ParameterName, float Time, float Value )
{
	UMovieSceneMaterialParameterSection* NearestSection = Cast<UMovieSceneMaterialParameterSection>(MovieSceneHelpers::FindNearestSectionAtTime(Sections, Time));
	if ( NearestSection == nullptr )
	{
		NearestSection = Cast<UMovieSceneMaterialParameterSection>(CreateNewSection());
		NearestSection->SetStartTime( Time );
		NearestSection->SetEndTime( Time );
		Sections.Add( NearestSection );
	}
	NearestSection->AddScalarParameterKey(ParameterName, Time, Value);
}

void UMovieSceneMaterialTrack::AddVectorParameterKey( FName ParameterName, float Time, FLinearColor Value )
{
	UMovieSceneMaterialParameterSection* NearestSection = Cast<UMovieSceneMaterialParameterSection>( MovieSceneHelpers::FindNearestSectionAtTime( Sections, Time ) );
	if ( NearestSection == nullptr )
	{
		NearestSection = Cast<UMovieSceneMaterialParameterSection>( CreateNewSection() );
		NearestSection->SetStartTime( Time );
		NearestSection->SetEndTime( Time );
		Sections.Add( NearestSection );
	}
	NearestSection->AddVectorParameterKey( ParameterName, Time, Value );
}

void UMovieSceneMaterialTrack::Eval( float Position, TArray<FScalarParameterNameAndValue>& OutScalarValues, TArray<FVectorParameterNameAndValue>& OutVectorValues ) const
{
	for ( UMovieSceneSection* Section : Sections )
	{
		UMovieSceneMaterialParameterSection* MaterialParameterSection = Cast<UMovieSceneMaterialParameterSection>( Section );
		if ( MaterialParameterSection != nullptr )
		{
			for ( const FScalarParameterNameAndCurve& ScalarParameterNameAndCurve : *MaterialParameterSection->GetScalarParameterNamesAndCurves() )
			{
				OutScalarValues.Add( FScalarParameterNameAndValue( ScalarParameterNameAndCurve.ParameterName, ScalarParameterNameAndCurve.ParameterCurve.Eval( Position ) ) );
			}
			for ( const FVectorParameterNameAndCurves& VectorParameterNameAndCurves : *MaterialParameterSection->GetVectorParameterNamesAndCurves() )
			{
				OutVectorValues.Add( FVectorParameterNameAndValue( VectorParameterNameAndCurves.ParameterName, FLinearColor(
					VectorParameterNameAndCurves.RedCurve.Eval( Position ),
					VectorParameterNameAndCurves.GreenCurve.Eval( Position ),
					VectorParameterNameAndCurves.BlueCurve.Eval( Position ),
					VectorParameterNameAndCurves.AlphaCurve.Eval( Position ) ) ) );
			}
		}
	}
}

UMovieSceneComponentMaterialTrack::UMovieSceneComponentMaterialTrack( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
	
}

TSharedPtr<IMovieSceneTrackInstance> UMovieSceneComponentMaterialTrack::CreateInstance()
{
	return MakeShareable( new FMovieSceneComponentMaterialTrackInstance( *this ) );
}


#if WITH_EDITORONLY_DATA
FText UMovieSceneComponentMaterialTrack::GetDisplayName() const
{
	return FText::FromString(FString::Printf(TEXT("Material Element %i"), MaterialIndex));
}
#endif
