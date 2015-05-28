// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCoreTypesPCH.h"
#include "MovieSceneColorSection.h"
#include "MovieSceneColorTrack.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneColorTrackInstance.h"

UMovieSceneColorTrack::UMovieSceneColorTrack( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
	, bIsSlateColor( false )
{
}

UMovieSceneSection* UMovieSceneColorTrack::CreateNewSection()
{
	return NewObject<UMovieSceneSection>(this, UMovieSceneColorSection::StaticClass(), NAME_None, RF_Transactional);
}

TSharedPtr<IMovieSceneTrackInstance> UMovieSceneColorTrack::CreateInstance()
{
	return MakeShareable( new FMovieSceneColorTrackInstance( *this ) ); 
}


bool UMovieSceneColorTrack::AddKeyToSection( float Time, const FColorKey& Key )
{
	bIsSlateColor = Key.bIsSlateColor;

	const UMovieSceneSection* NearestSection = MovieSceneHelpers::FindSectionAtTime( Sections, Time );
	if (!NearestSection || Key.bAddKeyEvenIfUnchanged || CastChecked<UMovieSceneColorSection>(NearestSection)->NewKeyIsNewData(Time, Key.Value))
	{
		Modify();
		UMovieSceneColorSection* NewSection = CastChecked<UMovieSceneColorSection>( FindOrAddSection( Time ) );

		NewSection->AddKey( Time, Key );

		return true;
	}
	return false;
}


bool UMovieSceneColorTrack::Eval( float Position, float LastPosition, FLinearColor& OutColor ) const
{
	const UMovieSceneSection* Section = MovieSceneHelpers::FindSectionAtTime( Sections, Position );

	if( Section )
	{
		OutColor = CastChecked<UMovieSceneColorSection>( Section )->Eval( Position, OutColor );
	}

	return Section != NULL;
}