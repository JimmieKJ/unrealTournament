// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Engine/NiagaraScriptSourceBase.h"
#include "NiagaraScriptSource.generated.h"


UCLASS(MinimalAPI)
class UNiagaraScriptSource : public UNiagaraScriptSourceBase
{
	GENERATED_UCLASS_BODY()

	/** Graph for particle update expression */
	UPROPERTY()
	class UNiagaraGraph*	UpdateGraph;

	// UObject interface.
	virtual void PostLoad() override;
	virtual void Compile() override;

	UNREALED_API void GetParticleAttributes(TArray<FName>& VectorOutputs);
	UNREALED_API void GetEmitterAttributes(TArray<FName>& VectorInputs, TArray<FName>& MatrixInputs);
};
