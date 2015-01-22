// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Runtime/Engine/Classes/Particles/ParticleSystem.h"
#include "ParticleDefinitions.h"
#include "ParticleSystemAuditCommandlet.generated.h"

UCLASS(config=Editor)
class UParticleSystemAuditCommandlet : public UCommandlet
{
	GENERATED_UCLASS_BODY()

	/** Helper struct for tracking disabled LOD level issues */
	struct FParticleSystemLODInfo
	{
	public:
		/** The method of LOD updating on the particle system */
		ParticleSystemLODMethod LODMethod;
		/** Indices of emitters with disabled LODLevel mismatches */
		TArray<int32>	EmittersWithDisableLODMismatch;
		/** true if the PSys has emitters w/ inter looping mismatches */
		bool bHasInterLoopingMismatch;
		/** true if the PSys has emitters w/ intra looping mismatches */
		bool bHasIntraLoopingMismatch;

		FParticleSystemLODInfo() :
			  bHasInterLoopingMismatch(false)
			, bHasIntraLoopingMismatch(false)
		{
		}

		inline bool operator==(const FParticleSystemLODInfo& Src)
		{
			return (
				(LODMethod == Src.LODMethod) && 
				(EmittersWithDisableLODMismatch == Src.EmittersWithDisableLODMismatch) && 
				(bHasInterLoopingMismatch == Src.bHasInterLoopingMismatch) && 
				(bHasIntraLoopingMismatch == Src.bHasIntraLoopingMismatch)
				);
		}

		inline FParticleSystemLODInfo& operator=(const FParticleSystemLODInfo& Src)
		{
			LODMethod = Src.LODMethod;
			EmittersWithDisableLODMismatch.Empty();
			EmittersWithDisableLODMismatch = Src.EmittersWithDisableLODMismatch;
			bHasInterLoopingMismatch = Src.bHasInterLoopingMismatch;
			bHasIntraLoopingMismatch = Src.bHasIntraLoopingMismatch;
			return *this;
		}
	};

	/** Helper struct for tracking duplicate module info */
	struct FParticleSystemDuplicateModuleInfo
	{
	public:
		/** The number of modules the particle system has */
		int32 ModuleCount;
		/** The amount of memory used by those modules */
		int32 ModuleMemory;
		/** The number of modules the particle system *could* have w/ duplicates removed */
		int32 RemovedDuplicateCount;
		/** The amount of memory there would be w/ duplicates removed */
		int32 RemovedDuplicateMemory;

		FParticleSystemDuplicateModuleInfo() :
		ModuleCount(0)
			, ModuleMemory(0)
			, RemovedDuplicateCount(0)
			, RemovedDuplicateMemory(0)
		{
		}

		inline bool operator==(const FParticleSystemDuplicateModuleInfo& Src)
		{
			return (
				(ModuleCount == Src.ModuleCount) &&
				(ModuleMemory == Src.ModuleMemory) &&
				(RemovedDuplicateCount == Src.RemovedDuplicateCount) &&
				(RemovedDuplicateMemory == Src.RemovedDuplicateMemory)
				);
		}

		inline FParticleSystemDuplicateModuleInfo& operator=(const FParticleSystemDuplicateModuleInfo& Src)
		{
			ModuleCount = Src.ModuleCount;
			ModuleMemory = Src.ModuleMemory;
			RemovedDuplicateCount = Src.RemovedDuplicateCount;
			RemovedDuplicateMemory = Src.RemovedDuplicateMemory;
			return *this;
		}
	};

	/** All particle systems w/ a NO LOD levels */
	TSet<FString> ParticleSystemsWithNoLODs;
	/** All particle systems w/ a single LOD level */
	TSet<FString> ParticleSystemsWithSingleLOD;
	/** All particle systems w/out fixed bounds set */
	TSet<FString> ParticleSystemsWithoutFixedBounds;
	/** All particle systems w/ LOD Method of Automatic & a check time of 0.0 */
	TSet<FString> ParticleSystemsWithBadLODCheckTimes;
	/** All particle systems w/ bOrientZAxisTowardCamera enabled */
	TSet<FString> ParticleSystemsWithOrientZAxisTowardCamera;
	/** All particle systems w/ missing materials */
	TSet<FString> ParticleSystemsWithMissingMaterials;
	/** All particle systems w/ no emitters */
	TSet<FString> ParticleSystemsWithNoEmitters;
	/** All particle systems w/ collision on in at least one emitter */
	TSet<FString> ParticleSystemsWithCollisionEnabled;
	/** All particle systems w/ a color scale over life that is a constant */
	TSet<FString> ParticleSystemsWithConstantColorScaleOverLife;
	/** All particle systems w/ a high spawn rate or burst */
	TSet<FString> ParticleSystemsWithHighSpawnRateOrBurst;
	/** All particle systems w/ a far LODDistance */
	TSet<FString> ParticleSystemsWithFarLODDistance;
	/** All particle systems w/ a color scale over life that is a constant */
	TMap<FString,int32> ParticleSystemsWithConstantColorScaleOverLifeCounts;
	/** Particle systems w/ disabled LOD level matches */
	TMap<FString,FParticleSystemLODInfo> ParticleSystemsWithLODLevelIssues;
	/** Particle system duplicate module information */
	TMap<FString,FParticleSystemDuplicateModuleInfo> PSysDuplicateModuleInfo;

	/** If a particle system has a spawn rate or burst count greater than this value, it will be reported */
	UPROPERTY(config)
	float HighSpawnRateOrBurstThreshold;

	/** If a particle system has an LODDistance larger than this value, it will be reported */
	UPROPERTY(config)
	float FarLODDistanceTheshold;

	/** Entry point */
	int32 Main(const FString& Params) override;

	/** Process all referenced particle systems */
	bool ProcessParticleSystems();

	/** 
	 *	Check the given ParticleSystem for LOD mismatch issues
	 *
	 *	@param	InPSys		The particle system to check
	 */
	void CheckPSysForLODMismatches(UParticleSystem* InPSys);

	/**
	 *	Determine the given ParticleSystems duplicate module information 
	 *
	 *	@param	InPSys		The particle system to check
	 */
	void CheckPSysForDuplicateModules(UParticleSystem* InPSys);

	/** Dump the results of the audit */
	virtual void DumpResults();

	/**
	 *	Dump the give list of particle systems to an audit CSV file...
	 *
	 *	@param	InPSysSet		The particle system set to dump
	 *	@param	InFilename		The name for the output file (short name)
	 *
	 *	@return	bool			true if successful, false if not
	 */
	bool DumpSimplePSysSet(TSet<FString>& InPSysSet, const TCHAR* InShortFilename);

	/** Generic function to handle dumping values to a CSV file */
	bool DumpSimpleSet(TSet<FString>& InSet, const TCHAR* InShortFilename, const TCHAR* InObjectClassName);

	/** Gets an archive to write to an output file */
	FArchive* GetOutputFile(const TCHAR* InFolderName, const TCHAR* InShortFilename);
};
