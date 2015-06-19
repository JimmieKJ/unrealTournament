// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCoreTypesPCH.h"
#include "MovieSceneByteSection.h"


UMovieSceneByteSection::UMovieSceneByteSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
}

uint8 UMovieSceneByteSection::Eval( float Position ) const
{
	return !!ByteCurve.Evaluate(Position);
}

void UMovieSceneByteSection::MoveSection( float DeltaPosition )
{
	Super::MoveSection( DeltaPosition );

	ByteCurve.ShiftCurve(DeltaPosition);
}

void UMovieSceneByteSection::DilateSection( float DilationFactor, float Origin )
{
	Super::DilateSection(DilationFactor, Origin);
	
	ByteCurve.ScaleCurve(Origin, DilationFactor);

}

void UMovieSceneByteSection::AddKey( float Time, uint8 Value )
{
	Modify();
	ByteCurve.UpdateOrAddKey(Time, Value ? 1 : 0);
}

bool UMovieSceneByteSection::NewKeyIsNewData(float Time, uint8 Value) const
{
	return ByteCurve.GetNumKeys() == 0 || Eval(Time) != Value;
}
