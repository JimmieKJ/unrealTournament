// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieScene3DAttachSection.h"
#include "MovieScene3DAttachTrack.h"


UMovieScene3DAttachSection::UMovieScene3DAttachSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
	AttachSocketName = NAME_None;
	bConstrainTx = true;
	bConstrainTy = true;
	bConstrainTz = true;
	bConstrainRx = true;
	bConstrainRy = true;
	bConstrainRz = true;
}


void UMovieScene3DAttachSection::Eval( USceneComponent* SceneComponent, float Position, AActor* Actor, FVector& OutTranslation, FRotator& OutRotation ) const
{
	if (Actor->GetRootComponent()->DoesSocketExist(AttachSocketName))
	{
		FTransform SocketTransform = Actor->GetRootComponent()->GetSocketTransform(AttachSocketName);
		OutTranslation = SocketTransform.GetLocation();
		OutRotation = SocketTransform.GetRotation().Rotator();
	}
	else
	{
		OutTranslation = Actor->GetActorLocation();
		OutRotation = Actor->GetActorRotation();
	}

	if (!bConstrainTx)
	{
		OutTranslation[0] = SceneComponent->GetRelativeTransform().GetLocation()[0];
	}

	if (!bConstrainTy)
	{
		OutTranslation[1] = SceneComponent->GetRelativeTransform().GetLocation()[1];
	}

	if (!bConstrainTz)
	{
		OutTranslation[1] = SceneComponent->GetRelativeTransform().GetLocation()[2];
	}

	if (!bConstrainRx)
	{
		OutRotation.Roll = SceneComponent->GetRelativeTransform().GetRotation().Rotator().Roll;
	}

	if (!bConstrainRx)
	{
		OutRotation.Pitch = SceneComponent->GetRelativeTransform().GetRotation().Rotator().Pitch;
	}

	if (!bConstrainRx)
	{
		OutRotation.Yaw = SceneComponent->GetRelativeTransform().GetRotation().Rotator().Yaw;
	}
}


void UMovieScene3DAttachSection::AddAttach( float Time, float SequenceEndTime, const FGuid& InAttachId )
{
	Modify();
	ConstraintId = InAttachId;
}

