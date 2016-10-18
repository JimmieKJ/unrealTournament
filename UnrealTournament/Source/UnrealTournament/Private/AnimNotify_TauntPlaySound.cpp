// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "AnimNotify_TauntPlaySound.h"

void UAnimNotify_TauntPlaySound::Notify(class USkeletalMeshComponent* MeshComp, class UAnimSequenceBase* Animation)
{
	// Don't call super to avoid call back in to blueprints
	if (Sound && MeshComp)
	{
		AUTCharacter* UTChar = Cast<AUTCharacter>(MeshComp->GetOwner());
		if (UTChar)
		{
			if (UTChar->CurrentTauntAudioComponent)
			{
				UTChar->CurrentTauntAudioComponent->Stop();
				UTChar->CurrentTauntAudioComponent = nullptr;
			}

			UTChar->CurrentTauntAudioComponent = UGameplayStatics::SpawnSoundAttached(Sound, MeshComp);
		}
		else
		{
			UGameplayStatics::PlaySoundAtLocation(MeshComp->GetWorld(), Sound, MeshComp->GetComponentLocation());
		}
	}
}

FString UAnimNotify_TauntPlaySound::GetNotifyName_Implementation() const
{
	if (Sound)
	{
		return Sound->GetName();
	}
	else
	{
		return Super::GetNotifyName_Implementation();
	}
}