// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"
#include "MovieScene2DTransformSection.h"
#include "MovieScene2DTransformTrack.h"

UMovieScene2DTransformSection::UMovieScene2DTransformSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{

}

void UMovieScene2DTransformSection::MoveSection( float DeltaTime )
{
	Super::MoveSection( DeltaTime );

	Rotation.ShiftCurve( DeltaTime );

	// Move all the curves in this section
	for( int32 Axis = 0; Axis < 2; ++Axis )
	{
		Translation[Axis].ShiftCurve( DeltaTime );
		Scale[Axis].ShiftCurve( DeltaTime );
		Shear[Axis].ShiftCurve( DeltaTime );
	}
}

void UMovieScene2DTransformSection::DilateSection( float DilationFactor, float Origin )
{
	Super::DilateSection(DilationFactor, Origin);

	Rotation.ScaleCurve( Origin, DilationFactor );

	for( int32 Axis = 0; Axis < 2; ++Axis )
	{
		Translation[Axis].ScaleCurve( Origin, DilationFactor );
		Scale[Axis].ScaleCurve( Origin, DilationFactor );
		Shear[Axis].ScaleCurve( Origin, DilationFactor  );
	}
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
	default:
		check( false );
		return NULL;
		break;
	}
}

FRichCurve& UMovieScene2DTransformSection::GetTranslationCurve(EAxis::Type Axis)
{
	return *ChooseCurve( Axis, Translation );
}

FRichCurve& UMovieScene2DTransformSection::GetRotationCurve()
{
	return Rotation;
}

FRichCurve& UMovieScene2DTransformSection::GetScaleCurve(EAxis::Type Axis)
{
	return *ChooseCurve(Axis, Scale);
}

FRichCurve& UMovieScene2DTransformSection::GetSheerCurve(EAxis::Type Axis)
{
	return *ChooseCurve(Axis, Shear);
}

FWidgetTransform UMovieScene2DTransformSection::Eval( float Position, const FWidgetTransform& DefaultValue ) const
{
	return FWidgetTransform(	FVector2D( Translation[0].Eval( Position, DefaultValue.Translation.X ), Translation[1].Eval( Position, DefaultValue.Translation.Y ) ),
								FVector2D( Scale[0].Eval( Position, DefaultValue.Scale.X ),	Scale[1].Eval( Position, DefaultValue.Scale.Y ) ),
								FVector2D( Shear[0].Eval( Position, DefaultValue.Shear.X ),	Shear[1].Eval( Position, DefaultValue.Shear.Y ) ),
								Rotation.Eval( Position, DefaultValue.Angle ) );
}


bool UMovieScene2DTransformSection::NewKeyIsNewData( float Time, const FWidgetTransform& Transform ) const
{
	bool bIsNewData = false;
	for(int32 Axis = 0; Axis < 2; ++Axis)
	{
		bIsNewData |= Translation[Axis].GetNumKeys() == 0;
		bIsNewData |= Scale[Axis].GetNumKeys() == 0;
		bIsNewData |= Shear[Axis].GetNumKeys() == 0;
	}

	bIsNewData |= Rotation.GetNumKeys() == 0;

	return bIsNewData || Eval(Time,Transform) != Transform;
}

void UMovieScene2DTransformSection::AddKeyToNamedCurve(float Time, const F2DTransformKey& TransformKey)
{
	static FName TranslationName("Translation");
	static FName ScaleName("Scale");
	static FName ShearName("Shear");
	static FName AngleName("Angle");

	FName CurveName = TransformKey.CurveName;

	if(CurveName == TranslationName)
	{
		AddKeyToCurve(Translation[0], Time, TransformKey.Value.Translation.X);
		AddKeyToCurve(Translation[1], Time, TransformKey.Value.Translation.Y);
	}
	else if(CurveName == ScaleName)
	{
		AddKeyToCurve(Scale[0], Time, TransformKey.Value.Scale.X);
		AddKeyToCurve(Scale[1], Time, TransformKey.Value.Scale.Y);
	}
	else if(CurveName == ShearName)
	{
		AddKeyToCurve(Shear[0], Time, TransformKey.Value.Shear.X);
		AddKeyToCurve(Shear[1], Time, TransformKey.Value.Shear.Y);
	}
	else if(CurveName == AngleName)
	{
		AddKeyToCurve(Rotation, Time, TransformKey.Value.Angle);
	}
}



void UMovieScene2DTransformSection::AddKey( float Time, const struct F2DTransformKey& TransformKey )
{
	Modify();

	if(TransformKey.CurveName == NAME_None)
	{
		AddKeyToCurve(Translation[0], Time, TransformKey.Value.Translation.X);
		AddKeyToCurve(Translation[1], Time, TransformKey.Value.Translation.Y);
		AddKeyToCurve(Scale[0], Time, TransformKey.Value.Scale.X);
		AddKeyToCurve(Scale[1], Time, TransformKey.Value.Scale.Y);
		AddKeyToCurve(Shear[0], Time, TransformKey.Value.Shear.X);
		AddKeyToCurve(Shear[1], Time, TransformKey.Value.Shear.Y);
		AddKeyToCurve(Rotation, Time, TransformKey.Value.Angle);
	}
	else
	{
		AddKeyToNamedCurve(Time, TransformKey);
	}
}