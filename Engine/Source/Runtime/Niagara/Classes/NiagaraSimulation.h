// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*==============================================================================
NiagaraSimulation.h: Niagara emitter simulation class
==============================================================================*/
#pragma once

#include "NiagaraScript.h"
#include "NiagaraComponent.h"
#include "NiagaraEvents.h"
#include "NiagaraDataSet.h"
#include "NiagaraEmitterProperties.h"

/**
* A Niagara particle simulation.
*/
struct FNiagaraSimulation
{
public:
	explicit FNiagaraSimulation(TWeakObjectPtr<UNiagaraEmitterProperties> InProps, FNiagaraEffectInstance* InParentEffectInstance);
	FNiagaraSimulation(TWeakObjectPtr<UNiagaraEmitterProperties> Props, FNiagaraEffectInstance* InParentEffectInstance, ERHIFeatureLevel::Type InFeatureLevel);
	virtual ~FNiagaraSimulation()
	{}

	NIAGARA_API void Init();

	/** Called after all emitters in an effect have been initialized, allows emitters to access information from one another. */
	void PostInit();

	void PreTick();
	void Tick(float DeltaSeconds);
	void TickEvents(float DeltaSeconds);

	void KillParticles();

	FBox GetBounds() const { return CachedBounds; }

	FNiagaraDataSet &GetData()	{ return Data; }

	FNiagaraConstants &GetConstants()	{ return ExternalConstants; }

	NiagaraEffectRenderer *GetEffectRenderer()	{ return EffectRenderer; }
	
	bool IsEnabled()const 	{ return bIsEnabled;  }

	void NIAGARA_API SetRenderModuleType(EEmitterRenderModuleType Type, ERHIFeatureLevel::Type FeatureLevel);

	int32 GetNumParticles()	{ return Data.GetNumInstances(); }

	TWeakObjectPtr<UNiagaraEmitterProperties> GetProperties()	{ return Props; }
	void SetProperties(TWeakObjectPtr<UNiagaraEmitterProperties> InProps);

	FNiagaraEffectInstance* GetParentEffectInstance()	{ return ParentEffectInstance; }

	float NIAGARA_API GetTotalCPUTime();
	int	NIAGARA_API GetTotalBytesUsed();

	void NIAGARA_API SetEnabled(bool bEnabled);

	float GetAge() { return Age; }
	int32 GetLoopCount()	{ return Loops; }
	void LoopRestart()	
	{ 
		Age = 0.0f;
		Loops++;
		SetTickState(NTS_Running);
	}

	ENiagaraTickState NIAGARA_API GetTickState()	{ return TickState; }
	void NIAGARA_API SetTickState(ENiagaraTickState InState)	{ TickState = InState; }

	FNiagaraDataSet* GetDataSet(FNiagaraDataSetID SetID);

	void SpawnBurst(uint32 Count) { SpawnRemainder += Count; }

private:
	TWeakObjectPtr<UNiagaraEmitterProperties> Props;		// points to an entry in the array of FNiagaraProperties stored in the EffectInstance (itself pointing to the effect's properties)

	/* The age of the emitter*/
	float Age;
	/* how many loops this emitter has gone through */
	uint32 Loops;
	/* If false, don't tick or render*/
	bool bIsEnabled;
	/* Seconds taken to process everything (including rendering) */
	float CPUTimeMS;
	/* Emitter tick state */
	ENiagaraTickState TickState;
	
	/** Local constant set. */
	FNiagaraConstants ExternalConstants;

	/** particle simulation data */
	FNiagaraDataSet Data;
	/** Keep partial particle spawns from last frame */
	float SpawnRemainder;
	/** The cached ComponentToWorld transform. */
	FTransform CachedComponentToWorld;
	/** Cached bounds. */
	FBox CachedBounds;

	NiagaraEffectRenderer *EffectRenderer;
	FNiagaraEffectInstance *ParentEffectInstance;

	TArray<FNiagaraDataSet> DataSets;
	TMap<FNiagaraDataSetID, FNiagaraDataSet*> DataSetMap;
	
	FNiagaraSpawnEventGenerator SpawnEventGenerator;
	bool bGenerateSpawnEvents;
	FNiagaraDeathEventGenerator DeathEventGenerator;
	bool bGenerateDeathEvents;
	
	/** Calc number to spawn */
	int32 CalcNumToSpawn(float DeltaSeconds)
	{
		if (TickState == NTS_Dead || TickState == NTS_Dieing || TickState == NTS_Suspended)
		{
			return 0;
		}

		float FloatNumToSpawn = SpawnRemainder + (DeltaSeconds * Props->SpawnRate);
		int32 NumToSpawn = FMath::FloorToInt(FloatNumToSpawn);
		SpawnRemainder = FloatNumToSpawn - NumToSpawn;
		return NumToSpawn;
	}
	
	/** Runs a script in the VM over a specific range of particles. */
	void RunVMScript(FNiagaraEmitterScriptProperties& ScriptProps, EUnusedAttributeBehaviour UnusedAttribBehaviour);
	void RunVMScript(FNiagaraEmitterScriptProperties& ScriptProps, EUnusedAttributeBehaviour UnusedAttribBehaviour, uint32 StartParticle);
	void RunVMScript(FNiagaraEmitterScriptProperties& ScriptProps, EUnusedAttributeBehaviour UnusedAttribBehaviour, uint32 StartParticle, uint32 NumParticles);

	/** Util to move a particle */
	void MoveParticleToIndex(int32 SrcIndex, int32 DestIndex)
	{
		for (int32 AttrIndex = 0; AttrIndex < Data.GetNumVariables(); AttrIndex++)
		{
			FVector4 *AttrPtr = Data.GetCurrentBuffer(AttrIndex);
			*(AttrPtr+DestIndex) = *(AttrPtr+SrcIndex);
		}
	}

	bool CheckAttriubtesForRenderer();
};
