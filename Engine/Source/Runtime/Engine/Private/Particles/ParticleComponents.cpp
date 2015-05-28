// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnParticleComponent.cpp: Particle component implementation.
=============================================================================*/

#include "EnginePrivate.h"
#include "Distributions/DistributionVectorConstant.h"
#include "Distributions/DistributionVectorConstantCurve.h"
#include "Distributions/DistributionVectorUniform.h"
#include "StaticMeshResources.h"
#include "ParticleDefinitions.h"
#include "Particles/EmitterCameraLensEffectBase.h"
#include "LevelUtils.h"
#include "ImageUtils.h"
#include "FXSystem.h"
#include "Net/UnrealNetwork.h"
#include "MessageLog.h"
#include "UObjectToken.h"
#include "MapErrors.h"
#if WITH_EDITOR
#include "ObjectEditorUtils.h"
#endif

#include "Particles/Collision/ParticleModuleCollision.h"
#include "Particles/Color/ParticleModuleColorOverLife.h"
#include "Particles/Event/ParticleModuleEventGenerator.h"
#include "Particles/Event/ParticleModuleEventReceiverBase.h"
#include "Particles/Lifetime/ParticleModuleLifetimeBase.h"
#include "Particles/Lifetime/ParticleModuleLifetime.h"
#include "Particles/Material/ParticleModuleMeshMaterial.h"
#include "Particles/Orbit/ParticleModuleOrbit.h"
#include "Particles/Size/ParticleModuleSize.h"
#include "Particles/Spawn/ParticleModuleSpawn.h"
#include "Particles/Spawn/ParticleModuleSpawnBase.h"
#include "Particles/TypeData/ParticleModuleTypeDataBase.h"
#include "Particles/TypeData/ParticleModuleTypeDataBeam2.h"
#include "Particles/TypeData/ParticleModuleTypeDataGpu.h"
#include "Particles/TypeData/ParticleModuleTypeDataMesh.h"
#include "Particles/Velocity/ParticleModuleVelocity.h"
#include "Particles/Emitter.h"
#include "Particles/EmitterCameraLensEffectBase.h"
#include "Particles/ParticleEventManager.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModule.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/ParticleSpriteEmitter.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Particles/ParticleSystemReplay.h"
#include "Distributions/DistributionFloatConstantCurve.h"
#include "Engine/InterpCurveEdSetup.h"
#include "GameFramework/GameState.h"


#define LOCTEXT_NAMESPACE "ParticleComponents"

DEFINE_LOG_CATEGORY(LogParticles);

int32 GParticleLODBias = 0;
FAutoConsoleVariableRef CVarParticleLODBias(
	TEXT("r.ParticleLODBias"),
	GParticleLODBias,
	TEXT("LOD bias for particle systems. Development feature, default is 0"),
	ECVF_Cheat
	);

/** Whether to allow particle systems to perform work. */
bool GIsAllowingParticles = true;

/** Whether to calculate LOD on the GameThread in-game. */
bool GbEnableGameThreadLODCalculation = true;

// Comment this in to debug empty emitter instance templates...
//#define _PSYSCOMP_DEBUG_INVALID_EMITTER_INSTANCE_TEMPLATES_

/*-----------------------------------------------------------------------------
	Particle scene view
-----------------------------------------------------------------------------*/
FSceneView*			GParticleView = NULL;

/*-----------------------------------------------------------------------------
	Conversion functions
-----------------------------------------------------------------------------*/
void Particle_ModifyFloatDistribution(UDistributionFloat* pkDistribution, float fScale)
{
	if (pkDistribution->IsA(UDistributionFloatConstant::StaticClass()))
	{
		UDistributionFloatConstant* pkDistConstant = Cast<UDistributionFloatConstant>(pkDistribution);
		pkDistConstant->Constant *= fScale;
	}
	else
	if (pkDistribution->IsA(UDistributionFloatUniform::StaticClass()))
	{
		UDistributionFloatUniform* pkDistUniform = Cast<UDistributionFloatUniform>(pkDistribution);
		pkDistUniform->Min *= fScale;
		pkDistUniform->Max *= fScale;
	}
	else
	if (pkDistribution->IsA(UDistributionFloatConstantCurve::StaticClass()))
	{
		UDistributionFloatConstantCurve* pkDistCurve = Cast<UDistributionFloatConstantCurve>(pkDistribution);

		int32 iKeys = pkDistCurve->GetNumKeys();
		int32 iCurves = pkDistCurve->GetNumSubCurves();

		for (int32 KeyIndex = 0; KeyIndex < iKeys; KeyIndex++)
		{
			float fKeyIn = pkDistCurve->GetKeyIn(KeyIndex);
			for (int32 SubIndex = 0; SubIndex < iCurves; SubIndex++)
			{
				float fKeyOut = pkDistCurve->GetKeyOut(SubIndex, KeyIndex);
				float ArriveTangent;
				float LeaveTangent;
				pkDistCurve->GetTangents(SubIndex, KeyIndex, ArriveTangent, LeaveTangent);

				pkDistCurve->SetKeyOut(SubIndex, KeyIndex, fKeyOut * fScale);
				pkDistCurve->SetTangents(SubIndex, KeyIndex, ArriveTangent * fScale, LeaveTangent * fScale);
			}
		}
	}
}

void Particle_ModifyVectorDistribution(UDistributionVector* pkDistribution, FVector& vScale)
{
	if (pkDistribution->IsA(UDistributionVectorConstant::StaticClass()))
	{
		UDistributionVectorConstant* pkDistConstant = Cast<UDistributionVectorConstant>(pkDistribution);
		pkDistConstant->Constant *= vScale;
	}
	else
	if (pkDistribution->IsA(UDistributionVectorUniform::StaticClass()))
	{
		UDistributionVectorUniform* pkDistUniform = Cast<UDistributionVectorUniform>(pkDistribution);
		pkDistUniform->Min *= vScale;
		pkDistUniform->Max *= vScale;
	}
	else
	if (pkDistribution->IsA(UDistributionVectorConstantCurve::StaticClass()))
	{
		UDistributionVectorConstantCurve* pkDistCurve = Cast<UDistributionVectorConstantCurve>(pkDistribution);

		int32 iKeys = pkDistCurve->GetNumKeys();
		int32 iCurves = pkDistCurve->GetNumSubCurves();

		for (int32 KeyIndex = 0; KeyIndex < iKeys; KeyIndex++)
		{
			float fKeyIn = pkDistCurve->GetKeyIn(KeyIndex);
			for (int32 SubIndex = 0; SubIndex < iCurves; SubIndex++)
			{
				float fKeyOut = pkDistCurve->GetKeyOut(SubIndex, KeyIndex);
				float ArriveTangent;
				float LeaveTangent;
				pkDistCurve->GetTangents(SubIndex, KeyIndex, ArriveTangent, LeaveTangent);

				switch (SubIndex)
				{
				case 1:
					pkDistCurve->SetKeyOut(SubIndex, KeyIndex, fKeyOut * vScale.Y);
					pkDistCurve->SetTangents(SubIndex, KeyIndex, ArriveTangent * vScale.Y, LeaveTangent * vScale.Y);
					break;
				case 2:
					pkDistCurve->SetKeyOut(SubIndex, KeyIndex, fKeyOut * vScale.Z);
					pkDistCurve->SetTangents(SubIndex, KeyIndex, ArriveTangent * vScale.Z, LeaveTangent * vScale.Z);
					break;
				case 0:
				default:
					pkDistCurve->SetKeyOut(SubIndex, KeyIndex, fKeyOut * vScale.X);
					pkDistCurve->SetTangents(SubIndex, KeyIndex, ArriveTangent * vScale.X, LeaveTangent * vScale.X);
					break;
				}
			}
		}
	}
}

/** Console command to reset all particle components. */
static void ResetAllParticleComponents()
{
	for (TObjectIterator<UParticleSystemComponent> It; It; ++It)
	{
		UParticleSystemComponent* ParticleSystemComponent = *It;
		ParticleSystemComponent->ResetParticles();
		ParticleSystemComponent->ActivateSystem(true);
		ParticleSystemComponent->bIsViewRelevanceDirty = true;
		ParticleSystemComponent->CachedViewRelevanceFlags.Empty();
		ParticleSystemComponent->ConditionalCacheViewRelevanceFlags();
		ParticleSystemComponent->ReregisterComponent();
	}
}
static FAutoConsoleCommand GResetAllParticleComponentsCmd(
	TEXT("fx.RestartAll"),
	TEXT("Restarts all particle system components"),
	FConsoleCommandDelegate::CreateStatic(ResetAllParticleComponents)
	);

/*-----------------------------------------------------------------------------
	UParticleLODLevel implementation.
-----------------------------------------------------------------------------*/
UParticleLODLevel::UParticleLODLevel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bEnabled = true;
	ConvertedModules = true;
	PeakActiveParticles = 0;
}


void UParticleLODLevel::CompileModules( FParticleEmitterBuildInfo& EmitterBuildInfo )
{
	check( RequiredModule );
	check( SpawnModule );

	// Store a few special modules.
	EmitterBuildInfo.RequiredModule = RequiredModule;
	EmitterBuildInfo.SpawnModule = SpawnModule;

	// Compile those special modules.
	RequiredModule->CompileModule( EmitterBuildInfo );
	if ( SpawnModule->bEnabled )
	{
		SpawnModule->CompileModule( EmitterBuildInfo );
	}

	// Compile all remaining modules.
	const int32 ModuleCount = Modules.Num();
	for ( int32 ModuleIndex = 0; ModuleIndex < ModuleCount; ++ModuleIndex )
	{
		UParticleModule* Module = Modules[ModuleIndex];
		if ( Module && Module->bEnabled )
		{
			Module->CompileModule( EmitterBuildInfo );
		}
	}

	// Estimate the maximum number of active particles.
	EmitterBuildInfo.EstimatedMaxActiveParticleCount = CalculateMaxActiveParticleCount();
}

void UParticleLODLevel::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITORONLY_DATA
	checkf(SpawnModule, TEXT("Missing spawn module on %s (%s)"), *(GetPathName()), 
		(GetOuter() ? (GetOuter()->GetOuter() ? *(GetOuter()->GetOuter()->GetPathName()) : *(GetOuter()->GetPathName())) : TEXT("???")));
#endif // WITH_EDITORONLY_DATA

	// Fix up emitters that have bEnabled flag set incorrectly
	if (RequiredModule)
	{
		RequiredModule->ConditionalPostLoad();
		if (RequiredModule->bEnabled != bEnabled)
		{
			RequiredModule->bEnabled = bEnabled;
		}
	}
}

void UParticleLODLevel::UpdateModuleLists()
{
	SpawningModules.Empty();
	SpawnModules.Empty();
	UpdateModules.Empty();
	OrbitModules.Empty();
	EventReceiverModules.Empty();
	EventGenerator = NULL;

	UParticleModule* Module;
	int32 TypeDataModuleIndex = -1;

	for (int32 i = 0; i < Modules.Num(); i++)
	{
		Module = Modules[i];
		if (!Module)
		{
			continue;
		}

		if (Module->bSpawnModule)
		{
			SpawnModules.Add(Module);
		}
		if (Module->bUpdateModule || Module->bFinalUpdateModule)
		{
			UpdateModules.Add(Module);
		}

		if (Module->IsA(UParticleModuleTypeDataBase::StaticClass()))
		{
			TypeDataModule = Module;
			if (!Module->bSpawnModule && !Module->bUpdateModule)
			{
				// For now, remove it from the list and set it as the TypeDataModule
				TypeDataModuleIndex = i;
			}
		}
		else
		if (Module->IsA(UParticleModuleSpawnBase::StaticClass()))
		{
			UParticleModuleSpawnBase* SpawnBase = Cast<UParticleModuleSpawnBase>(Module);
			SpawningModules.Add(SpawnBase);
		}
		else
		if (Module->IsA(UParticleModuleOrbit::StaticClass()))
		{
			UParticleModuleOrbit* Orbit = Cast<UParticleModuleOrbit>(Module);
			OrbitModules.Add(Orbit);
		}
		else
		if (Module->IsA(UParticleModuleEventGenerator::StaticClass()))
		{
			EventGenerator = Cast<UParticleModuleEventGenerator>(Module);
		}
		else
		if (Module->IsA(UParticleModuleEventReceiverBase::StaticClass()))
		{
			UParticleModuleEventReceiverBase* Event = Cast<UParticleModuleEventReceiverBase>(Module);
			EventReceiverModules.Add(Event);
		}
	}

	if (EventGenerator)
	{
		// Force the event generator module to the top of the module stack...
		Modules.RemoveSingle(EventGenerator);
		Modules.Insert(EventGenerator, 0);
	}

	if (TypeDataModuleIndex != -1)
	{
		Modules.RemoveAt(TypeDataModuleIndex);
	}

	if (TypeDataModule /**&& (Level == 0)**/)
	{
		UParticleModuleTypeDataMesh* MeshTD = Cast<UParticleModuleTypeDataMesh>(TypeDataModule);
		if (MeshTD
			&& MeshTD->Mesh
			&& MeshTD->Mesh->HasValidRenderData())
		{
			UParticleSpriteEmitter* SpriteEmitter = Cast<UParticleSpriteEmitter>(GetOuter());
			if (SpriteEmitter && (MeshTD->bOverrideMaterial == false))
			{
				FStaticMeshSection& Section = MeshTD->Mesh->RenderData->LODResources[0].Sections[0];
				UMaterialInterface* Material = MeshTD->Mesh->GetMaterial(Section.MaterialIndex);
				if (Material)
				{
					RequiredModule->Material = Material;
				}
			}
		}
	}
}


bool UParticleLODLevel::GenerateFromLODLevel(UParticleLODLevel* SourceLODLevel, float Percentage, bool bGenerateModuleData)
{
	// See if there are already modules in place
	if (Modules.Num() > 0)
	{
		UE_LOG(LogParticles, Log, TEXT("ERROR? - GenerateFromLODLevel - modules already present!"));
		return false;
	}

	bool	bResult	= true;

	// Allocate slots in the array...
	Modules.InsertZeroed(0, SourceLODLevel->Modules.Num());

	// Set the enabled flag
	bEnabled = SourceLODLevel->bEnabled;

	// Set up for undo/redo!
	SetFlags(RF_Transactional);

	// Required module...
	RequiredModule = CastChecked<UParticleModuleRequired>(
		SourceLODLevel->RequiredModule->GenerateLODModule(SourceLODLevel, this, Percentage, bGenerateModuleData));

	// Spawn module...
	SpawnModule = CastChecked<UParticleModuleSpawn>(
		SourceLODLevel->SpawnModule->GenerateLODModule(SourceLODLevel, this, Percentage, bGenerateModuleData));

	// TypeData module, if present...
	if (SourceLODLevel->TypeDataModule)
	{
		TypeDataModule = SourceLODLevel->TypeDataModule->GenerateLODModule(SourceLODLevel, this, Percentage, bGenerateModuleData);
	}

	// The remaining modules...
	for (int32 ModuleIndex = 0; ModuleIndex < SourceLODLevel->Modules.Num(); ModuleIndex++)
	{
		if (SourceLODLevel->Modules[ModuleIndex])
		{
			Modules[ModuleIndex] = SourceLODLevel->Modules[ModuleIndex]->GenerateLODModule(SourceLODLevel, this, Percentage, bGenerateModuleData);
		}
		else
		{
			Modules[ModuleIndex] = NULL;
		}
	}

	return bResult;
}


int32	UParticleLODLevel::CalculateMaxActiveParticleCount()
{
	check(RequiredModule != NULL);

	// Determine the lifetime for particles coming from the emitter
	float ParticleLifetime = 0.0f;
	float MaxSpawnRate = SpawnModule->GetEstimatedSpawnRate();
	int32 MaxBurstCount = SpawnModule->GetMaximumBurstCount();
	for (int32 ModuleIndex = 0; ModuleIndex < Modules.Num(); ModuleIndex++)
	{
		UParticleModuleLifetimeBase* LifetimeMod = Cast<UParticleModuleLifetimeBase>(Modules[ModuleIndex]);
		if (LifetimeMod != NULL)
		{
			ParticleLifetime += LifetimeMod->GetMaxLifetime();
		}

		UParticleModuleSpawnBase* SpawnMod = Cast<UParticleModuleSpawnBase>(Modules[ModuleIndex]);
		if (SpawnMod != NULL)
		{
			MaxSpawnRate += SpawnMod->GetEstimatedSpawnRate();
			MaxBurstCount += SpawnMod->GetMaximumBurstCount();
		}
	}

	// Determine the maximum duration for this particle system
	float MaxDuration = 0.0f;
	float TotalDuration = 0.0f;
	int32 TotalLoops = 0;
	if (RequiredModule != NULL)
	{
		// We don't care about delay wrt spawning...
		MaxDuration = FMath::Max<float>(RequiredModule->EmitterDuration, RequiredModule->EmitterDurationLow);
		TotalLoops = RequiredModule->EmitterLoops;
		TotalDuration = MaxDuration * TotalLoops;
	}

	// Determine the max
	int32 MaxAPC = 0;

	if (TotalDuration != 0.0f)
	{
		if (TotalLoops == 1)
		{
			// Special case for one loop... 
			if (ParticleLifetime < MaxDuration)
			{
				MaxAPC += FMath::CeilToInt(ParticleLifetime * MaxSpawnRate);
			}
			else
			{
				MaxAPC += FMath::CeilToInt(MaxDuration * MaxSpawnRate);
			}
			// Safety zone...
			MaxAPC += 1;
			// Add in the bursts...
			MaxAPC += MaxBurstCount;
		}
		else
		{
			if (ParticleLifetime < MaxDuration)
			{
				MaxAPC += FMath::CeilToInt(ParticleLifetime * MaxSpawnRate);
			}
			else
			{
				MaxAPC += (FMath::CeilToInt(FMath::CeilToInt(MaxDuration * MaxSpawnRate) * ParticleLifetime));
			}
			// Safety zone...
			MaxAPC += 1;
			// Add in the bursts...
			MaxAPC += MaxBurstCount;
			if (ParticleLifetime > MaxDuration)
			{
				MaxAPC += MaxBurstCount * FMath::CeilToInt(ParticleLifetime - MaxDuration);
			}
		}
	}
	else
	{
		// We are infinite looping... 
		// Single loop case is all we will worry about. Safer base estimate - but not ideal.
		if (ParticleLifetime < MaxDuration)
		{
			MaxAPC += FMath::CeilToInt(ParticleLifetime * FMath::CeilToInt(MaxSpawnRate));
		}
		else
		{
			if (ParticleLifetime != 0.0f)
			{
				if (ParticleLifetime <= MaxDuration)
				{
					MaxAPC += FMath::CeilToInt(MaxDuration * MaxSpawnRate);
				}
				else //if (ParticleLifetime > MaxDuration)
				{
					MaxAPC += FMath::CeilToInt(MaxDuration * MaxSpawnRate) * ParticleLifetime;
				}
			}
			else
			{
				// No lifetime, no duration...
				MaxAPC += FMath::CeilToInt(MaxSpawnRate);
			}
		}
		// Safety zone...
		MaxAPC += FMath::Max<int32>(FMath::CeilToInt(MaxSpawnRate * 0.032f), 2);
		// Burst
		MaxAPC += MaxBurstCount;
	}

	PeakActiveParticles = MaxAPC;

	return MaxAPC;
}


void UParticleLODLevel::ConvertToSpawnModule()
{
#if WITH_EDITOR
	// Move the required module SpawnRate and Burst information to a new SpawnModule.
	if (SpawnModule)
	{
//		UE_LOG(LogParticles, Warning, TEXT("LOD Level already has a spawn module!"));
		return;
	}

	UParticleEmitter* EmitterOuter = CastChecked<UParticleEmitter>(GetOuter());
	SpawnModule = NewObject<UParticleModuleSpawn>(EmitterOuter->GetOuter());
	check(SpawnModule);

	UDistributionFloat* SourceDist = RequiredModule->SpawnRate.Distribution;
	if (SourceDist)
	{
		SpawnModule->Rate.Distribution = Cast<UDistributionFloat>(StaticDuplicateObject(SourceDist, SpawnModule, TEXT("None")));
		SpawnModule->Rate.Distribution->bIsDirty = true;
		SpawnModule->Rate.Initialize();
	}

	// Now the burst list.
	int32 BurstCount = RequiredModule->BurstList.Num();
	if (BurstCount > 0)
	{
		SpawnModule->BurstList.AddZeroed(BurstCount);
		for (int32 BurstIndex = 0; BurstIndex < BurstCount; BurstIndex++)
		{
			SpawnModule->BurstList[BurstIndex].Count = RequiredModule->BurstList[BurstIndex].Count;
			SpawnModule->BurstList[BurstIndex].CountLow = RequiredModule->BurstList[BurstIndex].CountLow;
			SpawnModule->BurstList[BurstIndex].Time = RequiredModule->BurstList[BurstIndex].Time;
		}
	}

	MarkPackageDirty();
#endif	//#if WITH_EDITOR
}


int32 UParticleLODLevel::GetModuleIndex(UParticleModule* InModule)
{
	if (InModule)
	{
		if (InModule == RequiredModule)
		{
			return INDEX_REQUIREDMODULE;
		}
		else if (InModule == SpawnModule)
		{
			return INDEX_SPAWNMODULE;
		}
		else if (InModule == TypeDataModule)
		{
			return INDEX_TYPEDATAMODULE;
		}
		else
		{
			for (int32 ModuleIndex = 0; ModuleIndex < Modules.Num(); ModuleIndex++)
			{
				if (InModule == Modules[ModuleIndex])
				{
					return ModuleIndex;
				}
			}
		}
	}

	return INDEX_NONE;
}


UParticleModule* UParticleLODLevel::GetModuleAtIndex(int32 InIndex)
{
	// 'Normal' modules
	if (InIndex > INDEX_NONE)
	{
		if (InIndex < Modules.Num())
		{
			return Modules[InIndex];
		}

		return NULL;
	}

	switch (InIndex)
	{
	case INDEX_REQUIREDMODULE:		return RequiredModule;
	case INDEX_SPAWNMODULE:			return SpawnModule;
	case INDEX_TYPEDATAMODULE:		return TypeDataModule;
	}

	return NULL;
}


void UParticleLODLevel::SetLevelIndex(int32 InLevelIndex)
{
	// Remove the 'current' index from the validity flags and set the new one.
	RequiredModule->LODValidity &= ~(1 << Level);
	RequiredModule->LODValidity |= (1 << InLevelIndex);
	SpawnModule->LODValidity &= ~(1 << Level);
	SpawnModule->LODValidity |= (1 << InLevelIndex);
	if (TypeDataModule)
	{
		TypeDataModule->LODValidity &= ~(1 << Level);
		TypeDataModule->LODValidity |= (1 << InLevelIndex);
	}
	for (int32 ModuleIndex = 0; ModuleIndex < Modules.Num(); ModuleIndex++)
	{
		UParticleModule* CheckModule = Modules[ModuleIndex];
		if (CheckModule)
		{
			CheckModule->LODValidity &= ~(1 << Level);
			CheckModule->LODValidity |= (1 << InLevelIndex);
		}
	}

	Level = InLevelIndex;
}


bool UParticleLODLevel::IsModuleEditable(UParticleModule* InModule)
{
	// If the module validity flag is not set for this level, it is not editable.
	if ((InModule->LODValidity & (1 << Level)) == 0)
	{
		return false;
	}

	// If the module is shared w/ higher LOD levels, then it is not editable...
	int32 Validity = 0;
	if (Level > 0)
	{
		int32 Check = Level - 1;
		while (Check >= 0)
		{
			Validity |= (1 << Check);
			Check--;
		}

		if ((Validity & InModule->LODValidity) != 0)
		{
			return false;
		}
	}

	return true;
}

