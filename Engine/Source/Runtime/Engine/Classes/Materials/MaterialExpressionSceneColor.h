// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionSceneColor.generated.h"

UENUM()
namespace EMaterialSceneAttributeInputMode
{
	enum Type
	{
		Coordinates,
		OffsetFraction
	};
}

UCLASS(collapsecategories, hidecategories=Object)
class UMaterialExpressionSceneColor : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()
	
	/**
	* Coordinates - UV coordinates to apply to the scene color lookup.
	* OffsetFraction - 	An offset to apply to the scene color lookup in a 2d fraction of the screen.
	*/ 
	UPROPERTY(EditAnywhere, Category=MaterialExpressionSceneColor)
	TEnumAsByte<enum EMaterialSceneAttributeInputMode::Type> InputMode;

	/**
	* Based on the input mode the input will be treated as either:
	* UV coordinates to apply to the scene color lookup or 
	* an offset to apply to the scene color lookup, in a 2d fraction of the screen.
	*/
	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'ConstInput' if not specified"))
	FExpressionInput Input;

	UPROPERTY()
	FExpressionInput OffsetFraction_DEPRECATED;

	/** only used if Input is not hooked up */
	UPROPERTY(EditAnywhere, Category = MaterialExpressionSceneColor)
	FVector2D ConstInput;

	// Begin UObject interface.
	virtual void PostLoad() override;
	// End UObject interface.

	// Begin UMaterialExpression Interface
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual FString GetInputName(int32 InputIndex) const override;
	// End UMaterialExpression Interface
};



