// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"
#include "MovieSceneMarginSection.h"
#include "MovieSceneMarginTrack.h"

UMovieSceneMarginSection::UMovieSceneMarginSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
}

void UMovieSceneMarginSection::MoveSection( float DeltaTime, TSet<FKeyHandle>& KeyHandles )
{
	Super::MoveSection( DeltaTime, KeyHandles );

	// Move all the curves in this section
	LeftCurve.ShiftCurve(DeltaTime, KeyHandles);
	TopCurve.ShiftCurve(DeltaTime, KeyHandles);
	RightCurve.ShiftCurve(DeltaTime, KeyHandles);
	BottomCurve.ShiftCurve(DeltaTime, KeyHandles);
}

void UMovieSceneMarginSection::DilateSection( float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles )
{
	Super::DilateSection(DilationFactor, Origin, KeyHandles);

	LeftCurve.ScaleCurve(Origin, DilationFactor, KeyHandles);
	TopCurve.ScaleCurve(Origin, DilationFactor, KeyHandles);
	RightCurve.ScaleCurve(Origin, DilationFactor, KeyHandles);
	BottomCurve.ScaleCurve(Origin, DilationFactor, KeyHandles);
}

void UMovieSceneMarginSection::GetKeyHandles(TSet<FKeyHandle>& KeyHandles) const
{
	for (auto It(LeftCurve.GetKeyHandleIterator()); It; ++It)
	{
		float Time = LeftCurve.GetKeyTime(It.Key());
		if (IsTimeWithinSection(Time))
		{
			KeyHandles.Add(It.Key());
		}
	}
	for (auto It(TopCurve.GetKeyHandleIterator()); It; ++It)
	{
		float Time = TopCurve.GetKeyTime(It.Key());
		if (IsTimeWithinSection(Time))
		{
			KeyHandles.Add(It.Key());
		}
	}
	for (auto It(RightCurve.GetKeyHandleIterator()); It; ++It)
	{
		float Time = RightCurve.GetKeyTime(It.Key());
		if (IsTimeWithinSection(Time))
		{
			KeyHandles.Add(It.Key());
		}
	}
	for (auto It(BottomCurve.GetKeyHandleIterator()); It; ++It)
	{
		float Time = BottomCurve.GetKeyTime(It.Key());
		if (IsTimeWithinSection(Time))
		{
			KeyHandles.Add(It.Key());
		}
	}
}

FMargin UMovieSceneMarginSection::Eval( float Position, const FMargin& DefaultValue ) const
{
	return FMargin(	LeftCurve.Eval(Position, DefaultValue.Left),
					TopCurve.Eval(Position, DefaultValue.Top),
					RightCurve.Eval(Position, DefaultValue.Right),
					BottomCurve.Eval(Position, DefaultValue.Bottom));
}


template<typename CurveType>
CurveType* GetCurveForChannel( EKeyMarginChannel Channel, CurveType* LeftCurve, CurveType* TopCurve, CurveType* RightCurve, CurveType* BottomCurve )
{
	switch ( Channel )
	{
	case EKeyMarginChannel::Left:
		return LeftCurve;
	case EKeyMarginChannel::Top:
		return TopCurve;
	case EKeyMarginChannel::Right:
		return RightCurve;
	case EKeyMarginChannel::Bottom:
		return BottomCurve;
	}
	checkf(false, TEXT("Invalid curve channel"));
	return nullptr;
}


void UMovieSceneMarginSection::AddKey( float Time, const FMarginKey& Key, EMovieSceneKeyInterpolation KeyInterpolation )
{
	FRichCurve* KeyCurve = GetCurveForChannel( Key.Channel, &LeftCurve, &TopCurve, &RightCurve, &BottomCurve );
	AddKeyToCurve( *KeyCurve, Time, Key.Value, KeyInterpolation );
}


bool UMovieSceneMarginSection::NewKeyIsNewData( float Time, const FMarginKey& Key ) const
{
	const FRichCurve* KeyCurve = GetCurveForChannel( Key.Channel, &LeftCurve, &TopCurve, &RightCurve, &BottomCurve );
	return FMath::IsNearlyEqual( KeyCurve->Eval( Time ), Key.Value ) == false;
}

bool UMovieSceneMarginSection::HasKeys( const FMarginKey& Key ) const
{
	const FRichCurve* KeyCurve = GetCurveForChannel( Key.Channel, &LeftCurve, &TopCurve, &RightCurve, &BottomCurve );
	return KeyCurve->GetNumKeys() != 0;
}

void UMovieSceneMarginSection::SetDefault( const FMarginKey& Key )
{
	FRichCurve* KeyCurve = GetCurveForChannel( Key.Channel, &LeftCurve, &TopCurve, &RightCurve, &BottomCurve );
	SetCurveDefault( *KeyCurve, Key.Value );
}

