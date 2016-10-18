// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneParameterSection.h"

FScalarParameterNameAndCurve::FScalarParameterNameAndCurve( FName InParameterName )
{
	ParameterName = InParameterName;
}

FVectorParameterNameAndCurves::FVectorParameterNameAndCurves( FName InParameterName )
{
	ParameterName = InParameterName;
}

FColorParameterNameAndCurves::FColorParameterNameAndCurves( FName InParameterName )
{
	ParameterName = InParameterName;
}

UMovieSceneParameterSection::UMovieSceneParameterSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
}

void UMovieSceneParameterSection::AddScalarParameterKey( FName InParameterName, float InTime, float InValue )
{
	FRichCurve* ExistingCurve = nullptr;
	for ( FScalarParameterNameAndCurve& ScalarParameterNameAndCurve : ScalarParameterNamesAndCurves )
	{
		if ( ScalarParameterNameAndCurve.ParameterName == InParameterName )
		{
			ExistingCurve = &ScalarParameterNameAndCurve.ParameterCurve;
			break;
		}
	}
	if ( ExistingCurve == nullptr )
	{
		int32 NewIndex = ScalarParameterNamesAndCurves.Add( FScalarParameterNameAndCurve( InParameterName ) );
		ScalarParameterNamesAndCurves[NewIndex].Index = ScalarParameterNamesAndCurves.Num() + VectorParameterNamesAndCurves.Num() - 1;
		ExistingCurve = &ScalarParameterNamesAndCurves[NewIndex].ParameterCurve;
	}
	ExistingCurve->AddKey(InTime, InValue);
}

void UMovieSceneParameterSection::AddVectorParameterKey( FName InParameterName, float InTime, FVector InValue )
{
	FVectorParameterNameAndCurves* ExistingCurves = nullptr;
	for ( FVectorParameterNameAndCurves& VectorParameterNameAndCurve : VectorParameterNamesAndCurves )
	{
		if ( VectorParameterNameAndCurve.ParameterName == InParameterName )
		{
			ExistingCurves = &VectorParameterNameAndCurve;
			break;
		}
	}
	if ( ExistingCurves == nullptr )
	{
		int32 NewIndex = VectorParameterNamesAndCurves.Add( FVectorParameterNameAndCurves( InParameterName ) );
		VectorParameterNamesAndCurves[NewIndex].Index = VectorParameterNamesAndCurves.Num() + VectorParameterNamesAndCurves.Num() - 1;
		ExistingCurves = &VectorParameterNamesAndCurves[NewIndex];
	}
	ExistingCurves->XCurve.AddKey( InTime, InValue.X );
	ExistingCurves->YCurve.AddKey( InTime, InValue.Y );
	ExistingCurves->ZCurve.AddKey( InTime, InValue.Z );
}

void UMovieSceneParameterSection::AddColorParameterKey( FName InParameterName, float InTime, FLinearColor InValue )
{
	FColorParameterNameAndCurves* ExistingCurves = nullptr;
	for ( FColorParameterNameAndCurves& ColorParameterNameAndCurve : ColorParameterNamesAndCurves )
	{
		if ( ColorParameterNameAndCurve.ParameterName == InParameterName )
		{
			ExistingCurves = &ColorParameterNameAndCurve;
			break;
		}
	}
	if ( ExistingCurves == nullptr )
	{
		int32 NewIndex = ColorParameterNamesAndCurves.Add( FColorParameterNameAndCurves( InParameterName ) );
		ColorParameterNamesAndCurves[NewIndex].Index = ColorParameterNamesAndCurves.Num() + ColorParameterNamesAndCurves.Num() - 1;
		ExistingCurves = &ColorParameterNamesAndCurves[NewIndex];
	}
	ExistingCurves->RedCurve.AddKey( InTime, InValue.R );
	ExistingCurves->GreenCurve.AddKey( InTime, InValue.G );
	ExistingCurves->BlueCurve.AddKey( InTime, InValue.B );
	ExistingCurves->AlphaCurve.AddKey( InTime, InValue.A );
}

bool UMovieSceneParameterSection::RemoveScalarParameter( FName InParameterName )
{
	for ( int32 i = 0; i < ScalarParameterNamesAndCurves.Num(); i++ )
	{
		if ( ScalarParameterNamesAndCurves[i].ParameterName == InParameterName )
		{
			ScalarParameterNamesAndCurves.RemoveAt(i);
			UpdateParameterIndicesFromRemoval(i);
			return true;
		}
	}
	return false;
}

