// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionWorldPosition.generated.h"

/** Specifies which shader generated offsets should included in the world position (displacement/WPO etc.) */
UENUM()
enum EWorldPositionIncludedOffsets
{
	/** Absolute world position with all shader offsets applied */
	WPT_Default UMETA(DisplayName="Absolute World Position"),

	/** Absolute world position with no shader offsets applied */
	WPT_ExcludeAllShaderOffsets UMETA(DisplayName="Absolute World Position (No Shader Offsets)"),

	/** Camera relative world position with all shader offsets applied */
	WPT_CameraRelative UMETA(DisplayName="Camera Relative World Position"),

	/** Camera relative world position with no shader offsets applied */
	WPT_CameraRelativeNoOffsets UMETA(DisplayName="Camera Relative World Position (No Shader Offsets"),

	WPT_MAX
};

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UMaterialExpressionWorldPosition : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=UMaterialExpressionWorldPosition, meta=(DisplayName = "Shader Offsets"))
	TEnumAsByte<EWorldPositionIncludedOffsets> WorldPositionShaderOffset;
	
	// Begin UMaterialExpression Interface
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	// End UMaterialExpression Interface
};



