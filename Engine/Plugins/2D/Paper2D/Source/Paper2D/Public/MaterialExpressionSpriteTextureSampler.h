// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Runtime/Engine/Classes/Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "MaterialExpressionSpriteTextureSampler.generated.h"

// This is a texture sampler 2D with the parameter name fixed as 'SpriteTexture'. The texture specified here will be replaced by the SourceTexture of a Paper2D sprite if this material is used on a sprite.
UCLASS(hideCategories=(MaterialExpressionTextureSampleParameter, MaterialExpressionSpriteTextureSampler))
class PAPER2D_API UMaterialExpressionSpriteTextureSampler : public UMaterialExpressionTextureSampleParameter2D
{
	GENERATED_UCLASS_BODY()

	// UMaterialExpression interface
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#if WITH_EDITOR
	virtual FString GetKeywords() const override;
	virtual bool CanRenameNode() const override;
	virtual void GetExpressionToolTip(TArray<FString>& OutToolTip) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	// End of UMaterialExpression interface
};
