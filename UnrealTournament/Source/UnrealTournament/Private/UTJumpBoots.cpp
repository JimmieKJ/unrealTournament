// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTJumpBoots.h"
#include "UTReplicatedEmitter.h"
#include "UTCharacterMovement.h"
#include "UnrealNetwork.h"
#include "UTHUDWidget_Powerups.h"
#include "StatNames.h"
#include "UTJumpbootMessage.h"

AUTJumpBoots::AUTJumpBoots(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	NumJumps = 3;
	SuperJumpZ = 1500.0f;
	MultiJumpAirControl = 0.8f;
	bCallOwnerEvent = true;
	BasePickupDesireability = 0.8f;
	MaxMultiJumpZSpeed = 600.0f;
}

void AUTJumpBoots::AdjustOwner(bool bRemoveBonus)
{
	UUTCharacterMovement* Movement = GetUTOwner()->UTCharacterMovement;
	if (Movement != NULL)
	{
		if (bRemoveBonus)
		{
			Movement->MaxMultiJumpCount = ((UUTCharacterMovement*)GetUTOwner()->GetClass()->GetDefaultObject<AUTCharacter>()->GetCharacterMovement())->MaxMultiJumpCount;
			Movement->MultiJumpImpulse = ((UUTCharacterMovement*)GetUTOwner()->GetClass()->GetDefaultObject<AUTCharacter>()->GetCharacterMovement())->MultiJumpImpulse;
			Movement->DodgeJumpImpulse = ((UUTCharacterMovement*)GetUTOwner()->GetClass()->GetDefaultObject<AUTCharacter>()->GetCharacterMovement())->DodgeJumpImpulse;
			Movement->MultiJumpAirControl = ((UUTCharacterMovement*)GetUTOwner()->GetClass()->GetDefaultObject<AUTCharacter>()->GetCharacterMovement())->MultiJumpAirControl;
			Movement->bAllowDodgeMultijumps = ((UUTCharacterMovement*)GetUTOwner()->GetClass()->GetDefaultObject<AUTCharacter>()->GetCharacterMovement())->bAllowDodgeMultijumps;
			Movement->bAllowJumpMultijumps = ((UUTCharacterMovement*)GetUTOwner()->GetClass()->GetDefaultObject<AUTCharacter>()->GetCharacterMovement())->bAllowJumpMultijumps;
			Movement->MaxMultiJumpZSpeed = ((UUTCharacterMovement*)GetUTOwner()->GetClass()->GetDefaultObject<AUTCharacter>()->GetCharacterMovement())->MaxMultiJumpZSpeed;
			Movement->bAlwaysAllowFallingMultiJump = ((UUTCharacterMovement*)GetUTOwner()->GetClass()->GetDefaultObject<AUTCharacter>()->GetCharacterMovement())->bAlwaysAllowFallingMultiJump;

			GetUTOwner()->MaxSafeFallSpeed = GetUTOwner()->GetClass()->GetDefaultObject<AUTCharacter>()->MaxSafeFallSpeed;

			AGameMode* Game = GetWorld()->GetAuthGameMode();
			if (Game != NULL)
			{
				Game->SetPlayerDefaults(GetUTOwner());
			}
		}
		else
		{
			Movement->bAllowDodgeMultijumps = true;
			Movement->bAllowJumpMultijumps = true;
			Movement->MultiJumpAirControl = MultiJumpAirControl;
			Movement->MaxMultiJumpZSpeed = MaxMultiJumpZSpeed;
			Movement->bAlwaysAllowFallingMultiJump = true;

			if (Movement->MaxMultiJumpCount < 1)
			{
				Movement->MaxMultiJumpCount = 1;
				Movement->MultiJumpImpulse = SuperJumpZ;
				Movement->DodgeJumpImpulse = SuperJumpZ;
			}
			else
			{
				Movement->MultiJumpImpulse += SuperJumpZ;
				Movement->DodgeJumpImpulse += SuperJumpZ;
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
void AUTJumpBoots::ClientGivenTo_Internal(bool bAutoActivate)
{
	Super::ClientGivenTo_Internal(bAutoActivate);
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
	if (Role < ROLE_Authority && GetUTOwner() != NULL)
	{
		AdjustOwner(true);
	}
	Super::ClientRemoved_Implementation();
}

void AUTJumpBoots::OwnerEvent_Implementation(FName EventName)
{
	static FName NAME_MultiJump(TEXT("MultiJump"));
	static FName NAME_Jump(TEXT("Jump"));
	static FName NAME_Landed(TEXT("Landed"));

	if (Role == ROLE_Authority)
	{
		if (EventName == NAME_Jump)
		{
			if (UTOwner && Cast<AUTPlayerController>(UTOwner->GetController()) && UTOwner->IsLocallyControlled() && (NumJumps == 3))
			{
				AUTPlayerState* PS = Cast<AUTPlayerState>(GetUTOwner()->PlayerState);
				if (PS && (PS->GetStatsValue(NAME_BootJumps) == 0))
				{
					Cast<AUTPlayerController>(UTOwner->GetController())->SendPersonalMessage(UUTJumpbootMessage::StaticClass(), 0, NULL, NULL, NULL);
				}
			}
		}
		else if (EventName == NAME_MultiJump)
		{
			NumJumps--;
			if (SuperJumpEffect != NULL)
			{
				FActorSpawnParameters Params;
				Params.Owner = GetUTOwner();
				Params.Instigator = GetUTOwner();
				GetWorld()->SpawnActor<AUTReplicatedEmitter>(SuperJumpEffect, GetUTOwner()->GetActorLocation(), GetUTOwner()->GetActorRotation(), Params);
			}
			if (GetUTOwner())
			{
				AUTPlayerState* PS = Cast<AUTPlayerState>(GetUTOwner()->PlayerState);
				if (PS)
				{
					PS->ModifyStatsValue(NAME_BootJumps, 1);
					if (PS->Team)
					{
						PS->Team->ModifyStatsValue(NAME_BootJumps, 1);
					}
					if (UTOwner && (NumJumps > 1) && Cast<AUTPlayerController>(UTOwner->GetController()) && UTOwner->IsLocallyControlled())
					{
						Cast<AUTPlayerController>(UTOwner->GetController())->SendPersonalMessage(UUTJumpbootMessage::StaticClass(), 1, NULL, NULL, NULL);
					}
				}
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

void AUTJumpBoots::DropFrom(const FVector& StartLocation, const FVector& TossVelocity)
{
	if (NumJumps <= 0)
	{
		Destroy();
	}
	else
	{
		Super::DropFrom(StartLocation, TossVelocity);
	}
}

void AUTJumpBoots::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AUTJumpBoots, NumJumps, COND_None);
}

float AUTJumpBoots::BotDesireability_Implementation(APawn* Asker, AActor* Pickup, float PathDistance) const
{
	AUTCharacter* P = Cast<AUTCharacter>(Asker);
	if (P != NULL)
	{
		AUTJumpBoots* AlreadyHas = P->FindInventoryType<AUTJumpBoots>(GetClass());
		return (AlreadyHas != NULL) ? (BasePickupDesireability / (1 + AlreadyHas->NumJumps)) : BasePickupDesireability;
	}
	else
	{
		return 0.0f;
	}
}
float AUTJumpBoots::DetourWeight_Implementation(APawn* Asker, AActor* Pickup, float PathDistance) const
{
	AUTCharacter* P = Cast<AUTCharacter>(Asker);
	if (P != NULL)
	{
		AUTJumpBoots* AlreadyHas = P->FindInventoryType<AUTJumpBoots>(GetClass());
		// shorter distance to care about boots if have some already
		if (AlreadyHas == NULL || PathDistance < 2000.0f)
		{
			return BotDesireability(Asker, Pickup, PathDistance);
		}
		else
		{
			return 0.0f;
		}
	}
	else
	{
		return 0.0f;
	}
}

// Allows inventory items to decide if a widget should be allowed to render them.
bool AUTJumpBoots::HUDShouldRender(UUTHUDWidget* TargetWidget)
{
	return (TargetWidget && Cast<UUTHUDWidget_Powerups>(TargetWidget));
}
