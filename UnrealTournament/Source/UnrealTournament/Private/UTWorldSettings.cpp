// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTWorldSettings.h"
#include "UTDmgType_KillZ.h"
#include "Particles/ParticleSystemComponent.h"

AUTWorldSettings::AUTWorldSettings(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	KillZDamageType = UUTDmgType_KillZ::StaticClass();

	MaxImpactEffectVisibleLifetime = 60.0f;
	MaxImpactEffectInvisibleLifetime = 30.0f;
}

void AUTWorldSettings::BeginPlay()
{
	Super::BeginPlay();

	if (!bPendingKillPending)
	{
		GetWorldTimerManager().SetTimer(this, &AUTWorldSettings::ExpireImpactEffects, 0.5f, true);
	}
}

void AUTWorldSettings::AddImpactEffect(USceneComponent* NewEffect)
{
	bool bNeedsTiming = true;

	// do some checks for components we can assume will go away on their own
	UParticleSystemComponent* PSC = Cast <UParticleSystemComponent>(NewEffect);
	if (PSC != NULL)
	{
		if (PSC->bAutoDestroy && PSC->Template != NULL && IsLoopingParticleSystem(PSC->Template))
		{
			bNeedsTiming = false;
		}
	}
	else
	{
		UAudioComponent* AC = Cast<UAudioComponent>(NewEffect);
		if (AC != NULL)
		{
			if (AC->bAutoDestroy && AC->Sound != NULL && AC->Sound->GetDuration() < INDEFINITELY_LOOPING_DURATION)
			{
				bNeedsTiming = false;
			}
		}
	}

	if (bNeedsTiming)
	{
		// spawning is usually the hotspot over removal so add to end here and deal with slightly more cost on the other end
		new(TimedEffects) FTimedImpactEffect(NewEffect, GetWorld()->TimeSeconds);
	}
}

void AUTWorldSettings::ExpireImpactEffects()
{
	float WorldTime = GetWorld()->TimeSeconds;
	for (int32 i = 0; i < TimedEffects.Num(); i++)
	{
		if (TimedEffects[i].EffectComp == NULL || !TimedEffects[i].EffectComp->IsRegistered())
		{
			TimedEffects.RemoveAt(i--);
		}
		else
		{
			// try to get LastRenderTime of component
			// if it's not available, assume it's visible
			float LastRenderTime = WorldTime;
			UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(TimedEffects[i].EffectComp);
			if (Prim != NULL)
			{
				LastRenderTime = Prim->LastRenderTime;
			}
			float DesiredTimeout = (WorldTime - LastRenderTime < 1.0f) ? MaxImpactEffectVisibleLifetime : MaxImpactEffectInvisibleLifetime;
			if (DesiredTimeout > 0.0f && WorldTime - TimedEffects[i].CreationTime > DesiredTimeout)
			{
				TimedEffects[i].EffectComp->DetachFromParent();
				TimedEffects[i].EffectComp->DestroyComponent();
				TimedEffects.RemoveAt(i--);
			}
		}
	}
}