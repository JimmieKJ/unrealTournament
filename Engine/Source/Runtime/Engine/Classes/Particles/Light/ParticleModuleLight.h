// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "Particles/Light/ParticleModuleLightBase.h"
#include "ParticleModuleLight.generated.h"

UCLASS(editinlinenew, hidecategories=Object, MinimalAPI, meta=(DisplayName = "Light"))
class UParticleModuleLight : public UParticleModuleLightBase
{
	GENERATED_UCLASS_BODY()

	/** Whether to use physically based inverse squared falloff from the light.  If unchecked, the LightExponent distribution will be used instead. */
	UPROPERTY(EditAnywhere, Category=Light)
	bool bUseInverseSquaredFalloff;

	/** 
	 * Whether lights from this module should affect translucency.
	 * Use with caution.  Modules enabling this should only make a few particle lights at most, and the smaller they are, the less they will cost.
	 */
	UPROPERTY(EditAnywhere, Category=Light)
	bool bAffectsTranslucency;

	/** 
	 * Will draw wireframe spheres to preview the light radius if enabled.
	 * Note: this is intended for previewing and the value will not be saved, it will always revert to disabled.
	 */
	UPROPERTY(EditAnywhere, Transient, Category=Light)
	bool bPreviewLightRadius;

	/** Fraction of particles in this emitter to create lights on. */
	UPROPERTY(EditAnywhere, Category=Light)
	float SpawnFraction;

	/** Scale that is applied to the particle's color to calculate the light's color, and can be setup as a curve over the particle's lifetime. */
	UPROPERTY(EditAnywhere, Category=Light)
	struct FRawDistributionVector ColorScaleOverLife;

	/** Brightness scale for the light, which can be setup as a curve over the particle's lifetime. */
	UPROPERTY(EditAnywhere, Category=Light)
	struct FRawDistributionFloat BrightnessOverLife;

	/** Scales the particle's radius, to calculate the light's radius. */
	UPROPERTY(EditAnywhere, Category=Light)
	struct FRawDistributionFloat RadiusScale;

	/** Provides the light's exponent when inverse squared falloff is disabled. */
	UPROPERTY(EditAnywhere, Category=Light)
	struct FRawDistributionFloat LightExponent;

	UPROPERTY(EditAnywhere, Category = Light)
	bool bHighQualityLights;

	UPROPERTY(EditAnywhere, Category = Light)
	bool bShadowCastingLights;	

	/** Initializes the default values for this property */
	void InitializeDefaults();

	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	virtual void PostInitProperties() override;
	//~ End UObject Interface


	//Begin UParticleModule Interface
	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;
	virtual void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override;
	virtual uint32 RequiredBytes(UParticleModuleTypeDataBase* TypeData) override;
	virtual void SetToSensibleDefaults(UParticleEmitter* Owner) override;
	virtual EModuleType	GetModuleType() const override { return EPMT_Light; }
	virtual void Render3DPreview(FParticleEmitterInstance* Owner, const FSceneView* View,FPrimitiveDrawInterface* PDI) override;
	//End UParticleModule Interface

	void SpawnEx(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, struct FRandomStream* InRandomStream, FBaseParticle* ParticleBase);

	virtual bool CanTickInAnyThread() override;

private:
		
	uint64 SpawnHQLight(const FLightParticlePayload& Payload, const FBaseParticle& Particle, FParticleEmitterInstance* Owner);	
	void UpdateHQLight(UPointLightComponent* PointLightComponent, const FLightParticlePayload& Payload, const FBaseParticle& Particle, int32 ScreenAlignment, FVector ComponentScale, bool bLocalSpace, FSceneInterface* OwnerScene, bool bDoRTUpdate);
};