/*-----------------------------------------------------------------------------
	UParticleEmitter implementation.
-----------------------------------------------------------------------------*/
UParticleEmitter::UParticleEmitter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, QualityLevelSpawnRateScale(1.0f)
	, bDisabledLODsKeepEmitterAlive(false)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FName NAME_Particle_Emitter;
		FConstructorStatics()
			: NAME_Particle_Emitter(TEXT("Particle Emitter"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	EmitterName = ConstructorStatics.NAME_Particle_Emitter;
	ConvertedModules = true;
	PeakActiveParticles = 0;
#if WITH_EDITORONLY_DATA
	EmitterEditorColor = FColor(0, 150, 150, 255);
#endif // WITH_EDITORONLY_DATA

}

FParticleEmitterInstance* UParticleEmitter::CreateInstance(UParticleSystemComponent* InComponent)
{
	UE_LOG(LogParticles, Fatal,TEXT("UParticleEmitter::CreateInstance is pure virtual")); 
	return NULL; 
}

void UParticleEmitter::UpdateModuleLists()
{
	for (int32 LODIndex = 0; LODIndex < LODLevels.Num(); LODIndex++)
	{
		UParticleLODLevel* LODLevel = LODLevels[LODIndex];
		if (LODLevel)
		{
			LODLevel->UpdateModuleLists();
		}
	}
	Build();
}

/**
 *	Helper function for fixing up LODValidity issues on particle modules...
 *
 *	@param	LODIndex		The index of the LODLevel the module is from.
 *	@param	ModuleIndex		The index of the module being checked.
 *	@param	Emitter			The emitter owner.
 *	@param	CurrModule		The module being checked.
 *
 *	@return	 0		If there was no problem.
 *			 1		If there was a problem and it was fixed.
 *			-1		If there was a problem that couldn't be fixed.
 */
int32 ParticleEmitterHelper_FixupModuleLODErrors( int32 LODIndex, int32 ModuleIndex, 
	const UParticleEmitter* Emitter, UParticleModule* CurrModule )
{
	int32 Result = 1;
	bool bIsDirty = false;

	UObject* ModuleOuter = CurrModule->GetOuter();
	UObject* EmitterOuter = Emitter->GetOuter();
	if (ModuleOuter != EmitterOuter)
	{
		// Module has an incorrect outer
		CurrModule->Rename(NULL, EmitterOuter, REN_ForceNoResetLoaders|REN_DoNotDirty);
		bIsDirty = true;
	}

	if (CurrModule->LODValidity == 0)
	{
		// Immediately tag it for this lod level...
		CurrModule->LODValidity = (1 << LODIndex);
		bIsDirty = true;
	}
	else
	if (CurrModule->IsUsedInLODLevel(LODIndex) == false)
	{
		// Why was this even called here?? 
		// The assumption is that it should be called for the module in the given lod level...
		// So, we will tag it with this index.
		CurrModule->LODValidity |= (1 << LODIndex);
		bIsDirty = true;
	}

	if (LODIndex > 0)
	{
		int32 CheckIndex = LODIndex - 1;
		while (CheckIndex >= 0)
		{
			if (CurrModule->IsUsedInLODLevel(CheckIndex))
			{
				// Ensure that it is the same as the one it THINKS it is shared with...
				UParticleLODLevel* CheckLODLevel = Emitter->LODLevels[CheckIndex];

				if (CurrModule->IsA(UParticleModuleSpawn::StaticClass()))
				{
					if (CheckLODLevel->SpawnModule != CurrModule)
					{
						// Fix it up... Turn off the higher LOD flag
						CurrModule->LODValidity &= ~(1 << CheckIndex);
						bIsDirty = true;
					}
				}
				else
				if (CurrModule->IsA(UParticleModuleRequired::StaticClass()))
				{
					if (CheckLODLevel->RequiredModule != CurrModule)
					{
						// Fix it up... Turn off the higher LOD flag
						CurrModule->LODValidity &= ~(1 << CheckIndex);
						bIsDirty = true;
					}
				}
				else
				if (CurrModule->IsA(UParticleModuleTypeDataBase::StaticClass()))
				{
					if (CheckLODLevel->TypeDataModule != CurrModule)
					{
						// Fix it up... Turn off the higher LOD flag
						CurrModule->LODValidity &= ~(1 << CheckIndex);
						bIsDirty = true;
					}
				}
				else
				{
					if (ModuleIndex >= CheckLODLevel->Modules.Num())
					{
						UE_LOG(LogParticles, Warning, TEXT("\t\tMismatched module count at %2d in %s"), LODIndex, *(Emitter->GetPathName()));
						Result = -1;
					}
					else
					{
						UParticleModule* CheckModule = CheckLODLevel->Modules[ModuleIndex];
						if (CheckModule != CurrModule)
						{
							// Fix it up... Turn off the higher LOD flag
							CurrModule->LODValidity &= ~(1 << CheckIndex);
							bIsDirty = true;
						}
					}
				}
			}

			CheckIndex--;
		}
	}

	if ((bIsDirty == true) && IsRunningCommandlet())
	{
		CurrModule->MarkPackageDirty();
		Emitter->MarkPackageDirty();
	}

	return Result;
}

void UParticleEmitter::PostLoad()
{
	Super::PostLoad();

	for (int32 LODIndex = 0; LODIndex < LODLevels.Num(); LODIndex++)
	{
		UParticleLODLevel* LODLevel = LODLevels[LODIndex];
		if (LODLevel)
		{
			LODLevel->ConditionalPostLoad();

			FLinkerLoad* LODLevelLinker = LODLevel->GetLinker();
			if (LODLevel->SpawnModule == NULL)
			{
				// Force the conversion to SpawnModule
				UParticleSystem* PSys = Cast<UParticleSystem>(GetOuter());
				if (PSys)
				{
					UE_LOG(LogParticles, Warning, TEXT("LODLevel %d was not converted to spawn module - forcing: %s"), 
						LODLevel->Level, *(PSys->GetPathName()));
				}
				LODLevel->ConvertToSpawnModule();
			}

			check(LODLevel->SpawnModule);

		}
	}

#if WITH_EDITOR
	if ((GIsEditor == true) && 1)//(IsRunningCommandlet() == false))
	{
		ConvertedModules = false;
		PeakActiveParticles = 0;

		// Check for improper outers...
		UObject* EmitterOuter = GetOuter();
		bool bWarned = false;
		for (int32 LODIndex = 0; (LODIndex < LODLevels.Num()) && !bWarned; LODIndex++)
		{
			UParticleLODLevel* LODLevel = LODLevels[LODIndex];
			if (LODLevel)
			{
				LODLevel->ConditionalPostLoad();

				UParticleModule* Module = LODLevel->TypeDataModule;
				if (Module)
				{
					Module->ConditionalPostLoad();

					UObject* OuterObj = Module->GetOuter();
					check(OuterObj);
					if (OuterObj != EmitterOuter)
					{
						UE_LOG(LogParticles, Warning, TEXT("UParticleModule %s has an incorrect outer on %s... run FixupEmitters on package %s (%s)"),
							*(Module->GetPathName()), 
							*(EmitterOuter->GetPathName()),
							*(OuterObj->GetOutermost()->GetPathName()),
							*(GetOutermost()->GetPathName()));
						UE_LOG(LogParticles, Warning, TEXT("\tModule Outer..............%s"), *(OuterObj->GetPathName()));
						UE_LOG(LogParticles, Warning, TEXT("\tModule Outermost..........%s"), *(Module->GetOutermost()->GetPathName()));
						UE_LOG(LogParticles, Warning, TEXT("\tEmitter Outer.............%s"), *(EmitterOuter->GetPathName()));
						UE_LOG(LogParticles, Warning, TEXT("\tEmitter Outermost.........%s"), *(GetOutermost()->GetPathName()));
						bWarned = true;
					}
				}

				if (!bWarned)
				{
					for (int32 ModuleIndex = 0; (ModuleIndex < LODLevel->Modules.Num()) && !bWarned; ModuleIndex++)
					{
						Module = LODLevel->Modules[ModuleIndex];
						if (Module)
						{
							Module->ConditionalPostLoad();

							UObject* OuterObj = Module->GetOuter();
							check(OuterObj);
							if (OuterObj != EmitterOuter)
							{
								UE_LOG(LogParticles, Warning, TEXT("UParticleModule %s has an incorrect outer on %s... run FixupEmitters on package %s (%s)"),
									*(Module->GetPathName()), 
									*(EmitterOuter->GetPathName()),
									*(OuterObj->GetOutermost()->GetPathName()),
									*(GetOutermost()->GetPathName()));
								UE_LOG(LogParticles, Warning, TEXT("\tModule Outer..............%s"), *(OuterObj->GetPathName()));
								UE_LOG(LogParticles, Warning, TEXT("\tModule Outermost..........%s"), *(Module->GetOutermost()->GetPathName()));
								UE_LOG(LogParticles, Warning, TEXT("\tEmitter Outer.............%s"), *(EmitterOuter->GetPathName()));
								UE_LOG(LogParticles, Warning, TEXT("\tEmitter Outermost.........%s"), *(GetOutermost()->GetPathName()));
								bWarned = true;
							}
						}
					}
				}
			}
		}
	}
	else
#endif
	{
		for (int32 LODIndex = 0; LODIndex < LODLevels.Num(); LODIndex++)
		{
			UParticleLODLevel* LODLevel = LODLevels[LODIndex];
			if (LODLevel)
			{
				LODLevel->ConditionalPostLoad();
			}
		}
	}
   

	ConvertedModules = true;

	// this will look at all of the emitters and then remove ones that some how have become NULL (e.g. from a removal of an Emitter where content
	// is still referencing it)
	for (int32 LODIndex = 0; LODIndex < LODLevels.Num(); LODIndex++)
	{
		UParticleLODLevel* LODLevel = LODLevels[LODIndex];
		if (LODLevel)
		{
			for (int32 ModuleIndex = LODLevel->Modules.Num()-1; ModuleIndex >= 0; ModuleIndex--)
			{
				UParticleModule* ParticleModule = LODLevel->Modules[ModuleIndex];
				if( ParticleModule == NULL )
				{
					LODLevel->Modules.RemoveAt(ModuleIndex);
					MarkPackageDirty();
				}
			}
		}
	}


	UObject* MyOuter = GetOuter();
	UParticleSystem* PSysOuter = Cast<UParticleSystem>(MyOuter);
	bool bRegenDup = false;
	if (PSysOuter)
	{
		bRegenDup = PSysOuter->bRegenerateLODDuplicate;
	}

	// Clamp the detail spawn rate scale...
	QualityLevelSpawnRateScale = FMath::Clamp<float>(QualityLevelSpawnRateScale, 0.0f, 1.0f);

	UpdateModuleLists();
}

#if WITH_EDITOR
void UParticleEmitter::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	check(GIsEditor);

	// Reset the peak active particle counts.
	// This could check for changes to SpawnRate and Burst and only reset then,
	// but since we reset the particle system after any edited property, it
	// may as well just autoreset the peak counts.
	for (int32 LODIndex = 0; LODIndex < LODLevels.Num(); LODIndex++)
	{
		UParticleLODLevel* LODLevel = LODLevels[LODIndex];
		if (LODLevel)
		{
			LODLevel->PeakActiveParticles	= 1;
		}
	}

	UpdateModuleLists();

	for (TObjectIterator<UParticleSystemComponent> It;It;++It)
	{
		if (It->Template)
		{
			int32 i;

			for (i=0; i<It->Template->Emitters.Num(); i++)
			{
				if (It->Template->Emitters[i] == this)
				{
					It->UpdateInstances();
				}
			}
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (CalculateMaxActiveParticleCount() == false)
	{
		//
	}

	// Clamp the detail spawn rate scale...
	QualityLevelSpawnRateScale = FMath::Clamp<float>(QualityLevelSpawnRateScale, 0.0f, 1.0f);
}
#endif // WITH_EDITOR

void UParticleEmitter::SetEmitterName(FName Name)
{
	EmitterName = Name;
}

FName& UParticleEmitter::GetEmitterName()
{
	return EmitterName;
}

void UParticleEmitter::SetLODCount(int32 LODCount)
{
	// 
}

void UParticleEmitter::AddEmitterCurvesToEditor(UInterpCurveEdSetup* EdSetup)
{
	UE_LOG(LogParticles, Log, TEXT("UParticleEmitter::AddEmitterCurvesToEditor> Should no longer be called..."));
	return;
}

void UParticleEmitter::RemoveEmitterCurvesFromEditor(UInterpCurveEdSetup* EdSetup)
{
	for (int32 LODIndex = 0; LODIndex < LODLevels.Num(); LODIndex++)
	{
		UParticleLODLevel* LODLevel = LODLevels[LODIndex];
		// Remove the typedata curves...
		if (LODLevel->TypeDataModule && LODLevel->TypeDataModule->IsDisplayedInCurveEd(EdSetup))
		{
			LODLevel->TypeDataModule->RemoveModuleCurvesFromEditor(EdSetup);
		}

		// Remove the spawn module curves...
		if (LODLevel->SpawnModule && LODLevel->SpawnModule->IsDisplayedInCurveEd(EdSetup))
		{
			LODLevel->SpawnModule->RemoveModuleCurvesFromEditor(EdSetup);
		}

		// Remove each modules curves as well.
		for (int32 ii = 0; ii < LODLevel->Modules.Num(); ii++)
		{
			if (LODLevel->Modules[ii]->IsDisplayedInCurveEd(EdSetup))
			{
				// Remove it from the curve editor!
				LODLevel->Modules[ii]->RemoveModuleCurvesFromEditor(EdSetup);
			}
		}
	}
}

void UParticleEmitter::ChangeEditorColor(FColor& Color, UInterpCurveEdSetup* EdSetup)
{
#if WITH_EDITORONLY_DATA
	UParticleLODLevel* LODLevel = LODLevels[0];
	EmitterEditorColor = Color;
	for (int32 TabIndex = 0; TabIndex < EdSetup->Tabs.Num(); TabIndex++)
	{
		FCurveEdTab*	Tab = &(EdSetup->Tabs[TabIndex]);
		for (int32 CurveIndex = 0; CurveIndex < Tab->Curves.Num(); CurveIndex++)
		{
			FCurveEdEntry* Entry	= &(Tab->Curves[CurveIndex]);
			if (LODLevel->SpawnModule->Rate.Distribution == Entry->CurveObject)
			{
				Entry->CurveColor	= Color;
			}
		}
	}
#endif // WITH_EDITORONLY_DATA
}

void UParticleEmitter::AutoPopulateInstanceProperties(UParticleSystemComponent* PSysComp)
{
	for (int32 LODIndex = 0; LODIndex < LODLevels.Num(); LODIndex++)
	{
		UParticleLODLevel* LODLevel	= LODLevels[LODIndex];
		for (int32 ModuleIndex = 0; ModuleIndex < LODLevel->Modules.Num(); ModuleIndex++)
		{
			UParticleModule* Module = LODLevel->Modules[ModuleIndex];
			Module->AutoPopulateInstanceProperties(PSysComp);
		}
	}
}


int32 UParticleEmitter::CreateLODLevel(int32 LODLevel, bool bGenerateModuleData)
{
	int32					LevelIndex		= -1;
	UParticleLODLevel*	CreatedLODLevel	= NULL;

	if (LODLevels.Num() == 0)
	{
		LODLevel = 0;
	}

	// Is the requested index outside a viable range?
	if ((LODLevel < 0) || (LODLevel > LODLevels.Num()))
	{
		return -1;
	}

	// NextHighestLODLevel is the one that will be 'copied'
	UParticleLODLevel*	NextHighestLODLevel	= NULL;
	int32 NextHighIndex = -1;
	// NextLowestLODLevel is the one (and all ones lower than it) that will have their LOD indices updated
	UParticleLODLevel*	NextLowestLODLevel	= NULL;
	int32 NextLowIndex = -1;

	// Grab the two surrounding LOD levels...
	if (LODLevel == 0)
	{
		// It is being added at the front of the list... (highest)
		if (LODLevels.Num() > 0)
		{
			NextHighestLODLevel = LODLevels[0];
			NextHighIndex = 0;
			NextLowestLODLevel = NextHighestLODLevel;
			NextLowIndex = 0;
		}
	}
	else
	if (LODLevel > 0)
	{
		NextHighestLODLevel = LODLevels[LODLevel - 1];
		NextHighIndex = LODLevel - 1;
		if (LODLevel < LODLevels.Num())
		{
			NextLowestLODLevel = LODLevels[LODLevel];
			NextLowIndex = LODLevel;
		}
	}

	// Update the LODLevel index for the lower levels and
	// offset the LOD validity flags for the modules...
	if (NextLowestLODLevel)
	{
		for (int32 LowIndex = LODLevels.Num() - 1; LowIndex >= NextLowIndex; LowIndex--)
		{
			UParticleLODLevel* LowRemapLevel = LODLevels[LowIndex];
			if (LowRemapLevel)
			{
				LowRemapLevel->SetLevelIndex(LowIndex + 1);
			}
		}
	}

	// Create a ParticleLODLevel
	CreatedLODLevel = NewObject<UParticleLODLevel>(this);
	check(CreatedLODLevel);

	CreatedLODLevel->Level = LODLevel;
	CreatedLODLevel->bEnabled = true;
	CreatedLODLevel->ConvertedModules = true;
	CreatedLODLevel->PeakActiveParticles = 0;

	// Determine where to place it...
	if (LODLevels.Num() == 0)
	{
		LODLevels.InsertZeroed(0, 1);
		LODLevels[0] = CreatedLODLevel;
		CreatedLODLevel->Level	= 0;
	}
	else
	{
		LODLevels.InsertZeroed(LODLevel, 1);
		LODLevels[LODLevel] = CreatedLODLevel;
		CreatedLODLevel->Level = LODLevel;
	}

	if (NextHighestLODLevel)
	{
		// Generate from the higher LOD level
		if (CreatedLODLevel->GenerateFromLODLevel(NextHighestLODLevel, 100.0, bGenerateModuleData) == false)
		{
			UE_LOG(LogParticles, Warning, TEXT("Failed to generate LOD level %d from level %d"), LODLevel, NextHighestLODLevel->Level);
		}
	}
	else
	{
		// Create the RequiredModule
		UParticleModuleRequired* RequiredModule = NewObject<UParticleModuleRequired>(GetOuter());
		check(RequiredModule);
		RequiredModule->SetToSensibleDefaults(this);
		CreatedLODLevel->RequiredModule	= RequiredModule;

		// The SpawnRate for the required module
		RequiredModule->bUseLocalSpace			= false;
		RequiredModule->bKillOnDeactivate		= false;
		RequiredModule->bKillOnCompleted		= false;
		RequiredModule->EmitterDuration			= 1.0f;
		RequiredModule->EmitterLoops			= 0;
		RequiredModule->ParticleBurstMethod		= EPBM_Instant;
#if WITH_EDITORONLY_DATA
		RequiredModule->ModuleEditorColor		= FColor::MakeRandomColor();
#endif // WITH_EDITORONLY_DATA
		RequiredModule->InterpolationMethod		= PSUVIM_None;
		RequiredModule->SubImages_Horizontal	= 1;
		RequiredModule->SubImages_Vertical		= 1;
		RequiredModule->bScaleUV				= false;
		RequiredModule->RandomImageTime			= 0.0f;
		RequiredModule->RandomImageChanges		= 0;
		RequiredModule->bEnabled				= true;

		RequiredModule->LODValidity = (1 << LODLevel);

		// There must be a spawn module as well...
		UParticleModuleSpawn* SpawnModule = NewObject<UParticleModuleSpawn>(GetOuter());
		check(SpawnModule);
		CreatedLODLevel->SpawnModule = SpawnModule;
		SpawnModule->LODValidity = (1 << LODLevel);
		UDistributionFloatConstant* ConstantSpawn	= Cast<UDistributionFloatConstant>(SpawnModule->Rate.Distribution);
		ConstantSpawn->Constant					= 10;
		ConstantSpawn->bIsDirty					= true;
		SpawnModule->BurstList.Empty();

		// Copy the TypeData module
		CreatedLODLevel->TypeDataModule			= NULL;
	}

	LevelIndex	= CreatedLODLevel->Level;

	MarkPackageDirty();

	return LevelIndex;
}


bool UParticleEmitter::IsLODLevelValid(int32 LODLevel)
{
	for (int32 LODIndex = 0; LODIndex < LODLevels.Num(); LODIndex++)
	{
		UParticleLODLevel* CheckLODLevel	= LODLevels[LODIndex];
		if (CheckLODLevel->Level == LODLevel)
		{
			return true;
		}
	}

	return false;
}


void UParticleEmitter::EditorUpdateCurrentLOD(FParticleEmitterInstance* Instance)
{
#if WITH_EDITORONLY_DATA
	UParticleLODLevel*	CurrentLODLevel	= NULL;
	UParticleLODLevel*	Higher			= NULL;

	int32 SetLODLevel = -1;
	if (Instance->Component && Instance->Component->Template)
	{
		int32 DesiredLODLevel = Instance->Component->Template->EditorLODSetting;
		if (GIsEditor && GEngine->bEnableEditorPSysRealtimeLOD)
		{
			DesiredLODLevel = Instance->Component->GetCurrentLODIndex();
		}

		for (int32 LevelIndex = 0; LevelIndex < LODLevels.Num(); LevelIndex++)
		{
			Higher	= LODLevels[LevelIndex];
			if (Higher && (Higher->Level == DesiredLODLevel))
			{
				SetLODLevel = LevelIndex;
				break;
			}
		}
	}

	if (SetLODLevel == -1)
	{
		SetLODLevel = 0;
	}
	Instance->SetCurrentLODIndex(SetLODLevel, false);
#endif // WITH_EDITORONLY_DATA
}



UParticleLODLevel* UParticleEmitter::GetLODLevel(int32 LODLevel)
{
	if (LODLevel >= LODLevels.Num())
	{
		return NULL;
	}

	return LODLevels[LODLevel];
}


bool UParticleEmitter::AutogenerateLowestLODLevel(bool bDuplicateHighest)
{
	// Didn't find it?
	if (LODLevels.Num() == 1)
	{
		// We need to generate it...
		LODLevels.InsertZeroed(1, 1);
		UParticleLODLevel* LODLevel = NewObject<UParticleLODLevel>(this);
		check(LODLevel);
		LODLevels[1]					= LODLevel;
		LODLevel->Level					= 1;
		LODLevel->ConvertedModules		= true;
		LODLevel->PeakActiveParticles	= 0;

		// Grab LODLevel 0 for creation
		UParticleLODLevel* SourceLODLevel	= LODLevels[0];

		LODLevel->bEnabled				= SourceLODLevel->bEnabled;

		float Percentage	= 10.0f;
		if (SourceLODLevel->TypeDataModule)
		{
			UParticleModuleTypeDataBeam2*	Beam2TD		= Cast<UParticleModuleTypeDataBeam2>(SourceLODLevel->TypeDataModule);

			if (Beam2TD)
			{
				// For now, don't support LOD on beams and trails
				Percentage	= 100.0f;
			}
		}

		if (bDuplicateHighest == true)
		{
			Percentage = 100.0f;
		}

		if (LODLevel->GenerateFromLODLevel(SourceLODLevel, Percentage) == false)
		{
			UE_LOG(LogParticles, Warning, TEXT("Failed to generate LOD level %d from LOD level 0"), 1);
			return false;
		}

		MarkPackageDirty();
		return true;
	}

	return true;
}


bool UParticleEmitter::CalculateMaxActiveParticleCount()
{
	int32	CurrMaxAPC = 0;

	int32 MaxCount = 0;
	
	for (int32 LODIndex = 0; LODIndex < LODLevels.Num(); LODIndex++)
	{
		UParticleLODLevel* LODLevel = LODLevels[LODIndex];
		if (LODLevel && LODLevel->bEnabled)
		{
			bool bForceMaxCount = false;
			// Check for beams or trails
			if ((LODLevel->Level == 0) && (LODLevel->TypeDataModule != NULL))
			{
				UParticleModuleTypeDataBeam2* BeamTD = Cast<UParticleModuleTypeDataBeam2>(LODLevel->TypeDataModule);
				if (BeamTD)
				{
					bForceMaxCount = true;
					MaxCount = BeamTD->MaxBeamCount + 2;
				}
			}

			int32 LODMaxAPC = LODLevel->CalculateMaxActiveParticleCount();
			if (bForceMaxCount == true)
			{
				LODLevel->PeakActiveParticles = MaxCount;
				LODMaxAPC = MaxCount;
			}

			if (LODMaxAPC > CurrMaxAPC)
			{
				if (LODIndex > 0)
				{
					// Check for a ridiculous difference in counts...
					if ((CurrMaxAPC > 0) && (LODMaxAPC / CurrMaxAPC) > 2)
					{
						//UE_LOG(LogParticles, Log, TEXT("MaxActiveParticleCount Discrepancy?\n\tLOD %2d, Emitter %16s"), LODIndex, *GetName());
					}
				}
				CurrMaxAPC = LODMaxAPC;
			}
		}
	}

#if WITH_EDITOR
	if ((GIsEditor == true) && (CurrMaxAPC > 500))
	{
		//@todo. Added an option to the emitter to disable this warning - for 
		// the RARE cases where it is really required to render that many.
		UE_LOG(LogParticles, Warning, TEXT("MaxCount = %4d for Emitter %s (%s)"),
			CurrMaxAPC, *(GetName()), GetOuter() ? *(GetOuter()->GetPathName()) : TEXT("????"));
	}
#endif
	return true;
}


void UParticleEmitter::GetParametersUtilized(TArray<FString>& ParticleSysParamList,
											 TArray<FString>& ParticleParameterList)
{
	// Clear the lists
	ParticleSysParamList.Empty();
	ParticleParameterList.Empty();

	TArray<UParticleModule*> ProcessedModules;
	ProcessedModules.Empty();

	for (int32 LODIndex = 0; LODIndex < LODLevels.Num(); LODIndex++)
	{
		UParticleLODLevel* LODLevel = LODLevels[LODIndex];
		if (LODLevel)
		{
			int32 FindIndex;
			// Grab that parameters from each module...
			check(LODLevel->RequiredModule);
			if (ProcessedModules.Find(LODLevel->RequiredModule, FindIndex) == false)
			{
				LODLevel->RequiredModule->GetParticleSysParamsUtilized(ParticleSysParamList);
				LODLevel->RequiredModule->GetParticleParametersUtilized(ParticleParameterList);
				ProcessedModules.AddUnique(LODLevel->RequiredModule);
			}

			check(LODLevel->SpawnModule);
			if (ProcessedModules.Find(LODLevel->SpawnModule, FindIndex) == false)
			{
				LODLevel->SpawnModule->GetParticleSysParamsUtilized(ParticleSysParamList);
				LODLevel->SpawnModule->GetParticleParametersUtilized(ParticleParameterList);
				ProcessedModules.AddUnique(LODLevel->SpawnModule);
			}

			if (LODLevel->TypeDataModule)
			{
				if (ProcessedModules.Find(LODLevel->TypeDataModule, FindIndex) == false)
				{
					LODLevel->TypeDataModule->GetParticleSysParamsUtilized(ParticleSysParamList);
					LODLevel->TypeDataModule->GetParticleParametersUtilized(ParticleParameterList);
					ProcessedModules.AddUnique(LODLevel->TypeDataModule);
				}
			}
			
			for (int32 ModuleIndex = 0; ModuleIndex < LODLevel->Modules.Num(); ModuleIndex++)
			{
				UParticleModule* Module = LODLevel->Modules[ModuleIndex];
				if (Module)
				{
					if (ProcessedModules.Find(Module, FindIndex) == false)
					{
						Module->GetParticleSysParamsUtilized(ParticleSysParamList);
						Module->GetParticleParametersUtilized(ParticleParameterList);
						ProcessedModules.AddUnique(Module);
					}
				}
			}
		}
	}
}


void UParticleEmitter::Build()
{
	const int32 LODCount = LODLevels.Num();
	if ( LODCount > 0 )
	{
		UParticleLODLevel* LODLevel = LODLevels[0];
		UParticleModuleTypeDataBase* TypeDataModule = (UParticleModuleTypeDataBase*)(LODLevel ? LODLevel->TypeDataModule : NULL);
		if ( TypeDataModule && TypeDataModule->RequiresBuild() )
		{
			FParticleEmitterBuildInfo EmitterBuildInfo;
			LODLevel->CompileModules( EmitterBuildInfo );
			TypeDataModule->Build( EmitterBuildInfo );
		}
	}
}

bool UParticleEmitter::HasAnyEnabledLODs()const
{
	for (UParticleLODLevel* LodLevel : LODLevels)
	{
		if (LodLevel && LodLevel->bEnabled)
		{
			return true;
		}
	}
	
	return false;
}

/*-----------------------------------------------------------------------------
	UParticleSpriteEmitter implementation.
-----------------------------------------------------------------------------*/
UParticleSpriteEmitter::UParticleSpriteEmitter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}


void UParticleSpriteEmitter::PostLoad()
{
	Super::PostLoad();

	// Postload the materials
	for (int32 LODIndex = 0; LODIndex < LODLevels.Num(); LODIndex++)
	{
		UParticleLODLevel* LODLevel = LODLevels[LODIndex];
		if (LODLevel)
		{
			UParticleModuleRequired* RequiredModule = LODLevel->RequiredModule;
			if (RequiredModule)
			{
				if (RequiredModule->Material)
				{
					RequiredModule->Material->ConditionalPostLoad();
				}
			}
		}
	}
}

FParticleEmitterInstance* UParticleSpriteEmitter::CreateInstance(UParticleSystemComponent* InComponent)
{
	// If this emitter was cooked out or has no valid LOD levels don't create an instance for it.
	if ((bCookedOut == true) || (LODLevels.Num() == 0))
	{
		return NULL;
	}

	FParticleEmitterInstance* Instance = 0;

	UParticleLODLevel* LODLevel	= GetLODLevel(0);
	check(LODLevel);

	if (LODLevel->TypeDataModule)
	{
		//@todo. This will NOT work for trails/beams!
		UParticleModuleTypeDataBase* TypeData = CastChecked<UParticleModuleTypeDataBase>(LODLevel->TypeDataModule);
		if (TypeData)
		{
			Instance = TypeData->CreateInstance(this, InComponent);
		}
	}
	else
	{
		check(InComponent);
		Instance = new FParticleSpriteEmitterInstance();
		check(Instance);
		Instance->InitParameters(this, InComponent);
	}

	if (Instance)
	{
		Instance->CurrentLODLevelIndex	= 0;
		Instance->CurrentLODLevel		= LODLevels[Instance->CurrentLODLevelIndex];
		Instance->Init();
	}

	return Instance;
}

void UParticleSpriteEmitter::SetToSensibleDefaults()
{
#if WITH_EDITOR
	PreEditChange(NULL);
#endif // WITH_EDITOR

	UParticleLODLevel* LODLevel = LODLevels[0];

	// Spawn rate
	LODLevel->SpawnModule->LODValidity = 1;
	UDistributionFloatConstant* SpawnRateDist = Cast<UDistributionFloatConstant>(LODLevel->SpawnModule->Rate.Distribution);
	if (SpawnRateDist)
	{
		SpawnRateDist->Constant = 20.f;
	}

	// Create basic set of modules

	// Lifetime module
	UParticleModuleLifetime* LifetimeModule = NewObject<UParticleModuleLifetime>(GetOuter());
	UDistributionFloatUniform* LifetimeDist = Cast<UDistributionFloatUniform>(LifetimeModule->Lifetime.Distribution);
	if (LifetimeDist)
	{
		LifetimeDist->Min = 1.0f;
		LifetimeDist->Max = 1.0f;
		LifetimeDist->bIsDirty = true;
	}
	LifetimeModule->LODValidity = 1;
	LODLevel->Modules.Add(LifetimeModule);

	// Size module
	UParticleModuleSize* SizeModule = NewObject<UParticleModuleSize>(GetOuter());
	UDistributionVectorUniform* SizeDist = Cast<UDistributionVectorUniform>(SizeModule->StartSize.Distribution);
	if (SizeDist)
	{
		SizeDist->Min = FVector(25.f, 25.f, 25.f);
		SizeDist->Max = FVector(25.f, 25.f, 25.f);
		SizeDist->bIsDirty = true;
	}
	SizeModule->LODValidity = 1;
	LODLevel->Modules.Add(SizeModule);

	// Initial velocity module
	UParticleModuleVelocity* VelModule = NewObject<UParticleModuleVelocity>(GetOuter());
	UDistributionVectorUniform* VelDist = Cast<UDistributionVectorUniform>(VelModule->StartVelocity.Distribution);
	if (VelDist)
	{
		VelDist->Min = FVector(-10.f, -10.f, 50.f);
		VelDist->Max = FVector(10.f, 10.f, 100.f);
		VelDist->bIsDirty = true;
	}
	VelModule->LODValidity = 1;
	LODLevel->Modules.Add(VelModule);

	// Color over life module
	UParticleModuleColorOverLife* ColorModule = NewObject<UParticleModuleColorOverLife>(GetOuter());
	UDistributionVectorConstantCurve* ColorCurveDist = Cast<UDistributionVectorConstantCurve>(ColorModule->ColorOverLife.Distribution);
	if (ColorCurveDist)
	{
		// Add two points, one at time 0.0f and one at 1.0f
		for (int32 Key = 0; Key < 2; Key++)
		{
			int32	KeyIndex = ColorCurveDist->CreateNewKey(Key * 1.0f);
			for (int32 SubIndex = 0; SubIndex < 3; SubIndex++)
			{
				ColorCurveDist->SetKeyOut(SubIndex, KeyIndex, 1.0f);
			}
		}
		ColorCurveDist->bIsDirty = true;
	}
	ColorModule->AlphaOverLife.Distribution = NewObject<UDistributionFloatConstantCurve>(ColorModule);
	UDistributionFloatConstantCurve* AlphaCurveDist = Cast<UDistributionFloatConstantCurve>(ColorModule->AlphaOverLife.Distribution);
	if (AlphaCurveDist)
	{
		// Add two points, one at time 0.0f and one at 1.0f
		for (int32 Key = 0; Key < 2; Key++)
		{
			int32	KeyIndex = AlphaCurveDist->CreateNewKey(Key * 1.0f);
			if (Key == 0)
			{
				AlphaCurveDist->SetKeyOut(0, KeyIndex, 1.0f);
			}
			else
			{
				AlphaCurveDist->SetKeyOut(0, KeyIndex, 0.0f);
			}
		}
		AlphaCurveDist->bIsDirty = true;
	}
	ColorModule->LODValidity = 1;
	LODLevel->Modules.Add(ColorModule);

#if WITH_EDITOR
	PostEditChange();
#endif // WITH_EDITOR
}

/*-----------------------------------------------------------------------------
	UParticleSystem implementation.
-----------------------------------------------------------------------------*/
UParticleSystem::UParticleSystem(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	ThumbnailDistance = 200.0;
	ThumbnailWarmup = 1.0;
#endif // WITH_EDITORONLY_DATA
	UpdateTime_FPS = 60.0f;
	UpdateTime_Delta = 1.0f/60.0f;
	WarmupTime = 0.0f;
	WarmupTickRate = 0.0f;
#if WITH_EDITORONLY_DATA
	EditorLODSetting = 0;
#endif // WITH_EDITORONLY_DATA
	FixedRelativeBoundingBox.Min = FVector(-1.0f, -1.0f, -1.0f);

	FixedRelativeBoundingBox.Max = FVector(1.0f, 1.0f, 1.0f);

	LODMethod = PARTICLESYSTEMLODMETHOD_Automatic;
	LODDistanceCheckTime = 0.25f;
	bRegenerateLODDuplicate = false;
	ThumbnailImageOutOfDate = true;
#if WITH_EDITORONLY_DATA
	FloorMesh = TEXT("/Engine/EditorMeshes/AnimTreeEd_PreviewFloor.AnimTreeEd_PreviewFloor");
	FloorPosition = FVector(0.0f, 0.0f, 0.0f);
	FloorRotation = FRotator(0.0f, 0.0f, 0.0f);
	FloorScale = 1.0f;
	FloorScale3D = FVector(1.0f, 1.0f, 1.0f);
#endif // WITH_EDITORONLY_DATA

	MacroUVPosition = FVector(0.0f, 0.0f, 0.0f);

	MacroUVRadius = 200.0f;
}




ParticleSystemLODMethod UParticleSystem::GetCurrentLODMethod()
{
	return ParticleSystemLODMethod(LODMethod);
}


int32 UParticleSystem::GetLODLevelCount()
{
	return LODDistances.Num();
}


float UParticleSystem::GetLODDistance(int32 LODLevelIndex)
{
	if (LODLevelIndex >= LODDistances.Num())
	{
		return -1.0f;
	}

	return LODDistances[LODLevelIndex];
}

void UParticleSystem::SetCurrentLODMethod(ParticleSystemLODMethod InMethod)
{
	LODMethod = InMethod;
}


bool UParticleSystem::SetLODDistance(int32 LODLevelIndex, float InDistance)
{
	if (LODLevelIndex >= LODDistances.Num())
	{
		return false;
	}

	LODDistances[LODLevelIndex] = InDistance;

	return true;
}

#if WITH_EDITOR
void UParticleSystem::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UpdateTime_Delta = 1.0f / UpdateTime_FPS;

	//If the property is NULL then we don't really know what's happened. 
	//Could well be a module change, requiring all instances to be destroyed and recreated.
	bool bEmptyInstances = PropertyChangedEvent.Property == NULL;
	for (TObjectIterator<UParticleSystemComponent> It;It;++It)
	{
		if (It->Template == this)
		{
			It->UpdateInstances(bEmptyInstances);
		}
	}

	//cap the WarmupTickRate to realistic values
	if (WarmupTickRate <= 0)
	{
		WarmupTickRate = 0;
	}
	else if (WarmupTickRate > WarmupTime)
	{
		WarmupTickRate = WarmupTime;
	}

	ThumbnailImageOutOfDate = true;

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR

void UParticleSystem::PreSave()
{
	Super::PreSave();
#if WITH_EDITORONLY_DATA
	// Ensure that soloing is undone...
	int32 NumEmitters = FMath::Min(Emitters.Num(),SoloTracking.Num());
	for (int32 EmitterIdx = 0; EmitterIdx < NumEmitters; EmitterIdx++)
	{
		UParticleEmitter* Emitter = Emitters[EmitterIdx];
		Emitter->bIsSoloing = false;
		FLODSoloTrack& SoloTrack = SoloTracking[EmitterIdx];
		int32 NumLODs = FMath::Min(Emitter->LODLevels.Num(),SoloTrack.SoloEnableSetting.Num());
		for (int32 LODIdx = 0; LODIdx < NumLODs; LODIdx++)
		{
			UParticleLODLevel* LODLevel = Emitter->LODLevels[LODIdx];
			{
				// Restore the enabled settings - ie turn off soloing...
				LODLevel->bEnabled = SoloTrack.SoloEnableSetting[LODIdx];
			}
		}
	}
#endif // WITH_EDITORONLY_DATA
}

void UParticleSystem::PostLoad()
{
	Super::PostLoad();

	// Remove any old emitters
	bHasPhysics = false;
	for (int32 i = Emitters.Num() - 1; i >= 0; i--)
	{
		UParticleEmitter* Emitter = Emitters[i];
		if (Emitter == NULL)
		{
			// Empty emitter slots are ok with cooked content.
			if( !FPlatformProperties::RequiresCookedData() )
			{
				UE_LOG(LogParticles, Warning, TEXT("ParticleSystem contains empty emitter slots - %s"), *GetFullName());
			}
			continue;
		}

		Emitter->ConditionalPostLoad();

		if (Emitter->IsA(UParticleSpriteEmitter::StaticClass()))
		{
			UParticleSpriteEmitter* SpriteEmitter = Cast<UParticleSpriteEmitter>(Emitter);

			if (SpriteEmitter->bCookedOut == false)
			{
				UParticleLODLevel* LODLevel = SpriteEmitter->LODLevels[0];
				check(LODLevel);

				LODLevel->ConditionalPostLoad();
				
				//@todo. Move this into the editor and serialize?
				for (int32 LODIndex = 0; (LODIndex < Emitter->LODLevels.Num()) && (bHasPhysics == false); LODIndex++)
				{
					//@todo. This is a temporary fix for emitters that apply physics.
					// Check for collision modules with bApplyPhysics set to true
					UParticleLODLevel*  ParticleLODLevel = Emitter->LODLevels[LODIndex];
					if (ParticleLODLevel)
					{
						for (int32 ModuleIndex = 0; ModuleIndex < LODLevel->Modules.Num(); ModuleIndex++)
						{
							UParticleModuleCollision* CollisionModule = Cast<UParticleModuleCollision>(ParticleLODLevel->Modules[ModuleIndex]);
							if (CollisionModule)
							{
								if (CollisionModule->bApplyPhysics == true)
								{
									bHasPhysics = true;
									break;
								}
							}
						}
					}
				}
			}
		}
	}

	if (LODSettings.Num() == 0)
	{
		if (Emitters.Num() > 0)
		{
			UParticleEmitter* Emitter = Emitters[0];
			if (Emitter)
			{
				LODSettings.AddUninitialized(Emitter->LODLevels.Num());
				for (int32 LODIndex = 0; LODIndex < LODSettings.Num(); LODIndex++)
				{
					LODSettings[LODIndex] = FParticleSystemLOD::CreateParticleSystemLOD();
				}
			}
		}
		else
		{
			LODSettings.AddUninitialized();
			LODSettings[0] = FParticleSystemLOD::CreateParticleSystemLOD();
		}
	}

	// Add default LOD Distances
	if( LODDistances.Num() == 0 && Emitters.Num() > 0 )
	{
		UParticleEmitter* Emitter = Emitters[0];
		if (Emitter)
		{
			LODDistances.AddUninitialized(Emitter->LODLevels.Num());
			for (int32 LODIndex = 0; LODIndex < LODDistances.Num(); LODIndex++)
			{
				LODDistances[LODIndex] = LODIndex * 2500.0f;
			}
		}
	}

#if WITH_EDITOR
	// Due to there still being some ways that LODLevel counts get mismatched,
	// when loading in the editor LOD levels will always be checked and fixed
	// up... This can be removed once all the edge cases that lead to the
	// problem are found and fixed.
	if (GIsEditor)
	{
		// Fix the LOD distance array and mismatched lod levels
		int32 LODCount_0 = -1;
		for (int32 EmitterIndex = 0; EmitterIndex < Emitters.Num(); EmitterIndex++)
		{
			UParticleEmitter* Emitter  = Emitters[EmitterIndex];
			if (Emitter)
			{
				if (LODCount_0 == -1)
				{
					LODCount_0 = Emitter->LODLevels.Num();
				}
				else
				{
					int32 EmitterLODCount = Emitter->LODLevels.Num();
					if (EmitterLODCount != LODCount_0)
					{
						UE_LOG(LogParticles, Warning, TEXT("Emitter %d has mismatched LOD level count - expected %d, found %d. PS = %s"),
							EmitterIndex, LODCount_0, EmitterLODCount, *GetPathName());
						UE_LOG(LogParticles, Warning, TEXT("Fixing up now... Package = %s"), *(GetOutermost()->GetPathName()));

						if (EmitterLODCount > LODCount_0)
						{
							Emitter->LODLevels.RemoveAt(LODCount_0, EmitterLODCount - LODCount_0);
						}
						else
						{
							for (int32 NewLODIndex = EmitterLODCount; NewLODIndex < LODCount_0; NewLODIndex++)
							{
								if (Emitter->CreateLODLevel(NewLODIndex) != NewLODIndex)
								{
									UE_LOG(LogParticles, Warning, TEXT("Failed to add LOD level %s"), NewLODIndex);
								}
							}
						}
					}
				}
			}
		}

		if (LODCount_0 > 0)
		{
			if (LODDistances.Num() < LODCount_0)
			{
				for (int32 DistIndex = LODDistances.Num(); DistIndex < LODCount_0; DistIndex++)
				{
					float Distance = DistIndex * 2500.0f;
					LODDistances.Add(Distance);
				}
			}
			else
			if (LODDistances.Num() > LODCount_0)
			{
				LODDistances.RemoveAt(LODCount_0, LODDistances.Num() - LODCount_0);
			}
		}
		else
		{
			LODDistances.Empty();
		}

		if (LODCount_0 > 0)
		{
			if (LODSettings.Num() < LODCount_0)
			{
				for (int32 DistIndex = LODSettings.Num(); DistIndex < LODCount_0; DistIndex++)
				{
					LODSettings.Add(FParticleSystemLOD::CreateParticleSystemLOD());
				}
			}
			else
				if (LODSettings.Num() > LODCount_0)
				{
					LODSettings.RemoveAt(LODCount_0, LODSettings.Num() - LODCount_0);
				}
		}
		else
		{
			LODSettings.Empty();
		}
	}
#endif

#if WITH_EDITORONLY_DATA
	// Reset cascade's UI LOD setting to 0.
	EditorLODSetting = 0;
#endif // WITH_EDITORONLY_DATA

	FixedRelativeBoundingBox.IsValid = true;

	// Set up the SoloTracking...
	SetupSoloing();
}


SIZE_T UParticleSystem::GetResourceSize(EResourceSizeMode::Type Mode)
{
	return 0;
}

void UParticleSystem::UpdateColorModuleClampAlpha(UParticleModuleColorBase* ColorModule)
{
	if (ColorModule)
	{
		TArray<const FCurveEdEntry*> CurveEntries;
		ColorModule->RemoveModuleCurvesFromEditor(CurveEdSetup);
		ColorModule->AddModuleCurvesToEditor(CurveEdSetup, CurveEntries);
	}
}

void UParticleSystem::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	OutTags.Add( FAssetRegistryTag("HasGPUEmitter", HasGPUEmitter() ? TEXT("True") : TEXT("False"), FAssetRegistryTag::TT_Alphabetical) );
	Super::GetAssetRegistryTags(OutTags);
}


