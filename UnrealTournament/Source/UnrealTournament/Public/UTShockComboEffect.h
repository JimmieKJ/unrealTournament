// replicated shock combo effect for more reliable replication
// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTPhysicsVortex.h"
#include "UTImpactEffect.h"

#include "UTShockComboEffect.generated.h"

UCLASS(Abstract)
class AUTShockComboEffect : public AUTPhysicsVortex
{
	GENERATED_BODY()
public:

	AUTShockComboEffect(const FObjectInitializer& OI)
		: Super(OI)
	{
		PrimaryActorTick.bAllowTickOnDedicatedServer = false;
		SetReplicates(true);
		bNetTemporary = true;
	}

	/** audio/visual effect to accompany the physics effects applied by us */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<AUTImpactEffect> ImpactEffect;

	virtual void BeginPlay() override
	{
		Super::BeginPlay();

		// note: call even on server for audio replication if necessary
		if (ImpactEffect != NULL)
		{
			ImpactEffect.GetDefaultObject()->SpawnEffect(GetWorld(), GetTransform(), NULL, GetOwner(), (Instigator != NULL) ? Instigator->Controller : NULL, SRT_IfSourceNotReplicated);
		}
		if (GetNetMode() == NM_DedicatedServer)
		{
			SetLifeSpan(0.5f);
		}
	}

	/** make sure LifeSpan works even on client, needed with bNetTemporary */
	virtual void SetLifeSpan(float InLifespan) override
	{
		// Store the new value
		InitialLifeSpan = InLifespan;
		if (InLifespan > 0.0f)
		{
			GetWorldTimerManager().SetTimer(TimerHandle_LifeSpanExpired, this, &AActor::LifeSpanExpired, InLifespan);
		}
		else
		{
			GetWorldTimerManager().ClearTimer(TimerHandle_LifeSpanExpired);
		}
	}
};