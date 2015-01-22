// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// ParticleModuleLocationEmitter
//
// A location module that uses particles from another emitters particles as
// spawn points for its particles.
//
//=============================================================================

#pragma once
#include "Particles/Location/ParticleModuleLocationBase.h"
#include "ParticleModuleLocationEmitter.generated.h"

UENUM()
enum ELocationEmitterSelectionMethod
{
	ELESM_Random,
	ELESM_Sequential,
	ELESM_MAX,
};

UCLASS(editinlinenew, hidecategories=Object, MinimalAPI, meta=(DisplayName = "Emitter Initial Location"))
class UParticleModuleLocationEmitter : public UParticleModuleLocationBase
{
	GENERATED_UCLASS_BODY()

	//=============================================================================
	// Variables
	//=============================================================================
	// LocationEmitter
	
	/** The name of the emitter to use that the source location for particle. */
	UPROPERTY(EditAnywhere, export, noclear, Category=Location)
	FName EmitterName;

	/** 
	 *	The method to use when selecting a spawn target particle from the emitter.
	 *	Can be one of the following:
	 *		ELESM_Random		Randomly select a particle from the source emitter.
	 *		ELESM_Sequential	Step through each particle from the source emitter in order.
	 */
	UPROPERTY(EditAnywhere, Category=Location)
	TEnumAsByte<enum ELocationEmitterSelectionMethod> SelectionMethod;

	/** If true, the spawned particle should inherit the velocity of the source particle. */
	UPROPERTY(EditAnywhere, Category=Location)
	uint32 InheritSourceVelocity:1;

	/** Amount to scale the source velocity by when inheriting it. */
	UPROPERTY(EditAnywhere, Category=Location)
	float InheritSourceVelocityScale;

	/** If true, the spawned particle should inherit the rotation of the source particle. */
	UPROPERTY(EditAnywhere, Category=Location)
	uint32 bInheritSourceRotation:1;

	/** Amount to scale the source rotation by when inheriting it. */
	UPROPERTY(EditAnywhere, Category=Location)
	float InheritSourceRotationScale;


	// Begin UParticleModule Interface
	virtual void	Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;
	virtual uint32	RequiredBytesPerInstance(FParticleEmitterInstance* Owner = NULL) override;
	// End UParticleModule Interface
};



