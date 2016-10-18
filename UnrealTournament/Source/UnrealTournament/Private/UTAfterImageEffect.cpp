// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTAfterImageEffect.h"
#include "UnrealNetwork.h"
#include "Animation/AnimInstance.h"

void AUTAfterImageEffect::OnRep_Instigator()
{
	if (!bPlayedEffect)
	{
		bPlayedEffect = true;
		StartEffect();
	}
}
void AUTAfterImageEffect::OnRep_CharMesh()
{
	if (!bPlayedEffect)
	{
		bPlayedEffect = true;
		StartEffect();
	}
}
void AUTAfterImageEffect::InitFor_Implementation(APawn* InInstigator, const FRepCollisionShape& InCollision, UPrimitiveComponent* InBase, const FTransform& InDest)
{
	Super::InitFor_Implementation(InInstigator, InCollision, InBase, InDest);

	AUTCharacter* UTC = Cast<AUTCharacter>(InInstigator);
	if (UTC != NULL)
	{
		TeamColor = UTC->GetTeamColor();
	}
	ACharacter* C = Cast<ACharacter>(InInstigator);
	if (C != NULL && C->GetMesh() != NULL)
	{
		CharMesh = C->GetMesh()->SkeletalMesh;
		if (!bPlayedEffect)
		{
			bPlayedEffect = true;
			StartEffect();
		}
	}
}

void AUTAfterImageEffect::SetMeshProperties(USkeletalMeshComponent* DestComp)
{
	if (DestComp != NULL)
	{
		ACharacter* C = Cast<ACharacter>(Instigator);
		USkeletalMeshComponent* TemplateComp = NULL;
		if (C != NULL && C->GetMesh() != NULL)
		{
			DestComp->SetSkeletalMesh(C->GetMesh()->SkeletalMesh);
			TemplateComp = C->GetMesh();
			DestComp->SetMasterPoseComponent(TemplateComp);
		}
		else if (CharMesh != NULL)
		{
			DestComp->SetSkeletalMesh(CharMesh);
			TemplateComp = GetDefault<AUTCharacter>()->GetMesh();
		}
		if (TemplateComp != NULL)
		{
			DestComp->SetRelativeLocation(TemplateComp->RelativeLocation);
			DestComp->SetRelativeRotation(TemplateComp->RelativeRotation);
			DestComp->SetRelativeScale3D(TemplateComp->RelativeScale3D);
			DestComp->SetAnimInstanceClass(TemplateComp->AnimClass);

			// force mesh update, then freeze it
			DestComp->TickAnimation(0.0f, false);
			DestComp->RefreshBoneTransforms();
			DestComp->UpdateComponentToWorld();
			DestComp->SetMasterPoseComponent(NULL);
			DestComp->bPauseAnims = true;
		}
	}
}

void AUTAfterImageEffect::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AUTAfterImageEffect, CharMesh, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTAfterImageEffect, TeamColor, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTWeaponRedirector, PortalDest, COND_InitialOnly);
}