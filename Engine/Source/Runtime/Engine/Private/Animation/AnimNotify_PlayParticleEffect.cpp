// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Animation/AnimNotifies/AnimNotify_PlayParticleEffect.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"

/////////////////////////////////////////////////////
// UAnimNotify_PlayParticleEffect

UAnimNotify_PlayParticleEffect::UAnimNotify_PlayParticleEffect()
	: Super()
{
	Attached = true;

#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(192, 255, 99, 255);
#endif // WITH_EDITORONLY_DATA
}

void UAnimNotify_PlayParticleEffect::PostLoad()
{
	Super::PostLoad();

	RotationOffsetQuat = FQuat(RotationOffset);
}

#if WITH_EDITOR
void UAnimNotify_PlayParticleEffect::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.MemberProperty && PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UAnimNotify_PlayParticleEffect, RotationOffset))
	{
		RotationOffsetQuat = FQuat(RotationOffset);
	}
}
#endif

void UAnimNotify_PlayParticleEffect::Notify(class USkeletalMeshComponent* MeshComp, class UAnimSequenceBase* Animation)
{
	// Don't call super to avoid unnecessary call in to blueprints
	if (PSTemplate)
	{
		if (Attached)
		{
			UGameplayStatics::SpawnEmitterAttached(PSTemplate, MeshComp, SocketName, LocationOffset, RotationOffset);
		}
		else
		{
			const FTransform MeshTransform = MeshComp->GetSocketTransform(SocketName);
			FTransform SpawnTransform;
			SpawnTransform.SetLocation(MeshTransform.TransformPosition(LocationOffset));
			SpawnTransform.SetRotation(MeshTransform.GetRotation() * RotationOffsetQuat);
			UGameplayStatics::SpawnEmitterAtLocation(MeshComp->GetWorld(), PSTemplate, SpawnTransform);
		}
	}
}

FString UAnimNotify_PlayParticleEffect::GetNotifyName_Implementation() const
{
	if (PSTemplate)
	{
		return PSTemplate->GetName();
	}
	else
	{
		return Super::GetNotifyName_Implementation();
	}
}
