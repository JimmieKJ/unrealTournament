// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionCustomOutput.generated.h"

UCLASS(abstract,collapsecategories, hidecategories = Object, MinimalAPI)
class UMaterialExpressionCustomOutput : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	// Override to enable multiple outputs
	virtual int32 GetNumOutputs() const { return 1; };
	virtual FString GetFunctionName() const PURE_VIRTUAL(UMaterialExpressionCustomOutput::GetFunctionName, return TEXT("GetCustomOutput"););
};



