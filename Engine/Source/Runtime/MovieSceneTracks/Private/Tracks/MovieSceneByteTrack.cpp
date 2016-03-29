// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneByteSection.h"
#include "MovieSceneByteTrack.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneByteTrackInstance.h"


UMovieSceneByteTrack::UMovieSceneByteTrack( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{ }


UMovieSceneSection* UMovieSceneByteTrack::CreateNewSection()
{
	return NewObject<UMovieSceneSection>(this, UMovieSceneByteSection::StaticClass(), NAME_None, RF_Transactional);
}


TSharedPtr<IMovieSceneTrackInstance> UMovieSceneByteTrack::CreateInstance()
{
	return MakeShareable( new FMovieSceneByteTrackInstance( *this ) );
}


bool UMovieSceneByteTrack::Eval( float Position, float LastPostion, uint8& OutByte ) const
{	
	const UMovieSceneSection* Section = MovieSceneHelpers::FindNearestSectionAtTime( Sections, Position );

	if( Section )
	{
		if (!Section->IsInfinite())
		{
			Position = FMath::Clamp(Position, Section->GetStartTime(), Section->GetEndTime());
		}

		OutByte = CastChecked<UMovieSceneByteSection>( Section )->Eval( Position );
	}

	return Section != nullptr;
}

void UMovieSceneByteTrack::SetEnum(UEnum* InEnum)
{
	Enum = InEnum;
}


class UEnum* UMovieSceneByteTrack::GetEnum() const
{
	return Enum;
}
