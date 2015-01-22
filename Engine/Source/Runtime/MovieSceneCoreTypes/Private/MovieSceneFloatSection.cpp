// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCoreTypesPCH.h"
#include "MovieSceneFloatSection.h"


UMovieSceneFloatSection::UMovieSceneFloatSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
}

float UMovieSceneFloatSection::Eval( float Position ) const
{
	return FloatCurve.Eval( Position );
}

void UMovieSceneFloatSection::MoveSection( float DeltaPosition )
{
	Super::MoveSection( DeltaPosition );

	// Move the curve
	FloatCurve.ShiftCurve(DeltaPosition );
}

void UMovieSceneFloatSection::DilateSection( float DilationFactor, float Origin )
{	
	Super::DilateSection(DilationFactor, Origin);
	
	FloatCurve.ScaleCurve(Origin, DilationFactor);
}

void UMovieSceneFloatSection::AddKey( float Time, float Value )
{
	Modify();
	FloatCurve.UpdateOrAddKey(Time, Value);
}

bool UMovieSceneFloatSection::NewKeyIsNewData(float Time, float Value) const
{
	return FloatCurve.GetNumKeys() == 0 || !FMath::IsNearlyEqual(FloatCurve.Eval(Time), Value);
}
