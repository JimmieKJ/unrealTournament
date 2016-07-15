// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTInventory.h"
#include "UTArmor.h"
#include "UnrealNetwork.h"

AUTArmor::AUTArmor(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	ArmorAmount = 50;
	BasePickupDesireability = 1.5f;
	bDestroyWhenConsumed = true;
}

bool AUTArmor::AllowPickupBy(AUTCharacter* Other) const
{
	if (Other && !Other->IsRagdoll())
	{
		if ((Other->GetArmorAmount() < ArmorAmount) || (ArmorType == ArmorTypeName::Helmet))
		{
			return true;
		}
		AUTGameMode* UTGameMode = Other->GetWorld()->GetAuthGameMode<AUTGameMode>();
		if (UTGameMode && UTGameMode->bAllowAllArmorPickups)
		{
			return true;
		}
		if (NoPickupSound)
		{
			UUTGameplayStatics::UTPlaySound(GetWorld(), NoPickupSound, Other, SRT_All, false, FVector::ZeroVector, NULL, NULL, false);
		}
	}
	return false;
}

bool AUTArmor::HandleGivenTo_Implementation(AUTCharacter* NewOwner)
{
	NewOwner->GiveArmor(this);
	return true;
}

void AUTArmor::Removed()
{
	UE_LOG(UT, Warning, TEXT("AUTArmor::Removed Should never be called"));
	Super::Removed();
}
void AUTArmor::GivenTo(AUTCharacter* NewOwner, bool bAutoActivate)
{
	UE_LOG(UT, Warning, TEXT("AUTArmor::GivenTo Should never be called"));
	Super::GivenTo(NewOwner, bAutoActivate);
}

float AUTArmor::BotDesireability_Implementation(APawn* Asker, AActor* Pickup, float PathDistance) const
{
	AUTCharacter* P = Cast<AUTCharacter>(Asker);
	if (P == NULL)
	{
		return 0.0f;
	}
	else
	{
		// @TODO re-think with new armor system, whether team game, value of denying armor in duel
		return FMath::Max(0.f, 0.013f * BasePickupDesireability * (ArmorAmount - P->GetArmorAmount()));
	}
}

bool AUTArmor::HandleArmorEffects(AUTCharacter* HitPawn) const
{
	bool bResult = false;
	bool bRecentlyRendered = (HitPawn != NULL) && !HitPawn->IsPendingKillPending() && (HitPawn->GetWorld()->GetTimeSeconds() - HitPawn->GetLastRenderTime() < 1.0f);
	UParticleSystem* ChosenImpactEffect = (ShieldImpactEffect && (HitPawn->GetArmorAmount() + HitPawn->LastTakeHitInfo.Damage/2 > 100)) ? ShieldImpactEffect : ArmorImpactEffect;
	if (ChosenImpactEffect && bRecentlyRendered)
	{
		bResult = true;
		const FVector WorldHitLocation = HitPawn->GetActorLocation() + HitPawn->LastTakeHitInfo.RelHitLocation;
		FRotationMatrix CenterToHit((WorldHitLocation - HitPawn->GetActorLocation()).GetSafeNormal2D().Rotation());
		// play multiple hit effects if there were simultaneous hits
		// we don't have accurate hit info for the secondary hits so just use a random offset
		int32 NumEffects = (HitPawn->GetWorld()->GetNetMode() != NM_Client) ? 1 : FMath::Max<int32>(HitPawn->LastTakeHitInfo.Count, 1);
		for (int32 i = 0; i < NumEffects; i++)
		{
			FVector AdjustedHitLocation = WorldHitLocation;
			if (i > 0)
			{
				FVector RandomExtra = CenterToHit.GetUnitAxis(EAxis::Y) * FMath::FRandRange(-1.0f, 1.0f) + CenterToHit.GetUnitAxis(EAxis::Z) * FMath::FRandRange(-1.0f, 1.0f);
				RandomExtra = RandomExtra.GetSafeNormal() * FMath::FRandRange(15.0f, 30.0f);
				AdjustedHitLocation += RandomExtra;
			}
			FVector CenterLocation = HitPawn->GetActorLocation();
			CenterLocation.Z = AdjustedHitLocation.Z;
			UGameplayStatics::SpawnEmitterAttached(ChosenImpactEffect, HitPawn->GetRootComponent(), NAME_None, AdjustedHitLocation, FRotationMatrix::MakeFromZ(AdjustedHitLocation - CenterLocation).Rotator(), EAttachLocation::KeepWorldPosition);
		}
	}
	if (PlayArmorEffects(HitPawn))
	{
		return true;
	}
	return bResult;
}
