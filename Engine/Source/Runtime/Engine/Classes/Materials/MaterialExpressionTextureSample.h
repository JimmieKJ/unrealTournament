// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Materials/MaterialExpressionTextureBase.h"
#include "MaterialExpressionTextureSample.generated.h"


/** defines how MipValue is used */
UENUM()
enum ETextureMipValueMode
{
	TMVM_None UMETA(DisplayName="None (use computed mip level)"),
	TMVM_MipLevel UMETA(DisplayName="MipLevel (absolute, 0 is full resolution)"),
	TMVM_MipBias UMETA(DisplayName="MipBias (relative to the computed mip level)"),
	TMVM_MAX,
};

UCLASS(collapsecategories, hidecategories=Object)
class ENGINE_API UMaterialExpressionTextureSample : public UMaterialExpressionTextureBase
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'ConstCoordinate' if not specified"))
	FExpressionInput Coordinates;

	/** 
	 * Texture object input which overrides Texture if specified. 
	 * This only shows up in material functions and is used to implement texture parameters without actually putting the texture parameter in the function.
	 */
	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'Texture' if not specified"))
	FExpressionInput TextureObject;

	/** Meaning depends on MipValueMode, a single unit is one mip level  */
	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'ConstMipValue' if not specified"))
	FExpressionInput MipValue;

	/** Defines how the MipValue property is applied to the texture lookup */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionTextureSample, meta=(DisplayName = "MipValueMode"))
	TEnumAsByte<enum ETextureMipValueMode> MipValueMode;

	/** 
	 * Controls where the sampler for this texture lookup will come from.  
	 * Choose 'from texture asset' to make use of the UTexture addressing settings,
	 * Otherwise use one of the global samplers, which will not consume a sampler slot.
	 * This allows materials to use more than 16 unique textures on SM5 platforms.
	 */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionTextureSample)
	TEnumAsByte<enum ESamplerSourceMode> SamplerSource;

	/** only used if Coordinates is not hooked up */
	UPROPERTY(EditAnywhere, Category = MaterialExpressionTextureSample)
	uint32 ConstCoordinate;

	/** only used if MipValue is not hooked up */
	UPROPERTY(EditAnywhere, Category = MaterialExpressionTextureSample)
	int32 ConstMipValue;

	// Begin UObject Interface
#if WITH_EDITOR
	virtual bool CanEditChange(const UProperty* InProperty) const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	// End UObject Interface

	// Begin UMaterialExpression Interface
	virtual const TArray<FExpressionInput*> GetInputs() override;
	virtual FExpressionInput* GetInput(int32 InputIndex) override;
	virtual FString GetInputName(int32 InputIndex) const override;
	virtual int32 GetWidth() const override;
	virtual int32 GetLabelPadding() override { return 8; }
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual bool MatchesSearchQuery( const TCHAR* SearchQuery ) override;
#if WITH_EDITOR
	virtual uint32 GetInputType(int32 InputIndex) override;
#endif
	// End UMaterialExpression Interface

	void UpdateTextureResource(class UTexture* Texture);
};



