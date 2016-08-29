// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneCameraAnimSection.h"
#include "MovieSceneCameraAnimTrack.h"


UMovieSceneCameraAnimSection::UMovieSceneCameraAnimSection(const FObjectInitializer& ObjectInitializer)
 	: Super( ObjectInitializer )
{
	PlayRate = 1.f;
	PlayScale = 1.f;
	BlendInTime = 0.f;
	BlendOutTime = 0.f;
	bLooping = false;
	bRandomStartTime = false;
//	Duration = false;
	PlaySpace = ECameraAnimPlaySpace::CameraLocal;
	UserDefinedPlaySpace = FRotator::ZeroRotator;
}

void UMovieSceneCameraAnimSection::MoveSection(float DeltaPosition, TSet<FKeyHandle>& KeyHandles)
{
	Super::MoveSection( DeltaPosition, KeyHandles );

	// Move the curve
	//AnimWeightCurve.ShiftCurve(DeltaPosition, KeyHandles);
}


void UMovieSceneCameraAnimSection::DilateSection(float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles)
{	
	Super::DilateSection(DilationFactor, Origin, KeyHandles);
	
	//AnimWeightCurve.ScaleCurve(Origin, DilationFactor, KeyHandles);
}


void UMovieSceneCameraAnimSection::GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const
{
	if (!TimeRange.Overlaps(GetRange()))
	{
		return;
	}

// 	for (auto It(AnimWeightCurve.GetKeyHandleIterator()); It; ++It)
// 	{
// 		float Time = AnimWeightCurve.GetKeyTime(It.Key());
// 		if (TimeRange.Contains(Time))
// 		{
// 			KeyHandles.Add(It.Key());
// 		}
// 	}
}

TOptional<float> UMovieSceneCameraAnimSection::GetKeyTime( FKeyHandle KeyHandle ) const
{
	//if ( AnimWeightCurve.IsKeyHandleValid( KeyHandle ) )
	//{
	//	return TOptional<float>( AnimWeightCurve.GetKeyTime( KeyHandle ) );
	//}
	return TOptional<float>();
}


void UMovieSceneCameraAnimSection::SetKeyTime( FKeyHandle KeyHandle, float Time )
{
	//if ( AnimWeightCurve.IsKeyHandleValid( KeyHandle ) )
	//{
	//	AnimWeightCurve.SetKeyTime( KeyHandle, Time );
	//}
}
