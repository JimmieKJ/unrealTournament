// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Materials/MaterialExpression.h"

#include "MaterialExpressionLandscapeLayerSample.generated.h"

UCLASS(collapsecategories, hidecategories=Object)
class UMaterialExpressionLandscapeLayerSample : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=MaterialExpressionLandscapeLayerWeight)
	FName ParameterName;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionLandscapeLayerWeight)
	float PreviewWeight;

	/** GUID that should be unique within the material, this is used for parameter renaming. */
	UPROPERTY()
	FGuid ExpressionGUID;

public:

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



