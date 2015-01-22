// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// ParticleModuleLocationEmitterDirect
//
// A location module that uses particles from another emitters particles as
// position for its particles.
//
//=============================================================================

#pragma once
#include "Particles/Location/ParticleModuleLocationBase.h"
#include "ParticleModuleLocationEmitterDirect.generated.h"

UCLASS(editinlinenew, hidecategories=Object, meta=(DisplayName = "Emitter Direct Location"))
class UParticleModuleLocationEmitterDirect : public UParticleModuleLocationBase
{
	GENERATED_UCLASS_BODY()

	/** The name of the emitter to use as a source for the location of the particles. */
	UPROPERTY(EditAnywhere, export, noclear, Category=Location)
	FName EmitterName;


	// Begin UParticleModule Interface
	virtual void	Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;
	virtual void	Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override;
	// End UParticleModule Interface

};



