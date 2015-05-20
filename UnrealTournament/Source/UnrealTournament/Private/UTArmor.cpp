// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTInventory.h"
#include "UTArmor.h"
#include "UnrealNetwork.h"

AUTArmor::AUTArmor(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	ArmorAmount = 50;
	AbsorptionPct = 0.6f;
	bCallDamageEvents = true;
	BasePickupDesireability = 1.5f;
}

void AUTArmor::GivenTo(AUTCharacter* NewOwner, bool bAutoActivate)
{
	Super::GivenTo(NewOwner, bAutoActivate);

	if (ArmorType == FName(TEXT("Helmet")))
	{
		NewOwner->bIsWearingHelmet = true;
	}
	if (OverlayMaterial != NULL)
	{
		NewOwner->SetCharacterOverlay(OverlayMaterial, true);
	}
	if (StatsName != NAME_None)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(NewOwner->PlayerState);
		if (PS)
		{
			PS->ModifyStatsValue(StatsName, 1);
			if (PS->Team)
			{
				PS->Team->ModifyStatsValue(StatsName, 1);
			}
		}
	}

	NewOwner->CheckArmorStacking();
}

void AUTArmor::Removed()
{
	if (ArmorType == FName(TEXT("Helmet")))
	{
		GetUTOwner()->bIsWearingHelmet = false;
	}
	if (OverlayMaterial != NULL)
	{
		GetUTOwner()->SetCharacterOverlay(OverlayMaterial, false);
	}
	Super::Removed();
}

bool AUTArmor::ModifyDamageTaken_Implementation(int32& Damage, FVector& Momentum, AUTInventory*& HitArmor, AController* InstigatedBy, const FHitResult& HitInfo, AActor* DamageCauser, TSubclassOf<UDamageType> DamageType)
{
	const UDamageType* const DamageTypeCDO = (DamageType != NULL) ? DamageType->GetDefaultObject<UDamageType>() : GetDefault<UDamageType>();
	const UUTDamageType* const UTDamageTypeCDO = Cast<UUTDamageType>(DamageTypeCDO); // warning: may be NULL
	if (UTDamageTypeCDO != NULL && !UTDamageTypeCDO->bBlockedByArmor)
	{
		return false;
	}
	if (Damage > 0)
	{
		if (HitArmor == NULL)
		{
			HitArmor = this;
		}
		int32 Absorb = FMath::Min<int32>(ArmorAmount, FMath::Max<int32>(1, Damage * AbsorptionPct));
		if (bAbsorbMomentum)
		{
			Momentum *= 1.0f - float(Absorb) / float(Damage);
		}
		Damage -= Absorb;
		ReduceArmor(Absorb);
	}
	return false;
}

int32 AUTArmor::GetEffectiveHealthModifier_Implementation(bool bOnlyVisible) const
{
	if (!bOnlyVisible || OverlayMaterial != NULL)
	{
		return FMath::Min<int32>(UTOwner->Health * AbsorptionPct, ArmorAmount);
	}
	else
	{
		return 0;
	}
}

void AUTArmor::ReduceArmor(int32 Amount)
{
	ArmorAmount -= Amount;
	if (ArmorAmount <= 0)
	{
		Destroy();
	}
}

bool AUTArmor::StackPickup_Implementation(AUTInventory* ContainedInv)
{
	if (ContainedInv != NULL)
	{
		ArmorAmount = FMath::Clamp<int32>(ArmorAmount + Cast<AUTArmor>(ContainedInv)->ArmorAmount, ArmorAmount, GetClass()->GetDefaultObject<AUTArmor>()->ArmorAmount);
	}
	else
	{
		ArmorAmount = GetClass()->GetDefaultObject<AUTArmor>()->ArmorAmount;
	}
	GetUTOwner()->CheckArmorStacking();
	return true;
}

void AUTArmor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AUTArmor, ArmorAmount, COND_None);
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
		int32 MatchingArmor = 0;
		int32 TotalArmor = 0;
		float MaxAbsorbPct = 0.0f;
		for (TInventoryIterator<AUTArmor> It(P); It; ++It)
		{
			TotalArmor += It->ArmorAmount;
			MaxAbsorbPct = FMath::Max<float>(MaxAbsorbPct, It->AbsorptionPct);
			if (It->GetClass() == GetClass())
			{
				MatchingArmor += It->ArmorAmount;
			}
		}
		// if this armor will overwrite other armor consider full value regardless of stacking
		int32 Value = (MaxAbsorbPct < AbsorptionPct) ? (ArmorAmount - MatchingArmor) : FMath::Min<int32>(P->MaxStackedArmor - TotalArmor, ArmorAmount - MatchingArmor);
		float Desire = 0.013f * BasePickupDesireability * float(Value);
		//if (!WorldInfo.Game.bTeamGame && UTBot(C) != None && UTBot(C).Skill >= 4.0)
		//{
			// high skill bots keep considering powerups that they don't need if they can still pick them up
			// to deny the enemy any chance of getting them
		//	Desire = FMax(Desire, 0.001);
		//}
		return Desire;
	}
}

bool AUTArmor::HandleArmorEffects(AUTCharacter* HitPawn) const
{
	bool bResult = false;
	bool bRecentlyRendered = (HitPawn != NULL) && !HitPawn->IsPendingKillPending() && (HitPawn->GetWorld()->GetTimeSeconds() - HitPawn->GetLastRenderTime() < 1.0f);
	if (ArmorImpactEffect && bRecentlyRendered)
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
			UGameplayStatics::SpawnEmitterAttached(ArmorImpactEffect, HitPawn->GetRootComponent(), NAME_None, AdjustedHitLocation, FRotationMatrix::MakeFromZ(AdjustedHitLocation - CenterLocation).Rotator(), EAttachLocation::KeepWorldPosition);
		}
	}
	if (PlayArmorEffects(HitPawn))
	{
		return true;
	}
	return bResult;
}
