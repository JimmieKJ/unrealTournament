// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*==============================================================================
NiagaraSimulation.h: Niagara emitter simulation class
==============================================================================*/
#pragma once

#include "NiagaraScript.h"
#include "NiagaraComponent.h"
#include "NiagaraSimulation.generated.h"

DECLARE_CYCLE_STAT(TEXT("Tick"), STAT_NiagaraTick, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Simulate"), STAT_NiagaraSimulate, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Spawn"), STAT_NiagaraSpawn, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Kill"), STAT_NiagaraKill, STATGROUP_Niagara);


class NiagaraEffectRenderer;

UENUM()
enum EEmitterRenderModuleType
{
	RMT_None = 0,
	RMT_Sprites,
	RMT_Ribbon,
	RMT_Trails,
	RMT_Meshes
};



/** 
 *	FniagaraEmitterProperties stores the attributes of an FNiagaraSimulation
 *	that need to be serialized and are used for its initialization 
 */
USTRUCT()
struct FNiagaraEmitterProperties
{
	GENERATED_USTRUCT_BODY()
public:
	FNiagaraEmitterProperties() : 
		bIsEnabled(true), SpawnRate(50), UpdateScript(nullptr), SpawnScript(nullptr), Material(nullptr), 
		RenderModuleType(RMT_Sprites), RendererProperties(nullptr)
	{
		Name = FString(TEXT("New Emitter"));
	}
	UPROPERTY(EditAnywhere, Category = "Emitter Properties")
	FString Name;
	UPROPERTY(EditAnywhere, Category = "Emitter Properties")
	bool bIsEnabled;
	UPROPERTY(EditAnywhere, Category = "Emitter Properties")
	float SpawnRate;
	UPROPERTY(EditAnywhere, Category = "Emitter Properties")
	UNiagaraScript *UpdateScript;
	UPROPERTY(EditAnywhere, Category = "Emitter Properties")
	UNiagaraScript *SpawnScript;
	UPROPERTY(EditAnywhere, Category = "Emitter Properties")
	UMaterial *Material;
	UPROPERTY(EditAnywhere, Category = "Emitter Properties")
	TEnumAsByte<EEmitterRenderModuleType> RenderModuleType;

	UPROPERTY()
	class UNiagaraEffectRendererProperties *RendererProperties;

	UPROPERTY(EditAnywhere, Category = "Emitter Properties")
	FNiagaraConstantMap ExternalConstants;		// these are the update script constants from the effect editor; will be added to the emitter's constant map

	UPROPERTY(EditAnywhere, Category = "Emitter Properties")
	FNiagaraConstantMap ExternalSpawnConstants;		// these are the spawn script constants from the effect editor; will be added to the emitter's constant map
};



/**
* A niagara particle simulation.
*/
struct FNiagaraSimulation
{
public:
	explicit FNiagaraSimulation(FNiagaraEmitterProperties *InProps);
	FNiagaraSimulation(FNiagaraEmitterProperties *Props, ERHIFeatureLevel::Type InFeatureLevel);
	virtual ~FNiagaraSimulation()
	{}

	void Init();

	void Tick(float DeltaSeconds);


	FBox GetBounds() const { return CachedBounds; }


	FNiagaraEmitterParticleData &GetData()	{ return Data; }

	void SetConstants(const FNiagaraConstantMap &InMap)	{ Constants = InMap; }
	FNiagaraConstantMap &GetConstants()	{ return Constants; }

	NiagaraEffectRenderer *GetEffectRenderer()	{ return EffectRenderer; }
	
	bool IsEnabled()const 	{ return bIsEnabled;  }

	void NIAGARA_API SetRenderModuleType(EEmitterRenderModuleType Type, ERHIFeatureLevel::Type FeatureLevel);

	int32 GetNumParticles()	{ return Data.GetNumParticles(); }

	FNiagaraEmitterProperties *GetProperties()	{ return Props; }
	void SetProperties(FNiagaraEmitterProperties *InProps);

	float NIAGARA_API GetTotalCPUTime();
	int	NIAGARA_API GetTotalBytesUsed();

	void NIAGARA_API SetSpawnScript(UNiagaraScript* Script);
	void NIAGARA_API SetUpdateScript(UNiagaraScript* Script);
	void NIAGARA_API SetEnabled(bool bEnabled);
private:
	FNiagaraEmitterProperties *Props;		// points to an entry in the array of FNiagaraProperties stored in the EffectInstance (itself pointing to the effect's properties)

	/* The age of the emitter*/
	float Age;
	/* If false, don't tick or render*/
	bool bIsEnabled;
	/* Seconds taken to process everything (including rendering) */
	float CPUTimeMS;
	
	/** Local constant set. */
	FNiagaraConstantMap Constants;
	/** particle simulation data */
	FNiagaraEmitterParticleData Data;
	/** Keep partial particle spawns from last frame */
	float SpawnRemainder;
	/** The cached ComponentToWorld transform. */
	FTransform CachedComponentToWorld;
	/** Cached bounds. */
	FBox CachedBounds;

	NiagaraEffectRenderer *EffectRenderer;

	/** Calc number to spawn */
	int32 CalcNumToSpawn(float DeltaSeconds)
	{
		float FloatNumToSpawn = SpawnRemainder + (DeltaSeconds * Props->SpawnRate);
		int32 NumToSpawn = FMath::FloorToInt(FloatNumToSpawn);
		SpawnRemainder = FloatNumToSpawn - NumToSpawn;
		return NumToSpawn;
	}
	
	/** Runs a script in the VM over a specific range of particles. */
	void RunVMScript(UNiagaraScript* Script, EUnusedAttributeBehaviour UnusedAttribBehaviour);
	void RunVMScript(UNiagaraScript* Script, EUnusedAttributeBehaviour UnusedAttribBehaviour, uint32 StartParticle);
	void RunVMScript(UNiagaraScript* Script, EUnusedAttributeBehaviour UnusedAttribBehaviour, uint32 StartParticle, uint32 NumParticles);

	/** Util to move a particle */
	void MoveParticleToIndex(int32 SrcIndex, int32 DestIndex)
	{
		FVector4 *SrcPtr = Data.GetCurrentBuffer() + SrcIndex;
		FVector4 *DestPtr = Data.GetCurrentBuffer() + DestIndex;

		for (int32 AttrIndex = 0; AttrIndex < Data.GetNumAttributes(); AttrIndex++)
		{
			*DestPtr = *SrcPtr;
			DestPtr += Data.GetParticleAllocation();
			SrcPtr += Data.GetParticleAllocation();
		}
	}

	bool CheckAttriubtesForRenderer();
};
