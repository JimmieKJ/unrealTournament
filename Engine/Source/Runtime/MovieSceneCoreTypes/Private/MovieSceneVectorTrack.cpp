// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCoreTypesPCH.h"
#include "MovieSceneVectorSection.h"
#include "MovieSceneVectorTrack.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneVectorTrackInstance.h"

UMovieSceneVectorTrack::UMovieSceneVectorTrack( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
	NumChannelsUsed = 0;
}

UMovieSceneSection* UMovieSceneVectorTrack::CreateNewSection()
{
	return NewObject<UMovieSceneSection>(this, UMovieSceneVectorSection::StaticClass());
}

TSharedPtr<IMovieSceneTrackInstance> UMovieSceneVectorTrack::CreateInstance()
{
	return MakeShareable( new FMovieSceneVectorTrackInstance( *this ) );
}

bool UMovieSceneVectorTrack::AddKeyToSection( float Time, const FVector4& Value, int32 InChannelsUsed, FName CurveName, bool bAddKeyEvenIfUnchanged )
{
	const UMovieSceneSection* NearestSection = MovieSceneHelpers::FindSectionAtTime( Sections, Time );
	if (!NearestSection || bAddKeyEvenIfUnchanged || CastChecked<UMovieSceneVectorSection>(NearestSection)->NewKeyIsNewData(Time, Value))
	{
		Modify();

		UMovieSceneVectorSection* NewSection = Cast<UMovieSceneVectorSection>( FindOrAddSection( Time ) );
		// @todo Sequencer - I don't like setting the channels used here. It should only be checked here, and set on section creation
		NewSection->SetChannelsUsed(InChannelsUsed);

		NewSection->AddKey( Time, CurveName, Value );

		// We dont support one track containing multiple sections of differing channel amounts
		NumChannelsUsed = InChannelsUsed;

		return true;
	}
	return false;
}

bool UMovieSceneVectorTrack::AddKeyToSection( float Time, const FVectorKey<FVector4>& Key )
{
	return AddKeyToSection(Time, Key.Value, 4, Key.CurveName, Key.bAddKeyEvenIfUnchanged );
}

bool UMovieSceneVectorTrack::AddKeyToSection( float Time, const FVectorKey<FVector>& Key )
{
	return AddKeyToSection(Time, FVector4(Key.Value.X, Key.Value.Y, Key.Value.Z, 0.f), 3, Key.CurveName, Key.bAddKeyEvenIfUnchanged );
}

bool UMovieSceneVectorTrack::AddKeyToSection( float Time, const FVectorKey<FVector2D>& Key )
{
	return AddKeyToSection(Time, FVector4(Key.Value.X, Key.Value.Y, 0.f, 0.f), 2, Key.CurveName, Key.bAddKeyEvenIfUnchanged );
}

bool UMovieSceneVectorTrack::Eval( float Position, float LastPosition, FVector4& InOutVector ) const
{
	const UMovieSceneSection* Section = MovieSceneHelpers::FindSectionAtTime( Sections, Position );

	if( Section )
	{
		InOutVector = CastChecked<UMovieSceneVectorSection>( Section )->Eval( Position, InOutVector );
	}

	return Section != NULL;
}

