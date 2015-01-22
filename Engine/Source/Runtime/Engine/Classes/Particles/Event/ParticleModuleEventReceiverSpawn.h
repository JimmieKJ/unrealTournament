// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Particles/Event/ParticleModuleEventReceiverBase.h"
#include "ParticleModuleEventReceiverSpawn.generated.h"

UCLASS(editinlinenew, hidecategories=Object, meta=(DisplayName = "EventReceiver Spawn"))
class UParticleModuleEventReceiverSpawn : public UParticleModuleEventReceiverBase
{
	GENERATED_UCLASS_BODY()

	/** The number of particles to spawn. */
	UPROPERTY(EditAnywhere, Category=Spawn)
	struct FRawDistributionFloat SpawnCount;

	/** 
	 *	For Death-based event receiving, if this is true, it indicates that the 
	 *	ParticleTime of the event should be used to look-up the SpawnCount.
	 *	Otherwise (and in all other events received), use the emitter time of 
	 *	the event.
	 */
	UPROPERTY(EditAnywhere, Category=Spawn)
	uint32 bUseParticleTime:1;

	/**
	 *	If true, use the location of the particle system component for spawning.
	 *	if false (default), use the location of the particle event.
	 */
	UPROPERTY(EditAnywhere, Category=Location)
	uint32 bUsePSysLocation:1;

	/**
	 *	If true, use the velocity of the dying particle as the start velocity of 
	 *	the spawned particle.
	 */
	UPROPERTY(EditAnywhere, Category=Velocity)
	uint32 bInheritVelocity:1;

	/**
	 *	If bInheritVelocity is true, scale the velocity with this.
	 */
	UPROPERTY(EditAnywhere, Category=Velocity)
	struct FRawDistributionVector InheritVelocityScale;

	/** Initializes the default values for this property */
	void InitializeDefaults();

	//Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	virtual void PostInitProperties() override;
	//End UObject Interface

	// Begin UParticleModuleEventBase Interface
	virtual bool ProcessParticleEvent(FParticleEmitterInstance* Owner, FParticleEventData& InEvent, float DeltaTime) override;
	// End UParticleModuleEventBase Interface
};



