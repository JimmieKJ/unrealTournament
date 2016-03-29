// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionRotateAboutAxis.generated.h"

UCLASS(MinimalAPI)
class UMaterialExpressionRotateAboutAxis : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	FExpressionInput NormalizedRotationAxis;

	UPROPERTY()
	FExpressionInput RotationAngle;

	UPROPERTY()
	FExpressionInput PivotPoint;

	UPROPERTY()
	FExpressionInput Position;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionRotateAboutAxis)
	float Period;


	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif
	//~ End UMaterialExpression Interface

};



