// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/**
 *	ParticleModuleTrailBase
 *	Provides the base class for Trail emitter modules
 *
 */

#pragma once
#include "Particles/ParticleModule.h"
#include "ParticleModuleTrailBase.generated.h"

UCLASS(editinlinenew, hidecategories=Object, abstract, meta=(DisplayName = "Trail"))
class UParticleModuleTrailBase : public UParticleModule
{
	GENERATED_UCLASS_BODY()


	//~ Begin UParticleModule Interface
	virtual EModuleType	GetModuleType() const override {	return EPMT_Trail;	}
	virtual bool CanTickInAnyThread() override
	{
		return true;	
	}
	//~ End UParticleModule Interface
};



