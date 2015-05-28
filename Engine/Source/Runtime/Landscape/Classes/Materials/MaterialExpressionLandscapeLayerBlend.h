// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionLandscapeLayerBlend.generated.h"

UENUM()
enum ELandscapeLayerBlendType
{
	LB_WeightBlend,
	LB_AlphaBlend,
	LB_HeightBlend,
	LB_MAX,
};

USTRUCT()
struct FLayerBlendInput
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=LayerBlendInput)
	FName LayerName;

	UPROPERTY(EditAnywhere, Category=LayerBlendInput)
	TEnumAsByte<ELandscapeLayerBlendType> BlendType;

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'ConstLayerInput' if not specified"))
	FExpressionInput LayerInput;

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'ConstHeightInput' if not specified"))
	FExpressionInput HeightInput;

	UPROPERTY(EditAnywhere, Category=LayerBlendInput)
	float PreviewWeight;

	/** only used if LayerInput is not hooked up */
	UPROPERTY(EditAnywhere, Category = LayerBlendInput)
	FVector ConstLayerInput;

	/** only used if HeightInput is not hooked up */
	UPROPERTY(EditAnywhere, Category=LayerBlendInput)
	float ConstHeightInput;

	FLayerBlendInput()
		: BlendType(0)
		, PreviewWeight(0.0f)
		, ConstLayerInput(0.0f, 0.0f, 0.0f)
		, ConstHeightInput(0.0f)
	{
	}
};

UCLASS(collapsecategories, hidecategories=Object)
class UMaterialExpressionLandscapeLayerBlend : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=MaterialExpressionLandscapeLayerBlend)
	TArray<FLayerBlendInput> Layers;

	/** GUID that should be unique within the material, this is used for parameter renaming. */
	UPROPERTY()
	FGuid ExpressionGUID;


	// Begin UObject Interface
	void Serialize(FArchive& Ar) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	// End UObject Interface

	// Begin UMaterialExpression Interface
#endif
	virtual bool IsResultMaterialAttributes(int32 OutputIndex) override;
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual const TArray<FExpressionInput*> GetInputs() override;
	virtual FExpressionInput* GetInput(int32 InputIndex) override;
	virtual FString GetInputName(int32 InputIndex) const override;
	virtual UTexture* GetReferencedTexture() override;
	// End UMaterialExpression Interface

	LANDSCAPE_API virtual FGuid& GetParameterExpressionId() override;

	/**
	 * Get list of parameter names for static parameter sets
	 */
	void GetAllParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds) const;
};