bool UParticleSystem::CalculateMaxActiveParticleCounts()
{
	bool bSuccess = true;

	for (int32 EmitterIndex = 0; EmitterIndex < Emitters.Num(); EmitterIndex++)
	{
		UParticleEmitter* Emitter = Emitters[EmitterIndex];
		if (Emitter)
		{
			if (Emitter->CalculateMaxActiveParticleCount() == false)
			{
				bSuccess = false;
			}
		}
	}

	return bSuccess;
}


void UParticleSystem::GetParametersUtilized(TArray<TArray<FString> >& ParticleSysParamList,
											TArray<TArray<FString> >& ParticleParameterList)
{
	ParticleSysParamList.Empty();
	ParticleParameterList.Empty();

	for (int32 EmitterIndex = 0; EmitterIndex < Emitters.Num(); EmitterIndex++)
	{
		int32 CheckIndex;
		CheckIndex = ParticleSysParamList.AddZeroed();
		check(CheckIndex == EmitterIndex);
		CheckIndex = ParticleParameterList.AddZeroed();
		check(CheckIndex == EmitterIndex);

		UParticleEmitter* Emitter = Emitters[EmitterIndex];
		if (Emitter)
		{
			Emitter->GetParametersUtilized(
				ParticleSysParamList[EmitterIndex],
				ParticleParameterList[EmitterIndex]);
		}
	}
}


void UParticleSystem::SetupSoloing()
{
#if WITH_EDITOR
	if (GIsEditor)
	{
		if (Emitters.Num())
		{
			// Store the settings of bEnabled for each LODLevel in each emitter
			UParticleEmitter* ZeroEmitter = NULL;
			for (int32 EmitterIdx = 0; EmitterIdx < Emitters.Num(); EmitterIdx++)
			{
				UParticleEmitter* Emitter = Emitters[EmitterIdx];
				if ((Emitter != NULL) && (ZeroEmitter == NULL))
				{
					ZeroEmitter = Emitter;
					break;
				}
			}
			check(ZeroEmitter != NULL);

			SoloTracking.Empty(Emitters.Num());
			SoloTracking.AddZeroed(Emitters.Num());
			for (int32 SoloIdx = 0; SoloIdx < SoloTracking.Num(); SoloIdx++)
			{
				FLODSoloTrack& SoloTrack = SoloTracking[SoloIdx];
				SoloTrack.SoloEnableSetting.Empty(ZeroEmitter->LODLevels.Num());
				SoloTrack.SoloEnableSetting.AddZeroed(ZeroEmitter->LODLevels.Num());
			}

			for (int32 EmitterIdx = 0; EmitterIdx < Emitters.Num(); EmitterIdx++)
			{
				UParticleEmitter* Emitter = Emitters[EmitterIdx];
				if (Emitter != NULL)
				{
					FLODSoloTrack& SoloTrack = SoloTracking[EmitterIdx];
					for (int32 LODIdx = 0; LODIdx < Emitter->LODLevels.Num(); LODIdx++)
					{
						UParticleLODLevel* LODLevel = Emitter->LODLevels[LODIdx];
						check(LODLevel);
						SoloTrack.SoloEnableSetting[LODIdx] = LODLevel->bEnabled;
					}
				}
			}
		}
	}
#endif
}


