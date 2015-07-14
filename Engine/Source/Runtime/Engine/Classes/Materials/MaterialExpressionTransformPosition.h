// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionTransformPosition.generated.h"

UENUM()
enum EMaterialPositionTransformSource
{
	/** Local space */
	TRANSFORMPOSSOURCE_Local UMETA(DisplayName="Local Space"),

	/** Absolute World Space */
	TRANSFORMPOSSOURCE_World UMETA(DisplayName="Absolute World Space"),

	/** Camera Relative World Space for better precision */
	TRANSFORMPOSSOURCE_TranslatedWorld  UMETA(DisplayName="Camera Relative World Space"),

	/** View space */
	TRANSFORMPOSSOURCE_View  UMETA(DisplayName="View Space"),

	TRANSFORMPOSSOURCE_MAX,
};

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UMaterialExpressionTransformPosition : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	/** input expression for this transform */
	UPROPERTY()
	FExpressionInput Input;

	/** source format of the position that will be transformed */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionTransformPosition, meta=(DisplayName = "Source"))
	TEnumAsByte<enum EMaterialPositionTransformSource> TransformSourceType;

	/** type of transform to apply to the input expression */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionTransformPosition, meta=(DisplayName = "Destination"))
	TEnumAsByte<enum EMaterialPositionTransformSource> TransformType;


	// Begin UMaterialExpression Interface
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	// End UMaterialExpression Interface
};



