// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*==============================================================================
	ParticleModuleVectorFieldScale: Per-particle vector field scale.
==============================================================================*/

#pragma once
#include "Particles/VectorField/ParticleModuleVectorFieldBase.h"
#include "ParticleModuleVectorFieldScale.generated.h"

UCLASS(editinlinenew, hidecategories=Object, MinimalAPI, meta=(DisplayName = "Vector Field Scale"))
class UParticleModuleVectorFieldScale : public UParticleModuleVectorFieldBase
{
	GENERATED_UCLASS_BODY()

	/** Per-particle vector field scale. Evaluated using emitter time. */
	UPROPERTY(EditAnywhere, Category=VectorField)
	class UDistributionFloat* VectorFieldScale;

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
	friend class FParticleModuleVectorFieldScaleDetails;
};



