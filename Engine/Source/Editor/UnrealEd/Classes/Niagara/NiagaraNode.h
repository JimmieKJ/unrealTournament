// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "NiagaraNode.generated.h"

UCLASS()
class UNREALED_API UNiagaraNode : public UEdGraphNode
{
	GENERATED_UCLASS_BODY()

public:

	/** Get the Niagara graph that owns this node */
	const class UNiagaraGraph* GetNiagaraGraph()const;
	class UNiagaraGraph* GetNiagaraGraph();

	/** Get the source object */
	class UNiagaraScriptSource* GetSource()const;

	virtual void Compile(class INiagaraCompiler* Compiler, TArray<FNiagaraNodeResult>& Outputs){ check(0); }

	UEdGraphPin* GetInputPin(int32 InputIndex) const;
	void GetInputPins(TArray<class UEdGraphPin*>& OutInputPins) const;
	UEdGraphPin* GetOutputPin(int32 OutputIndex) const;
	void GetOutputPins(TArray<class UEdGraphPin*>& OutOutputPins) const;

};
