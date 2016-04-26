// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*==============================================================================
NiagaraSimulation.h: Niagara emitter simulation class
==============================================================================*/

#pragma once

#include "NiagaraDataSet.h"
#include "NiagaraEvents.generated.h"

struct FNiagaraSimulation;
struct FNiagaraEventReceiverProperties;

/**
Base class for actions that an event receiver will perform at the emitter level.
*/
UCLASS(abstract)
class UNiagaraEventReceiverEmitterAction : public UObject
{
	GENERATED_BODY()
public:
	virtual void PerformAction(FNiagaraSimulation& OwningSim, FNiagaraEventReceiverProperties& OwningEventReceiver){}
};

UCLASS(EditInlineNew)
class UNiagaraEventReceiverEmitterAction_SpawnParticles : public UNiagaraEventReceiverEmitterAction
{
	GENERATED_BODY()

public:

	/** Number of particles to spawn per event received. */
	UPROPERTY(EditAnywhere, Category = "Spawn")
	uint32 NumParticles;

	virtual void PerformAction(FNiagaraSimulation& OwningSim, FNiagaraEventReceiverProperties& OwningEventReceiver)override;
};

/*
* Generates an event for every particle death in an emitter. Each event contains all particle attributes at the moment of death.
*/
class FNiagaraDeathEventGenerator
{
	/** The owning simulation. */
	FNiagaraSimulation* OwnerSim;

	/** Storage for dead particles. Source of event data. */
	FNiagaraDataSet DataSet;

	/*
	Temp storage for writing the attribute data of dead particles.
	Can likely get rid of this and write directly to the DataSet with a bit more work.
	*/
	TArray<TArray<FVector4>> TempStorage;

	/** The number of dead particles we've seen this frame. */
	int32 NumDead;
public:

	FNiagaraDeathEventGenerator(FNiagaraSimulation* InOwnerSim)
		: OwnerSim(InOwnerSim)
		, DataSet(FNiagaraDataSetID::DeathEvent)
		, NumDead(0)
	{

	}

	void Reset()
	{
		DataSet.Reset();
		NumDead = 0;
		TempStorage.Empty();
	}

	FNiagaraDataSet& GetDataSet(){ return DataSet; }
	void BeginTrackingDeaths();
	void OnDeath(int32 DeadIndex);
	void EndTrackingDeaths();
};

/**
* Generates events for every particle spawned in an emitter. Each event contains the particles attriubtes as initialised by the spawn script.
*/
class FNiagaraSpawnEventGenerator
{
	FNiagaraSimulation* OwnerSim;

	/**
	Storage for spawned particles. Source of event data.
	For quickness, we currently make a copy of all spawned particles.
	It may be possible in future to remove this and access the Particle data directly with some refactoring of the events stuff.
	*/
	FNiagaraDataSet DataSet;
public:

	void Reset()
	{
		DataSet.Reset();
	}

	FNiagaraDataSet& GetDataSet(){ return DataSet; }

	void OnSpawned(int32 SpawnIndex, int32 NumSpawned);

	FNiagaraSpawnEventGenerator(FNiagaraSimulation* InOwnerSim)
		: OwnerSim(InOwnerSim)
		, DataSet(FNiagaraDataSetID::SpawnEvent)
	{

	}
};


