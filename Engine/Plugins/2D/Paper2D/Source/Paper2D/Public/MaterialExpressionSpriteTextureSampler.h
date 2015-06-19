// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Runtime/Engine/Classes/Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "MaterialExpressionSpriteTextureSampler.generated.h"

// This is a texture sampler 2D with a special automatically defined parameter name. The texture specified here will be replaced by the SourceTexture or an AdditionalSourceTextures entry of a Paper2D sprite if this material is used on a sprite.
UCLASS(hideCategories=(MaterialExpressionTextureSampleParameter, MaterialExpressionSpriteTextureSampler))
class PAPER2D_API UMaterialExpressionSpriteTextureSampler : public UMaterialExpressionTextureSampleParameter2D
{
	GENERATED_UCLASS_BODY()

	// Is this a sampler for the default SourceTexture or the AdditionalSourceTextures list?
	UPROPERTY(EditAnywhere, Category=Paper2D)
	bool bSampleAdditionalTextures;

	// This is the slot index into the AdditionalSourceTextures array
	UPROPERTY(EditAnywhere, Category=Paper2D, meta=(EditCondition=bSampleAdditionalTextures))
	int32 AdditionalSlotIndex;

	// Friendly label for the texture slot, displayed in the Sprite Editor if not empty
	UPROPERTY(EditAnywhere, Category=Paper2D)
	FText SlotDisplayName;

	// UMaterialExpression interface
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#if WITH_EDITOR
	virtual FText GetKeywords() const override;
	virtual bool CanRenameNode() const override;
	virtual void GetExpressionToolTip(TArray<FString>& OutToolTip) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	// End of UMaterialExpression interface
};
