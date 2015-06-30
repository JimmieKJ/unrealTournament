// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionViewProperty.generated.h"

UENUM()
enum EMaterialExposedViewProperty
{	
	/** Horizontal and vertical fields of view in radian */
	MEVP_FieldOfView UMETA(DisplayName="FieldOfView"),
	/** Horizontal and vertical size of the view in pixels */
	MEVP_ViewSize UMETA(DisplayName="ViewSize"),
	/** Horizontal and vertical size of view's render target's texel in the uv space (1 / ViewSize) */
	MEVP_TexelSize UMETA(DisplayName="TexelSize"),

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