bool UParticleSystem::ToggleSoloing(class UParticleEmitter* InEmitter)
{
	bool bSoloingReturn = false;
	if (InEmitter != NULL)
	{
		bool bOtherEmitterIsSoloing = false;
		// Set the given one
		int32 SelectedIndex = -1;
		for (int32 EmitterIdx = 0; EmitterIdx < Emitters.Num(); EmitterIdx++)
		{
			UParticleEmitter* Emitter = Emitters[EmitterIdx];
			check(Emitter != NULL);
			if (Emitter == InEmitter)
			{
				SelectedIndex = EmitterIdx;
			}
			else
			{
				if (Emitter->bIsSoloing == true)
				{
					bOtherEmitterIsSoloing = true;
					bSoloingReturn = true;
				}
			}
		}

		if (SelectedIndex != -1)
		{
			InEmitter->bIsSoloing = !InEmitter->bIsSoloing;
			for (int32 EmitterIdx = 0; EmitterIdx < Emitters.Num(); EmitterIdx++)
			{
				UParticleEmitter* Emitter = Emitters[EmitterIdx];
				FLODSoloTrack& SoloTrack = SoloTracking[EmitterIdx];
				if (EmitterIdx == SelectedIndex)
				{
					for (int32 LODIdx = 0; LODIdx < InEmitter->LODLevels.Num(); LODIdx++)
					{
						UParticleLODLevel* LODLevel = InEmitter->LODLevels[LODIdx];
						if (InEmitter->bIsSoloing == false)
						{
							if (bOtherEmitterIsSoloing == false)
							{
								// Restore the enabled settings - ie turn off soloing...
								LODLevel->bEnabled = SoloTrack.SoloEnableSetting[LODIdx];
							}
							else
							{
								// Disable the emitter
								LODLevel->bEnabled = false;
							}
						}
						else 
						if (bOtherEmitterIsSoloing == true)
						{
							// Need to restore old settings of this emitter as it is now soloing
							LODLevel->bEnabled = SoloTrack.SoloEnableSetting[LODIdx];
						}
					}
				}
				else
				{
					// Restore all other emitters if this disables soloing...
					if ((InEmitter->bIsSoloing == false) && (bOtherEmitterIsSoloing == false))
					{
						for (int32 LODIdx = 0; LODIdx < Emitter->LODLevels.Num(); LODIdx++)
						{
							UParticleLODLevel* LODLevel = Emitter->LODLevels[LODIdx];
							// Restore the enabled settings - ie turn off soloing...
							LODLevel->bEnabled = SoloTrack.SoloEnableSetting[LODIdx];
						}
					}
					else
					{
						if (Emitter->bIsSoloing == false)
						{
							for (int32 LODIdx = 0; LODIdx < Emitter->LODLevels.Num(); LODIdx++)
							{
								UParticleLODLevel* LODLevel = Emitter->LODLevels[LODIdx];
								// Disable the emitter
								LODLevel->bEnabled = false;
							}
						}
					}
				}
			}
		}

		// We checked the other emitters above...
		// Make sure we catch the case of the first one toggled to true!
		if (InEmitter->bIsSoloing == true)
		{
			bSoloingReturn = true;
		}
	}

	return bSoloingReturn;
}


bool UParticleSystem::TurnOffSoloing()
{
	for (int32 EmitterIdx = 0; EmitterIdx < Emitters.Num(); EmitterIdx++)
	{
		UParticleEmitter* Emitter = Emitters[EmitterIdx];
		if (Emitter != NULL)
		{
			FLODSoloTrack& SoloTrack = SoloTracking[EmitterIdx];
			for (int32 LODIdx = 0; LODIdx < Emitter->LODLevels.Num(); LODIdx++)
			{
				UParticleLODLevel* LODLevel = Emitter->LODLevels[LODIdx];
				if (LODLevel != NULL)
				{
					// Restore the enabled settings - ie turn off soloing...
					LODLevel->bEnabled = SoloTrack.SoloEnableSetting[LODIdx];
				}
			}
			Emitter->bIsSoloing = false;
		}
	}

	return true;
}


void UParticleSystem::SetupLODValidity()
{
	for (int32 EmitterIdx = 0; EmitterIdx < Emitters.Num(); EmitterIdx++)
	{
		UParticleEmitter* Emitter = Emitters[EmitterIdx];
		if (Emitter != NULL)
		{
			for (int32 Pass = 0; Pass < 2; Pass++)
			{
				for (int32 LODIdx = 0; LODIdx < Emitter->LODLevels.Num(); LODIdx++)
				{
					UParticleLODLevel* LODLevel = Emitter->LODLevels[LODIdx];
					if (LODLevel != NULL)
					{
						for (int32 ModuleIdx = -3; ModuleIdx < LODLevel->Modules.Num(); ModuleIdx++)
						{
							int32 ModuleFetchIdx;
							switch (ModuleIdx)
							{
							case -3:	ModuleFetchIdx = INDEX_REQUIREDMODULE;	break;
							case -2:	ModuleFetchIdx = INDEX_SPAWNMODULE;		break;
							case -1:	ModuleFetchIdx = INDEX_TYPEDATAMODULE;	break;
							default:	ModuleFetchIdx = ModuleIdx;				break;
							}

							UParticleModule* Module = LODLevel->GetModuleAtIndex(ModuleFetchIdx);
							if (Module != NULL)
							{
								// On pass 1, clear the LODValidity flags
								// On pass 2, set it
								if (Pass == 0)
								{
									Module->LODValidity = 0;
								}
								else
								{
									Module->LODValidity |= (1 << LODIdx);
								}
							}
						}
					}
				}
			}
		}
	}
}

#if WITH_EDITOR

bool UParticleSystem::RemoveAllDuplicateModules(bool bInMarkForCooker, TMap<UObject*,bool>* OutRemovedModules)
{
	// Generate a map of module classes used to instances of those modules 
	TMap<UClass*,TMap<UParticleModule*,int32> > ClassToModulesMap;
	for (int32 EmitterIdx = 0; EmitterIdx < Emitters.Num(); EmitterIdx++)
	{
		UParticleEmitter* Emitter = Emitters[EmitterIdx];
		if (Emitter != NULL)
		{
			if (Emitter->bCookedOut == false)
			{
				for (int32 LODIdx = 0; LODIdx < Emitter->LODLevels.Num(); LODIdx++)
				{
					UParticleLODLevel* LODLevel = Emitter->LODLevels[LODIdx];
					if (LODLevel != NULL)
					{
						for (int32 ModuleIdx = -1; ModuleIdx < LODLevel->Modules.Num(); ModuleIdx++)
						{
							UParticleModule* Module = NULL;
							if (ModuleIdx == -1)
							{
								Module = LODLevel->SpawnModule;
							}
							else
							{
								Module = LODLevel->Modules[ModuleIdx];
							}
							if (Module != NULL)
							{
								TMap<UParticleModule*,int32>* ModuleList = ClassToModulesMap.Find(Module->GetClass());
								if (ModuleList == NULL)
								{
									TMap<UParticleModule*,int32> TempModuleList;
									ClassToModulesMap.Add(Module->GetClass(), TempModuleList);
									ModuleList = ClassToModulesMap.Find(Module->GetClass());
								}
								check(ModuleList);
								int32* ModuleCount = ModuleList->Find(Module);
								if (ModuleCount == NULL)
								{
									int32 TempModuleCount = 0;
									ModuleList->Add(Module, TempModuleCount);
									ModuleCount = ModuleList->Find(Module);
								}
								check(ModuleCount);
								(*ModuleCount)++;
							}
						}
					}
				}
			}
		}
	}

	// Now we have a list of module classes and the modules they contain...
	// Find modules of the same class that have the exact same settings.
	TMap<UParticleModule*, TArray<UParticleModule*> > DuplicateModules;
	TMap<UParticleModule*,bool> FoundAsADupeModules;
	TMap<UParticleModule*, UParticleModule*> ReplaceModuleMap;
	bool bRemoveDuplicates = true;
	for (TMap<UClass*,TMap<UParticleModule*,int32> >::TIterator ModClassIt(ClassToModulesMap); ModClassIt; ++ModClassIt)
	{
		UClass* ModuleClass = ModClassIt.Key();
		TMap<UParticleModule*,int32>& ModuleMap = ModClassIt.Value();
		if (ModuleMap.Num() > 1)
		{
			// There is more than one of this module, so see if there are dupes...
			TArray<UParticleModule*> ModuleArray;
			for (TMap<UParticleModule*,int32>::TIterator ModuleIt(ModuleMap); ModuleIt; ++ModuleIt)
			{
				ModuleArray.Add(ModuleIt.Key());
			}

			// For each module, see if it it a duplicate of another
			for (int32 ModuleIdx = 0; ModuleIdx < ModuleArray.Num(); ModuleIdx++)
			{
				UParticleModule* SourceModule = ModuleArray[ModuleIdx];
				if (FoundAsADupeModules.Find(SourceModule) == NULL)
				{
					for (int32 InnerModuleIdx = ModuleIdx + 1; InnerModuleIdx < ModuleArray.Num(); InnerModuleIdx++)
					{
						UParticleModule* CheckModule = ModuleArray[InnerModuleIdx];
						if (FoundAsADupeModules.Find(CheckModule) == NULL)
						{
							bool bIsDifferent = false;
							static const FName CascadeCategory(TEXT("Cascade"));
							// Copy non component properties from the old actor to the new actor
							for (UProperty* Property = ModuleClass->PropertyLink; Property != NULL; Property = Property->PropertyLinkNext)
							{
								bool bIsTransient = (Property->PropertyFlags & CPF_Transient) != 0;
								bool bIsEditorOnly = (Property->PropertyFlags & CPF_EditorOnly) != 0;
								bool bIsCascade = (FObjectEditorUtils::GetCategoryFName(Property) == CascadeCategory);
								// Ignore 'Cascade' category, transient, native and EditorOnly properties...
								if (!bIsTransient && !bIsEditorOnly && !bIsCascade)
								{
									for( int32 iProp=0; iProp<Property->ArrayDim; iProp++ )
									{
										bool bIsIdentical = Property->Identical_InContainer(SourceModule, CheckModule, iProp, PPF_DeepComparison);
										if (bIsIdentical == false)
										{
											bIsDifferent = true;
											break;
										}
									}
								}
							}

							if (bIsDifferent == false)
							{
								TArray<UParticleModule*>* DupedModules = DuplicateModules.Find(SourceModule);
								if (DupedModules == NULL)
								{
									TArray<UParticleModule*> TempDupedModules;
									DuplicateModules.Add(SourceModule, TempDupedModules);
									DupedModules = DuplicateModules.Find(SourceModule);
								}
								check(DupedModules);
								if (ReplaceModuleMap.Find(CheckModule) == NULL)
								{
									ReplaceModuleMap.Add(CheckModule, SourceModule);
								}
								else
								{
									UE_LOG(LogParticles, Error, TEXT("Module already in replacement map - ABORTING CONVERSION!!!!"));
									bRemoveDuplicates = false;
								}
								DupedModules->AddUnique(CheckModule);
								FoundAsADupeModules.Add(CheckModule, true);
							}
						}
					}
				}
			}
		}
	}

	// If not errors were found, and there are duplicates, remove them...
	if (bRemoveDuplicates && (ReplaceModuleMap.Num() > 0))
	{
		TArray<UParticleModule*> RemovedModules;
		for (int32 EmitterIdx = 0; EmitterIdx < Emitters.Num(); EmitterIdx++)
		{
			UParticleEmitter* Emitter = Emitters[EmitterIdx];
			if (Emitter != NULL)
			{
				if (Emitter->bCookedOut == false)
				{
					for (int32 LODIdx = 0; LODIdx < Emitter->LODLevels.Num(); LODIdx++)
					{
						UParticleLODLevel* LODLevel = Emitter->LODLevels[LODIdx];
						if (LODLevel != NULL)
						{
							for (int32 ModuleIdx = -1; ModuleIdx < LODLevel->Modules.Num(); ModuleIdx++)
							{
								UParticleModule* Module = NULL;
								if (ModuleIdx == -1)
								{
									Module = LODLevel->SpawnModule;
								}
								else
								{
									Module = LODLevel->Modules[ModuleIdx];
								}
								if (Module != NULL)
								{
									UParticleModule** ReplacementModule = ReplaceModuleMap.Find(Module);
									if (ReplacementModule != NULL)
									{
										UParticleModule* ReplaceMod = *ReplacementModule;
										if (ModuleIdx == -1)
										{
											LODLevel->SpawnModule = CastChecked<UParticleModuleSpawn>(ReplaceMod);
										}
										else
										{
											LODLevel->Modules[ModuleIdx] = ReplaceMod;
										}

										if (bInMarkForCooker == true)
										{
											RemovedModules.AddUnique(Module);
										}
									}
								}
							}
						}
					}
				}
			}
		}

		if (bInMarkForCooker == true)
		{
			for (int32 MarkIdx = 0; MarkIdx < RemovedModules.Num(); MarkIdx++)
			{
				UParticleModule* RemovedModule = RemovedModules[MarkIdx];
				RemovedModule->SetFlags(RF_Transient);
				if (OutRemovedModules != NULL)
				{
					OutRemovedModules->Add(RemovedModule, true);
				}
			}
		}

		// Update the list of modules in each emitter
		UpdateAllModuleLists();
	}

	return true;
}


void UParticleSystem::UpdateAllModuleLists()
{
	for (int32 EmitterIdx = 0; EmitterIdx < Emitters.Num(); EmitterIdx++)
	{
		UParticleEmitter* Emitter = Emitters[EmitterIdx];
		if (Emitter != NULL)
		{
			for (int32 LODIdx = 0; LODIdx < Emitter->LODLevels.Num(); LODIdx++)
			{
				UParticleLODLevel* LODLevel = Emitter->LODLevels[LODIdx];
				if (LODLevel != NULL)
				{
					LODLevel->UpdateModuleLists();
				}
			}
		}
	}
}
#endif


void UParticleSystem::BuildEmitters()
{
	const int32 EmitterCount = Emitters.Num();
	for ( int32 EmitterIndex = 0; EmitterIndex < EmitterCount; ++EmitterIndex )
	{
		UParticleEmitter* Emitter = Emitters[EmitterIndex];
		if ( Emitter )
		{
			Emitter->Build();
		}
	}
}

void UParticleSystem::ComputeCanTickInAnyThread()
{
	check(!bIsElligibleForAsyncTickComputed);
	bIsElligibleForAsyncTickComputed = true;

	bIsElligibleForAsyncTick = true; // assume everything is async
	int32 EmitterIndex;
	for (EmitterIndex = 0; EmitterIndex < Emitters.Num(); EmitterIndex++)
	{
		UParticleEmitter* Emitter = Emitters[EmitterIndex];
		if (Emitter)
		{
			for (int32 LevelIndex = 0; LevelIndex < Emitter->LODLevels.Num(); LevelIndex++)
			{
				UParticleLODLevel* LODLevel	= Emitter->LODLevels[LevelIndex];
				if (LODLevel)
				{
					for (int32 ModuleIndex = 0; ModuleIndex < LODLevel->Modules.Num(); ModuleIndex++)
					{
						UParticleModule* Module	= LODLevel->Modules[ModuleIndex];
						if (Module && !Module->CanTickInAnyThread())
						{
							bIsElligibleForAsyncTick = false;
							return;
						}
					}
				}
			}

		}
	}
}

bool UParticleSystem::ContainsEmitterType(UClass* TypeData)
{
	for ( int32 EmitterIndex = 0; EmitterIndex < Emitters.Num(); ++EmitterIndex)
	{
		UParticleEmitter* Emitter = Emitters[EmitterIndex];
		if (Emitter)
		{
			UParticleLODLevel* LODLevel = Emitter->LODLevels[0];
			if (LODLevel && LODLevel->TypeDataModule && LODLevel->TypeDataModule->IsA(TypeData))
			{
				return true;
			}
		}
	}
	
	return false;
}

bool UParticleSystem::HasGPUEmitter() const
{
	for (int32 EmitterIndex = 0; EmitterIndex < Emitters.Num(); ++EmitterIndex)
	{
		// We can just check for the GPU type data at the highest LOD.
		UParticleLODLevel* LODLevel = Emitters[EmitterIndex]->LODLevels[0];
		if( LODLevel )
		{
			UParticleModule* TypeDataModule = LODLevel->TypeDataModule;
			if( TypeDataModule && TypeDataModule->IsA(UParticleModuleTypeDataGpu::StaticClass()) )
			{
				return true;
			}
		}
	}
	return false;
}

UParticleSystemComponent::UParticleSystemComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), FXSystem(NULL), ReleaseResourcesFence(NULL)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_DuringPhysics;
	PrimaryComponentTick.bAllowTickOnDedicatedServer = false;
	bTickInEditor = true;
	MaxTimeBeforeForceUpdateTransform = 5.0f;
	bAutoActivate = true;
	bResetOnDetach = false;
	OldPosition = FVector(0.0f, 0.0f, 0.0f);

	PartSysVelocity = FVector(0.0f, 0.0f, 0.0f);

	WarmupTime = 0.0f;
	SecondsBeforeInactive = 1.0f;
	bIsTransformDirty = false;
	bSkipUpdateDynamicDataDuringTick = false;
	bIsViewRelevanceDirty = true;
	CustomTimeDilation = 1.0f;
	bAllowConcurrentTick = true;
	bWasActive = false;
#if WITH_EDITORONLY_DATA
	EditorDetailMode = -1;
#endif // WITH_EDITORONLY_DATA
	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	bGenerateOverlapEvents = false;

	bCastVolumetricTranslucentShadow = true;

	// Disable receiving decals by default.
	bReceivesDecals = false;
}

#if WITH_EDITOR
void UParticleSystemComponent::CheckForErrors()
{
	ForceAsyncWorkCompletion(ENSURE_AND_STALL);
	for (int32 IPIndex = 0; IPIndex < InstanceParameters.Num(); IPIndex++)
	{
		FParticleSysParam& Param = InstanceParameters[IPIndex];
		if (Param.ParamType == PSPT_Actor)
		{
			if (Param.Actor == NULL)
			{
				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("InstanceParamIndex"), IPIndex);
				Arguments.Add(TEXT("PathName"), FText::FromString(GetPathName()));
				FMessageLog("MapCheck").Warning()
					->AddToken(FUObjectToken::Create(this))
					->AddToken(FTextToken::Create(FText::Format( LOCTEXT( "MapCheck_Message_PSysCompErrorEmptyActorRef", "PSysComp has an empty parameter actor reference at index {InstanceParamIndex} ({PathName})" ), Arguments ) ))
					->AddToken(FMapErrorToken::Create(FMapErrors::PSysCompErrorEmptyActorRef));
			}
		}
		else
		if (Param.ParamType == PSPT_Material)
		{
			if (Param.Material == NULL)
			{
				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("InstanceParamIndex"), IPIndex);
				Arguments.Add(TEXT("PathName"), FText::FromString(GetPathName()));
				FMessageLog("MapCheck").Warning()
					->AddToken(FUObjectToken::Create(this))
					->AddToken(FTextToken::Create(FText::Format( LOCTEXT( "MapCheck_Message_PSysCompErrorEmptyMaterialRef", "PSysComp has an empty parameter material reference at index {InstanceParamIndex} ({PathName})" ), Arguments ) ))
					->AddToken(FMapErrorToken::Create(FMapErrors::PSysCompErrorEmptyMaterialRef));
			}
		}
	}
}
#endif

void UParticleSystemComponent::PostLoad()
{
	ForceAsyncWorkCompletion(ENSURE_AND_STALL);
	Super::PostLoad();

	if (Template)
	{
		Template->ConditionalPostLoad();
	}
	bIsViewRelevanceDirty = true;
}

void UParticleSystemComponent::Serialize( FArchive& Ar )
{
	ForceAsyncWorkCompletion(ENSURE_AND_STALL);
	Super::Serialize( Ar );
	
	// Take instance particle count/ size into account.
	for (int32 InstanceIndex = 0; InstanceIndex < EmitterInstances.Num(); InstanceIndex++)
	{
		FParticleEmitterInstance* EmitterInstance = EmitterInstances[InstanceIndex];
		if( EmitterInstance != NULL )
		{
			int32 Num, Max;
			EmitterInstance->GetAllocatedSize(Num, Max);
			Ar.CountBytes(Num, Max);
		}
	}
}

void UParticleSystemComponent::BeginDestroy()
{
	ForceAsyncWorkCompletion(ENSURE_AND_STALL);
	Super::BeginDestroy();
	ResetParticles(true);
}

void UParticleSystemComponent::FinishDestroy()
{
	ForceAsyncWorkCompletion(ENSURE_AND_STALL);
	for (int32 EmitterIndex = 0; EmitterIndex < EmitterInstances.Num(); EmitterIndex++)
	{
		FParticleEmitterInstance* EmitInst = EmitterInstances[EmitterIndex];
		if (EmitInst)
		{
#if STATS
			EmitInst->PreDestructorCall();
#endif
			delete EmitInst;
			EmitterInstances[EmitterIndex] = NULL;
		}
	}
	Super::FinishDestroy();
}


SIZE_T UParticleSystemComponent::GetResourceSize(EResourceSizeMode::Type Mode)
{
	ForceAsyncWorkCompletion(ENSURE_AND_STALL);
	int32 ResSize = 0;
	for (int32 EmitterIdx = 0; EmitterIdx < EmitterInstances.Num(); EmitterIdx++)
	{
		FParticleEmitterInstance* EmitterInstance = EmitterInstances[EmitterIdx];
		if (EmitterInstance != NULL)
		{
			// If the data manager has the PSys, force it to report, regardless of a PSysComp scene info being present...
			ResSize += EmitterInstance->GetResourceSize(Mode);
		}
	}
	return ResSize;
}


bool UParticleSystemComponent::ParticleLineCheck(FHitResult& Hit, AActor* SourceActor, const FVector& End, const FVector& Start, const FVector& HalfExtent, const FCollisionObjectQueryParams& ObjectParams)
{
	ForceAsyncWorkCompletion(ENSURE_AND_STALL);
	check(GetWorld());
	static FName NAME_ParticleCollision = FName(TEXT("ParticleCollision"));

	if ( HalfExtent.IsZero() )
	{
		return GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, ObjectParams, FCollisionQueryParams(NAME_ParticleCollision, true, SourceActor));
	}
	else
	{
		FCollisionQueryParams BoxParams(false);
		BoxParams.TraceTag = NAME_ParticleCollision;
		BoxParams.AddIgnoredActor(SourceActor);
		return GetWorld()->SweepSingleByObjectType(Hit, Start, End, FQuat::Identity, ObjectParams, FCollisionShape::MakeBox(HalfExtent), BoxParams);
	}
}

void UParticleSystemComponent::OnRegister()
{
	ForceAsyncWorkCompletion(STALL);
	check(FXSystem == NULL);
	check(World != NULL);

	if (World->Scene)
	{
		FXSystem = World->Scene->GetFXSystem();
	}

	Super::OnRegister();

	// If we were active before but are not now, activate us
	if (bWasActive && !bIsActive)
	{
		Activate(true);
	}

	UE_LOG(LogParticles,Verbose,
		TEXT("OnRegister %s Component=0x%p Scene=0x%p FXSystem=0x%p"),
		Template != NULL ? *Template->GetName() : TEXT("NULL"), this, World->Scene, FXSystem);

	if (LODLevel == -1)
	{
		// Force it to LODLevel 0
		LODLevel = 0;
	}
}

void UParticleSystemComponent::OnUnregister()
{
	ForceAsyncWorkCompletion(STALL);
	UE_LOG(LogParticles,Verbose,
		TEXT("OnUnregister %s Component=0x%p Scene=0x%p FXSystem=0x%p"),
		Template != NULL ? *Template->GetName() : TEXT("NULL"), this, World->Scene, FXSystem);

	bWasActive = bIsActive;

	ResetParticles(true);
	FXSystem = NULL;
	Super::OnUnregister();

	// sanity check
	check(FXSystem == NULL);
}

void UParticleSystemComponent::CreateRenderState_Concurrent()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_ParticleSystemComponent_CreateRenderState_Concurrent);

	ForceAsyncWorkCompletion(ENSURE_AND_STALL);
	check( GetWorld() );
	UE_LOG(LogParticles,Verbose,
		TEXT("CreateRenderState_Concurrent @ %fs %s"), GetWorld()->TimeSeconds,
		Template != NULL ? *Template->GetName() : TEXT("NULL"));

	// NULL out template if we're not allowing particles. This is not done in the Editor to avoid clobbering content via PIE.
	if( !GIsAllowingParticles && !GIsEditor )
	{
		Template = NULL;
	}

	if (Template && Template->bHasPhysics)
	{
		PrimaryComponentTick.TickGroup = TG_PrePhysics;
			
		AEmitter* EmitterOwner = Cast<AEmitter>(GetOwner());
		if (EmitterOwner)
		{
			EmitterOwner->PrimaryActorTick.TickGroup = TG_PrePhysics;
		}
	}

	Super::CreateRenderState_Concurrent();

	bJustRegistered = true;
}



