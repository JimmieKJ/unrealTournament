// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCoreTypesPCH.h"
#include "MovieScene3DTransformSection.h"
#include "MovieScene3DTransformTrack.h"

UMovieScene3DTransformSection::UMovieScene3DTransformSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{

}

void UMovieScene3DTransformSection::MoveSection( float DeltaTime )
{
	Super::MoveSection( DeltaTime );

	// Move all the curves in this section
	for( int32 Axis = 0; Axis < 3; ++Axis )
	{
		Translation[Axis].ShiftCurve( DeltaTime );
		Rotation[Axis].ShiftCurve( DeltaTime );
		Scale[Axis].ShiftCurve( DeltaTime );
	}
}

void UMovieScene3DTransformSection::DilateSection( float DilationFactor, float Origin )
{
	Super::DilateSection(DilationFactor, Origin);

	for( int32 Axis = 0; Axis < 3; ++Axis )
	{
		Translation[Axis].ScaleCurve( Origin, DilationFactor );
		Rotation[Axis].ScaleCurve( Origin, DilationFactor );
		Scale[Axis].ScaleCurve( Origin, DilationFactor );
	}
}

	

bool UMovieScene3DTransformSection::EvalTranslation( float Time, FVector& OutTranslation ) const
{
	bool bHasTranslationKeys = Translation[0].GetNumKeys() > 0 || Translation[1].GetNumKeys() > 0 || Translation[2].GetNumKeys() > 0;

	if( bHasTranslationKeys )
	{
		// Evaluate each component of translation if there are any keys
		OutTranslation.X = Translation[0].Eval( Time );
		OutTranslation.Y = Translation[1].Eval( Time );
		OutTranslation.Z = Translation[2].Eval( Time );
	}

	return bHasTranslationKeys;
}

bool UMovieScene3DTransformSection::EvalRotation( float Time, FRotator& OutRotation ) const
{
	bool bHasRotationKeys = Rotation[0].GetNumKeys() > 0 || Rotation[1].GetNumKeys() > 0 || Rotation[2].GetNumKeys() > 0;

	if( bHasRotationKeys )
	{
		// Evaluate each component of rotation if there are any keys
		OutRotation.Roll = Rotation[0].Eval( Time );
		OutRotation.Pitch = Rotation[1].Eval( Time );
		OutRotation.Yaw = Rotation[2].Eval( Time );
	}

	return bHasRotationKeys;

}

bool UMovieScene3DTransformSection::EvalScale( float Time, FVector& OutScale ) const
{
	bool bHasScaleKeys = Scale[0].GetNumKeys() > 0 || Scale[1].GetNumKeys() > 0 || Scale[2].GetNumKeys() > 0;
	
	if( bHasScaleKeys )
	{
		// Evaluate each component of scale if there are any keys
		OutScale.X = Scale[0].Eval( Time );
		OutScale.Y = Scale[1].Eval( Time );
		OutScale.Z = Scale[2].Eval( Time );
	}

	return bHasScaleKeys;

}

/**
 * Chooses an appropriate curve from an axis and a set of curves
 */
static FRichCurve* ChooseCurve( EAxis::Type Axis, FRichCurve* Curves )
{
	switch( Axis )
	{
	case EAxis::X:
		return &Curves[0];
		break;
	case EAxis::Y:
		return &Curves[1];
		break;
	case EAxis::Z:
		return &Curves[2];
		break;
	default:
		check( false );
		return NULL;
		break;
	}
}

FRichCurve& UMovieScene3DTransformSection::GetTranslationCurve( EAxis::Type Axis ) 
{
	return *ChooseCurve( Axis, Translation );
}

FRichCurve& UMovieScene3DTransformSection::GetRotationCurve( EAxis::Type Axis )
{
	return *ChooseCurve( Axis, Rotation );
}

FRichCurve& UMovieScene3DTransformSection::GetScaleCurve( EAxis::Type Axis )
{
	return *ChooseCurve( Axis, Scale );
}



void UMovieScene3DTransformSection::AddTranslationKeys( const FTransformKey& TransformKey )
{
	const float Time = TransformKey.GetKeyTime();

	const bool bUnwindRotation = false; // Not needed for translation

	// Key each component.  Note: we always key each component if no key exists
	if( Translation[0].GetNumKeys() == 0 || TransformKey.ShouldKeyTranslation( EAxis::X ) )
	{
		AddKeyToCurve( Translation[0], Time, TransformKey.GetTranslationValue().X );
	}

	if( Translation[1].GetNumKeys() == 0 || TransformKey.ShouldKeyTranslation( EAxis::Y ) )
	{ 
		AddKeyToCurve( Translation[1], Time, TransformKey.GetTranslationValue().Y );
	}

	if( Translation[2].GetNumKeys() == 0 || TransformKey.ShouldKeyTranslation( EAxis::Z ) )
	{ 
		AddKeyToCurve( Translation[2], Time, TransformKey.GetTranslationValue().Z );
	}
}

void UMovieScene3DTransformSection::AddRotationKeys( const FTransformKey& TransformKey, const bool bUnwindRotation )
{
	const float Time = TransformKey.GetKeyTime();

	// Key each component.  Note: we always key each component if no key exists
	if( Rotation[0].GetNumKeys() == 0 || TransformKey.ShouldKeyRotation( EAxis::X ) )
	{
		AddKeyToCurve( Rotation[0], Time, TransformKey.GetRotationValue().Roll );
	}

	if( Rotation[1].GetNumKeys() == 0 || TransformKey.ShouldKeyRotation( EAxis::Y ) )
	{ 
		AddKeyToCurve( Rotation[1], Time, TransformKey.GetRotationValue().Pitch );
	}

	if( Rotation[2].GetNumKeys() == 0 || TransformKey.ShouldKeyRotation( EAxis::Z ) )
	{ 
		AddKeyToCurve( Rotation[2], Time, TransformKey.GetRotationValue().Yaw );
	}
}

void UMovieScene3DTransformSection::AddScaleKeys( const FTransformKey& TransformKey )
{
	const float Time = TransformKey.GetKeyTime();

	const bool bUnwindRotation = false; // Not needed for scale

	// Key each component.  Note: we always key each component if no key exists
	if( Scale[0].GetNumKeys() == 0 || TransformKey.ShouldKeyScale( EAxis::X ) )
	{
		AddKeyToCurve( Scale[0], Time, TransformKey.GetScaleValue().X );
	}

	if( Scale[1].GetNumKeys() == 0 || TransformKey.ShouldKeyScale( EAxis::Y ) )
	{ 
		AddKeyToCurve( Scale[1], Time, TransformKey.GetScaleValue().Y );
	}

	if( Scale[2].GetNumKeys() == 0 || TransformKey.ShouldKeyScale( EAxis::Z ) )
	{ 
		AddKeyToCurve( Scale[2], Time, TransformKey.GetScaleValue().Z );
	}
}

bool UMovieScene3DTransformSection::NewKeyIsNewData(const FTransformKey& TransformKey) const
{
	bool bHasEmptyKeys = false;
	for (int32 i = 0; i < 3; ++i)
	{
		bHasEmptyKeys = bHasEmptyKeys ||
			Translation[i].GetNumKeys() == 0 ||
			Rotation[i].GetNumKeys() == 0 ||
			Scale[i].GetNumKeys() == 0;
	}
	return bHasEmptyKeys || TransformKey.ShouldKeyAny();
}
