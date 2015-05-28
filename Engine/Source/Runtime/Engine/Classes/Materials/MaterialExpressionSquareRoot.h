// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionSquareRoot.generated.h"

UCLASS(collapsecategories, hidecategories=Object)
class UMaterialExpressionSquareRoot : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	FExpressionInput Input;


	// Begin UMaterialExpression Interface
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#if WITH_EDITOR
	virtual FText GetKeywords() const override {return FText::FromString(TEXT("sqrt"));}
#endif // WITH_EDITOR
	// End UMaterialExpression Interface
};



