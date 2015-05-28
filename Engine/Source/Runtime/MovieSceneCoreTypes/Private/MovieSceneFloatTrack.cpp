// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCoreTypesPCH.h"
#include "MovieSceneFloatSection.h"
#include "MovieSceneFloatTrack.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneFloatTrackInstance.h"


UMovieSceneFloatTrack::UMovieSceneFloatTrack( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
}

UMovieSceneSection* UMovieSceneFloatTrack::CreateNewSection()
{
	return NewObject<UMovieSceneSection>(this, UMovieSceneFloatSection::StaticClass(), NAME_None, RF_Transactional);
}

TSharedPtr<IMovieSceneTrackInstance> UMovieSceneFloatTrack::CreateInstance()
{
	return MakeShareable( new FMovieSceneFloatTrackInstance( *this ) ); 
}


bool UMovieSceneFloatTrack::AddKeyToSection( float Time, float Value )
{
	const UMovieSceneSection* NearestSection = MovieSceneHelpers::FindSectionAtTime( Sections, Time );
	if (!NearestSection || CastChecked<UMovieSceneFloatSection>(NearestSection)->NewKeyIsNewData(Time, Value))
	{
		Modify();

		UMovieSceneFloatSection* NewSection = Cast<UMovieSceneFloatSection>( FindOrAddSection( Time ) );

		NewSection->AddKey( Time, Value );

		return true;
	}
	return false;
}

bool UMovieSceneFloatTrack::Eval( float Position, float LastPosition, float& OutFloat ) const
{
	const UMovieSceneSection* Section = MovieSceneHelpers::FindSectionAtTime( Sections, Position );

	if( Section )
	{
		OutFloat = CastChecked<UMovieSceneFloatSection>( Section )->Eval( Position );
	}

	return Section != NULL;
}
