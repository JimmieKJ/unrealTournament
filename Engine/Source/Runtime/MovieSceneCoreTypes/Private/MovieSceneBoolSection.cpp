// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCoreTypesPCH.h"
#include "MovieSceneBoolSection.h"


UMovieSceneBoolSection::UMovieSceneBoolSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
}

bool UMovieSceneBoolSection::Eval( float Position ) const
{
	return !!BoolCurve.Evaluate(Position);
}

void UMovieSceneBoolSection::MoveSection( float DeltaPosition )
{
	Super::MoveSection( DeltaPosition );

	BoolCurve.ShiftCurve(DeltaPosition);
}

void UMovieSceneBoolSection::DilateSection( float DilationFactor, float Origin )
{
	Super::DilateSection(DilationFactor, Origin);
	
	BoolCurve.ScaleCurve(Origin, DilationFactor);

}

void UMovieSceneBoolSection::AddKey( float Time, bool Value )
{
	Modify();
	BoolCurve.UpdateOrAddKey(Time, Value ? 1 : 0);
}

bool UMovieSceneBoolSection::NewKeyIsNewData(float Time, bool Value) const
{
	return BoolCurve.GetNumKeys() == 0 || Eval(Time) != Value;
}