bool UMovieSceneParameterSection::RemoveVectorParameter( FName InParameterName )
{
	for ( int32 i = 0; i < VectorParameterNamesAndCurves.Num(); i++ )
	{
		if ( VectorParameterNamesAndCurves[i].ParameterName == InParameterName )
		{
			VectorParameterNamesAndCurves.RemoveAt( i );
			UpdateParameterIndicesFromRemoval( i );
			return true;
		}
	}
	return false;
}

bool UMovieSceneParameterSection::RemoveColorParameter( FName InParameterName )
{
	for ( int32 i = 0; i < ColorParameterNamesAndCurves.Num(); i++ )
	{
		if ( ColorParameterNamesAndCurves[i].ParameterName == InParameterName )
		{
			ColorParameterNamesAndCurves.RemoveAt( i );
			UpdateParameterIndicesFromRemoval( i );
			return true;
		}
	}
	return false;
}

TArray<FScalarParameterNameAndCurve>* UMovieSceneParameterSection::GetScalarParameterNamesAndCurves()
{
	return &ScalarParameterNamesAndCurves;
}

TArray<FVectorParameterNameAndCurves>* UMovieSceneParameterSection::GetVectorParameterNamesAndCurves()
{
	return &VectorParameterNamesAndCurves;
}

TArray<FColorParameterNameAndCurves>* UMovieSceneParameterSection::GetColorParameterNamesAndCurves()
{
	return &ColorParameterNamesAndCurves;
}

void UMovieSceneParameterSection::GetParameterNames( TSet<FName>& ParameterNames ) const
{
	for ( const FScalarParameterNameAndCurve& ScalarParameterNameAndCurve : ScalarParameterNamesAndCurves )
	{
		ParameterNames.Add( ScalarParameterNameAndCurve.ParameterName );
	}
	for ( const FVectorParameterNameAndCurves& VectorParameterNameAndCurves : VectorParameterNamesAndCurves )
	{
		ParameterNames.Add( VectorParameterNameAndCurves.ParameterName );
	}
	for ( const FColorParameterNameAndCurves& ColorParameterNameAndCurves : ColorParameterNamesAndCurves )
	{
		ParameterNames.Add( ColorParameterNameAndCurves.ParameterName );
	}
}

void UMovieSceneParameterSection::Eval( float Position,
	TArray<FScalarParameterNameAndValue>& OutScalarValues,
	TArray<FVectorParameterNameAndValue>& OutVectorValues,
	TArray<FColorParameterNameAndValue>& OutColorValues ) const
{
	for ( const FScalarParameterNameAndCurve& ScalarParameterNameAndCurve : ScalarParameterNamesAndCurves )
	{
		OutScalarValues.Add( FScalarParameterNameAndValue( ScalarParameterNameAndCurve.ParameterName, ScalarParameterNameAndCurve.ParameterCurve.Eval( Position ) ) );
	}
	for ( const FVectorParameterNameAndCurves& VectorParameterNameAndCurves : VectorParameterNamesAndCurves )
	{
		OutVectorValues.Add( FVectorParameterNameAndValue( VectorParameterNameAndCurves.ParameterName, FVector(
			VectorParameterNameAndCurves.XCurve.Eval( Position ),
			VectorParameterNameAndCurves.YCurve.Eval( Position ),
			VectorParameterNameAndCurves.ZCurve.Eval( Position ) ) ) );
	}
	for ( const FColorParameterNameAndCurves& ColorParameterNameAndCurves : ColorParameterNamesAndCurves )
	{
		OutColorValues.Add( FColorParameterNameAndValue( ColorParameterNameAndCurves.ParameterName, FLinearColor(
			ColorParameterNameAndCurves.RedCurve.Eval( Position ),
			ColorParameterNameAndCurves.GreenCurve.Eval( Position ),
			ColorParameterNameAndCurves.BlueCurve.Eval( Position ),
			ColorParameterNameAndCurves.AlphaCurve.Eval( Position ) ) ) );
	}
}

void UMovieSceneParameterSection::UpdateParameterIndicesFromRemoval( int32 RemovedIndex )
{
	for ( FScalarParameterNameAndCurve& ScalarParameterAndCurve : ScalarParameterNamesAndCurves )
	{
		if ( ScalarParameterAndCurve.Index > RemovedIndex )
		{
			ScalarParameterAndCurve.Index--;
		}
	}
	for ( FVectorParameterNameAndCurves& VectorParameterAndCurves : VectorParameterNamesAndCurves )
	{
		if ( VectorParameterAndCurves.Index > RemovedIndex )
		{
			VectorParameterAndCurves.Index--;
		}
	}
	for ( FColorParameterNameAndCurves& ColorParameterAndCurves : ColorParameterNamesAndCurves )
	{
		if ( ColorParameterAndCurves.Index > RemovedIndex )
		{
			ColorParameterAndCurves.Index--;
		}
	}
}

