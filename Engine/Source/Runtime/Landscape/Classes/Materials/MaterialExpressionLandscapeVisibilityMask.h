// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionLandscapeVisibilityMask.generated.h"

UCLASS(collapsecategories, hidecategories=Object)
class UMaterialExpressionLandscapeVisibilityMask : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	/** GUID that should be unique within the material, this is used for parameter renaming. */
	UPROPERTY()
	FGuid ExpressionGUID;

public:

	LANDSCAPE_API static FName ParameterName;

	// Begin UMaterialExpression Interface
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual UTexture* GetReferencedTexture() override;
	// End UMaterialExpression Interface

	LANDSCAPE_API virtual FGuid& GetParameterExpressionId() override;

	/**
	 * Called to get list of parameter names for static parameter sets
	 */
	void GetAllParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds) const;
};



