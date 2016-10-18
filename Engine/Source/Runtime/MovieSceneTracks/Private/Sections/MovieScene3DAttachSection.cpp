// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieScene3DAttachSection.h"
#include "MovieScene3DAttachTrack.h"


UMovieScene3DAttachSection::UMovieScene3DAttachSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
	AttachSocketName = NAME_None;
	AttachComponentName = NAME_None;
	bConstrainTx = true;
	bConstrainTy = true;
	bConstrainTz = true;
	bConstrainRx = true;
	bConstrainRy = true;
	bConstrainRz = true;
}


void UMovieScene3DAttachSection::Eval( USceneComponent* SceneComponent, float Position, AActor* ParentActor, FVector& OutTranslation, FRotator& OutRotation ) const
{
	bool bFoundAttachment = false;
	if(AttachSocketName != NAME_None)
	{
		if(AttachComponentName != NAME_None )
		{
			TInlineComponentArray<USceneComponent*> PotentialAttachComponents(ParentActor);
			for(USceneComponent* PotentialAttachComponent : PotentialAttachComponents)
			{
				if(PotentialAttachComponent->GetFName() == AttachComponentName && PotentialAttachComponent->DoesSocketExist(AttachSocketName))
				{
					FTransform SocketTransform = PotentialAttachComponent->GetSocketTransform(AttachSocketName);
					OutTranslation = SocketTransform.GetLocation();
					OutRotation = SocketTransform.GetRotation().Rotator();
					bFoundAttachment = true;
					break;
				}
			}
		}
		else if (ParentActor->GetRootComponent()->DoesSocketExist(AttachSocketName))
		{
			FTransform SocketTransform = ParentActor->GetRootComponent()->GetSocketTransform(AttachSocketName);
			OutTranslation = SocketTransform.GetLocation();
			OutRotation = SocketTransform.GetRotation().Rotator();
			bFoundAttachment = true;
		}
	}
	else if(AttachComponentName != NAME_None )
	{
		TInlineComponentArray<USceneComponent*> PotentialAttachComponents(ParentActor);
		for(USceneComponent* PotentialAttachComponent : PotentialAttachComponents)
		{
			if(PotentialAttachComponent->GetFName() == AttachComponentName)
			{
				FTransform ComponentTransform = PotentialAttachComponent->GetComponentToWorld();
				OutTranslation = ComponentTransform.GetLocation();
				OutRotation = ComponentTransform.GetRotation().Rotator();
				bFoundAttachment = true;
				break;
			}
		}
	}

	if (!bFoundAttachment)
	{
		if (ParentActor->GetDefaultAttachComponent())
		{
			OutTranslation = ParentActor->GetDefaultAttachComponent()->GetComponentLocation();
			OutRotation = ParentActor->GetDefaultAttachComponent()->GetComponentRotation();
		}
		else
		{
			OutTranslation = ParentActor->GetActorLocation();
			OutRotation = ParentActor->GetActorRotation();
		}
	}

	FTransform ParentTransform(OutRotation, OutTranslation);
	FTransform RelativeTransform = SceneComponent->GetRelativeTransform();

	FTransform NewRelativeTransform = RelativeTransform * ParentTransform;

	OutTranslation = NewRelativeTransform.GetLocation();
	OutRotation = NewRelativeTransform.GetRotation().Rotator();

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
		OutTranslation[2] = SceneComponent->GetRelativeTransform().GetLocation()[2];
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
	if (TryModify())
	{
		ConstraintId = InAttachId;
	}
}

