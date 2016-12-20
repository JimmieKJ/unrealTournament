// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTDamageType.h"
#include "UTAnnouncer.h"
#include "UTTimedPowerup.h"

UUTDamageType::UUTDamageType(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	SelfMomentumBoost = 1.f;
	DamageImpulse = 50000.0f;
	DestructibleImpulse = 50000.0f;
	bForceZMomentum = true;
	ForceZMomentumPct = 0.4f;
	GibHealthThreshold = -50;
	GibDamageThreshold = 99;
	bCausesBlood = true;
	bCausesPainSound = true;
	bBlockedByArmor = true;
	RewardAnnouncementClass = NULL;
	WeaponSpreeCount = 15;

	static ConstructorHelpers::FObjectFinder<UCurveLinearColor> DefaultFlash(TEXT("CurveLinearColor'/Game/RestrictedAssets/Effects/RedHitFlash.RedHitFlash'"));
	BodyDamageColor = DefaultFlash.Object;
	bBodyDamageColorRimOnly = true;

	static ConstructorHelpers::FObjectFinder<UCurveLinearColor> ArmorFlash(TEXT("CurveLinearColor'/Game/RestrictedAssets/Effects/GoldHitFlash.GoldHitFlash'"));
	ArmorDamageColor = ArmorFlash.Object;

	static ConstructorHelpers::FObjectFinder<UCurveLinearColor> HealthFlash(TEXT("CurveLinearColor'/Game/RestrictedAssets/Effects/BlueHitFlash.BlueHitFlash'"));
	SuperHealthDamageColor = HealthFlash.Object;

	ConsoleDeathMessage = NSLOCTEXT("UTDeathMessages","GenericConsoleDeathMessage","{Player1Name} killed {Player2Name} with the {WeaponName}.");
	MaleSuicideMessage = NSLOCTEXT("UTDeathMessages","GenericMaleSuicideMessage","{Player2Name} killed himself with the {WeaponName}.");
	FemaleSuicideMessage = NSLOCTEXT("UTDeathMessages","GenericFemaleSuicideMessage","{Player2Name} killed herself with the {WeaponName}.");
	SelfVictimMessage = NSLOCTEXT("UTDeathMessages", "SelfVictimMessage", "You killed yourself.");
	AssociatedWeaponName = NSLOCTEXT("UTDeathMessages","EnvironmentMessage","Environmental");
	SpecialRewardText = NSLOCTEXT("UTDeathMessages", "GenericSpecialReward", "Special!");

	static ConstructorHelpers::FObjectFinder<UTexture> SkullTex(TEXT("Texture'/Game/RestrictedAssets/UI/HUDAtlas01.HUDAtlas01'"));
	HUDIcon.Texture = SkullTex.Object;
	HUDIcon.U = 727;
	HUDIcon.V = 0;
	HUDIcon.UL = 29;
	HUDIcon.VL = 37;
}

FVector UTGetDamageMomentum(const FDamageEvent& DamageEvent, const AActor* HitActor, const AController* EventInstigator)
{
	if (DamageEvent.IsOfType(FUTPointDamageEvent::ClassID))
	{
		return ((const FUTPointDamageEvent&)DamageEvent).Momentum;
	}
	else if (DamageEvent.IsOfType(FUTRadialDamageEvent::ClassID))
	{
		const FUTRadialDamageEvent& RadialEvent = (const FUTRadialDamageEvent&)DamageEvent;
		float Magnitude = RadialEvent.BaseMomentumMag;
		// default to taking the average of all hit components
		if (RadialEvent.ComponentHits.Num() == 0)
		{
			// don't think this can happen but doesn't hurt to be safe
			return (HitActor->GetActorLocation() - RadialEvent.Origin).GetSafeNormal() * Magnitude;
		}
		// accommodate origin being same as hit location
		else if (RadialEvent.ComponentHits.Num() == 1 && (RadialEvent.ComponentHits[0].Location - RadialEvent.Origin).IsNearlyZero() && RadialEvent.ComponentHits[0].Component.IsValid())
		{
			if ((RadialEvent.ComponentHits[0].TraceStart - RadialEvent.ComponentHits[0].TraceEnd).IsNearlyZero())
			{
				// 'fake' hit generated because no component trace succeeded even though radius check worked
				// in this case, use direction to component center
				return (RadialEvent.ComponentHits[0].Component->GetComponentLocation() - RadialEvent.Origin).GetSafeNormal() * Magnitude;
			}
			else
			{
				return (RadialEvent.ComponentHits[0].TraceEnd - RadialEvent.ComponentHits[0].TraceStart).GetSafeNormal() * Magnitude;
			}
		}
		else
		{
			FVector Avg(FVector::ZeroVector);
			for (int32 i = 0; i < RadialEvent.ComponentHits.Num(); i++)
			{
				Avg += RadialEvent.ComponentHits[i].Location;
			}

			return (Avg / RadialEvent.ComponentHits.Num() - RadialEvent.Origin).GetSafeNormal() * Magnitude;
		}
	}
	else
	{
		TSubclassOf<UDamageType> DamageType = DamageEvent.DamageTypeClass;
		if (DamageType == NULL)
		{
			DamageType = UUTDamageType::StaticClass();
		}
		FHitResult HitInfo;
		FVector MomentumDir;
		DamageEvent.GetBestHitInfo(HitActor, EventInstigator, HitInfo, MomentumDir);
		return MomentumDir * DamageType.GetDefaultObject()->DamageImpulse;
	}
}

void UUTDamageType::ScoreKill_Implementation(AUTPlayerState* KillerState, AUTPlayerState* VictimState, APawn* KilledPawn) const
{
}

bool UUTDamageType::ShouldGib_Implementation(AUTCharacter* Victim) const
{
	return (Victim->Health <= GibHealthThreshold || Victim->LastTakeHitInfo.Damage * Victim->LastTakeHitInfo.Count >= GibDamageThreshold);
}

bool  UUTDamageType::OverrideDeathSound_Implementation(AUTCharacter* Victim) const
{
	return !bCausesPainSound;
}

void UUTDamageType::PlayHitEffects_Implementation(AUTCharacter* HitPawn, bool bPlayedArmorEffect) const
{
	if (BodyDamageColor != NULL && (HitPawn->LastTakeHitInfo.Damage > 0 || (HitPawn->LastTakeHitInfo.HitArmor != NULL && !bPlayedArmorEffect)))
	{
		if (HitPawn->LastTakeHitInfo.HitArmor == AUTTimedPowerup::StaticClass())
		{
			HitPawn->SetBodyColorFlash(SuperHealthDamageColor, true);
		}
		else if (bBlockedByArmor && HitPawn->LastTakeHitInfo.HitArmor && ArmorDamageColor)
		{
			HitPawn->SetBodyColorFlash(ArmorDamageColor, true);
		}
		else
		{
			HitPawn->SetBodyColorFlash(BodyDamageColor, bBodyDamageColorRimOnly);
		}
	}
}

UAnimMontage* UUTDamageType::GetDeathAnim_Implementation(AUTCharacter* DyingPawn) const
{
	return NULL;
}

void UUTDamageType::PlayDeathEffects_Implementation(AUTCharacter* DyingPawn) const
{
}

void UUTDamageType::PlayGibEffects_Implementation(AUTGib* Gib) const
{
}

void UUTDamageType::PrecacheAnnouncements(UUTAnnouncer* Announcer) const
{
	Announcer->PrecacheAnnouncement(SpreeSoundName);
}
