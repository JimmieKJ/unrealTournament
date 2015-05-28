// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCoreTypesPCH.h"
#include "MovieSceneBoolSection.h"
#include "MovieSceneBoolTrack.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneBoolTrackInstance.h"


UMovieSceneBoolTrack::UMovieSceneBoolTrack( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
}

UMovieSceneSection* UMovieSceneBoolTrack::CreateNewSection()
{
	return NewObject<UMovieSceneSection>(this, UMovieSceneBoolSection::StaticClass(), NAME_None, RF_Transactional);
}

TSharedPtr<IMovieSceneTrackInstance> UMovieSceneBoolTrack::CreateInstance()
{
	return MakeShareable( new FMovieSceneBoolTrackInstance( *this ) );
}

bool UMovieSceneBoolTrack::AddKeyToSection( float Time, bool Value )
{
	const UMovieSceneSection* NearestSection = MovieSceneHelpers::FindSectionAtTime( Sections, Time );
	if (!NearestSection || CastChecked<UMovieSceneBoolSection>(NearestSection)->NewKeyIsNewData(Time, Value))
	{
		Modify();

		UMovieSceneBoolSection* NewSection = CastChecked<UMovieSceneBoolSection>(FindOrAddSection(  Time ));
	
		NewSection->AddKey( Time, Value );

		return true;
	}
	return false;
}

bool UMovieSceneBoolTrack::Eval( float Position, float LastPostion, bool& OutBool ) const
{	
	const UMovieSceneSection* Section = MovieSceneHelpers::FindSectionAtTime( Sections, Position );

	if( Section )
	{
		OutBool = CastChecked<UMovieSceneBoolSection>( Section )->Eval( Position );
	}

	return Section != NULL;
}
