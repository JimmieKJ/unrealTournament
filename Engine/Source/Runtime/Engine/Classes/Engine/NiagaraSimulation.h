// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*==============================================================================
NiagaraSimulation.h: Niagara emitter simulation class
==============================================================================*/
#pragma once

#include "NiagaraScript.h"
#include "NiagaraSpriteRendererProperties.h"
#include "Components/NiagaraComponent.h"
#include "Runtime/VectorVM/Public/VectorVM.h"
#include "NiagaraSimulation.generated.h"

DECLARE_CYCLE_STAT(TEXT("Tick"), STAT_NiagaraTick, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Simulate"), STAT_NiagaraSimulate, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Spawn + Kill"), STAT_NiagaraSpawnAndKill, STATGROUP_Niagara);


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
	UNiagaraEffectRendererProperties *RendererProperties;

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
	explicit ENGINE_API FNiagaraSimulation(FNiagaraEmitterProperties *InProps);
	ENGINE_API FNiagaraSimulation(FNiagaraEmitterProperties *Props, ERHIFeatureLevel::Type InFeatureLevel);
	virtual ~FNiagaraSimulation()
	{}

	void Tick(float DeltaSeconds);


	FBox GetBounds() const { return CachedBounds; }


	FNiagaraEmitterParticleData &GetData()	{ return Data; }

	void SetConstants(const FNiagaraConstantMap &InMap)	{ Constants = InMap; }
	FNiagaraConstantMap &GetConstants()	{ return Constants; }

	NiagaraEffectRenderer *GetEffectRenderer()	{ return EffectRenderer; }
	
	bool IsEnabled()	{ return bIsEnabled;  }
	void SetEnabled(bool bInEnabled)	{ bIsEnabled = bInEnabled;  }

	ENGINE_API void SetRenderModuleType(EEmitterRenderModuleType Type, ERHIFeatureLevel::Type FeatureLevel);

	int32 GetNumParticles()	{ return Data.GetNumParticles(); }

	FNiagaraEmitterProperties *GetProperties()	{ return Props; }
	void SetProperties(FNiagaraEmitterProperties *InProps)	{ Props = InProps; }

	ENGINE_API float GetTotalCPUTime();
	ENGINE_API int	GetTotalBytesUsed();

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

	/** Run VM to update particle positions */
	void UpdateParticles(
		float DeltaSeconds,
		FVector4* PrevParticles,
		int32 PrevNumVectorsPerAttribute,
		FVector4* Particles,
		int32 NumVectorsPerAttribute,
		int32 NumParticles
		);

	int32 SpawnAndKillParticles(int32 NumToSpawn);


	/** Spawn a new particle at this index */
	int32 SpawnParticles(int32 NumToSpawn);

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

};
