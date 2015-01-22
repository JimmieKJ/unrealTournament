// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "GameFramework/Actor.h"

#include "ParticleEventManager.generated.h"

struct FParticleEventSpawnData;
struct FParticleEventDeathData;
struct FParticleEventCollideData;
struct FParticleEventBurstData;
class UParticleSystemComponent;

UCLASS(config=Game, NotBlueprintable, notplaceable)
class AParticleEventManager : public AActor
{
	GENERATED_UCLASS_BODY()

	virtual void HandleParticleSpawnEvents( UParticleSystemComponent* Component, const TArray<FParticleEventSpawnData>& SpawnEvents );
	virtual void HandleParticleDeathEvents( UParticleSystemComponent* Component, const TArray<FParticleEventDeathData>& DeathEvents );
	virtual void HandleParticleCollisionEvents( UParticleSystemComponent* Component, const TArray<FParticleEventCollideData>& CollisionEvents );
	virtual void HandleParticleBurstEvents( UParticleSystemComponent* Component, const TArray<FParticleEventBurstData>& BurstEvents );
};

