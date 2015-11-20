// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Particles/Location/ParticleModuleLocationBase.h"
#include "ParticleModuleSourceMovement.generated.h"

UCLASS(editinlinenew, hidecategories=Object, meta=(DisplayName = "Source Movement"))
class UParticleModuleSourceMovement : public UParticleModuleLocationBase
{
	GENERATED_UCLASS_BODY()

	/** 
	 *	The scale factor to apply to the source movement before adding to the particle location.
	 *	The value is looked up using the particles RELATIVE time [0..1].
	 */
	UPROPERTY(EditAnywhere, Category=SourceMovement)
	struct FRawDistributionVector SourceMovementScale;

	//Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	virtual void PostInitProperties() override;
	//End UObject Interface

	//Begin UParticleModule Interface
	virtual void FinalUpdate(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override;
	virtual bool CanTickInAnyThread() override
	{
		return false;
	}
	//End UParticleModule Interface

	/** Initializes the default values for this property */
	void InitializeDefaults();
};



