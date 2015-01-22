// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionReflectionVectorWS.generated.h"

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UMaterialExpressionReflectionVectorWS : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	/** Optional world normal to reflect the camera view vector about. If unconnected, pixel normal is used */
	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to Reflection Vector if not specified"))
	FExpressionInput CustomWorldNormal;

	/** (true): The specified world normal will be normalized. (false): WorldNormal will just be used as is, faster but possible artifacts if normal length isn't 1 */
	UPROPERTY(EditAnywhere, Category=UMaterialExpressionReflectionVectorWS, meta=(DisplayName = "Normalize custom world normal"))
	uint32 bNormalizeCustomWorldNormal : 1;

	// Begin UMaterialExpression Interface
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	// End UMaterialExpression Interface
};



