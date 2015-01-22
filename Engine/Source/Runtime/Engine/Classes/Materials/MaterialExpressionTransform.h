// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionTransform.generated.h"

UENUM()
enum EMaterialVectorCoordTransformSource
{
	TRANSFORMSOURCE_World UMETA(DisplayName="World"),
	TRANSFORMSOURCE_Local UMETA(DisplayName="Local"),
	TRANSFORMSOURCE_Tangent UMETA(DisplayName="Tangent"),
	TRANSFORMSOURCE_View UMETA(DisplayName="View"),
	TRANSFORMSOURCE_MAX,
};

UENUM()
enum EMaterialVectorCoordTransform
{
	TRANSFORM_World UMETA(DisplayName="World"),
	TRANSFORM_View UMETA(DisplayName="View"),
	TRANSFORM_Local UMETA(DisplayName="Local"),
	TRANSFORM_Tangent UMETA(DisplayName="Tangent"),
	TRANSFORM_MAX,
};

UCLASS(collapsecategories, hidecategories=Object)
class UMaterialExpressionTransform : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	/** input expression for this transform */
	UPROPERTY()
	FExpressionInput Input;

	/** Source coordinate space of the FVector */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionTransform, meta=(DisplayName = "Source"))
	TEnumAsByte<enum EMaterialVectorCoordTransformSource> TransformSourceType;

	/** Destination coordinate space of the FVector */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionTransform, meta=(DisplayName = "Destination"))
	TEnumAsByte<enum EMaterialVectorCoordTransform> TransformType;


	// Begin UMaterialExpression Interface
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	// End UMaterialExpression Interface
};



