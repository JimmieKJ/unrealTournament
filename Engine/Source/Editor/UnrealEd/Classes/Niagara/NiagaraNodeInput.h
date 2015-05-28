// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "NiagaraNodeInput.generated.h"

UCLASS(MinimalAPI)
class UNiagaraNodeInput : public UNiagaraNode
{
	GENERATED_UCLASS_BODY()

	void ReallocatePins();

public:

	UPROPERTY(EditAnywhere, Category = Input)
	FNiagaraVariableInfo Input;

	//TODO: Customize the details for this and hide these when they're not relevant.

	UPROPERTY(EditAnywhere, Category = Constant)
	float FloatDefault;

	UPROPERTY(EditAnywhere, Category = Constant)
	FVector4 VectorDefault;

	UPROPERTY(EditAnywhere, Category = Constant)
	FMatrix MatrixDefault;

	/** Allows code to explicitly disable exposing of certain inputs e.g. system constants such as Delta Time. */
	UPROPERTY()
	uint32 bCanBeExposed:1;

	/** When true, and this input is a constant, the input is exposed to the effect editor. */
	UPROPERTY(EditAnywhere, Category = Constant, meta = (editcondition = "bCanBeExposed"))
	uint32 bExposeWhenConstant:1;

	// Begin UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	// End UObject interface

	// Begin EdGraphNode interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	// End EdGraphNode interface

	virtual void Compile(class INiagaraCompiler* Compiler, TArray<FNiagaraNodeResult>& Outputs)override;
	
	bool IsConstant()const;
	bool IsExposedConstant()const;

	static const FLinearColor TitleColor_Attribute;
	static const FLinearColor TitleColor_Constant;
};

