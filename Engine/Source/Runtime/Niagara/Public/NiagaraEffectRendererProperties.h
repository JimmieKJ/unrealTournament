// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraEffectRendererProperties.generated.h"

/**
* Emitter properties base class
* Each Effectrenderer derives from this with its own class, and returns it in GetProperties; a copy
* of those specific properties is stored on UNiagaraEmitterProperties (on the effect) for serialization 
* and handed back to the effect renderer on load.
*/
UCLASS()
class NIAGARA_API UNiagaraEffectRendererProperties : public UObject
{
	GENERATED_UCLASS_BODY()
	UNiagaraEffectRendererProperties()
	{}

	UPROPERTY()
	FName dummy;
};


