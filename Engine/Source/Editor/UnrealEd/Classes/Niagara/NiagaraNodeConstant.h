// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraCommon.h"
#include "NiagaraNodeConstant.generated.h"

UCLASS(MinimalAPI, Deprecated)
class UDEPRECATED_NiagaraNodeConstant : public UNiagaraNode
{
	GENERATED_UCLASS_BODY()

public:
	/** The type of the constant we're creating. */
	UPROPERTY(EditAnywhere, meta = (DisplayName = "Name"), Category="Constant")
	FName ConstName;

	/** The type of the constant we're creating. */
	UPROPERTY(EditAnywhere, Category = "Constant")
	ENiagaraDataType DataType;
	
	UPROPERTY(EditAnywhere, Category = "Constant")
	uint32 bNeedsDefault : 1;

	UPROPERTY(EditAnywhere, Category = "Constant")
	uint32 bExposeToEffectEditor : 1;

	bool IsExposedToEffectEditor()	{ return bExposeToEffectEditor; }

	// Begin UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	// End UObject interface
	
	// Begin EdGraphNode interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	// End EdGraphNode interface

	// Begin UNiagaraNode interface
	UNREALED_API virtual void Compile(class INiagaraCompiler* Compiler, TArray<FNiagaraNodeResult>& Outputs) override;
	// End UNiagaraNode interface
};

