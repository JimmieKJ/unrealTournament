// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*==============================================================================
	ParticleModuleVectorFieldScaleOverLife: Per-particle vector field scale over life.
==============================================================================*/

#pragma once
#include "Particles/VectorField/ParticleModuleVectorFieldBase.h"
#include "ParticleModuleVectorFieldScaleOverLife.generated.h"

UCLASS(editinlinenew, hidecategories=Object, MinimalAPI, meta=(DisplayName = "VF Scale/Life"))
class UParticleModuleVectorFieldScaleOverLife : public UParticleModuleVectorFieldBase
{
	GENERATED_UCLASS_BODY()

	/** Per-particle vector field scale. Evaluated using particle relative time. */
	UPROPERTY(EditAnywhere, Category=VectorField)
	class UDistributionFloat* VectorFieldScaleOverLife;

	// Begin UObject Interface
	virtual void PostInitProperties() override;
	// End UObject Interface

	// Begin UParticleModule Interface
	virtual void CompileModule(struct FParticleEmitterBuildInfo& EmitterInfo) override;

#if WITH_EDITOR
	virtual bool IsValidForLODLevel(UParticleLODLevel* LODLevel, FString& OutErrorString) override;
#endif
	// End UParticleModule Interface

protected:
	friend class FParticleModuleVectorFieldScaleOverLifeDetails;
};



