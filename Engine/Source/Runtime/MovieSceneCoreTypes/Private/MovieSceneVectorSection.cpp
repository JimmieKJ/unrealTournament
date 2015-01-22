// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCoreTypesPCH.h"
#include "MovieSceneVectorSection.h"
#include "MovieSceneVectorTrack.h"

UMovieSceneVectorSection::UMovieSceneVectorSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{

}

FVector4 UMovieSceneVectorSection::Eval( float Position, const FVector4& DefaultVector ) const
{
	return FVector4(
		Curves[0].Eval( Position, DefaultVector.X ),
		Curves[1].Eval( Position, DefaultVector.Y ),
		Curves[2].Eval( Position, DefaultVector.Z ),
		Curves[3].Eval( Position, DefaultVector.W ) );
}

void UMovieSceneVectorSection::AddKey( float Time, FName CurveName, const FVector4& Value )
{
	Modify();

	if( CurveName == NAME_None )
	{
		check(ChannelsUsed >= 2 && ChannelsUsed <= 4);

		for (int32 i = 0; i < ChannelsUsed; ++i)
		{
			Curves[i].UpdateOrAddKey(Time, Value[i]);
		}
	}
	else
	{
		AddKeyToNamedCurve( Time, CurveName, Value );
	}
}

bool UMovieSceneVectorSection::NewKeyIsNewData(float Time, const FVector4& Value) const
{
	check(ChannelsUsed >= 2 && ChannelsUsed <= 4);

	bool bNewData = false;
	for (int32 i = 0; i < ChannelsUsed; ++i)
	{
		float OriginalData = Curves[i].Eval(Time);
		// don't re-add keys if the data already matches
		if (Curves[i].GetNumKeys() == 0 || !FMath::IsNearlyEqual(OriginalData, Value[i]))
		{
			bNewData = true;
			break;
		}
	}
	return bNewData;
}

void UMovieSceneVectorSection::AddKeyToNamedCurve(float Time, FName CurveName, const FVector4& Value)
{
	static FName X("X");
	static FName Y("Y");
	static FName Z("Z");
	static FName W("W");

	if (CurveName == X)
	{
		Curves[0].UpdateOrAddKey(Time, Value.X);
	}
	else if (CurveName == Y)
	{
		Curves[1].UpdateOrAddKey(Time, Value.Y);
	}
	else if (CurveName == Z)
	{
		Curves[2].UpdateOrAddKey(Time, Value.Z);
	}
	else if (CurveName == W)
	{
		Curves[3].UpdateOrAddKey(Time, Value.W);
	}
}

void UMovieSceneVectorSection::MoveSection( float DeltaTime )
{
	check(ChannelsUsed >= 2 && ChannelsUsed <= 4);

	Super::MoveSection( DeltaTime );

	for (int32 i = 0; i < ChannelsUsed; ++i)
	{
		Curves[i].ShiftCurve(DeltaTime);
	}
}

void UMovieSceneVectorSection::DilateSection( float DilationFactor, float Origin )
{
	check(ChannelsUsed >= 2 && ChannelsUsed <= 4);
	Super::DilateSection(DilationFactor, Origin);

	for (int32 i = 0; i < ChannelsUsed; ++i)
	{
		Curves[i].ScaleCurve(Origin, DilationFactor);
	}
}
