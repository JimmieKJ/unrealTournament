// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionGIReplace.generated.h"

UCLASS()
class UMaterialExpressionGIReplace : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	/** Used for direct lighting computations e.g. real-time shaders */
	UPROPERTY()
	FExpressionInput Default;

	/** Used for baked indirect lighting e.g. Lightmass */
	UPROPERTY()
	FExpressionInput StaticIndirect;

	/** Used for dynamic indirect lighting e.g. Light Propagation Volumes */
	UPROPERTY()
	FExpressionInput DynamicIndirect;

	// Begin UMaterialExpression Interface
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	// End UMaterialExpression Interface
};



