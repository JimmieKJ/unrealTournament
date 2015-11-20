// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneFloatSection.h"


UMovieSceneFloatSection::UMovieSceneFloatSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{ }


float UMovieSceneFloatSection::Eval( float Position ) const
{
	return FloatCurve.Eval( Position );
}


void UMovieSceneFloatSection::MoveSection( float DeltaPosition, TSet<FKeyHandle>& KeyHandles )
{
	Super::MoveSection( DeltaPosition, KeyHandles );

	// Move the curve
	FloatCurve.ShiftCurve(DeltaPosition, KeyHandles);
}


void UMovieSceneFloatSection::DilateSection( float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles )
{	
	Super::DilateSection(DilationFactor, Origin, KeyHandles);
	
	FloatCurve.ScaleCurve(Origin, DilationFactor, KeyHandles);
}


void UMovieSceneFloatSection::GetKeyHandles(TSet<FKeyHandle>& KeyHandles) const
{
	for (auto It(FloatCurve.GetKeyHandleIterator()); It; ++It)
	{
		float Time = FloatCurve.GetKeyTime(It.Key());
		if (IsTimeWithinSection(Time))
		{
			KeyHandles.Add(It.Key());
		}
	}
}


void UMovieSceneFloatSection::AddKey( float Time, const float& Value, EMovieSceneKeyInterpolation KeyInterpolation )
{
	AddKeyToCurve( FloatCurve, Time, Value, KeyInterpolation );
}


bool UMovieSceneFloatSection::NewKeyIsNewData(float Time, const float& Value) const
{
	return FMath::IsNearlyEqual( FloatCurve.Eval( Time ), Value ) == false;
}

bool UMovieSceneFloatSection::HasKeys( const float& Value ) const
{
	return FloatCurve.GetNumKeys() > 0;
}

void UMovieSceneFloatSection::SetDefault( const float& Value )
{
	SetCurveDefault( FloatCurve, Value );
}