void UMovieSceneParameterSection::GatherCurves(TArray<const FRichCurve*>& OutCurves) const
{
	for ( const FScalarParameterNameAndCurve& ScalarParameterNameAndCurve : ScalarParameterNamesAndCurves )
	{
		OutCurves.Add(&ScalarParameterNameAndCurve.ParameterCurve);
	}

	for ( const FVectorParameterNameAndCurves& VectorParameterNameAndCurves : VectorParameterNamesAndCurves )
	{
		OutCurves.Add(&VectorParameterNameAndCurves.XCurve);
		OutCurves.Add(&VectorParameterNameAndCurves.YCurve);
		OutCurves.Add(&VectorParameterNameAndCurves.ZCurve);
	}

	for ( const FColorParameterNameAndCurves& ColorParameterNameAndCurve : ColorParameterNamesAndCurves )
	{
		OutCurves.Add(&ColorParameterNameAndCurve.RedCurve);
		OutCurves.Add(&ColorParameterNameAndCurve.BlueCurve);
		OutCurves.Add(&ColorParameterNameAndCurve.GreenCurve);
		OutCurves.Add(&ColorParameterNameAndCurve.AlphaCurve);
	}
}

void UMovieSceneParameterSection::GatherCurves(TArray<FRichCurve*>& OutCurves)
{
	for ( FScalarParameterNameAndCurve& ScalarParameterNameAndCurve : ScalarParameterNamesAndCurves )
	{
		OutCurves.Add(&ScalarParameterNameAndCurve.ParameterCurve);
	}

	for ( FVectorParameterNameAndCurves& VectorParameterNameAndCurves : VectorParameterNamesAndCurves )
	{
		OutCurves.Add(&VectorParameterNameAndCurves.XCurve);
		OutCurves.Add(&VectorParameterNameAndCurves.YCurve);
		OutCurves.Add(&VectorParameterNameAndCurves.ZCurve);
	}

	for ( FColorParameterNameAndCurves& ColorParameterNameAndCurve : ColorParameterNamesAndCurves )
	{
		OutCurves.Add(&ColorParameterNameAndCurve.RedCurve);
		OutCurves.Add(&ColorParameterNameAndCurve.BlueCurve);
		OutCurves.Add(&ColorParameterNameAndCurve.GreenCurve);
		OutCurves.Add(&ColorParameterNameAndCurve.AlphaCurve);
	}
}



/* UMovieSceneSection overrides
 *****************************************************************************/

void UMovieSceneParameterSection::DilateSection(float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles)
{
	Super::DilateSection(DilationFactor, Origin, KeyHandles);

	TArray<FRichCurve*> AllCurves;
	GatherCurves(AllCurves);

	for (auto Curve : AllCurves)
	{
		Curve->ScaleCurve(Origin, DilationFactor, KeyHandles);
	}
}

void UMovieSceneParameterSection::GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const
{
	if (!TimeRange.Overlaps(GetRange()))
	{
		return;
	}

	TArray<const FRichCurve*> AllCurves;
	GatherCurves(AllCurves);

	for (auto Curve : AllCurves)
	{
		for (auto It(Curve->GetKeyHandleIterator()); It; ++It)
		{
			float Time = Curve->GetKeyTime(It.Key());
			if (TimeRange.Contains(Time))
			{
				OutKeyHandles.Add(It.Key());
			}
		}
	}
}


void UMovieSceneParameterSection::MoveSection(float DeltaPosition, TSet<FKeyHandle>& KeyHandles)
{
	Super::MoveSection(DeltaPosition, KeyHandles);

	TArray<FRichCurve*> AllCurves;
	GatherCurves(AllCurves);

	for (auto Curve : AllCurves)
	{
		Curve->ShiftCurve(DeltaPosition, KeyHandles);
	}
}


TOptional<float> UMovieSceneParameterSection::GetKeyTime( FKeyHandle KeyHandle ) const
{
	TArray<const FRichCurve*> AllCurves;
	GatherCurves( AllCurves );

	for ( auto Curve : AllCurves )
	{
		if ( Curve->IsKeyHandleValid( KeyHandle ) )
		{
			return TOptional<float>( Curve->GetKeyTime( KeyHandle ) );
		}
	}
	return TOptional<float>();
}


void UMovieSceneParameterSection::SetKeyTime( FKeyHandle KeyHandle, float Time )
{
	TArray<FRichCurve*> AllCurves;
	GatherCurves( AllCurves );

	for ( auto Curve : AllCurves )
	{
		if ( Curve->IsKeyHandleValid( KeyHandle ) )
		{
			Curve->SetKeyTime( KeyHandle, Time );
			break;
		}
	}
}
