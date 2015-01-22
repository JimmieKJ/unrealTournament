// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DPrivatePCH.h"
#include "MaterialExpressionSpriteTextureSampler.h"

#define LOCTEXT_NAMESPACE "Paper2D"

//////////////////////////////////////////////////////////////////////////
// UMaterialExpressionSpriteTextureSampler

UMaterialExpressionSpriteTextureSampler::UMaterialExpressionSpriteTextureSampler(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ParameterName = TEXT("SpriteTexture");
}

void UMaterialExpressionSpriteTextureSampler::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Paper2D"));
	OutCaptions.Add(TEXT("SpriteTexture"));
}

#if WITH_EDITOR
FString UMaterialExpressionSpriteTextureSampler::GetKeywords() const
{
	FString ParentKeywords = Super::GetKeywords();
	return ParentKeywords + TEXT(" Paper2D Sprite");
}

bool UMaterialExpressionSpriteTextureSampler::CanRenameNode() const
{
	// The parameter name is read only on this guy
	return false;
}

void UMaterialExpressionSpriteTextureSampler::GetExpressionToolTip(TArray<FString>& OutToolTip)
{
	OutToolTip.Add(LOCTEXT("SpriteTextureSamplerTooltip", "This is a texture sampler 2D with the parameter name fixed as 'SpriteTexture'. The texture specified here will be replaced by the SourceTexture of a Paper2D sprite if this material is used on a sprite.").ToString());
}

void UMaterialExpressionSpriteTextureSampler::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Ensure that the parameter name never changes
	ParameterName = TEXT("SpriteTexture");
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif

#undef LOCTEXT_NAMESPACE