void UParticleSystemComponent::SendRenderTransform_Concurrent()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_ParticleSystemComponent_SendRenderTransform_Concurrent);

	ForceAsyncWorkCompletion(ENSURE_AND_STALL);
	if (bIsActive)
	{
		if (bSkipUpdateDynamicDataDuringTick == false)
		{
			Super::SendRenderTransform_Concurrent();
			return;
		}
	}
	// skip the Primitive component update to avoid updating the render thread
	UActorComponent::SendRenderTransform_Concurrent();
}

void UParticleSystemComponent::SendRenderDynamicData_Concurrent()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_ParticleSystemComponent_SendRenderDynamicData_Concurrent);

	ForceAsyncWorkCompletion(ENSURE_AND_STALL);
	Super::SendRenderDynamicData_Concurrent();

	FParticleSystemSceneProxy* PSysSceneProxy = (FParticleSystemSceneProxy*)SceneProxy;
	if (PSysSceneProxy != NULL)
	{
		// check to see if this PSC is active.  When you attach a PSC it gets added to the DataManager
		// even if it might be bIsActive = false  (e.g. attach and later in the frame activate it)
		// or also for PSCs that are attached to a SkelComp which is being attached and reattached but the PSC itself is not active!
		if (bIsActive)
		{
			UpdateDynamicData(PSysSceneProxy);
		}
		else
		{
			// so if we just were deactivated we want to update the renderer with NULL so the renderer will clear out the data there and not have outdated info which may/will cause a crash
			if (bWasDeactivated || bWasCompleted)
			{
				PSysSceneProxy->UpdateData(NULL);
			}
		}
	}
}

void UParticleSystemComponent::DestroyRenderState_Concurrent()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_ParticleSystemComponent_DestroyRenderState_Concurrent);

	ForceAsyncWorkCompletion(ENSURE_AND_STALL);
	check( GetWorld() );
	UE_LOG(LogParticles,Verbose,
		TEXT("DestroyRenderState_Concurrent @ %fs %s"), GetWorld()->TimeSeconds,
		Template != NULL ? *Template->GetName() : TEXT("NULL"));

	if (bResetOnDetach)
	{
		// Empty the EmitterInstance array.
		ResetParticles();
	}

	Super::DestroyRenderState_Concurrent();
}


FDynamicEmitterDataBase* UParticleSystemComponent::CreateDynamicDataFromReplay( FParticleEmitterInstance* EmitterInstance, 
	const FDynamicEmitterReplayDataBase* EmitterReplayData, bool bSelected )
{
	checkSlow(EmitterInstance && EmitterInstance->CurrentLODLevel);
	check( EmitterReplayData != NULL );

	// Allocate the appropriate type of emitter data
	FDynamicEmitterDataBase* EmitterData = NULL;

	switch( EmitterReplayData->eEmitterType )
	{
		case DET_Sprite:
			{
				// Allocate the dynamic data
				FDynamicSpriteEmitterData* NewEmitterData = ::new FDynamicSpriteEmitterData(EmitterInstance->CurrentLODLevel->RequiredModule);

				// Fill in the source data
				const FDynamicSpriteEmitterReplayData* SpriteEmitterReplayData =
					static_cast< const FDynamicSpriteEmitterReplayData* >( EmitterReplayData );
				NewEmitterData->Source = *SpriteEmitterReplayData;

				// Setup dynamic render data.  Only call this AFTER filling in source data for the emitter.
				NewEmitterData->Init( bSelected );

				EmitterData = NewEmitterData;
			}
			break;

		case DET_Mesh:
			{
				// Allocate the dynamic data
				// PVS-Studio does not understand the checkSlow above, so it is warning us that EmitterInstance->CurrentLODLevel may be null.
				FDynamicMeshEmitterData* NewEmitterData = ::new FDynamicMeshEmitterData(EmitterInstance->CurrentLODLevel->RequiredModule); //-V595

				// Fill in the source data
				const FDynamicMeshEmitterReplayData* MeshEmitterReplayData =
					static_cast< const FDynamicMeshEmitterReplayData* >( EmitterReplayData );
				NewEmitterData->Source = *MeshEmitterReplayData;

				// Setup dynamic render data.  Only call this AFTER filling in source data for the emitter.

				// @todo: Currently we're assuming the original emitter instance is bound to the same mesh as
				//        when the replay was generated (safe), and various mesh/material indices are intact.  If
				//        we ever support swapping meshes/material on the fly, we'll need cache the mesh
				//        reference and mesh component/material indices in the actual replay data.

				if( EmitterInstance != NULL )
				{
					FParticleMeshEmitterInstance* MeshEmitterInstance =
						static_cast< FParticleMeshEmitterInstance* >( EmitterInstance );
					NewEmitterData->Init(
						bSelected,
						MeshEmitterInstance,
						MeshEmitterInstance->MeshTypeData->Mesh
						);
					EmitterData = NewEmitterData;
				}
			}
			break;

		case DET_Beam2:
			{
				// Allocate the dynamic data
				FDynamicBeam2EmitterData* NewEmitterData = ::new FDynamicBeam2EmitterData(EmitterInstance->CurrentLODLevel->RequiredModule);

				// Fill in the source data
				const FDynamicBeam2EmitterReplayData* Beam2EmitterReplayData =
					static_cast< const FDynamicBeam2EmitterReplayData* >( EmitterReplayData );
				NewEmitterData->Source = *Beam2EmitterReplayData;

				// Setup dynamic render data.  Only call this AFTER filling in source data for the emitter.
				NewEmitterData->Init( bSelected );

				EmitterData = NewEmitterData;
			}
			break;

		case DET_Ribbon:
			{
				// Allocate the dynamic data
				FDynamicRibbonEmitterData* NewEmitterData = ::new FDynamicRibbonEmitterData(EmitterInstance->CurrentLODLevel->RequiredModule);

				// Fill in the source data
				const FDynamicRibbonEmitterReplayData* Trail2EmitterReplayData = static_cast<const FDynamicRibbonEmitterReplayData*>(EmitterReplayData);
				NewEmitterData->Source = *Trail2EmitterReplayData;
				// Setup dynamic render data.  Only call this AFTER filling in source data for the emitter.
				NewEmitterData->Init(bSelected);
				EmitterData = NewEmitterData;
			}
			break;

		case DET_AnimTrail:
			{
				// Allocate the dynamic data
				FDynamicAnimTrailEmitterData* NewEmitterData = ::new FDynamicAnimTrailEmitterData(EmitterInstance->CurrentLODLevel->RequiredModule);
				// Fill in the source data
				const FDynamicTrailsEmitterReplayData* AnimTrailEmitterReplayData = static_cast<const FDynamicTrailsEmitterReplayData*>(EmitterReplayData);
				NewEmitterData->Source = *AnimTrailEmitterReplayData;
				// Setup dynamic render data.  Only call this AFTER filling in source data for the emitter.
				NewEmitterData->Init(bSelected);
				EmitterData = NewEmitterData;
			}
			break;

		default:
			{
				// @todo: Support capture of other particle system types
			}
			break;
	}

	return EmitterData;
}




FParticleDynamicData* UParticleSystemComponent::CreateDynamicData()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_ParticleSystemComponent_CreateDynamicData);

	// Only proceed if we have any live particles or if we're actively replaying/capturing
	if (EmitterInstances.Num() > 0)
	{
		int32 LiveCount = 0;
		for (int32 EmitterIndex = 0; EmitterIndex < EmitterInstances.Num(); EmitterIndex++)
		{
			FParticleEmitterInstance* EmitInst = EmitterInstances[EmitterIndex];
			if (EmitInst)
			{
				if (EmitInst->ActiveParticles > 0)
				{
					LiveCount++;
				}
			}
		}

		if (!bForceLODUpdateFromRenderer
			&& LiveCount == 0
			&& ReplayState == PRS_Disabled)
		{
			return NULL;
		}
	}

	FParticleDynamicData* ParticleDynamicData = new FParticleDynamicData();
	{
		SCOPE_CYCLE_COUNTER(STAT_ParticleMemTime);
		INC_DWORD_STAT(STAT_DynamicPSysCompCount);
		INC_DWORD_STAT_BY(STAT_DynamicPSysCompMem, sizeof(FParticleDynamicData));
	}
	if (Template)
	{
		ParticleDynamicData->SystemPositionForMacroUVs = ComponentToWorld.TransformPosition(Template->MacroUVPosition);
		ParticleDynamicData->SystemRadiusForMacroUVs = Template->MacroUVRadius;
	}

	if( ReplayState == PRS_Replaying )
	{
		// Do we have any replay data to play back?
		UParticleSystemReplay* ReplayData = FindReplayClipForIDNumber( ReplayClipIDNumber );
		if( ReplayData != NULL )
		{
			// Make sure the current frame index is in a valid range
			if( ReplayData->Frames.IsValidIndex( ReplayFrameIndex ) )
			{
				// Grab the current particle system replay frame
				const FParticleSystemReplayFrame& CurReplayFrame = ReplayData->Frames[ ReplayFrameIndex ];


				// Fill the emitter dynamic buffers with data from our replay
				ParticleDynamicData->DynamicEmitterDataArray.Empty( CurReplayFrame.Emitters.Num() );
				for( int32 CurEmitterIndex = 0; CurEmitterIndex < CurReplayFrame.Emitters.Num(); ++CurEmitterIndex )
				{
					const FParticleEmitterReplayFrame& CurEmitter = CurReplayFrame.Emitters[ CurEmitterIndex ];

					const FDynamicEmitterReplayDataBase* CurEmitterReplay = CurEmitter.FrameState;
					check( CurEmitterReplay != NULL );

					FParticleEmitterInstance* EmitterInst = NULL;
					if( EmitterInstances.IsValidIndex( CurEmitter.OriginalEmitterIndex ) )
					{
						// Fill dynamic data from the replay frame data for this emitter so we can render it
						// Grab the original emitter instance for that this replay was generated from
						FDynamicEmitterDataBase* NewDynamicEmitterData =
							CreateDynamicDataFromReplay( EmitterInstances[ CurEmitter.OriginalEmitterIndex ], CurEmitterReplay, IsOwnerSelected() );

						if( NewDynamicEmitterData != NULL )
						{
							ParticleDynamicData->DynamicEmitterDataArray.Add( NewDynamicEmitterData );
						}
					}
				}
			}
		}
	}
	else
	{
		FParticleSystemReplayFrame* NewReplayFrame = NULL;
		if( ReplayState == PRS_Capturing )
		{
			ForceAsyncWorkCompletion(ENSURE_AND_STALL);
			check(IsInGameThread());
			// If we don't have any replay data for this component yet, create some now
			UParticleSystemReplay* ReplayData = FindReplayClipForIDNumber( ReplayClipIDNumber );
			if( ReplayData == NULL )
			{
				// Create a new replay clip!
				ReplayData = NewObject<UParticleSystemReplay>(this);

				// Set the clip ID number
				ReplayData->ClipIDNumber = ReplayClipIDNumber;

				// Add this to the component's list of clips
				ReplayClips.Add( ReplayData );

				// We're modifying the component by adding a new replay clip
				MarkPackageDirty();
			}


			// Add a new frame!
			{
				const int32 NewFrameIndex = ReplayData->Frames.Num();
				new( ReplayData->Frames ) FParticleSystemReplayFrame;
				NewReplayFrame = &ReplayData->Frames[ NewFrameIndex ];

				// We're modifying the component by adding a new frame
				MarkPackageDirty();
			}
		}

		// Is the particle system allowed to run?
		if( bForcedInActive == false )
		{
			ParticleDynamicData->DynamicEmitterDataArray.Empty(EmitterInstances.Num());

			for (int32 EmitterIndex = 0; EmitterIndex < EmitterInstances.Num(); EmitterIndex++)
			{
				FDynamicEmitterDataBase* NewDynamicEmitterData = NULL;
				FParticleEmitterInstance* EmitterInst = EmitterInstances[EmitterIndex];
				if (EmitterInst)
				{
					// Generate the dynamic data for this emitter
					NewDynamicEmitterData = EmitterInst->GetDynamicData( IsOwnerSelected() );
					if( NewDynamicEmitterData != NULL )
					{
						NewDynamicEmitterData->bValid = true;
						ParticleDynamicData->DynamicEmitterDataArray.Add( NewDynamicEmitterData );
						// Are we current capturing particle state?
						if( ReplayState == PRS_Capturing )
						{
							// Capture replay data for this particle system
							// NOTE: This call should always succeed if GetDynamicData succeeded earlier
							FDynamicEmitterReplayDataBase* NewEmitterReplayData = EmitterInst->GetReplayData();
							check( NewEmitterReplayData != NULL );


							// @todo: We could drastically reduce the size of replays in memory and
							//		on disk by implementing delta compression here.

							// Allocate a new emitter frame
							check(NewReplayFrame != NULL);
							const int32 NewFrameEmitterIndex = NewReplayFrame->Emitters.Num();
							new( NewReplayFrame->Emitters ) FParticleEmitterReplayFrame;
							FParticleEmitterReplayFrame* NewEmitterReplayFrame = &NewReplayFrame->Emitters[ NewFrameEmitterIndex ];

							// Store the replay state for this emitter frame.  Note that this will be
							// deleted when the parent object is garbage collected.
							NewEmitterReplayFrame->EmitterType = NewEmitterReplayData->eEmitterType;
							NewEmitterReplayFrame->OriginalEmitterIndex = EmitterIndex;
							NewEmitterReplayFrame->FrameState = NewEmitterReplayData;
						}
					}
				}
			}
		}
	}

	return ParticleDynamicData;
}

int32 UParticleSystemComponent::GetNumMaterials() const
{
	if (Template)
	{
		return Template->Emitters.Num();
	}
	return 0;
}

UMaterialInterface* UParticleSystemComponent::GetMaterial(int32 ElementIndex) const
{
	if (EmitterMaterials.IsValidIndex(ElementIndex) && EmitterMaterials[ElementIndex] != NULL)
	{
		return EmitterMaterials[ElementIndex];
	}
	if (Template && Template->Emitters.IsValidIndex(ElementIndex))
	{
		UParticleEmitter* Emitter = Template->Emitters[ElementIndex];
		if (Emitter && Emitter->LODLevels.Num() > 0)
		{
			UParticleLODLevel* EmitterLODLevel = Emitter->LODLevels[0];
			if (EmitterLODLevel && EmitterLODLevel->RequiredModule)
			{
				return EmitterLODLevel->RequiredModule->Material;
			}
		}
	}
	return NULL;
}

void UParticleSystemComponent::SetMaterial(int32 ElementIndex, UMaterialInterface* Material)
{
	ForceAsyncWorkCompletion(STALL);
	if (Template && Template->Emitters.IsValidIndex(ElementIndex))
	{
		if (!EmitterMaterials.IsValidIndex(ElementIndex))
		{
			EmitterMaterials.AddZeroed(ElementIndex + 1 - EmitterMaterials.Num());
		}
		EmitterMaterials[ElementIndex] = Material;
		bIsViewRelevanceDirty = true;
	}
}

void UParticleSystemComponent::ClearDynamicData()
{
	ForceAsyncWorkCompletion(ENSURE_AND_STALL);
	if (SceneProxy)
	{
		FParticleSystemSceneProxy* ParticleSceneProxy = (FParticleSystemSceneProxy*)SceneProxy;
		ParticleSceneProxy->UpdateData(NULL);
	}
}

void UParticleSystemComponent::UpdateDynamicData(FParticleSystemSceneProxy* Proxy)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_ParticleSystemComponent_UpdateDynamicData);

	ForceAsyncWorkCompletion(ENSURE_AND_STALL);
	if (SceneProxy)
	{
		// Create the dynamic data for rendering this particle system
		FParticleDynamicData* ParticleDynamicData = CreateDynamicData();

		// Render the particles
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		//@todo.SAS. Remove thisline  - it is used for debugging purposes...
		Proxy->SetLastDynamicData(Proxy->GetDynamicData());
		//@todo.SAS. END
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		Proxy->UpdateData( ParticleDynamicData );
	}
}

void UParticleSystemComponent::UpdateLODInformation()
{
	ForceAsyncWorkCompletion(ENSURE_AND_STALL);
	if (GetWorld()->IsGameWorld() || (GIsEditor && GEngine->bEnableEditorPSysRealtimeLOD))
	{
		if (SceneProxy)
		{
			if (EmitterInstances.Num() > 0)
			{
				uint8 CheckLODMethod = PARTICLESYSTEMLODMETHOD_DirectSet;
				if (bOverrideLODMethod)
				{
					CheckLODMethod = LODMethod;
				}
				else
				{
					if (Template)
					{
						CheckLODMethod = Template->LODMethod;
					}
				}

				if (CheckLODMethod == PARTICLESYSTEMLODMETHOD_Automatic)
				{
					FParticleSystemSceneProxy* ParticleSceneProxy = (FParticleSystemSceneProxy*)SceneProxy;
					float PendingDistance = ParticleSceneProxy->GetPendingLODDistance();
					if (PendingDistance > 0.0f)
					{
						int32 LODIndex = 0;
						for (int32 LODDistIndex = 1; LODDistIndex < Template->LODDistances.Num(); LODDistIndex++)
						{
							if (Template->LODDistances[LODDistIndex] > ParticleSceneProxy->GetPendingLODDistance())
							{
								break;
							}
							LODIndex = LODDistIndex;
						}

						if (LODIndex != LODLevel)
						{
							SetLODLevel(LODIndex);
						}
					}
				}
			}
		}
	}
	else
	{
#if WITH_EDITORONLY_DATA
		if (LODLevel != EditorLODLevel)
		{
			SetLODLevel(EditorLODLevel);
		}
#endif // WITH_EDITORONLY_DATA
	}
}

void UParticleSystemComponent::OrientZAxisTowardCamera()
{
	ForceAsyncWorkCompletion(ENSURE_AND_STALL);

	//@TODO: CAMERA: How does this work for stereo and/or split-screen?
	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();

	// Orient the Z axis toward the camera
	if (PlayerController && PlayerController->PlayerCameraManager)
	{
		// Direction of the camera
		FVector DirToCamera = PlayerController->PlayerCameraManager->GetCameraLocation() - GetComponentLocation();
		DirToCamera.Normalize();

		// Convert the camera direction to local space
		DirToCamera = ComponentToWorld.InverseTransformVectorNoScale(DirToCamera);
		
		// Local Z axis
		const FVector LocalZAxis = FVector(0,0,1);

		// Find angle between z-axis and the camera direction
		const FQuat PointTo = FQuat::FindBetween(LocalZAxis, DirToCamera);
		
		// Adjust our rotation
		const FRotator AdjustmentAngle(PointTo);
		RelativeRotation += AdjustmentAngle;

		// Mark the component transform as dirty if the rotation has changed.
		bIsTransformDirty |= !AdjustmentAngle.IsZero();
	}
}

#if WITH_EDITOR
void UParticleSystemComponent::PreEditChange(UProperty* PropertyThatWillChange)
{
	ForceAsyncWorkCompletion(ENSURE_AND_STALL);
	bool bShouldResetParticles = true;

	if (PropertyThatWillChange)
	{
		const FName PropertyName = PropertyThatWillChange->GetFName();

		// Don't reset particles for properties that won't affect the particles
		if (PropertyName == TEXT("bCastVolumetricTranslucentShadow")
			|| PropertyName == TEXT("bCastDynamicShadow")
			|| PropertyName == TEXT("bAffectDynamicIndirectLighting")
			|| PropertyName == TEXT("CastShadow"))
		{
			bShouldResetParticles = false;
		}
	}

	if (bShouldResetParticles)
	{
		ResetParticles();
	}
	
	Super::PreEditChange(PropertyThatWillChange);
}

void UParticleSystemComponent::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	ForceAsyncWorkCompletion(ENSURE_AND_STALL);
	if (PropertyChangedEvent.PropertyChain.Num() > 0)
	{
		UProperty* MemberProperty = PropertyChangedEvent.PropertyChain.GetActiveMemberNode()->GetValue();
		if ( MemberProperty != NULL )
		{
			FName PropertyName = PropertyChangedEvent.Property->GetFName();
			if ((PropertyName == TEXT("Color")) ||
				(PropertyName == TEXT("R")) ||
				(PropertyName == TEXT("G")) ||
				(PropertyName == TEXT("B")))
			{
				//@todo. once the property code can give the correct index, only update
				// the entry that was actually changed!
				// This function does not return an index into the array at the moment...
				// int32 InstParamIdx = PropertyChangedEvent.GetArrayIndex(TEXT("InstanceParameters"));
				for (int32 InstIdx = 0; InstIdx < InstanceParameters.Num(); InstIdx++)
				{
					FParticleSysParam& PSysParam = InstanceParameters[InstIdx];
					if ((PSysParam.ParamType == PSPT_Vector) || (PSysParam.ParamType == PSPT_VectorRand))
					{
						PSysParam.Vector.X = PSysParam.Color.R / 255.0f;
						PSysParam.Vector.Y = PSysParam.Color.G / 255.0f;
						PSysParam.Vector.Z = PSysParam.Color.B / 255.0f;
					}
				}
			}
		}
	}

	bIsViewRelevanceDirty = true;
	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR

FBoxSphereBounds UParticleSystemComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBox BoundingBox;
	BoundingBox.Init();

	if (FXConsoleVariables::bAllowCulling == false)
	{
		BoundingBox.Min = FVector(-HALF_WORLD_MAX);
		BoundingBox.Max = FVector(+HALF_WORLD_MAX);
	}
	else if(Template && Template->bUseFixedRelativeBoundingBox)
	{
		// Use hardcoded relative bounding box from template.
		BoundingBox	= Template->FixedRelativeBoundingBox.TransformBy(LocalToWorld);
	}
	else
	{
		for (int32 i=0; i<EmitterInstances.Num(); i++)
		{
			FParticleEmitterInstance* EmitterInstance = EmitterInstances[i];
			if( EmitterInstance && EmitterInstance->HasActiveParticles() )
			{
				BoundingBox += EmitterInstance->GetBoundingBox();
			}
		}

		// Expand the actual bounding-box slightly so it will be valid longer in the case of expanding particle systems.
		const FVector ExpandAmount = BoundingBox.GetExtent() * 0.1f;
		BoundingBox = FBox(BoundingBox.Min - ExpandAmount, BoundingBox.Max + ExpandAmount);
	}

	return FBoxSphereBounds(BoundingBox);
}

void UParticleSystemComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	ForceAsyncWorkCompletion(ENSURE_AND_STALL);
	SCOPE_CYCLE_COUNTER(STAT_PSysCompTickTime);

	bool bDisallowAsync = false;

	// Bail out if inactive and not AutoActivate
	if ((bIsActive == false) && (bAutoActivate == false))
	{
		// Disable our tick here, will be enabled when activating
		SetComponentTickEnabled(false);
		return;
	}
	DeltaTimeTick = DeltaTime;

	UWorld* World = GetWorld();
	check(World);
	// Bail out if there is no template, there are no instances, or we're running a dedicated server and we don't update on those
	if ((Template == NULL) || (EmitterInstances.Num() == 0) || (Template->Emitters.Num() == 0) || 
		((bUpdateOnDedicatedServer == false) && (GetNetMode() == NM_DedicatedServer)))
	{
		return;
	}

	// System settings may have been lowered. Support late deactivation.
	int32 DetailModeCVar = GetCachedScalabilityCVars().DetailMode;
	const bool bDetailModeAllowsRendering	= DetailMode <= DetailModeCVar;
	if ( bDetailModeAllowsRendering == false )
	{
		if ( bIsActive )
		{
			DeactivateSystem();
			Super::MarkRenderDynamicDataDirty();
		}
		return;
	} 
	else
	{
		bool bRequiresReset = false;
		for (int32 EmitterIndex = 0; EmitterIndex < EmitterInstances.Num(); ++EmitterIndex)
		{
			FParticleEmitterInstance* Instance = EmitterInstances[EmitterIndex];
			if (Instance && Instance->SpriteTemplate)
			{
				if(Instance->SpriteTemplate->DetailMode > DetailModeCVar)
				{
					bRequiresReset = true;
					break;
				}
			}
		}
		if (bRequiresReset)
		{
			ResetParticles(true);
			InitializeSystem();
		}
	}

	// Bail out if MaxSecondsBeforeInactive > 0 and we haven't been rendered the last MaxSecondsBeforeInactive seconds.
	if (bWarmingUp == false)
	{
		const float MaxSecondsBeforeInactive = FMath::Max( SecondsBeforeInactive, Template->SecondsBeforeInactive );

		// Clamp MaxSecondsBeforeInactive to be at least twice the maximum
		// smoothed frame time (45.45ms) because the rendering thread runs one 
		// frame behind the game thread and so smaller time differences cannot 
		// be reliably detected.
		const float ClampedMaxSecondsBeforeInactive = MaxSecondsBeforeInactive > 0 ? FMath::Max(MaxSecondsBeforeInactive, 0.1f) : 0;

		if( ClampedMaxSecondsBeforeInactive > 0 
			&&	AccumTickTime > ClampedMaxSecondsBeforeInactive//SecondsBeforeInactive
			&&	GetWorld()->IsGameWorld() )
		{
			const float CurrentTimeSeconds = World->GetTimeSeconds();
			if( CurrentTimeSeconds > (LastRenderTime + ClampedMaxSecondsBeforeInactive) )
			{
				bForcedInActive = true;

				SpawnEvents.Empty();
				DeathEvents.Empty();
				CollisionEvents.Empty();
				KismetEvents.Empty();

				return;
			}
		}

		AccumLODDistanceCheckTime += DeltaTime;
		if (AccumLODDistanceCheckTime > Template->LODDistanceCheckTime)
		{
			AccumLODDistanceCheckTime = 0.0f;

			if (ShouldComputeLODFromGameThread())
			{
				bool bCalculateLODLevel = 
 					(bOverrideLODMethod == true) ? (LODMethod == PARTICLESYSTEMLODMETHOD_Automatic) : 
 					 	(Template ? (Template->LODMethod == PARTICLESYSTEMLODMETHOD_Automatic) : false);
				if (bCalculateLODLevel == true)
				{
					FVector EffectPosition = GetComponentLocation();
					int32 DesiredLODLevel = DetermineLODLevelForLocation(EffectPosition);
					SetLODLevel(DesiredLODLevel);
				}
			}
			else
			{
				// Periodically force an LOD update from the renderer if we are
				// using rendering results to make LOD decisions.
				bForceLODUpdateFromRenderer = true;
				UpdateLODInformation();
			}
		}
	}

	bForcedInActive = false;
	DeltaTime *= CustomTimeDilation;
	DeltaTimeTick = DeltaTime;

	AccumTickTime += DeltaTime;

	// Orient the Z axis toward the camera
	if (Template->bOrientZAxisTowardCamera)
	{
		OrientZAxisTowardCamera();
	}

	if (Template->SystemUpdateMode == EPSUM_FixedTime)
	{
		// Use the fixed delta time!
		DeltaTime = Template->UpdateTime_Delta;
	}

	// Clear out the events.
	SpawnEvents.Empty();
	DeathEvents.Empty();
	CollisionEvents.Empty();
	BurstEvents.Empty();
	TotalActiveParticles = 0;
	bNeedsFinalize = true;
	if (!ThisTickFunction || !CanTickInAnyThread() || FXConsoleVariables::bFreezeParticleSimulation || !FXConsoleVariables::bAllowAsyncTick)
	{
		bDisallowAsync = true;
	}
	if (bDisallowAsync)
	{
		if (!FXConsoleVariables::bFreezeParticleSimulation)
		{
			ComputeTickComponent_Concurrent();
		}
		FinalizeTickComponent();
	}
	else
	{
		// set up async task and the game thread task to finalize the results.
		DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.AsyncParticleTick"),
			STAT_FSimpleDelegateGraphTask_AsyncParticleTick,
			STATGROUP_TaskGraphTasks);

		AsyncWork = FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateUObject(this, &UParticleSystemComponent::ComputeTickComponent_Concurrent),
			GET_STATID(STAT_FSimpleDelegateGraphTask_AsyncParticleTick), NULL, ENamedThreads::AnyThread
		);

		DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.FinalizeParticleTick"),
			STAT_FSimpleDelegateGraphTask_FinalizeParticleTick,
			STATGROUP_TaskGraphTasks);

		FGraphEventRef Finalize = FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateUObject(this, &UParticleSystemComponent::FinalizeTickComponent),
			GET_STATID(STAT_FSimpleDelegateGraphTask_FinalizeParticleTick), AsyncWork, ENamedThreads::GameThread
		);

		ThisTickFunction->GetCompletionHandle()->DontCompleteUntil(Finalize);
	}
}

int32 UParticleSystemComponent::GetCurrentDetailMode() const
{
#if WITH_EDITORONLY_DATA
	if (!GEngine->bEnableEditorPSysRealtimeLOD && EditorDetailMode >= 0)
	{
		return EditorDetailMode;
	}
	else
#endif
	{
		return GetCachedScalabilityCVars().DetailMode;
	}
}

void UParticleSystemComponent::ComputeTickComponent_Concurrent()
{
	SCOPE_CYCLE_COUNTER(STAT_ParticleComputeTickTime);
	FScopeCycleCounterUObject AdditionalScope(AdditionalStatObject(), GET_STATID(STAT_ParticleComputeTickTime));
	// Tick Subemitters.
	int32 EmitterIndex;
	for (EmitterIndex = 0; EmitterIndex < EmitterInstances.Num(); EmitterIndex++)
	{
		FParticleEmitterInstance* Instance = EmitterInstances[EmitterIndex];

		if (EmitterIndex + 1 < EmitterInstances.Num())
		{
			FParticleEmitterInstance* NextInstance = EmitterInstances[EmitterIndex+1];
			FPlatformMisc::Prefetch(NextInstance);
		}

		if (Instance && Instance->SpriteTemplate)
		{
			check(Instance->SpriteTemplate->LODLevels.Num() > 0);

			UParticleLODLevel* SpriteLODLevel = Instance->SpriteTemplate->GetCurrentLODLevel(Instance);
			if (SpriteLODLevel && SpriteLODLevel->bEnabled)
			{
				Instance->Tick(DeltaTimeTick, bSuppressSpawning);

				if (!Instance->Tick_MaterialOverrides())
				{
					if (EmitterMaterials.IsValidIndex(EmitterIndex))
					{
						if (EmitterMaterials[EmitterIndex])
						{
							Instance->CurrentMaterial = EmitterMaterials[EmitterIndex];
						}
					}
				}
				TotalActiveParticles += Instance->ActiveParticles;
			}
		}
	}
}

void UParticleSystemComponent::FinalizeTickComponent()
{
	SCOPE_CYCLE_COUNTER(STAT_ParticleFinalizeTickTime);
	if (!bNeedsFinalize)
	{
		return;
	}
	AsyncWork = NULL; // this task is done
	bNeedsFinalize = false;
	if (FXConsoleVariables::bFreezeParticleSimulation == false)
	{
		int32 EmitterIndex;
		// Now, process any events that have occurred.
		for (EmitterIndex = 0; EmitterIndex < EmitterInstances.Num(); EmitterIndex++)
		{
			FParticleEmitterInstance* Instance = EmitterInstances[EmitterIndex];

			if (EmitterIndex + 1 < EmitterInstances.Num())
			{
				FParticleEmitterInstance* NextInstance = EmitterInstances[EmitterIndex+1];
				FPlatformMisc::Prefetch(NextInstance);
			}

			if (Instance && Instance->SpriteTemplate)
			{
				UParticleLODLevel* SpriteLODLevel = Instance->SpriteTemplate->GetCurrentLODLevel(Instance);
				if (SpriteLODLevel && SpriteLODLevel->bEnabled)
				{
					Instance->ProcessParticleEvents(DeltaTimeTick, bSuppressSpawning);
				}
			}
		}

		AParticleEventManager* EventManager = (World ? World->MyParticleEventManager : NULL);
		if (EventManager)
		{
			if (SpawnEvents.Num() > 0) EventManager->HandleParticleSpawnEvents(this, SpawnEvents);
			if (DeathEvents.Num() > 0) EventManager->HandleParticleDeathEvents(this, DeathEvents);
			if (CollisionEvents.Num() > 0) EventManager->HandleParticleCollisionEvents(this, CollisionEvents);
			if (BurstEvents.Num() > 0) EventManager->HandleParticleBurstEvents(this, BurstEvents);
		}
	}
	// Clear out the Kismet events, as they should have been processed by now...
	KismetEvents.Empty();

	// Indicate that we have been ticked since being registered.
	bJustRegistered = false;

	// If component has just totally finished, call script event.
	const bool bIsCompleted = HasCompleted(); 
	if (bIsCompleted && !bWasCompleted)
	{
		UE_LOG(LogParticles,Verbose,
			TEXT("HasCompleted()==true @ %fs %s"), World->TimeSeconds,
			Template != NULL ? *Template->GetName() : TEXT("NULL"));

		OnSystemFinished.Broadcast(this);

		// When system is done - destroy all subemitters etc. We don't need them any more.
		ResetParticles();
		bIsActive = false;
		SetComponentTickEnabled(false);

		if (bAutoDestroy)
		{
			DestroyComponent();
		}
	}
	bWasCompleted = bIsCompleted;

	// Update bounding box.
	if (!bWarmingUp && !bIsCompleted && !Template->bUseFixedRelativeBoundingBox && !bIsTransformDirty)
	{
		// Force an update every once in a while to shrink the bounds.
		TimeSinceLastForceUpdateTransform += DeltaTimeTick;
		if(TimeSinceLastForceUpdateTransform > MaxTimeBeforeForceUpdateTransform)
		{
			bIsTransformDirty = true;
		}
		else
		{
			// Compute the new system bounding box.
			FBox BoundingBox;
			BoundingBox.Init();

			for (int32 i=0; i<EmitterInstances.Num(); i++)
			{
				FParticleEmitterInstance* Instance = EmitterInstances[i];
				if (Instance && Instance->SpriteTemplate)
				{
					UParticleLODLevel* SpriteLODLevel = Instance->SpriteTemplate->GetCurrentLODLevel(Instance);
					if (SpriteLODLevel && SpriteLODLevel->bEnabled)
					{
						BoundingBox += Instance->GetBoundingBox();
					}
				}
			}

			// Only update the primitive's bounding box in the octree if the system bounding box has gotten larger.
			if(!Bounds.GetBox().IsInside(BoundingBox.Min) || !Bounds.GetBox().IsInside(BoundingBox.Max))
			{
				bIsTransformDirty = true;
			}
		}
	}

	// Update if the component transform has been dirtied.
	if(bIsTransformDirty)
	{
		UpdateComponentToWorld();
		
		TimeSinceLastForceUpdateTransform = 0.0f;
		bIsTransformDirty = false;
	}

	const float InvDeltaTime = (DeltaTimeTick > 0.0f) ? 1.0f / DeltaTimeTick : 0.0f;
	PartSysVelocity = (GetComponentLocation() - OldPosition) * InvDeltaTime;
	OldPosition = GetComponentLocation();

	if (bIsViewRelevanceDirty)
	{
		ConditionalCacheViewRelevanceFlags();
	}

	if (bSkipUpdateDynamicDataDuringTick == false)
	{
		Super::MarkRenderDynamicDataDirty();
	}

}

void UParticleSystemComponent::WaitForAsyncAndFinalize(EForceAsyncWorkCompletion Behavior) const
{
	if (!AsyncWork->IsComplete())
	{
		check(IsInGameThread());
		SCOPE_CYCLE_COUNTER(STAT_GTSTallTime);
		double StartTime = FPlatformTime::Seconds();
		QUICK_SCOPE_CYCLE_COUNTER(STAT_UParticleSystemComponent_WaitForAsyncAndFinalize);
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(AsyncWork, ENamedThreads::GameThread_Local);
		float ThisTime = float(FPlatformTime::Seconds() - StartTime) * 1000.0f;
		if (Behavior != SILENT)
		{
			UE_LOG(LogParticles, Warning, TEXT("Stalled gamethread waiting for particles %5.2fms '%s' '%s'"), ThisTime, *GetFullNameSafe(this), *GetFullNameSafe(Template));
			if (Behavior == ENSURE_AND_STALL)
			{
				ensureMsgf(0,TEXT("Stalled gamethread waiting for particles %5.2fms '%s' '%s'"), ThisTime, *GetFullNameSafe(this), *GetFullNameSafe(Template));
			}
		}
		const_cast<UParticleSystemComponent*>(this)->FinalizeTickComponent();
	}
}

void UParticleSystemComponent::InitParticles()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_ParticleSystemComponent_InitParticles);

	if (IsTemplate() == true)
	{
		return;
	}
	ForceAsyncWorkCompletion(ENSURE_AND_STALL);

	check(GetWorld());
	UE_LOG(LogParticles,Verbose,
		TEXT("InitParticles @ %fs %s"), GetWorld()->TimeSeconds,
		Template != NULL ? *Template->GetName() : TEXT("NULL"));

	if (Template != NULL)
	{
		WarmupTime = Template->WarmupTime;
		WarmupTickRate = Template->WarmupTickRate;
		bIsViewRelevanceDirty = true;
		const int32 GlobalDetailMode = GetCachedScalabilityCVars().DetailMode;

		// If nothing is initialized, create EmitterInstances here.
		if (EmitterInstances.Num() == 0)
		{
			const bool bDetailModeAllowsRendering	= DetailMode <= GlobalDetailMode;
			if (bDetailModeAllowsRendering && (CanEverRender()))
			{
				EmitterInstances.Empty(Template->Emitters.Num());
				for (int32 Idx = 0; Idx < Template->Emitters.Num(); Idx++)
				{
					// Must have a slot for each emitter instance - even if it's NULL.
					// This is so the indexing works correctly.
					UParticleEmitter* Emitter = Template->Emitters[Idx];
					if (Emitter && Emitter->DetailMode <= GlobalDetailMode && Emitter->HasAnyEnabledLODs())
					{
						EmitterInstances.Add(Emitter->CreateInstance(this));
					}
					else
					{
						int32 NewIndex = EmitterInstances.AddUninitialized();
						EmitterInstances[NewIndex] = NULL;
					}
				}
				bWasCompleted = false;
			}
		}
		else
		{
			// create new instances as needed
			for (int32 Idx = 0; Idx < EmitterInstances.Num(); Idx++)
			{
				FParticleEmitterInstance* Instance = EmitterInstances[Idx];
				if (Instance)
				{
					Instance->SetHaltSpawning(false);
				}
			}
			while (EmitterInstances.Num() < Template->Emitters.Num())
			{
				int32					Index	= EmitterInstances.Num();
				UParticleEmitter*	Emitter	= Template->Emitters[Index];
				if (Emitter && Emitter->DetailMode <= GlobalDetailMode)
				{
					FParticleEmitterInstance* Instance = Emitter->CreateInstance(this);
					EmitterInstances.Add(Instance);
					if (Instance)
					{
						Instance->InitParameters(Emitter, this);
					}
				}
				else
				{
					int32 NewIndex = EmitterInstances.AddUninitialized();
					EmitterInstances[NewIndex] = NULL;
				}
			}

			int32 PreferredLODLevel = LODLevel;
			// re-initialize the instances
			bool bClearDynamicData = false;
			for (int32 Idx = 0; Idx < EmitterInstances.Num(); Idx++)
			{
				FParticleEmitterInstance* Instance = EmitterInstances[Idx];
				if (Instance != NULL)
				{
					UParticleEmitter* Emitter = NULL;
					if (Idx < Template->Emitters.Num())
					{
						Emitter = Template->Emitters[Idx];
					}
					if (Emitter && Emitter->DetailMode <= GlobalDetailMode)
					{
						Instance->InitParameters(Emitter, this, false);
						Instance->Init();
						if (PreferredLODLevel >= Emitter->LODLevels.Num())
						{
							PreferredLODLevel = Emitter->LODLevels.Num() - 1;
						}
					}
					else
					{
						// Get rid of the 'phantom' instance
#if STATS
						Instance->PreDestructorCall();
#endif
						delete Instance;
						EmitterInstances[Idx] = NULL;
						bClearDynamicData = true;
					}
				}
				else
				{
					UParticleEmitter* Emitter = NULL;
					if (Idx < Template->Emitters.Num())
					{
						Emitter = Template->Emitters[Idx];
					}
					if (Emitter && Emitter->DetailMode <= GlobalDetailMode)
					{
						Instance = Emitter->CreateInstance(this);
						EmitterInstances[Idx] = Instance;
						if (Instance != NULL)
						{
							Instance->InitParameters(Emitter, this, false);
							Instance->Init();
							if (PreferredLODLevel >= Emitter->LODLevels.Num())
							{
								PreferredLODLevel = Emitter->LODLevels.Num() - 1;
							}
						}
					}
					else
					{
						EmitterInstances[Idx] = NULL;
					}
				}
			}

			if (bClearDynamicData)
			{
				ClearDynamicData();
			}

			if (PreferredLODLevel != LODLevel)
			{
				// This should never be higher...
				check(PreferredLODLevel < LODLevel);
				LODLevel = PreferredLODLevel;
			}

			for (int32 Idx = 0; Idx < EmitterInstances.Num(); Idx++)
			{
				FParticleEmitterInstance* Instance = EmitterInstances[Idx];
				// set the LOD levels here
				if (Instance)
				{
					Instance->CurrentLODLevelIndex	= LODLevel;
					Instance->CurrentLODLevel		= Instance->SpriteTemplate->LODLevels[Instance->CurrentLODLevelIndex];
				}
			}
		}
	}
}

void UParticleSystemComponent::ResetParticles(bool bEmptyInstances)
{
	ForceAsyncWorkCompletion(ENSURE_AND_STALL);
	UE_LOG(LogParticles,Verbose,
		TEXT("ResetParticles @ %fs %s bEmptyInstances=%s"), GetWorld() ? GetWorld()->TimeSeconds : 0.0f,
		Template != NULL ? *Template->GetName() : TEXT("NULL"), bEmptyInstances ? TEXT("true") : TEXT("false"));

	UWorld* OwningWorld = GetWorld();

 	const bool bIsGameWorld = OwningWorld ? OwningWorld->IsGameWorld() : !GIsEditor;

	// Remove instances from scene.
	for( int32 InstanceIndex=0; InstanceIndex<EmitterInstances.Num(); InstanceIndex++ )
	{
		FParticleEmitterInstance* EmitterInstance = EmitterInstances[InstanceIndex];
		if( EmitterInstance )
		{
			if (GbEnableGameThreadLODCalculation == false)
			{
				if (!(!bIsGameWorld || bEmptyInstances))
				{
					EmitterInstance->SpriteTemplate	= NULL;
					EmitterInstance->Component	= NULL;
				}
			}
		}
	}

	// Set the system as deactive
	bIsActive	= false;

	// Remove instances if we're not running gameplay.ww
	if (!bIsGameWorld || bEmptyInstances)
	{
		for (int32 EmitterIndex = 0; EmitterIndex < EmitterInstances.Num(); EmitterIndex++)
		{
			FParticleEmitterInstance* EmitInst = EmitterInstances[EmitterIndex];
			if (EmitInst)
			{
#if STATS
				EmitInst->PreDestructorCall();
#endif
				delete EmitInst;
				EmitterInstances[EmitterIndex] = NULL;
			}
		}
		EmitterInstances.Empty();
		ClearDynamicData();
	}
	else
	{
		for (int32 EmitterIndex = 0; EmitterIndex < EmitterInstances.Num(); EmitterIndex++)
		{
			FParticleEmitterInstance* EmitInst = EmitterInstances[EmitterIndex];
			if (EmitInst)
			{
				EmitInst->Rewind();
			}
		}
	}

	// Mark render state dirty to deregister the component with the scene.
	MarkRenderStateDirty();
}

void UParticleSystemComponent::ResetBurstLists()
{
	ForceAsyncWorkCompletion(STALL);
	for (int32 i=0; i<EmitterInstances.Num(); i++)
	{
		if (EmitterInstances[i])
		{
			EmitterInstances[i]->ResetBurstList();
		}
	}
}

void UParticleSystemComponent::SetTemplate(class UParticleSystem* NewTemplate)
{
	SCOPE_CYCLE_COUNTER(STAT_ParticleSetTemplateTime);
	ForceAsyncWorkCompletion(STALL);

	if( GIsAllowingParticles || GIsEditor ) 
	{
		bIsViewRelevanceDirty = true;

		bool bIsTemplate = IsTemplate();
		bWasCompleted = false;
		// remember if we were active and therefore should restart after setting up the new template
		bWasActive = bIsActive && !bWasDeactivated; 
		bool bResetInstances = false;
		if (NewTemplate != Template)
		{
			bResetInstances = true;
		}
		if (bIsTemplate == false)
		{
			ResetParticles(bResetInstances);
		}

		Template = NewTemplate;
		if (Template)
		{
			WarmupTime = Template->WarmupTime;
		}
		else
		{
			WarmupTime = 0.0f;
		}

		if(NewTemplate && IsRegistered())
		{
			if ((bAutoActivate || bWasActive) && (bIsTemplate == false))
			{
				ActivateSystem();
			}
			else
			{
				InitializeSystem();
			}

			if (!SceneProxy || bResetInstances)
			{
				MarkRenderStateDirty();
			}
		}
	}
	else
	{
		Template = NULL;
	}
	EmitterMaterials.Empty();
}

void UParticleSystemComponent::ActivateSystem(bool bFlagAsJustAttached)
{
	SCOPE_CYCLE_COUNTER(STAT_ParticleActivateTime);
	ForceAsyncWorkCompletion(STALL);

	if (IsTemplate() == true || !IsRegistered() || 	!FApp::CanEverRender())
	{
		return;
	}

	check(GetWorld());
	UE_LOG(LogParticles,Verbose,
		TEXT("ActivateSystem @ %fs %s"), GetWorld()->TimeSeconds,
		Template != NULL ? *Template->GetName() : TEXT("NULL"));

	const bool bIsGameWorld = GetWorld()->IsGameWorld();

	if (UE_LOG_ACTIVE(LogParticles,VeryVerbose))
	{
		if (Template)
		{
			if (EmitterInstances.Num() > 0)
			{
				int32 LiveCount = 0;

				for (int32 EmitterIndex =0; EmitterIndex < EmitterInstances.Num(); EmitterIndex++)
				{
					FParticleEmitterInstance* EmitInst = EmitterInstances[EmitterIndex];
					if (EmitInst)
					{
						LiveCount += EmitInst->ActiveParticles;
					}
				}

				if (LiveCount > 0)
				{
					UE_LOG(LogParticles, Log, TEXT("ActivateSystem called on PSysComp w/ live particles - %5d, %s"),
						LiveCount, *(Template->GetFullName()));
				}
			}
		}
	}

	// System settings may have been lowered. Support late deactivation.
	const bool bDetailModeAllowsRendering	= DetailMode <= GetCachedScalabilityCVars().DetailMode;

	if( GIsAllowingParticles && bDetailModeAllowsRendering )
	{
		if (bFlagAsJustAttached)
		{
			bJustRegistered = true;
		}
	
		// Stop suppressing particle spawning.
		bSuppressSpawning = false;
		
		// Set the system as active
		bool bNeedToUpdateTransform = bWasDeactivated;
		bWasCompleted = false;
		bWasDeactivated = false;
		bIsActive = true;
		bWasActive = false; // Set to false now, it may get set to true when it's deactivated due to unregister
		SetComponentTickEnabled(true);

		// if no instances, or recycling
		if (EmitterInstances.Num() == 0 || (bIsGameWorld && (!bAutoActivate || bHasBeenActivated)))
		{
			InitializeSystem();
		}
		else if (EmitterInstances.Num() > 0 && !bIsGameWorld)
		{
			// If currently running, re-activating rewinds the emitter to the start. Existing particles should stick around.
			for (int32 i=0; i<EmitterInstances.Num(); i++)
			{
				if (EmitterInstances[i])
				{
					EmitterInstances[i]->Rewind();
					EmitterInstances[i]->SetHaltSpawning(false);
				}
			}
		}


		// Force an LOD update
		if ((bIsGameWorld || (GIsEditor && GEngine->bEnableEditorPSysRealtimeLOD)) && (GbEnableGameThreadLODCalculation == true))
		{
			FVector EffectPosition = GetComponentLocation();
			int32 DesiredLODLevel = DetermineLODLevelForLocation(EffectPosition);
			if (DesiredLODLevel != LODLevel)
			{
				SetLODLevel(DesiredLODLevel);
			}
		}
		else
		{
			bForceLODUpdateFromRenderer = true;
		}


		// Flag the system as having been activated at least once
		bHasBeenActivated = true;

		int32 DesiredLODLevel = 0;
		bool bCalculateLODLevel = 
			(bOverrideLODMethod == true) ? (LODMethod != PARTICLESYSTEMLODMETHOD_DirectSet) : 
				(Template ? (Template->LODMethod != PARTICLESYSTEMLODMETHOD_DirectSet) : false);

		if (bCalculateLODLevel)
		{
			FVector EffectPosition = GetComponentLocation();
			DesiredLODLevel = DetermineLODLevelForLocation(EffectPosition);
			if (GbEnableGameThreadLODCalculation == true)
			{
				if (DesiredLODLevel != LODLevel)
				{
					bIsActive = true;
					SetComponentTickEnabled(true);
				}
				SetLODLevel(DesiredLODLevel);
			}
		}

		if (WarmupTime != 0.0f)
		{
			bool bSaveSkipUpdate = bSkipUpdateDynamicDataDuringTick;
			bSkipUpdateDynamicDataDuringTick = true;
			bWarmingUp = true;
			ResetBurstLists();

			float WarmupElapsed = 0.f;
			float WarmupTimestep = 0.032f;
			if (WarmupTickRate > 0)
			{
				WarmupTimestep = (WarmupTickRate <= WarmupTime) ? WarmupTickRate : WarmupTime;
			}

			while (WarmupElapsed < WarmupTime)
			{
				TickComponent(WarmupTimestep, LEVELTICK_All, NULL);
				WarmupElapsed += WarmupTimestep;
			}

			bWarmingUp = false;
			WarmupTime = 0.0f;
			bSkipUpdateDynamicDataDuringTick = bSaveSkipUpdate;
		}
		AccumTickTime = 0.0;
	}

	// Mark render state dirty to ensure the scene proxy is added and registered with the scene.
	MarkRenderStateDirty();

	if(!bWasDeactivated && !bWasCompleted && ensure(GetWorld()))
	{
		LastRenderTime = GetWorld()->GetTimeSeconds();
	}
}


