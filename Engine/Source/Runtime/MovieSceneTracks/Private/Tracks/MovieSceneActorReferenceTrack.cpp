// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneActorReferenceSection.h"
#include "MovieSceneActorReferenceTrack.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneActorReferenceTrackInstance.h"


UMovieSceneActorReferenceTrack::UMovieSceneActorReferenceTrack( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{ }


UMovieSceneSection* UMovieSceneActorReferenceTrack::CreateNewSection()
{
	return NewObject<UMovieSceneSection>(this, UMovieSceneActorReferenceSection::StaticClass(), NAME_None, RF_Transactional);
}


TSharedPtr<IMovieSceneTrackInstance> UMovieSceneActorReferenceTrack::CreateInstance()
{
	return MakeShareable( new FMovieSceneActorReferenceTrackInstance( *this ) ); 
}


bool UMovieSceneActorReferenceTrack::Eval( float Position, float LastPosition, FGuid& OutActorReferenceGuid ) const
{
	const UMovieSceneSection* Section = MovieSceneHelpers::FindNearestSectionAtTime( Sections, Position );

	if ( Section )
	{
		if ( !Section->IsInfinite() )
		{
			Position = FMath::Clamp( Position, Section->GetStartTime(), Section->GetEndTime() );
		}

		OutActorReferenceGuid = CastChecked<UMovieSceneActorReferenceSection>( Section )->Eval( Position );
	}

	return Section != nullptr;
}

