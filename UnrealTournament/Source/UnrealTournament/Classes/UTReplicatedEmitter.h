// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTReplicatedEmitter.generated.h"

/** temporary one-shot emitter that is replicated to clients
 * this is used when we need to spawn an effect that can't be easily simulated or triggered on clients through a simple RepNotify
 * on dedicated servers, the effect will not play but the Actor should still be spawned and will stay alive long enough to send to current clients\
 * the emitter defaults to being based on its Owner, so that the Base can effectively be passed as part of the spawn parameters
 */
UCLASS(BlueprintType, CustomConstructor, Abstract)
class AUTReplicatedEmitter : public AActor
{
	GENERATED_UCLASS_BODY()

	AUTReplicatedEmitter(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
	{
		PSC = PCIP.CreateDefaultSubobject<UParticleSystemComponent>(this, TEXT("Particles"));
		InitialLifeSpan = 10.0f;
		DedicatedServerLifeSpan = 0.5f;
	}

	UPROPERTY(VisibleDefaultsOnly, Category = Emitter)
	TSubobjectPtr<UParticleSystemComponent> PSC;
	/** lifespan when in dedicated server mode - basically how long it's worthwhile to consider sending the effect to clients before it's obsolete */
	UPROPERTY(EditDefaultsOnly, Category = Emitter)
	float DedicatedServerLifeSpan;
	/** if set and emitter is given an Owner, find mesh component closest to that Actor's root and attach to it using BaseSocketName */
	UPROPERTY(EditDefaultsOnly, Category = Emitter)
	bool bAttachToOwnerMesh;
	/** the socket to attach to (if any) */
	UPROPERTY(EditDefaultsOnly, Category = Emitter)
	FName BaseSocketName;

	virtual void PostInitializeComponents() OVERRIDE
	{
		if (GetNetMode() == NM_DedicatedServer)
		{
			InitialLifeSpan = (DedicatedServerLifeSpan <= 0.0f) ? 0.5f : DedicatedServerLifeSpan;
		}
		Super::PostInitializeComponents();
	}

	virtual void RegisterAllComponents() OVERRIDE
	{
		if (GetOwner() != NULL)
		{
			AttachRootComponentToActor(GetOwner(), BaseSocketName, EAttachLocation::SnapToTarget);
		}
		if (GetNetMode() != NM_DedicatedServer)
		{
			Super::RegisterAllComponents();
		}
	}
};