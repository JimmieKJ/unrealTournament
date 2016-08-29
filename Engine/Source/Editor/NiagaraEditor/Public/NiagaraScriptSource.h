// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "NiagaraScriptSourceBase.h"
#include "NiagaraScriptSource.generated.h"


UCLASS(MinimalAPI)
class UNiagaraScriptSource : public UNiagaraScriptSourceBase
{
	GENERATED_UCLASS_BODY()

	/** Graph for particle update expression */
	UPROPERTY()
	class UNiagaraGraph*	NodeGraph;

	/** The same node graph from above but with all function calls merge into a single graph. */
	UPROPERTY()
	class UNiagaraGraph*	FlattenedNodeGraph;

	// UObject interface.
	virtual void PostLoad() override;
	virtual void Compile() override;

	void GetEmitterAttributes(TArray<FName>& VectorInputs, TArray<FName>& MatrixInputs);
};
