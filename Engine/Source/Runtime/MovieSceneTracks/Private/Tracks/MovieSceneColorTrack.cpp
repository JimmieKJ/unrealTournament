// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneColorSection.h"
#include "MovieSceneColorTrack.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneColorTrackInstance.h"


UMovieSceneColorTrack::UMovieSceneColorTrack( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{ }


UMovieSceneSection* UMovieSceneColorTrack::CreateNewSection()
{
	return NewObject<UMovieSceneSection>(this, UMovieSceneColorSection::StaticClass(), NAME_None, RF_Transactional);
}


TSharedPtr<IMovieSceneTrackInstance> UMovieSceneColorTrack::CreateInstance()
{
	return MakeShareable( new FMovieSceneColorTrackInstance( *this ) ); 
}


bool UMovieSceneColorTrack::Eval( float Position, float LastPosition, FLinearColor& OutColor ) const
{
	const UMovieSceneSection* Section = MovieSceneHelpers::FindNearestSectionAtTime( Sections, Position );

	if( Section )
	{
		if (!Section->IsInfinite())
		{
			Position = FMath::Clamp(Position, Section->GetStartTime(), Section->GetEndTime());
		}

		OutColor = CastChecked<UMovieSceneColorSection>( Section )->Eval( Position, OutColor );
	}

	return Section != nullptr;
}

