// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCoreTypesPCH.h"
#include "MovieSceneColorSection.h"


UMovieSceneColorSection::UMovieSceneColorSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
}

void UMovieSceneColorSection::MoveSection( float DeltaTime )
{
	Super::MoveSection( DeltaTime );

	// Move all the curves in this section
	RedCurve.ShiftCurve(DeltaTime);
	GreenCurve.ShiftCurve(DeltaTime);
	BlueCurve.ShiftCurve(DeltaTime);
	AlphaCurve.ShiftCurve(DeltaTime);
}

void UMovieSceneColorSection::DilateSection( float DilationFactor, float Origin )
{
	Super::DilateSection(DilationFactor, Origin);

	RedCurve.ScaleCurve(Origin, DilationFactor);
	GreenCurve.ScaleCurve(Origin, DilationFactor);
	BlueCurve.ScaleCurve(Origin, DilationFactor);
	AlphaCurve.ScaleCurve(Origin, DilationFactor);

}

FLinearColor UMovieSceneColorSection::Eval( float Position, const FLinearColor& DefaultColor ) const
{
	return FLinearColor(RedCurve.Eval(Position, DefaultColor.R),
						GreenCurve.Eval(Position, DefaultColor.G),
						BlueCurve.Eval(Position, DefaultColor.B),
						AlphaCurve.Eval(Position, DefaultColor.A));
}

void UMovieSceneColorSection::AddKey( float Time, const FColorKey& Key )
{
	Modify();

	if( Key.CurveName == NAME_None )
	{
		AddKeyToCurve(RedCurve, Time, Key.Value.R);
		AddKeyToCurve(GreenCurve, Time, Key.Value.G);
		AddKeyToCurve(BlueCurve, Time, Key.Value.B);
		AddKeyToCurve(AlphaCurve, Time, Key.Value.A);
	}
	else
	{
		AddKeyToNamedCurve( Time, Key );
	}
}

bool UMovieSceneColorSection::NewKeyIsNewData(float Time, FLinearColor Value) const
{
	return RedCurve.GetNumKeys() == 0 ||
		GreenCurve.GetNumKeys() == 0 ||
		BlueCurve.GetNumKeys() == 0 ||
		AlphaCurve.GetNumKeys() == 0 ||
		!Eval(Time,Value).Equals(Value);
}

void UMovieSceneColorSection::AddKeyToNamedCurve(float Time, const FColorKey& Key)
{
	static FName R("R");
	static FName G("G");
	static FName B("B");
	static FName A("A");

	FName CurveName = Key.CurveName;
	if (CurveName == R)
	{
		AddKeyToCurve(RedCurve, Time, Key.Value.R);
	}
	else if (CurveName == G)
	{
		AddKeyToCurve(GreenCurve, Time, Key.Value.G);
	}
	else if (CurveName == B)
	{
		AddKeyToCurve(BlueCurve, Time, Key.Value.B);
	}
	else if (CurveName == A)
	{
		AddKeyToCurve(AlphaCurve, Time, Key.Value.A);
	}
}