// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "GameplayCueNotify_HitImpact.h"

UGameplayCueNotify_HitImpact::UGameplayCueNotify_HitImpact(const FObjectInitializer& PCIP)
: Super(PCIP)
{

}

bool UGameplayCueNotify_HitImpact::HandlesEvent(EGameplayCueEvent::Type EventType) const
{
	return (EventType == EGameplayCueEvent::Executed);
}

void UGameplayCueNotify_HitImpact::HandleGameplayCue(AActor* Self, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters)
{
	check(EventType == EGameplayCueEvent::Executed);
	check(Self);
	
	const FHitResult* HitResult = Parameters.EffectContext.GetHitResult();
	if (HitResult)
	{
		if (ParticleSystem)
		{
			UGameplayStatics::SpawnEmitterAtLocation(Self, ParticleSystem, HitResult->ImpactPoint, HitResult->ImpactNormal.Rotation(), true);
		}
	}
	else
	{
		if (ParticleSystem)
		{
			UGameplayStatics::SpawnEmitterAtLocation(Self, ParticleSystem, Self->GetActorLocation(), Self->GetActorRotation(), true);
		}
	}
}
