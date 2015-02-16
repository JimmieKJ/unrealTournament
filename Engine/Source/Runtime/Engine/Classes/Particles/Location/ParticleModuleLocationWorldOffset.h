// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Particles/Location/ParticleModuleLocation.h"
#include "ParticleModuleLocationWorldOffset.generated.h"

UCLASS(editinlinenew, meta=(DisplayName = "World Offset"))
class UParticleModuleLocationWorldOffset : public UParticleModuleLocation
{
	GENERATED_UCLASS_BODY()


protected:
	//Begin UParticleModuleLocation Interface
	virtual void SpawnEx(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, struct FRandomStream* InRandomStream, FBaseParticle* ParticleBase) override;
	//End UParticleModuleLocation Interface
};

