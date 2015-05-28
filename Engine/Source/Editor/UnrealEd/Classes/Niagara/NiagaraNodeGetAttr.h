// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "NiagaraNodeGetAttr.generated.h"

UCLASS(MinimalAPI, Deprecated)
class UDEPRECATED_NiagaraNodeGetAttr : public UNiagaraNode
{
	GENERATED_UCLASS_BODY()

public:

	/** Name of attribute we are getting */
	UPROPERTY()
	FName	AttrName;

	// Begin EdGraphNode interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	// End EdGraphNode interface

	// Begin UNiagaraNode interface
	UNREALED_API virtual void Compile(class INiagaraCompiler* Compiler, TArray<FNiagaraNodeResult>& Output) override;
	// End UNiagaraNode interface
};

