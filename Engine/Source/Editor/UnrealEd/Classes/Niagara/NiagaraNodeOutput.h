// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "NiagaraNodeOutput.generated.h"

UCLASS(MinimalAPI)
class UNiagaraNodeOutput : public UNiagaraNode
{
	GENERATED_UCLASS_BODY()

	void ReallocatePins();
public:

	UPROPERTY(EditAnywhere, Category = Output)
	TArray<FNiagaraVariableInfo> Outputs;

	// Begin UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	// End UObject interface

	// Begin EdGraphNode interface
	virtual void AllocateDefaultPins() override;
	virtual bool CanUserDeleteNode() const override { return false; }
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	// End EdGraphNode interface

	virtual void Compile(class INiagaraCompiler* Compiler, TArray<FNiagaraNodeResult>& OutputExpressions)override;
};

