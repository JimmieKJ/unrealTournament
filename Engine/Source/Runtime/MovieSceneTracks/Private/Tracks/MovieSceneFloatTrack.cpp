// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneFloatSection.h"
#include "MovieSceneFloatTrack.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneFloatTrackInstance.h"


UMovieSceneFloatTrack::UMovieSceneFloatTrack( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{ }


UMovieSceneSection* UMovieSceneFloatTrack::CreateNewSection()
{
	return NewObject<UMovieSceneSection>(this, UMovieSceneFloatSection::StaticClass(), NAME_None, RF_Transactional);
}


TSharedPtr<IMovieSceneTrackInstance> UMovieSceneFloatTrack::CreateInstance()
{
	return MakeShareable( new FMovieSceneFloatTrackInstance( *this ) ); 
}


bool UMovieSceneFloatTrack::Eval( float Position, float LastPosition, float& OutFloat ) const
{
	const UMovieSceneSection* Section = MovieSceneHelpers::FindNearestSectionAtTime( Sections, Position );

	if( Section )
	{
		if (!Section->IsInfinite())
		{
			Position = FMath::Clamp(Position, Section->GetStartTime(), Section->GetEndTime());
		}

		OutFloat = CastChecked<UMovieSceneFloatSection>( Section )->Eval( Position );
	}

	return Section != nullptr;
}

