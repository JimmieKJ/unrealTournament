// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "NiagaraNode.h"
#include "NiagaraNodeWriteDataSet.generated.h"

UCLASS(MinimalAPI)
class UNiagaraNodeWriteDataSet : public UNiagaraNode
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY(EditAnywhere, Category = DataSet)
	FNiagaraDataSetID DataSet;

	UPROPERTY(EditAnywhere, Category = Variables)
	TArray<FNiagaraVariableInfo> Variables;

	//~ Begin UObject Interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject Interface

	//~ Begin EdGraphNode Interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	//~ End EdGraphNode Interface

	virtual void Compile(class INiagaraCompiler* Compiler, TArray<FNiagaraNodeResult>& Outputs)override;
};

