// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "AnimNotify_GroupTauntCascade.h"

void UAnimNotify_GroupTauntCascade::Notify(class USkeletalMeshComponent* MeshComp, class UAnimSequenceBase* Animation)
{
	// Don't call super to avoid call back in to blueprints
	if (MeshComp)
	{
		AUTCharacter* UTChar = Cast<AUTCharacter>(MeshComp->GetOwner());
		if (UTChar)
		{
			UTChar->CascadeGroupTaunt();
		}
	}
}