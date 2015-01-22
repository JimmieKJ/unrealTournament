// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"
#include "MovieSceneMarginSection.h"
#include "MovieSceneMarginTrack.h"

UMovieSceneMarginSection::UMovieSceneMarginSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
}

void UMovieSceneMarginSection::MoveSection( float DeltaTime )
{
	Super::MoveSection( DeltaTime );

	// Move all the curves in this section
	LeftCurve.ShiftCurve(DeltaTime);
	TopCurve.ShiftCurve(DeltaTime);
	RightCurve.ShiftCurve(DeltaTime);
	BottomCurve.ShiftCurve(DeltaTime);
}

void UMovieSceneMarginSection::DilateSection( float DilationFactor, float Origin )
{
	Super::DilateSection(DilationFactor, Origin);

	LeftCurve.ScaleCurve(Origin, DilationFactor);
	TopCurve.ScaleCurve(Origin, DilationFactor);
	RightCurve.ScaleCurve(Origin, DilationFactor);
	BottomCurve.ScaleCurve(Origin, DilationFactor);
}

FMargin UMovieSceneMarginSection::Eval( float Position, const FMargin& DefaultValue ) const
{
	return FMargin(	LeftCurve.Eval(Position, DefaultValue.Left),
					TopCurve.Eval(Position, DefaultValue.Top),
					RightCurve.Eval(Position, DefaultValue.Right),
					BottomCurve.Eval(Position, DefaultValue.Bottom));
}

void UMovieSceneMarginSection::AddKey( float Time, const FMarginKey& MarginKey )
{
	Modify();

	if( MarginKey.CurveName == NAME_None )
	{
		AddKeyToCurve(LeftCurve, Time, MarginKey.Value.Left);
		AddKeyToCurve(TopCurve, Time, MarginKey.Value.Top);
		AddKeyToCurve(RightCurve, Time, MarginKey.Value.Right);
		AddKeyToCurve(BottomCurve, Time, MarginKey.Value.Bottom);
	}
	else
	{
		AddKeyToNamedCurve( Time, MarginKey );
	}
}

bool UMovieSceneMarginSection::NewKeyIsNewData(float Time, const FMargin& Value) const
{
	return TopCurve.GetNumKeys() == 0 ||
		LeftCurve.GetNumKeys() == 0 ||
		RightCurve.GetNumKeys() == 0 ||
		BottomCurve.GetNumKeys() == 0 ||
		(Eval(Time,Value) != Value);
}

void UMovieSceneMarginSection::AddKeyToNamedCurve( float Time, const FMarginKey& MarginKey )
{
	static FName Left("Left");
	static FName Top("Top");
	static FName Right("Right");
	static FName Bottom("Bottom");

	FName CurveName = MarginKey.CurveName;

	if( CurveName == Left )
	{
		AddKeyToCurve(LeftCurve, Time, MarginKey.Value.Left);
	}
	else if( CurveName == Top )
	{
		AddKeyToCurve(TopCurve, Time, MarginKey.Value.Top);
	}
	else if( CurveName == Right )
	{
		AddKeyToCurve(RightCurve, Time, MarginKey.Value.Right);
	}
	else if( CurveName == Bottom )
	{
		AddKeyToCurve(BottomCurve, Time, MarginKey.Value.Bottom);
	}
}

