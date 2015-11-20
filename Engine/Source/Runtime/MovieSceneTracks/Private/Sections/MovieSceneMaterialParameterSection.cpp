// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneMaterialParameterSection.h"

FScalarParameterNameAndCurve::FScalarParameterNameAndCurve( FName InParameterName )
{
	ParameterName = InParameterName;
}

FVectorParameterNameAndCurves::FVectorParameterNameAndCurves( FName InParameterName )
{
	ParameterName = InParameterName;
}

UMovieSceneMaterialParameterSection::UMovieSceneMaterialParameterSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
}

void UMovieSceneMaterialParameterSection::AddScalarParameterKey( FName InParameterName, float InTime, float InValue )
{
	FRichCurve* ExistingCurve = nullptr;
	for ( FScalarParameterNameAndCurve& ScalarParameterNameAndCurve : ScalarParameterNamesAndCurves )
	{
		if ( ScalarParameterNameAndCurve.ParameterName == InParameterName )
		{
			ExistingCurve = &ScalarParameterNameAndCurve.ParameterCurve;
			break;
		}
	}
	if ( ExistingCurve == nullptr )
	{
		int32 NewIndex = ScalarParameterNamesAndCurves.Add( FScalarParameterNameAndCurve( InParameterName ) );
		ScalarParameterNamesAndCurves[NewIndex].Index = ScalarParameterNamesAndCurves.Num() + VectorParameterNamesAndCurves.Num() - 1;
		ExistingCurve = &ScalarParameterNamesAndCurves[NewIndex].ParameterCurve;
	}
	ExistingCurve->AddKey(InTime, InValue);
}

void UMovieSceneMaterialParameterSection::AddVectorParameterKey( FName InParameterName, float InTime, FLinearColor InValue )
{
	FVectorParameterNameAndCurves* ExistingCurves = nullptr;
	for ( FVectorParameterNameAndCurves& VectorParameterNameAndCurve : VectorParameterNamesAndCurves )
	{
		if ( VectorParameterNameAndCurve.ParameterName == InParameterName )
		{
			ExistingCurves = &VectorParameterNameAndCurve;
			break;
		}
	}
	if ( ExistingCurves == nullptr )
	{
		int32 NewIndex = VectorParameterNamesAndCurves.Add( FVectorParameterNameAndCurves( InParameterName ) );
		VectorParameterNamesAndCurves[NewIndex].Index = VectorParameterNamesAndCurves.Num() + VectorParameterNamesAndCurves.Num() - 1;
		ExistingCurves = &VectorParameterNamesAndCurves[NewIndex];
	}
	ExistingCurves->RedCurve.AddKey( InTime, InValue.R );
	ExistingCurves->GreenCurve.AddKey( InTime, InValue.G );
	ExistingCurves->BlueCurve.AddKey( InTime, InValue.B );
	ExistingCurves->AlphaCurve.AddKey( InTime, InValue.A );
}

bool UMovieSceneMaterialParameterSection::RemoveScalarParameter( FName InParameterName )
{
	for ( int32 i = 0; i < ScalarParameterNamesAndCurves.Num(); i++ )
	{
		if ( ScalarParameterNamesAndCurves[i].ParameterName == InParameterName )
		{
			ScalarParameterNamesAndCurves.RemoveAt(i);
			UpdateParameterIndicesFromRemoval(i);
			return true;
		}
	}
	return false;
}

bool UMovieSceneMaterialParameterSection::RemoveVectorParameter( FName InParameterName )
{
	for ( int32 i = 0; i < VectorParameterNamesAndCurves.Num(); i++ )
	{
		if ( VectorParameterNamesAndCurves[i].ParameterName == InParameterName )
		{
			VectorParameterNamesAndCurves.RemoveAt( i );
			UpdateParameterIndicesFromRemoval( i );
			return true;
		}
	}
	return false;
}

TArray<FScalarParameterNameAndCurve>* UMovieSceneMaterialParameterSection::GetScalarParameterNamesAndCurves()
{
	return &ScalarParameterNamesAndCurves;
}

TArray<FVectorParameterNameAndCurves>* UMovieSceneMaterialParameterSection::GetVectorParameterNamesAndCurves()
{
	return &VectorParameterNamesAndCurves;
}

void UMovieSceneMaterialParameterSection::UpdateParameterIndicesFromRemoval( int32 RemovedIndex )
{
	for ( FScalarParameterNameAndCurve& ScalarParameterAndCurve : ScalarParameterNamesAndCurves )
	{
		if ( ScalarParameterAndCurve.Index > RemovedIndex )
		{
			ScalarParameterAndCurve.Index--;
		}
	}
	for ( FVectorParameterNameAndCurves& VectorParameterAndCurves : VectorParameterNamesAndCurves )
	{
		if ( VectorParameterAndCurves.Index > RemovedIndex )
		{
			VectorParameterAndCurves.Index--;
		}
	}
}