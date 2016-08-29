// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneCameraShakeSection.h"
#include "MovieSceneCameraShakeTrack.h"


UMovieSceneCameraShakeSection::UMovieSceneCameraShakeSection(const FObjectInitializer& ObjectInitializer)
 	: Super( ObjectInitializer )
{
	PlayScale = 1.f;
	PlaySpace = ECameraAnimPlaySpace::CameraLocal;
	UserDefinedPlaySpace = FRotator::ZeroRotator;
}

void UMovieSceneCameraShakeSection::MoveSection(float DeltaPosition, TSet<FKeyHandle>& KeyHandles)
{
	Super::MoveSection( DeltaPosition, KeyHandles );

	// Move the curve
//	ShakeWeightCurve.ShiftCurve(DeltaPosition, KeyHandles);
}


void UMovieSceneCameraShakeSection::DilateSection(float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles)
{	
	Super::DilateSection(DilationFactor, Origin, KeyHandles);
	
//	ShakeWeightCurve.ScaleCurve(Origin, DilationFactor, KeyHandles);
}


void UMovieSceneCameraShakeSection::GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const
{
	if (!TimeRange.Overlaps(GetRange()))
	{
		return;
	}

// 	for (auto It(ShakeWeightCurve.GetKeyHandleIterator()); It; ++It)
// 	{
// 		float Time = ShakeWeightCurve.GetKeyTime(It.Key());
// 		if (TimeRange.Contains(Time))
// 		{
// 			KeyHandles.Add(It.Key());
// 		}
// 	}
}


TOptional<float> UMovieSceneCameraShakeSection::GetKeyTime( FKeyHandle KeyHandle ) const
{
	//if ( ShakeWeightCurve.IsKeyHandleValid( KeyHandle ) )
	//{
	//	return TOptional<float>( ShakeWeightCurve.GetKeyTime( KeyHandle ) );
	//}
	return TOptional<float>();
}


void UMovieSceneCameraShakeSection::SetKeyTime( FKeyHandle KeyHandle, float Time )
{
	//if ( ShakeWeightCurve.IsKeyHandleValid( KeyHandle ) )
	//{
	//	ShakeWeightCurve.SetKeyTime( KeyHandle, Time );
	//}
}
