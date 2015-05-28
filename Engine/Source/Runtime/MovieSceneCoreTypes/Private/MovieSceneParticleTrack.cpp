// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCoreTypesPCH.h"
#include "MovieSceneParticleSection.h"
#include "MovieSceneParticleTrack.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneParticleTrackInstance.h"

#define LOCTEXT_NAMESPACE "MovieSceneParticleTrack"

UMovieSceneParticleTrack::UMovieSceneParticleTrack( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
}

FName UMovieSceneParticleTrack::GetTrackName() const
{
	return FName("ParticleSystem");
}


TSharedPtr<IMovieSceneTrackInstance> UMovieSceneParticleTrack::CreateInstance()
{
	return MakeShareable( new FMovieSceneParticleTrackInstance( *this ) ); 
}

TArray<UMovieSceneSection*> UMovieSceneParticleTrack::GetAllSections() const
{
	TArray<UMovieSceneSection*> OutSections;
	for (int32 SectionIndex = 0; SectionIndex < ParticleSections.Num(); ++SectionIndex)
	{
		OutSections.Add( ParticleSections[SectionIndex] );
	}
	return OutSections;
}

void UMovieSceneParticleTrack::RemoveAllAnimationData()
{
}

bool UMovieSceneParticleTrack::HasSection( UMovieSceneSection* Section ) const
{
	return ParticleSections.Find( Section ) != INDEX_NONE;
}

void UMovieSceneParticleTrack::RemoveSection( UMovieSceneSection* Section )
{
	ParticleSections.Remove( Section );
}

bool UMovieSceneParticleTrack::IsEmpty() const
{
	return ParticleSections.Num() == 0;
}

TRange<float> UMovieSceneParticleTrack::GetSectionBoundaries() const
{
	TArray< TRange<float> > Bounds;
	for (int32 i = 0; i < ParticleSections.Num(); ++i)
	{
		Bounds.Add(ParticleSections[i]->GetRange());
	}
	return TRange<float>::Hull(Bounds);
}

void UMovieSceneParticleTrack::AddNewParticleSystem(float KeyTime, bool bTrigger)
{
	EParticleKey::Type KeyType = bTrigger ? EParticleKey::Trigger : EParticleKey::Toggle;
	// @todo Instead of a 0.1 second event, this should be 0 seconds, requires handling 0 size sections
	float Duration = KeyType == EParticleKey::Trigger ? 0.1f : 1.f;

	UMovieSceneParticleSection* NewSection = NewObject<UMovieSceneParticleSection>(this);
	NewSection->InitialPlacement(ParticleSections, KeyTime, KeyTime + Duration, SupportsMultipleRows());
	NewSection->SetKeyType(KeyType);

	ParticleSections.Add(NewSection);
}

#undef LOCTEXT_NAMESPACE
