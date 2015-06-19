// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Materials/MaterialExpressionParameter.h"
#include "MaterialExpressionStaticComponentMaskParameter.generated.h"

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UMaterialExpressionStaticComponentMaskParameter : public UMaterialExpressionParameter
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	FExpressionInput Input;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionStaticComponentMaskParameter)
	uint32 DefaultR:1;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionStaticComponentMaskParameter)
	uint32 DefaultG:1;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionStaticComponentMaskParameter)
	uint32 DefaultB:1;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionStaticComponentMaskParameter)
	uint32 DefaultA:1;


public:

	// Begin UMaterialExpression Interface
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	// End UMaterialExpression Interface

	/** Return whether this is the named parameter, and fill in its value */
	bool IsNamedParameter(FName InParameterName, bool& OutR, bool& OutG, bool& OutB, bool& OutA, FGuid&OutExpressionGuid) const;
};



