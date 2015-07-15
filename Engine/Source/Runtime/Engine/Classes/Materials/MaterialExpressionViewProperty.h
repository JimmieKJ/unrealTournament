// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionViewProperty.generated.h"

UENUM()
enum EMaterialExposedViewProperty
{	
	/** Horizontal and vertical size of the view's buffer in pixels */
	MEVP_BufferSize UMETA(DisplayName="RenderTargetSize"),
	/** Horizontal and vertical field of view angles in radian */
	MEVP_FieldOfView UMETA(DisplayName="FieldOfView"),
	/** Tan(FieldOfView * 0.5) */
	MEVP_TanHalfFieldOfView UMETA(DisplayName="TanHalfFieldOfView"),
	/** Horizontal and vertical size of the view in pixels */
	MEVP_ViewSize UMETA(DisplayName="ViewSize"),
	/** Absolute world space camera position */
	MEVP_WorldSpaceCameraPosition UMETA(DisplayName="CameraPosition (Absolute World Space)"),

	MEVP_MAX,
};

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UMaterialExpressionViewProperty : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()
	
	/** View input property to be accessed */
	UPROPERTY(EditAnywhere, Category=UMaterialExpressionViewProperty, meta=(DisplayName = "View Property"))
	TEnumAsByte<EMaterialExposedViewProperty> Property;
	
	// Begin UMaterialExpression Interface
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	// End UMaterialExpression Interface
};
