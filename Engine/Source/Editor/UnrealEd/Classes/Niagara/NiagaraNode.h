// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "NiagaraNode.generated.h"

UCLASS()
class UNREALED_API UNiagaraNode : public UEdGraphNode
{
	GENERATED_UCLASS_BODY()

public:

	/** Get the Niagara graph that owns this node */
	class UNiagaraGraph* GetNiagaraGraph();

	/** Get the source object */
	class UNiagaraScriptSource* GetSource();

	virtual void Compile(class INiagaraCompiler* Compiler, TArray<FNiagaraNodeResult>& Outputs){ check(0); }
};
