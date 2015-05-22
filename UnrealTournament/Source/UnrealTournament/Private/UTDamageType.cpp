// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTDamageType.h"
#include "UTRewardMessage.h"

UUTDamageType::UUTDamageType(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	// hack to allow blueprint damagetypes to have an event graph
	// TODO: disabled at the moment as blueprints have all kinds of issues interacting with functions on default objects due to implicit variable creation
	//GetClass()->ClassFlags &= ~CLASS_Const;

	SelfMomentumBoost = 1.f;
	DamageImpulse = 50000.0f;
	DestructibleImpulse = 50000.0f;
	bForceZMomentum = true;
	ForceZMomentumPct = 0.4f;
	GibHealthThreshold = -50;
	GibDamageThreshold = 99;
	bCausesBlood = true;
	bBlockedByArmor = true;
	RewardAnnouncementClass = NULL;
	WeaponSpreeCount = 15;

	static ConstructorHelpers::FObjectFinder<UCurveLinearColor> DefaultFlash(TEXT("CurveLinearColor'/Game/RestrictedAssets/Effects/RedHitFlash.RedHitFlash'"));
	BodyDamageColor = DefaultFlash.Object;
	bBodyDamageColorRimOnly = true;

	ConsoleDeathMessage = NSLOCTEXT("UTDeathMessages","GenericConsoleDeathMessage","{Player1Name} killed {Player2Name} with the {WeaponName}.");
	MaleSuicideMessage = NSLOCTEXT("UTDeathMessages","GenericMaleSuicideMessage","{Player2Name} killed himself with the {WeaponName}.");
	FemaleSuicideMessage = NSLOCTEXT("UTDeathMessages","GenericFemaleSuicideMessage","{Player2Name} killed herself with the {WeaponName}.");
	SelfVictimMessage = NSLOCTEXT("UTDeathMessages", "SelfVictimMessage", "You killed yourself.");
	AssociatedWeaponName = NSLOCTEXT("UTDeathMessages","EnvironmentMessage","Environmental");
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
	AUTPlayerController* PC = KillerState ? Cast<AUTPlayerController>(KillerState->GetOwner()) : NULL;
	if (PC != NULL && RewardAnnouncementClass)
	{
		PC->SendPersonalMessage(RewardAnnouncementClass);
	}
}

bool UUTDamageType::ShouldGib_Implementation(AUTCharacter* Victim) const
{
	return (Victim->Health <= GibHealthThreshold || Victim->LastTakeHitInfo.Damage >= GibDamageThreshold);
}

void UUTDamageType::PlayHitEffects_Implementation(AUTCharacter* HitPawn, bool bPlayedArmorEffect) const
{
	if (BodyDamageColor != NULL && (HitPawn->LastTakeHitInfo.Damage > 0 || (HitPawn->LastTakeHitInfo.HitArmor != NULL && !bPlayedArmorEffect)))
	{
		HitPawn->SetBodyColorFlash(BodyDamageColor, bBodyDamageColorRimOnly);
	}
}

void UUTDamageType::PlayDeathEffects_Implementation(AUTCharacter* DyingPawn) const
{
}

void UUTDamageType::PlayGibEffects_Implementation(AUTGib* Gib) const
{
}