// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*==============================================================================
	ParticleModules_VectorField.cpp: Vector field module implementations.
==============================================================================*/

#include "EnginePrivate.h"
#include "FXSystem.h"
#include "ParticleDefinitions.h"
#include "Particles/TypeData/ParticleModuleTypeDataGpu.h"
#include "Particles/VectorField/ParticleModuleVectorFieldLocal.h"
#include "Particles/VectorField/ParticleModuleVectorFieldGlobal.h"
#include "Particles/VectorField/ParticleModuleVectorFieldRotation.h"
#include "Particles/VectorField/ParticleModuleVectorFieldRotationRate.h"
#include "Particles/VectorField/ParticleModuleVectorFieldScale.h"
#include "Particles/VectorField/ParticleModuleVectorFieldScaleOverLife.h"
#include "Particles/ParticleLODLevel.h"

/*------------------------------------------------------------------------------
	Global vector field scale.
------------------------------------------------------------------------------*/
UParticleModuleVectorFieldBase::UParticleModuleVectorFieldBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UParticleModuleVectorFieldGlobal::UParticleModuleVectorFieldGlobal(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UParticleModuleVectorFieldGlobal::CompileModule(FParticleEmitterBuildInfo& EmitterInfo)
{
	EmitterInfo.GlobalVectorFieldScale = GlobalVectorFieldScale;
	EmitterInfo.GlobalVectorFieldTightness = GlobalVectorFieldTightness;
}

/*------------------------------------------------------------------------------
	Per-particle vector field scale.
------------------------------------------------------------------------------*/
UParticleModuleVectorFieldScale::UParticleModuleVectorFieldScale(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UParticleModuleVectorFieldScale::InitializeDefaults()
{
	if (!VectorFieldScale)
	{
		UDistributionFloatConstant* DistributionVectorFieldScale = NewObject<UDistributionFloatConstant>(this, TEXT("DistributionVectorFieldScale"));
		DistributionVectorFieldScale->Constant = 1.0f;
		VectorFieldScale = DistributionVectorFieldScale;
	}
}
void UParticleModuleVectorFieldScale::PostInitProperties()
{
	Super::PostInitProperties();
	if (!HasAnyFlags(RF_ClassDefaultObject | RF_NeedLoad))
	{
		InitializeDefaults();
	}
}

#if WITH_EDITOR
void UParticleModuleVectorFieldScale::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	InitializeDefaults();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR

void UParticleModuleVectorFieldScale::CompileModule(FParticleEmitterBuildInfo& EmitterInfo)
{
	EmitterInfo.VectorFieldScale.ScaleByDistribution(VectorFieldScale);
}

#if WITH_EDITOR

bool UParticleModuleVectorFieldScale::IsValidForLODLevel(UParticleLODLevel* LODLevel, FString& OutErrorString)
{
	if (LODLevel->TypeDataModule && LODLevel->TypeDataModule->IsA(UParticleModuleTypeDataGpu::StaticClass()))
	{
		if(!IsDistributionAllowedOnGPU(VectorFieldScale))
		{
			OutErrorString = GetDistributionNotAllowedOnGPUText(StaticClass()->GetName(), "VectorFieldScale" ).ToString();
			return false;
		}
	}

	return true;
}

#endif
/*------------------------------------------------------------------------------
	Per-particle vector field scale over life.
------------------------------------------------------------------------------*/
UParticleModuleVectorFieldScaleOverLife::UParticleModuleVectorFieldScaleOverLife(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UParticleModuleVectorFieldScaleOverLife::InitializeDefaults()
{
	if (!VectorFieldScaleOverLife)
	{
		UDistributionFloatConstant* DistributionVectorFieldScaleOverLife = NewObject<UDistributionFloatConstant>(this, TEXT("DistributionVectorFieldScaleOverLife"));
		DistributionVectorFieldScaleOverLife->Constant = 1.0f;
		VectorFieldScaleOverLife = DistributionVectorFieldScaleOverLife;
	}
}

void UParticleModuleVectorFieldScaleOverLife::PostInitProperties()
{
	Super::PostInitProperties();
	if (!HasAnyFlags(RF_ClassDefaultObject | RF_NeedLoad))
	{
		InitializeDefaults();
	}
}

#if WITH_EDITOR
void UParticleModuleVectorFieldScaleOverLife::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	InitializeDefaults();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR

void UParticleModuleVectorFieldScaleOverLife::CompileModule(FParticleEmitterBuildInfo& EmitterInfo)
{
	EmitterInfo.VectorFieldScaleOverLife.ScaleByDistribution(VectorFieldScaleOverLife);
}

#if WITH_EDITOR

bool UParticleModuleVectorFieldScaleOverLife::IsValidForLODLevel(UParticleLODLevel* LODLevel, FString& OutErrorString)
{
	if (LODLevel->TypeDataModule && LODLevel->TypeDataModule->IsA(UParticleModuleTypeDataGpu::StaticClass()))
	{
		if(!IsDistributionAllowedOnGPU(VectorFieldScaleOverLife))
		{
			OutErrorString = GetDistributionNotAllowedOnGPUText(StaticClass()->GetName(), "VectorFieldScaleOverLife" ).ToString();
			return false;
		}
	}

	return true;
}

#endif
/*------------------------------------------------------------------------------
	Local vector fields.
------------------------------------------------------------------------------*/
UParticleModuleVectorFieldLocal::UParticleModuleVectorFieldLocal(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RelativeScale3D = FVector(1.0f, 1.0f, 1.0f);

	Intensity = 1.0;
	Tightness = 0.0;
}

void UParticleModuleVectorFieldLocal::CompileModule(FParticleEmitterBuildInfo& EmitterInfo)
{
	EmitterInfo.LocalVectorField = VectorField;
	EmitterInfo.LocalVectorFieldTransform.SetTranslation(RelativeTranslation);
	EmitterInfo.LocalVectorFieldTransform.SetRotation(RelativeRotation.Quaternion());
	EmitterInfo.LocalVectorFieldTransform.SetScale3D(RelativeScale3D);
	EmitterInfo.LocalVectorFieldIntensity = Intensity;
	EmitterInfo.LocalVectorFieldTightness = Tightness;
	EmitterInfo.bLocalVectorFieldIgnoreComponentTransform = bIgnoreComponentTransform;
	EmitterInfo.bLocalVectorFieldTileX = bTileX;
	EmitterInfo.bLocalVectorFieldTileY = bTileY;
	EmitterInfo.bLocalVectorFieldTileZ = bTileZ;
}

/*------------------------------------------------------------------------------
	Local vector initial rotation.
------------------------------------------------------------------------------*/
UParticleModuleVectorFieldRotation::UParticleModuleVectorFieldRotation(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UParticleModuleVectorFieldRotation::CompileModule(FParticleEmitterBuildInfo& EmitterInfo)
{
	EmitterInfo.LocalVectorFieldMinInitialRotation = MinInitialRotation;
	EmitterInfo.LocalVectorFieldMaxInitialRotation = MaxInitialRotation;
}

/*------------------------------------------------------------------------------
	Local vector field rotation rate.
------------------------------------------------------------------------------*/
UParticleModuleVectorFieldRotationRate::UParticleModuleVectorFieldRotationRate(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UParticleModuleVectorFieldRotationRate::CompileModule(FParticleEmitterBuildInfo& EmitterInfo)
{
	EmitterInfo.LocalVectorFieldRotationRate += RotationRate;
}
