// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTJumpBoots.h"
#include "UTReplicatedEmitter.h"

AUTJumpBoots::AUTJumpBoots(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	NumJumps = 3;
	SuperJumpZ = 1500.0f;
	bCallOwnerEvent = true;
}

void AUTJumpBoots::AdjustOwner(bool bRemoveBonus)
{
	UUTCharacterMovement* Movement = Cast<UUTCharacterMovement>(GetUTOwner()->CharacterMovement);
	if (Movement != NULL)
	{
		if (bRemoveBonus)
		{
			Movement->MaxMultiJumpCount = ((UUTCharacterMovement*)GetUTOwner()->GetClass()->GetDefaultObject<AUTCharacter>()->CharacterMovement.Get())->MaxMultiJumpCount;
			Movement->MultiJumpImpulse = ((UUTCharacterMovement*)GetUTOwner()->GetClass()->GetDefaultObject<AUTCharacter>()->CharacterMovement.Get())->MultiJumpImpulse;
			GetUTOwner()->MaxSafeFallSpeed = GetUTOwner()->GetClass()->GetDefaultObject<AUTCharacter>()->MaxSafeFallSpeed;
		}
		else
		{
			if (Movement->MaxMultiJumpCount <= 1)
			{
				Movement->MaxMultiJumpCount = 2;
				Movement->MultiJumpImpulse = SuperJumpZ;
			}
			else
			{
				Movement->MultiJumpImpulse += SuperJumpZ;
			}
			GetUTOwner()->MaxSafeFallSpeed += SuperJumpZ;
		}
	}
}
void AUTJumpBoots::GivenTo(AUTCharacter* NewOwner, bool bAutoActivate)
{
	Super::GivenTo(NewOwner, bAutoActivate);
	AdjustOwner(false);
}
void AUTJumpBoots::ClientGivenTo_Implementation(bool bAutoActivate)
{
	Super::ClientGivenTo_Implementation(bAutoActivate);
	if (Role < ROLE_Authority)
	{
		AdjustOwner(false);
	}
}
void AUTJumpBoots::Removed()
{
	AdjustOwner(true);
	Super::Removed();
}
void AUTJumpBoots::ClientRemoved_Implementation()
{
	if (Role < ROLE_Authority)
	{
		AdjustOwner(true);
	}
	Super::ClientRemoved_Implementation();
}

void AUTJumpBoots::OwnerEvent_Implementation(FName EventName)
{
	static FName NAME_MultiJump(TEXT("MultiJump"));
	static FName NAME_Landed(TEXT("Landed"));

	if (Role == ROLE_Authority)
	{
		if (EventName == NAME_MultiJump)
		{
			NumJumps--;
			if (SuperJumpEffect != NULL)
			{
				FActorSpawnParameters Params;
				Params.Owner = GetUTOwner();
				Params.Instigator = GetUTOwner();
				GetWorld()->SpawnActor<AUTReplicatedEmitter>(SuperJumpEffect, GetUTOwner()->GetActorLocation(), GetUTOwner()->GetActorRotation(), Params);
			}
			UUTGameplayStatics::UTPlaySound(GetWorld(), SuperJumpSound, GetUTOwner(), SRT_AllButOwner);
		}
		else if (EventName == NAME_Landed && NumJumps <= 0)
		{
			Destroy();
		}
	}
	else if (EventName == NAME_MultiJump)
	{
		UUTGameplayStatics::UTPlaySound(GetWorld(), SuperJumpSound, GetUTOwner(), SRT_AllButOwner);
	}
}

bool AUTJumpBoots::StackPickup_Implementation(AUTInventory* ContainedInv)
{
	if (ContainedInv != NULL)
	{
		NumJumps = FMath::Min<int32>(NumJumps + Cast<AUTJumpBoots>(ContainedInv)->NumJumps, GetClass()->GetDefaultObject<AUTJumpBoots>()->NumJumps);
	}
	else
	{
		NumJumps = GetClass()->GetDefaultObject<AUTJumpBoots>()->NumJumps;
	}
	return true;
}

void AUTJumpBoots::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AUTJumpBoots, NumJumps, COND_OwnerOnly);
}