void UParticleSystemComponent::DeactivateSystem()
{
	if (IsTemplate() == true)
	{
		return;
	}
	ForceAsyncWorkCompletion(STALL);

	check(GetWorld());
	UE_LOG(LogParticles,Verbose,
		TEXT("DeactivateSystem @ %fs %s"), GetWorld()->TimeSeconds,
		Template != NULL ? *Template->GetName() : TEXT("NULL"));

	bSuppressSpawning = true;
	bWasDeactivated = true;

	bool bShouldMarkRenderStateDirty = false;
	for (int32 i = 0; i < EmitterInstances.Num(); i++)
	{
		FParticleEmitterInstance*	Instance = EmitterInstances[i];
		if (Instance)
		{
			if (Instance->bKillOnDeactivate)
			{
				//UE_LOG(LogParticles, Log, TEXT("%s killed on deactivate"),EmitterInstances(i)->GetName());
#if STATS
				Instance->PreDestructorCall();
#endif
				// clean up other instances that may point to this one
				for (int32 InnerIndex=0; InnerIndex < EmitterInstances.Num(); InnerIndex++)
				{
					if (InnerIndex != i && EmitterInstances[InnerIndex] != NULL)
					{
						EmitterInstances[InnerIndex]->OnEmitterInstanceKilled(Instance);
					}
				}
				delete Instance;
				EmitterInstances[i] = NULL;
				bShouldMarkRenderStateDirty = true;
			}
			else
			{
				Instance->OnDeactivateSystem();
			}
		}
	}

	if (bShouldMarkRenderStateDirty)
	{
		ClearDynamicData();
		MarkRenderStateDirty();
	}

	LastRenderTime = GetWorld()->GetTimeSeconds();
}

void UParticleSystemComponent::ComputeCanTickInAnyThread()
{
	check(!bIsElligibleForAsyncTickComputed);
	bIsElligibleForAsyncTick = false;
	if (Template)
	{
		bIsElligibleForAsyncTickComputed = true;
		bIsElligibleForAsyncTick = Template->CanTickInAnyThread();
	}
}

bool UParticleSystemComponent::ShouldActivate() const
{
	return (Super::ShouldActivate() || (bWasDeactivated || bWasCompleted));
}

void UParticleSystemComponent::Activate(bool bReset) 
{
	// If the particle system can't ever render (ie on dedicated server or in a commandlet) than do not activate...
	if (FApp::CanEverRender())
	{
		ForceAsyncWorkCompletion(STALL);
		if (bReset || ShouldActivate()==true)
		{
			ActivateSystem(bReset);
		}
	}
}

void UParticleSystemComponent::Deactivate()
{
	ForceAsyncWorkCompletion(STALL);
	if (ShouldActivate()==false)
	{
		DeactivateSystem();
	}
}

void UParticleSystemComponent::ApplyWorldOffset(const FVector& InOffset, bool bWorldShift)
{
	Super::ApplyWorldOffset(InOffset, bWorldShift);

	OldPosition+= InOffset;
	
	for (auto It = EmitterInstances.CreateIterator(); It; ++It)
	{
		FParticleEmitterInstance* EmitterInstance = *It;
		if (EmitterInstance != NULL)
		{
			EmitterInstance->ApplyWorldOffset(InOffset, bWorldShift);
		}
	}
}

void UParticleSystemComponent::ResetToDefaults()
{
	ForceAsyncWorkCompletion(STALL);
	if (!IsTemplate())
	{
		// make sure we're fully stopped and unregistered
		DeactivateSystem();
		SetTemplate(NULL);

		if(IsRegistered())
		{
			UnregisterComponent();
		}

		UParticleSystemComponent* Default = (UParticleSystemComponent*)(GetArchetype());

		// copy all non-native, non-duplicatetransient, non-Component properties we have from all classes up to and including UActorComponent
		for (UProperty* Property = GetClass()->PropertyLink; Property != NULL; Property = Property->PropertyLinkNext)
		{
			if ( !(Property->PropertyFlags & CPF_DuplicateTransient) && !(Property->PropertyFlags & (CPF_InstancedReference | CPF_ContainsInstancedReference)) &&
				Property->GetOwnerClass()->IsChildOf(UActorComponent::StaticClass()) )
			{
				Property->CopyCompleteValue_InContainer(this, Default);
			}
		}
	}
}

void UParticleSystemComponent::UpdateInstances(bool bEmptyInstances)
{
	if (GIsEditor && IsRegistered())
	{
		ForceAsyncWorkCompletion(STALL);
		ResetParticles(bEmptyInstances);

		InitializeSystem();
		if (bAutoActivate)
		{
			ActivateSystem();
		}

		if (Template && Template->bUseFixedRelativeBoundingBox)
		{
			UpdateComponentToWorld();
		}
	}
}

int32 UParticleSystemComponent::GetNumActiveParticles() const
{
	ForceAsyncWorkCompletion(STALL);
	int32 NumParticles = 0;
	for (int32 i=0; i<EmitterInstances.Num(); i++)
	{
		FParticleEmitterInstance* Instance = EmitterInstances[i];

		if (Instance != NULL)
		{
			NumParticles += Instance->ActiveParticles;
		}
	}
	return NumParticles;
}

void UParticleSystemComponent::GetOwnedTrailEmitters(TArray< struct FParticleAnimTrailEmitterInstance* >& OutTrailEmitters, const void* InOwner, bool bSetOwner)
{
	for (FParticleEmitterInstance* Inst : EmitterInstances)
	{
		if (Inst && Inst->IsTrailEmitter())
		{
			FParticleAnimTrailEmitterInstance* TrailEmitter = (FParticleAnimTrailEmitterInstance*)Inst;
			if (bSetOwner)
			{
				TrailEmitter->Owner = InOwner;
				OutTrailEmitters.Add(TrailEmitter);
			}
			else if (TrailEmitter->Owner == InOwner)
			{
				OutTrailEmitters.Add(TrailEmitter);
			}
		}
	}
}

void UParticleSystemComponent::BeginTrails(FName InFirstSocketName, FName InSecondSocketName, ETrailWidthMode InWidthMode, float InWidth)
{
	ActivateSystem(true);
	for (FParticleEmitterInstance* Inst : EmitterInstances)
	{
		if (Inst)
		{
			Inst->BeginTrail();
			Inst->SetTrailSourceData(InFirstSocketName, InSecondSocketName, InWidthMode, InWidth);
		}
	}
}

void UParticleSystemComponent::EndTrails()
{
	for (FParticleEmitterInstance* Inst : EmitterInstances)
	{
		if (Inst)
		{
			Inst->EndTrail();
		}
	}
	DeactivateSystem();
}

void UParticleSystemComponent::SetTrailSourceData(FName InFirstSocketName, FName InSecondSocketName, ETrailWidthMode InWidthMode, float InWidth)
{
	for (FParticleEmitterInstance* Inst : EmitterInstances)
	{
		if (Inst)
		{
			Inst->SetTrailSourceData(InFirstSocketName, InSecondSocketName, InWidthMode, InWidth);
		}
	}
}

bool UParticleSystemComponent::HasCompleted()
{
	ForceAsyncWorkCompletion(STALL);
	bool bHasCompleted = true;

	// If we're currently capturing or replaying captured frames, then we'll stay active for that
	if( ReplayState != PRS_Disabled )
	{
		// While capturing, we want to stay active so that we'll just record empty frame data for
		// completed particle systems.  While replaying, we never want our particles/meshes removed from
		// the scene, so we'll force the system to stay alive!
		return false;
	}

	bool bClearDynamicData = false;
	for (int32 i=0; i<EmitterInstances.Num(); i++)
	{
		FParticleEmitterInstance* Instance = EmitterInstances[i];

		if (Instance && Instance->CurrentLODLevel)
		{
			if (Instance->CurrentLODLevel->bEnabled)
			{
				if (Instance->CurrentLODLevel->RequiredModule->EmitterLoops > 0 || Instance->IsTrailEmitter())
				{
					if (bWasDeactivated && bSuppressSpawning)
					{
						if (Instance->ActiveParticles != 0)
						{
							bHasCompleted = false;
						}
					}
					else
					{
						if (Instance->HasCompleted())
						{
							if (Instance->bKillOnCompleted)
							{
#if STATS
								Instance->PreDestructorCall();
#endif
								// clean up other instances that may point to this one
								for (int32 InnerIndex=0; InnerIndex < EmitterInstances.Num(); InnerIndex++)
								{
									if (InnerIndex != i && EmitterInstances[InnerIndex] != NULL)
									{
										EmitterInstances[InnerIndex]->OnEmitterInstanceKilled(Instance);
									}
								}
								delete Instance;
								EmitterInstances[i] = NULL;
								bClearDynamicData = true;
							}
						}
						else
						{
							bHasCompleted = false;
						}
					}
				}
				else
				{
					if (bWasDeactivated)
					{
						if (Instance->ActiveParticles != 0)
						{
							bHasCompleted = false;
						}
					}
					else
					{
						bHasCompleted = false;
					}
				}
			}
			else
			{
				UParticleEmitter* Em = CastChecked<UParticleEmitter>(Instance->CurrentLODLevel->GetOuter());
				if (Em && Em->bDisabledLODsKeepEmitterAlive)
				{
					bHasCompleted = false;
				}
			}
		}
	}

	if (bClearDynamicData)
	{
		ClearDynamicData();
	}
	
	return bHasCompleted;
}

void UParticleSystemComponent::InitializeSystem()
{
	SCOPE_CYCLE_COUNTER(STAT_ParticleInitializeTime);
	ForceAsyncWorkCompletion(STALL);

	if (!IsRegistered() || (FXSystem == NULL))
	{
		// Don't warn in a commandlet, we're expected not to have a scene / FX system.
		if (!IsRunningCommandlet() && !IsRunningDedicatedServer())
		{
			UE_LOG(LogParticles,Warning,TEXT("InitializeSystem called on an unregistered component. Template=%s Component=%s"),
				Template ? *Template->GetPathName() : TEXT("NULL"),
				*GetPathName()
				);
		}
		return;
	}

	// At this point the component must be associated with an FX system.
	check( FXSystem != NULL );

	check(GetWorld());
	UE_LOG(LogParticles,Verbose,
		TEXT("InitializeSystem @ %fs %s Component=0x%p FXSystem=0x%p"), GetWorld()->TimeSeconds,
		Template != NULL ? *Template->GetName() : TEXT("NULL"), this, FXSystem);

	// System settings may have been lowered. Support late deactivation.
	const bool bDetailModeAllowsRendering	= DetailMode <= GetCachedScalabilityCVars().DetailMode;

	if( GIsAllowingParticles && bDetailModeAllowsRendering )
	{
		if (IsTemplate() == true)
		{
			return;
		}

		if( Template != NULL )
		{
			EmitterDelay = Template->Delay;

			if( Template->bUseDelayRange )
			{
				const float	Rand = FMath::FRand();
				EmitterDelay	 = Template->DelayLow + ((Template->Delay - Template->DelayLow) * Rand);
			}
		}

		// Allocate the emitter instances and particle data
		InitParticles();
		if (IsRegistered())
		{
			AccumTickTime = 0.0;
			if ((bIsActive == false) && (bAutoActivate == true) && (bWasDeactivated == false))
			{
				SetActive(true);
			}
		}
	}
}


FString UParticleSystemComponent::GetDetailedInfoInternal() const
{
	FString Result;  

	if( Template != NULL )
	{
		Result = Template->GetPathName( NULL );
	}
	else
	{
		Result = TEXT("No_ParticleSystem");
	}

	return Result;  
}




void UParticleSystemComponent::ConditionalCacheViewRelevanceFlags(class UParticleSystem* NewTemplate)
{
	ForceAsyncWorkCompletion(ENSURE_AND_STALL);
	if (NewTemplate && (NewTemplate != Template))
	{
		bIsViewRelevanceDirty = true;
	}

	if (bIsViewRelevanceDirty)
	{
		UParticleSystem* TemplateToCache = Template;
		if (NewTemplate)
		{
			TemplateToCache = NewTemplate;
		}
		CacheViewRelevanceFlags(TemplateToCache);
		MarkRenderStateDirty();
	}
}

void UParticleSystemComponent::CacheViewRelevanceFlags(UParticleSystem* TemplateToCache)
{
	ForceAsyncWorkCompletion(ENSURE_AND_STALL);
	CachedViewRelevanceFlags.Empty();

	if (TemplateToCache)
	{
		for (int32 EmitterIndex = 0; EmitterIndex < TemplateToCache->Emitters.Num(); EmitterIndex++)
		{
			UParticleSpriteEmitter* Emitter = Cast<UParticleSpriteEmitter>(TemplateToCache->Emitters[EmitterIndex]);
			if (Emitter == NULL)
			{
				// Handle possible empty slots in the emitter array.
				continue;
			}
			FParticleEmitterInstance* EmitterInst = NULL;
			if (EmitterIndex < EmitterInstances.Num())
			{
				EmitterInst = EmitterInstances[EmitterIndex];
			}

			//@TODO I suspect this function can get called before emitter instances are created. That is bad and should be fixed up.
			if ( EmitterInst == NULL )
			{
				continue;
			}

			for (int32 LODIndex = 0; LODIndex < Emitter->LODLevels.Num(); LODIndex++)
			{
				UParticleLODLevel* EmitterLODLevel = Emitter->LODLevels[LODIndex];

				// Prime the array
				// This code assumes that the particle system emitters all have the same number of LODLevels. 
				if (LODIndex >= CachedViewRelevanceFlags.Num())
				{
					CachedViewRelevanceFlags.AddZeroed(1);
				}
				FMaterialRelevance& LODViewRel = CachedViewRelevanceFlags[LODIndex];
				check(EmitterLODLevel->RequiredModule);

				if (EmitterLODLevel->bEnabled == true)
				{
					auto World = GetWorld();
					EmitterInst->GatherMaterialRelevance(&LODViewRel, EmitterLODLevel, World ? World->FeatureLevel : GMaxRHIFeatureLevel);
				}
			}
		}
	}
	bIsViewRelevanceDirty = false;
}

void UParticleSystemComponent::RewindEmitterInstances()
{
	ForceAsyncWorkCompletion(STALL);
	for (int32 EmitterIndex = 0; EmitterIndex < EmitterInstances.Num(); EmitterIndex++)
	{
		FParticleEmitterInstance* EmitterInst = EmitterInstances[EmitterIndex];
		if (EmitterInst)
		{
			EmitterInst->Rewind();
		}
	}
}


void UParticleSystemComponent::SetBeamEndPoint(int32 EmitterIndex,FVector NewEndPoint)
{
	ForceAsyncWorkCompletion(STALL);
	if ((EmitterIndex >= 0) && (EmitterIndex < EmitterInstances.Num()))
	{
		FParticleEmitterInstance* EmitterInst = EmitterInstances[EmitterIndex];
		if (EmitterInst)
		{
			EmitterInst->SetBeamEndPoint(NewEndPoint);
		}
	}
}


void UParticleSystemComponent::SetBeamSourcePoint(int32 EmitterIndex,FVector NewSourcePoint,int32 SourceIndex)
{
	ForceAsyncWorkCompletion(STALL);
	if ((EmitterIndex >= 0) && (EmitterIndex < EmitterInstances.Num()))
	{
		FParticleEmitterInstance* EmitterInst = EmitterInstances[EmitterIndex];
		if (EmitterInst)
		{
			EmitterInst->SetBeamSourcePoint(NewSourcePoint, SourceIndex);
		}
	}
}


void UParticleSystemComponent::SetBeamSourceTangent(int32 EmitterIndex,FVector NewTangentPoint,int32 SourceIndex)
{
	ForceAsyncWorkCompletion(STALL);
	if ((EmitterIndex >= 0) && (EmitterIndex < EmitterInstances.Num()))
	{
		FParticleEmitterInstance* EmitterInst = EmitterInstances[EmitterIndex];
		if (EmitterInst)
		{
			EmitterInst->SetBeamSourceTangent(NewTangentPoint, SourceIndex);
		}
	}
}


void UParticleSystemComponent::SetBeamSourceStrength(int32 EmitterIndex,float NewSourceStrength,int32 SourceIndex)
{
	ForceAsyncWorkCompletion(STALL);
	if ((EmitterIndex >= 0) && (EmitterIndex < EmitterInstances.Num()))
	{
		FParticleEmitterInstance* EmitterInst = EmitterInstances[EmitterIndex];
		if (EmitterInst)
		{
			EmitterInst->SetBeamSourceStrength(NewSourceStrength, SourceIndex);
		}
	}
}


void UParticleSystemComponent::SetBeamTargetPoint(int32 EmitterIndex,FVector NewTargetPoint,int32 TargetIndex)
{
	ForceAsyncWorkCompletion(STALL);
	if ((EmitterIndex >= 0) && (EmitterIndex < EmitterInstances.Num()))
	{
		FParticleEmitterInstance* EmitterInst = EmitterInstances[EmitterIndex];
		if (EmitterInst)
		{
			EmitterInst->SetBeamTargetPoint(NewTargetPoint, TargetIndex);
		}
	}
}


void UParticleSystemComponent::SetBeamTargetTangent(int32 EmitterIndex,FVector NewTangentPoint,int32 TargetIndex)
{
	ForceAsyncWorkCompletion(STALL);
	if ((EmitterIndex >= 0) && (EmitterIndex < EmitterInstances.Num()))
	{
		FParticleEmitterInstance* EmitterInst = EmitterInstances[EmitterIndex];
		if (EmitterInst)
		{
			EmitterInst->SetBeamTargetTangent(NewTangentPoint, TargetIndex);
		}
	}
}

void UParticleSystemComponent::SetBeamTargetStrength(int32 EmitterIndex,float NewTargetStrength,int32 TargetIndex)
{
	ForceAsyncWorkCompletion(STALL);
	if ((EmitterIndex >= 0) && (EmitterIndex < EmitterInstances.Num()))
	{
		FParticleEmitterInstance* EmitterInst = EmitterInstances[EmitterIndex];
		if (EmitterInst)
		{
			EmitterInst->SetBeamTargetStrength(NewTargetStrength, TargetIndex);
		}
	}
}


void UParticleSystemComponent::SetEmitterEnable(FName EmitterName, bool bNewEnableState)
{
	ForceAsyncWorkCompletion(STALL);
	for (int32 EmitterIndex = 0; EmitterIndex < EmitterInstances.Num(); ++EmitterIndex)
	{
		FParticleEmitterInstance* EmitterInst = EmitterInstances[EmitterIndex];
		if (EmitterInst && EmitterInst->SpriteTemplate)
		{
			if (EmitterInst->SpriteTemplate->EmitterName == EmitterName)
			{
				EmitterInst->SetHaltSpawning(!bNewEnableState);
			}
		}
	}
}

int32 UParticleSystemComponent::DetermineLODLevelForLocation(const FVector& EffectLocation)
{
	// No particle system, ignore
	if (Template == NULL)
	{
		return 0;
	}

	// Don't bother if we only have 1 LOD level...
	if (Template->LODDistances.Num() <= 1)
	{
		return 0;
	}
	check(IsInGameThread());
	int32 Retval = 0;
	
	// Run this for all local player controllers.
	// If several are found (split screen?). Take the closest for highest LOD.
	UWorld* World = GetWorld();
	if(World != NULL)
	{
		TArray<FVector,TInlineAllocator<8> > PlayerViewLocations;
		if (World->GetPlayerControllerIterator())
		{
			for( FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator )
			{
				APlayerController* PlayerController = *Iterator;
				if(PlayerController->IsLocalPlayerController())
				{
					FVector* POVLoc = new(PlayerViewLocations) FVector;
					FRotator POVRotation;
					PlayerController->GetPlayerViewPoint(*POVLoc, POVRotation);
				}
			}
		}
		else
		{
			PlayerViewLocations.Append(World->ViewLocationsRenderedLastFrame);
		}

		// This will now put everything in LODLevel 0 (high detail) by default
		float LODDistance = 0.0f;
		if (PlayerViewLocations.Num())
		{
			LODDistance = WORLD_MAX;
			int32 NumLocations = PlayerViewLocations.Num();
			for (int32 i = 0; i < NumLocations; ++i)
			{
				float DistanceToEffect = FVector(PlayerViewLocations[i] - EffectLocation).Size();
				if (DistanceToEffect < LODDistance)
				{
					LODDistance = DistanceToEffect;
				}
			}
		}

		// Find appropriate LOD based on distance
		Retval = Template->LODDistances.Num() - 1;
		for (int32 LODIdx = 1; LODIdx < Template->LODDistances.Num(); LODIdx++)
		{
			if (LODDistance < Template->LODDistances[LODIdx])
			{
				Retval = LODIdx - 1;
				break;
			}
		}
	}

	return Retval;
}

void UParticleSystemComponent::SetLODLevel(int32 InLODLevel)
{
	ForceAsyncWorkCompletion(STALL);
	if (Template == NULL)
	{
		return;
	}

	if (Template->LODDistances.Num() == 0)
	{
		return;
	}

	int32 NewLODLevel = FMath::Clamp(InLODLevel + GParticleLODBias,0,Template->GetLODLevelCount()-1);
	if (LODLevel != NewLODLevel)
	{
		MarkRenderStateDirty();

		const int32 OldLODLevel = LODLevel;
		LODLevel = NewLODLevel;

		for (int32 i=0; i<EmitterInstances.Num(); i++)
		{
			FParticleEmitterInstance* Instance = EmitterInstances[i];
			if (Instance)
			{
				Instance->SetCurrentLODIndex(LODLevel, true);
			}
		}
	}
}

int32 UParticleSystemComponent::GetLODLevel()
{
	return LODLevel;
}

/** 
 *	Set a named float instance parameter on this ParticleSystemComponent. 
 *	Updates the parameter if it already exists, or creates a new entry if not. 
 */
void UParticleSystemComponent::SetFloatParameter(FName Name, float Param)
{
	if(Name == NAME_None)
	{
		return;
	}
	ForceAsyncWorkCompletion(STALL);

	// First see if an entry for this name already exists
	for (int32 i = 0; i < InstanceParameters.Num(); i++)
	{
		FParticleSysParam& P = InstanceParameters[i];
		if (P.Name == Name && P.ParamType == PSPT_Scalar)
		{
			P.Scalar = Param;
			return;
		}
	}

	// We didn't find one, so create a new one.
	int32 NewParamIndex = InstanceParameters.AddZeroed();
	InstanceParameters[NewParamIndex].Name = Name;
	InstanceParameters[NewParamIndex].ParamType = PSPT_Scalar;
	InstanceParameters[NewParamIndex].Scalar = Param;
}


