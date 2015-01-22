// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Particles/ParticleModule.h"
#include "ParticleModuleTypeDataBase.generated.h"

struct FParticleEmitterInstance;

UCLASS(editinlinenew, hidecategories=Object, abstract, MinimalAPI)
class UParticleModuleTypeDataBase : public UParticleModule
{
	GENERATED_UCLASS_BODY()


	// Begin UParticleModule Interface
	virtual EModuleType	GetModuleType() const override {	return EPMT_TypeData;	}
	// End UParticleModule Interface

	/**
	 * Build any resources required for simulating the emitter.
	 * @param EmitterBuildInfo - Information compiled for the emitter.
	 */
	virtual void Build( struct FParticleEmitterBuildInfo& EmitterBuildInfo ) {}

	/**
	 * Return whether the type data module requires a build step.
	 */
	virtual bool RequiresBuild() const { return false; }

	// @todo document
	virtual FParticleEmitterInstance* CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent);


	// @todo document
	virtual bool		SupportsSpecificScreenAlignmentFlags() const	{	return false;			}
	// @todo document
	virtual bool		SupportsSubUV() const	{ return false; }
	// @todo document
	virtual bool		IsAMeshEmitter() const	{ return false; }
};