void UParticleSystemComponent::SetFloatRandParameter(FName ParameterName,float Param,float ParamLow)
{
	if (ParameterName == NAME_None)
	{
		return;
	}
	ForceAsyncWorkCompletion(STALL);

	// First see if an entry for this name already exists
	for (int32 i = 0; i < InstanceParameters.Num(); i++)
	{
		FParticleSysParam& P = InstanceParameters[i];
		if (P.Name == ParameterName && P.ParamType == PSPT_ScalarRand)
		{
			P.Scalar = Param;
			P.Scalar_Low = ParamLow;
			return;
		}
	}

	// We didn't find one, so create a new one.
	int32 NewParamIndex = InstanceParameters.AddZeroed();
	InstanceParameters[NewParamIndex].Name = ParameterName;
	InstanceParameters[NewParamIndex].ParamType = PSPT_ScalarRand;
	InstanceParameters[NewParamIndex].Scalar = Param;
	InstanceParameters[NewParamIndex].Scalar_Low = ParamLow;
}


void UParticleSystemComponent::SetVectorParameter(FName Name, FVector Param)
{
	if (Name == NAME_None)
	{
		return;
	}
	ForceAsyncWorkCompletion(STALL);

	// First see if an entry for this name already exists
	for (int32 i = 0; i < InstanceParameters.Num(); i++)
	{
		FParticleSysParam& P = InstanceParameters[i];
		if (P.Name == Name && P.ParamType == PSPT_Vector)
		{
			P.Vector = Param;
			return;
		}
	}

	// We didn't find one, so create a new one.
	int32 NewParamIndex = InstanceParameters.AddZeroed();
	InstanceParameters[NewParamIndex].Name = Name;
	InstanceParameters[NewParamIndex].ParamType = PSPT_Vector;
	InstanceParameters[NewParamIndex].Vector = Param;
}


void UParticleSystemComponent::SetVectorRandParameter(FName ParameterName,const FVector& Param,const FVector& ParamLow)
{
	if (ParameterName == NAME_None)
	{
		return;
	}
	ForceAsyncWorkCompletion(STALL);

	// First see if an entry for this name already exists
	for (int32 i = 0; i < InstanceParameters.Num(); i++)
	{
		FParticleSysParam& P = InstanceParameters[i];
		if (P.Name == ParameterName && P.ParamType == PSPT_VectorRand)
		{
			P.Vector = Param;
			P.Vector_Low = ParamLow;
			return;
		}
	}

	// We didn't find one, so create a new one.
	int32 NewParamIndex = InstanceParameters.AddZeroed();
	InstanceParameters[NewParamIndex].Name = ParameterName;
	InstanceParameters[NewParamIndex].ParamType = PSPT_VectorRand;
	InstanceParameters[NewParamIndex].Vector = Param;
	InstanceParameters[NewParamIndex].Vector_Low = ParamLow;
}


void UParticleSystemComponent::SetColorParameter(FName Name, FLinearColor Param)
{
	if(Name == NAME_None)
	{
		return;
	}
	ForceAsyncWorkCompletion(STALL);

	FColor NewColor(Param);

	// First see if an entry for this name already exists
	for (int32 i = 0; i < InstanceParameters.Num(); i++)
	{
		FParticleSysParam& P = InstanceParameters[i];
		if (P.Name == Name && P.ParamType == PSPT_Color)
		{
			P.Color = NewColor;
			return;
		}
	}

	// We didn't find one, so create a new one.
	int32 NewParamIndex = InstanceParameters.AddZeroed();
	InstanceParameters[NewParamIndex].Name = Name;
	InstanceParameters[NewParamIndex].ParamType = PSPT_Color;
	InstanceParameters[NewParamIndex].Color = NewColor;
}


void UParticleSystemComponent::SetActorParameter(FName Name, AActor* Param)
{
	if(Name == NAME_None)
	{
		return;
	}
	ForceAsyncWorkCompletion(STALL);

	// First see if an entry for this name already exists
	for (int32 i = 0; i < InstanceParameters.Num(); i++)
	{
		FParticleSysParam& P = InstanceParameters[i];
		if (P.Name == Name && P.ParamType == PSPT_Actor)
		{
			P.Actor = Param;
			return;
		}
	}

	// We didn't find one, so create a new one.
	int32 NewParamIndex = InstanceParameters.AddZeroed();
	InstanceParameters[NewParamIndex].Name = Name;
	InstanceParameters[NewParamIndex].ParamType = PSPT_Actor;
	InstanceParameters[NewParamIndex].Actor = Param;
}


void UParticleSystemComponent::SetMaterialParameter(FName Name, UMaterialInterface* Param)
{
	if(Name == NAME_None)
	{
		return;
	}
	ForceAsyncWorkCompletion(STALL);

	// First see if an entry for this name already exists
	for (int32 i = 0; i < InstanceParameters.Num(); i++)
	{
		FParticleSysParam& P = InstanceParameters[i];
		if (P.Name == Name && P.ParamType == PSPT_Material)
		{
			bIsViewRelevanceDirty = (P.Material != Param) ? true : false;
			P.Material = Param;
			return;
		}
	}

	// We didn't find one, so create a new one.
	int32 NewParamIndex = InstanceParameters.AddZeroed();
	InstanceParameters[NewParamIndex].Name = Name;
	InstanceParameters[NewParamIndex].ParamType = PSPT_Material;
	bIsViewRelevanceDirty = (InstanceParameters[NewParamIndex].Material != Param) ? true : false;
	InstanceParameters[NewParamIndex].Material = Param;
}


bool UParticleSystemComponent::GetFloatParameter(const FName InName,float& OutFloat)
{
	// Always fail if we pass in no name.
	if(InName == NAME_None)
	{
		return false;
	}

	for (int32 i = 0; i < InstanceParameters.Num(); i++)
	{
		FParticleSysParam& Param = InstanceParameters[i];
		if (Param.Name == InName)
		{
			if (Param.ParamType == PSPT_Scalar)
			{
				OutFloat = Param.Scalar;
				return true;
			}
			else if (Param.ParamType == PSPT_ScalarRand)
			{
				check(IsInGameThread());
				OutFloat = Param.Scalar + (Param.Scalar_Low - Param.Scalar) * FMath::SRand();
				return true;
			}
		}
	}

	return false;
}


bool UParticleSystemComponent::GetVectorParameter(const FName InName,FVector& OutVector)
{
	// Always fail if we pass in no name.
	if(InName == NAME_None)
	{
		return false;
	}

	for (int32 i = 0; i < InstanceParameters.Num(); i++)
	{
		FParticleSysParam& Param = InstanceParameters[i];
		if (Param.Name == InName)
		{
			if (Param.ParamType == PSPT_Vector)
			{
				OutVector = Param.Vector;
				return true;
			}
			else if (Param.ParamType == PSPT_VectorRand)
			{
				check(IsInGameThread());
				FVector RandValue(FMath::SRand(), FMath::SRand(), FMath::SRand());
				OutVector = Param.Vector + (Param.Vector_Low - Param.Vector) * RandValue;
				return true;
			}
		}
	}

	return false;
}


bool UParticleSystemComponent::GetColorParameter(const FName InName,FLinearColor& OutColor)
{
	// Always fail if we pass in no name.
	if(InName == NAME_None)
	{
		return false;
	}

	for (int32 i = 0; i < InstanceParameters.Num(); i++)
	{
		FParticleSysParam& Param = InstanceParameters[i];
		if (Param.Name == InName && Param.ParamType == PSPT_Color)
		{
			OutColor = FLinearColor(Param.Color);
			return true;
		}
	}

	return false;
}


bool UParticleSystemComponent::GetActorParameter(const FName InName,class AActor*& OutActor)
{
	// Always fail if we pass in no name.
	if (InName == NAME_None)
	{
		return false;
	}

	for (int32 i = 0; i < InstanceParameters.Num(); i++)
	{
		FParticleSysParam& Param = InstanceParameters[i];
		if (Param.Name == InName && Param.ParamType == PSPT_Actor)
		{
			OutActor = Param.Actor;
			return true;
		}
	}

	return false;
}


bool UParticleSystemComponent::GetMaterialParameter(const FName InName,class UMaterialInterface*& OutMaterial)
{
	// Always fail if we pass in no name.
	if (InName == NAME_None)
	{
		return false;
	}

	for (int32 i = 0; i < InstanceParameters.Num(); i++)
	{
		FParticleSysParam& Param = InstanceParameters[i];
		if (Param.Name == InName && Param.ParamType == PSPT_Material)
		{
			OutMaterial = Param.Material;
			return true;
		}
	}

	return false;
}


void UParticleSystemComponent::ClearParameter(FName ParameterName, EParticleSysParamType ParameterType)
{
	for (int32 i = 0; i < InstanceParameters.Num(); i++)
	{
		if (InstanceParameters[i].Name == ParameterName && (ParameterType == PSPT_None || InstanceParameters[i].ParamType == ParameterType))
		{
			InstanceParameters.RemoveAt(i--);
		}
	}
}


void UParticleSystemComponent::AutoPopulateInstanceProperties()
{
	check(IsInGameThread());
	if (Template)
	{
		for (int32 EmitterIndex = 0; EmitterIndex < Template->Emitters.Num(); EmitterIndex++)
		{
			UParticleEmitter* Emitter = Template->Emitters[EmitterIndex];
			Emitter->AutoPopulateInstanceProperties(this);
		}
	}
}


void UParticleSystemComponent::GetUsedMaterials( TArray<UMaterialInterface*>& OutMaterials ) const
{
	if( Template )
	{
		for( int32 EmitterIdx = 0; EmitterIdx < Template->Emitters.Num(); ++EmitterIdx )
		{
			const UParticleEmitter* Emitter = Template->Emitters[ EmitterIdx ];
			for( int32 LodLevel = 0; LodLevel < Emitter->LODLevels.Num(); ++LodLevel )
			{
				const UParticleLODLevel* LOD = Emitter->LODLevels[ LodLevel ];
				// Only process enabled emitters
				if( LOD->bEnabled )
				{
					// Assume no materials will be found on the modules.  If this remains true, the material off the required module will be taken
					bool bMaterialsFound = false;
					for( int32 ModuleIdx = 0; ModuleIdx < LOD->Modules.Num(); ++ModuleIdx )
					{
						UParticleModule* Module = LOD->Modules[ ModuleIdx ];
						if( Module->bEnabled && // Module is enabled
								 Module->IsA( UParticleModuleMeshMaterial::StaticClass() ) && // Module is a mesh material module
								 LOD->TypeDataModule && // There is a type data module
								 LOD->TypeDataModule->bEnabled && // They type data module is enabled 
								 LOD->TypeDataModule->IsA( UParticleModuleTypeDataMesh::StaticClass() ) ) // The type data module is mesh type data
						{
							// Module is a valid TypeDataMesh module
							const UParticleModuleTypeDataMesh* TypeDataModule = Cast<UParticleModuleTypeDataMesh>( LOD->TypeDataModule );
							if( !TypeDataModule->bOverrideMaterial )
							{
								// If the material isnt being overriden by the required module, for each mesh section, find the corresponding entry in the mesh material module
								// If that entry does not exist, take the material directly off the mesh section
								const UParticleModuleMeshMaterial* MaterialModule = Cast<UParticleModuleMeshMaterial>( LOD->Modules[ ModuleIdx ] );
								if( TypeDataModule->Mesh )
								{
									OutMaterials.Append(TypeDataModule->Mesh->Materials);
								}
							}
						}
					}
					if( bMaterialsFound == false )
					{
						// If no material overrides were found in any of the lod modules, take the material off the required module
						OutMaterials.Add( LOD->RequiredModule->Material );
					}
				}
			}
		}
	}
}


void UParticleSystemComponent::ReportEventSpawn(const FName InEventName, const float InEmitterTime, 
	const FVector InLocation, const FVector InVelocity, const TArray<UParticleModuleEventSendToGame*>& InEventData)
{
	FParticleEventSpawnData* SpawnData = new(SpawnEvents)FParticleEventSpawnData;
	SpawnData->Type = EPET_Spawn;
	SpawnData->EventName = InEventName;
	SpawnData->EmitterTime = InEmitterTime;
	SpawnData->Location = InLocation;
	SpawnData->Velocity = InVelocity;
	SpawnData->EventData = InEventData;
}

void UParticleSystemComponent::ReportEventDeath(const FName InEventName, const float InEmitterTime, 
	const FVector InLocation, const FVector InVelocity, const TArray<UParticleModuleEventSendToGame*>& InEventData, const float InParticleTime)
{
	FParticleEventDeathData* DeathData = new(DeathEvents)FParticleEventDeathData;
	DeathData->Type = EPET_Death;
	DeathData->EventName = InEventName;
	DeathData->EmitterTime = InEmitterTime;
	DeathData->Location = InLocation;
	DeathData->Velocity = InVelocity;
	DeathData->EventData = InEventData;
	DeathData->ParticleTime = InParticleTime;
}

void UParticleSystemComponent::ReportEventCollision(const FName InEventName, const float InEmitterTime, 
	const FVector InLocation, const FVector InDirection, const FVector InVelocity, const TArray<UParticleModuleEventSendToGame*>& InEventData,
	const float InParticleTime, const FVector InNormal, const float InTime, const int32 InItem, const FName InBoneName)
{
	FParticleEventCollideData* CollideData = new(CollisionEvents)FParticleEventCollideData;
	CollideData->Type = EPET_Collision;
	CollideData->EventName = InEventName;
	CollideData->EmitterTime = InEmitterTime;
	CollideData->Location = InLocation;
	CollideData->Direction = InDirection;
	CollideData->Velocity = InVelocity;
	CollideData->EventData = InEventData;
	CollideData->ParticleTime = InParticleTime;
	CollideData->Normal = InNormal;
	CollideData->Time = InTime;
	CollideData->Item = InItem;
	CollideData->BoneName = InBoneName;
}

void UParticleSystemComponent::ReportEventBurst(const FName InEventName, const float InEmitterTime, const int32 InParticleCount,
	const FVector InLocation, const TArray<UParticleModuleEventSendToGame*>& InEventData)
{
	FParticleEventBurstData* BurstData = new(BurstEvents)FParticleEventBurstData;
	BurstData->Type = EPET_Burst;
	BurstData->EventName = InEventName;
	BurstData->EmitterTime = InEmitterTime;
	BurstData->ParticleCount = InParticleCount;
	BurstData->Location = InLocation;
	BurstData->EventData = InEventData;
}

void UParticleSystemComponent::GenerateParticleEvent(const FName InEventName, const float InEmitterTime,
	const FVector InLocation, const FVector InDirection, const FVector InVelocity)
{
	FParticleEventKismetData* KismetData = new(KismetEvents)FParticleEventKismetData;
	KismetData->Type = EPET_Blueprint;
	KismetData->EventName = InEventName;
	KismetData->EmitterTime = InEmitterTime;
	KismetData->Location = InLocation;
	KismetData->Velocity = InVelocity;
}

void UParticleSystemComponent::KillParticlesForced()
{
	ForceAsyncWorkCompletion(STALL);
	for (int32 EmitterIndex=0;EmitterIndex<EmitterInstances.Num();EmitterIndex++)
	{
		if (EmitterInstances[EmitterIndex])
		{
			EmitterInstances[EmitterIndex]->KillParticlesForced();
		}
	}
}


void UParticleSystemComponent::ForceUpdateBounds()
{
	ForceAsyncWorkCompletion(STALL);
	FBox BoundingBox;

	BoundingBox.Init();

	const int32 EmitterCount = EmitterInstances.Num();
	for ( int32 EmitterIndex = 0; EmitterIndex < EmitterCount; ++EmitterIndex )
	{
		FParticleEmitterInstance* Instance = EmitterInstances[EmitterIndex];
		if ( Instance )
		{
			Instance->ForceUpdateBoundingBox();
			BoundingBox += Instance->GetBoundingBox();
		}
	}

	// Expand the actual bounding-box slightly so it will be valid longer in the case of expanding particle systems.
	const FVector ExpandAmount = BoundingBox.GetExtent() * 0.1f;
	BoundingBox = FBox(BoundingBox.Min - ExpandAmount, BoundingBox.Max + ExpandAmount);

	// Update our bounds.
	Bounds = BoundingBox;
}


bool UParticleSystemComponent::ShouldComputeLODFromGameThread()
{
	bool bUseGameThread = false;
	UWorld* World = GetWorld();
	if (World && World->IsGameWorld() && GbEnableGameThreadLODCalculation)
	{
		check(IsInGameThread());

		for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			APlayerController* PlayerController = *Iterator;
			if (PlayerController->IsLocalPlayerController())
			{
				bUseGameThread = true;
				break;
			}
		}
	}
	return bUseGameThread;
}


UParticleSystemReplay* UParticleSystemComponent::FindReplayClipForIDNumber( const int32 InClipIDNumber )
{
	// @todo: If we ever end up with more than a few clips, consider changing this to a hash
	for( int32 CurClipIndex = 0; CurClipIndex < ReplayClips.Num(); ++CurClipIndex )
	{
		UParticleSystemReplay* CurReplayClip = ReplayClips[ CurClipIndex ];
		if( CurReplayClip != NULL )
		{
			if( CurReplayClip->ClipIDNumber == InClipIDNumber )
			{
				// Found it!  We're done.
				return CurReplayClip;
			}
		}
	}

	// Not found
	return NULL;
}

UMaterialInstanceDynamic* UParticleSystemComponent::CreateNamedDynamicMaterialInstance(FName Name, class UMaterialInterface* SourceMaterial)
{
	int32 Index = GetNamedMaterialIndex(Name);
	if (INDEX_NONE == Index)
	{
		UE_LOG(LogParticles, Warning, TEXT("CreateNamedDynamicMaterialInstance on %s: This material wasn't found. Check the particle system's named material slots in cascade."), *GetPathName(), *Name.ToString());
		return NULL;
	}

	if (SourceMaterial)
	{
		SetMaterial(Index, SourceMaterial);
	}

	UMaterialInterface* MaterialInstance = GetMaterial(Index);
	UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(MaterialInstance);

	if (MaterialInstance && !MID)
	{
		// Create and set the dynamic material instance.
		MID = UMaterialInstanceDynamic::Create(MaterialInstance, this);
		SetMaterial(Index, MID);
	}
	else if (!MaterialInstance)
	{
		UE_LOG(LogParticles, Warning, TEXT("CreateDynamicMaterialInstance on %s: Material index %d is invalid."), *GetPathName(), Index);
	}

	return MID;
}

UMaterialInterface* UParticleSystemComponent::GetNamedMaterial(FName Name) const
{
	int32 Index = GetNamedMaterialIndex(Name);
	if (INDEX_NONE != Index)
	{
		if (EmitterMaterials.IsValidIndex(Index) && nullptr == EmitterMaterials[Index])
		{
			//Material has been overridden externally
			return EmitterMaterials[Index];
		}
		else
		{
			//This slot hasn't been overridden so just used the default.
			return Template->NamedMaterialSlots[Index].Material;
		}
	}
	//Could not find this named materials slot.
	return NULL;
}

int32 UParticleSystemComponent::GetNamedMaterialIndex(FName Name) const
{
	TArray<FNamedEmitterMaterial>& Slots = Template->NamedMaterialSlots;
	for (int32 SlotIdx = 0; SlotIdx < Slots.Num(); ++SlotIdx)
	{
		if (Name == Slots[SlotIdx].Name)
		{
			return SlotIdx;
		}
	}
	return INDEX_NONE;
}

UParticleSystemReplay::UParticleSystemReplay(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UParticleSystemReplay::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	// Serialize clip ID number
	Ar << ClipIDNumber;

	// Serialize our native members
	Ar << Frames;
}

/** FParticleSystemReplayFrame serialization operator */
FArchive& operator<<( FArchive& Ar, FParticleSystemReplayFrame& Obj )
{
	if( Ar.IsLoading() )
	{
		// Zero out the struct if we're loading from disk since we won't be cleared by default
		FMemory::Memzero( &Obj, sizeof( FParticleEmitterReplayFrame ) );
	}

	// Serialize emitter frames
	Ar << Obj.Emitters;

	return Ar;
}



/** FParticleEmitterReplayFrame serialization operator */
FArchive& operator<<( FArchive& Ar, FParticleEmitterReplayFrame& Obj )
{
	if( Ar.IsLoading() )
	{
		// Zero out the struct if we're loading from disk since we won't be cleared by default
		FMemory::Memzero( &Obj, sizeof( FParticleEmitterReplayFrame ) );
	}

	// Emitter type
	Ar << Obj.EmitterType;

	// Original emitter index
	Ar << Obj.OriginalEmitterIndex;

	if( Ar.IsLoading() )
	{
		switch( Obj.EmitterType )
		{
			case DET_Sprite:
				Obj.FrameState = new FDynamicSpriteEmitterReplayData();
				break;

			case DET_Mesh:
				Obj.FrameState = new FDynamicMeshEmitterReplayData();
				break;

			case DET_Beam2:
				Obj.FrameState = new FDynamicBeam2EmitterReplayData();
				break;

			case DET_Ribbon:
				Obj.FrameState = new FDynamicRibbonEmitterReplayData();
				break;

			case DET_AnimTrail:
				Obj.FrameState = new FDynamicTrailsEmitterReplayData();
				break;

			default:
				{
					// @todo: Support other particle types
					Obj.FrameState = NULL;
				}
				break;
		}
	}

	if( Obj.FrameState != NULL )
	{
		// Serialize this emitter frame state
		Obj.FrameState->Serialize( Ar );
	}

	return Ar;
}

AEmitterCameraLensEffectBase::AEmitterCameraLensEffectBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer
		.DoNotCreateDefaultSubobject(TEXT("Sprite"))
		.DoNotCreateDefaultSubobject(TEXT("ArrowComponent0"))
	)
{
	DistFromCamera = 90.0f;
	InitialLifeSpan = 10.0f;
	BaseFOV = 80.0f;
	bDestroyOnSystemFinish = true;

	GetParticleSystemComponent()->bOnlyOwnerSee = true;
	GetParticleSystemComponent()->SecondsBeforeInactive = 0.0f;
}

void AEmitterCameraLensEffectBase::UpdateLocation(const FVector& CamLoc, const FRotator& CamRot, float CamFOVDeg)
{
	FRotationMatrix M(CamRot);

	// the particle is FACING X being parallel to the Y axis.  So we just flip the entire thing to face toward the player who is looking down X
	const FVector& X = M.GetScaledAxis( EAxis::X );
	M.SetAxis(0, -X);
	M.SetAxis(1, -M.GetScaledAxis( EAxis::Y ));

	const FRotator& NewRot = M.Rotator();

	// base dist uses BaseFOV which is set on the indiv camera lens effect class
	const float DistAdjustedForFOV = DistFromCamera * ( FMath::Tan(float(BaseFOV*0.5f*PI/180.f)) / FMath::Tan(float(CamFOVDeg*0.5f*PI/180.f)) );

	//UE_LOG(LogParticles, Warning, TEXT("DistAdjustedForFOV: %f  BaseFOV: %f  CamFOVDeg: %f"), DistAdjustedForFOV, BaseFOV, CamFOVDeg );

	SetActorLocationAndRotation( CamLoc + X * DistAdjustedForFOV, NewRot, false );
}

void AEmitterCameraLensEffectBase::Destroyed()
{
	if (BaseCamera != NULL)
	{
		BaseCamera->RemoveCameraLensEffect(this);
	}
	Super::Destroyed();
}

void AEmitterCameraLensEffectBase::RegisterCamera(APlayerCameraManager* C)
{
	BaseCamera = C;
}

void AEmitterCameraLensEffectBase::NotifyRetriggered() {}

void AEmitterCameraLensEffectBase::PostInitializeComponents()
{
	GetParticleSystemComponent()->SetDepthPriorityGroup(SDPG_Foreground);
	Super::PostInitializeComponents();
	ActivateLensEffect();
}

void AEmitterCameraLensEffectBase::ActivateLensEffect()
{
	// only play the camera effect on clients
	check(GetWorld());
	if( GetNetMode() != NM_DedicatedServer )
	{
		UParticleSystem* PSToActuallySpawn;
		if( GetWorld()->GameState && GetWorld()->GameState->ShouldShowGore() )
		{
			PSToActuallySpawn = PS_CameraEffect;
		}
		else
		{
			PSToActuallySpawn = PS_CameraEffectNonExtremeContent;
		}

		if( PSToActuallySpawn != NULL )
		{
			SetTemplate( PS_CameraEffect );
		}
	}
}

#undef LOCTEXT_NAMESPACE

