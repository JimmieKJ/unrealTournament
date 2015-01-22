// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PaperSpriteAtlas.generated.h"

// Groups together a set of sprites that will try to share the same texture atlas (allowing them to be combined into a single draw call)
UCLASS(MinimalAPI, BlueprintType, Experimental, meta=(DisplayThumbnail = "true"))
class UPaperSpriteAtlas : public UObject
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITORONLY_DATA
	// Description of this atlas, which shows up in the content browser tooltip
	UPROPERTY(EditAnywhere, Category=General)
	FString AtlasDescription;

	// Maximum atlas page width (single pages might be smaller)
	UPROPERTY(EditAnywhere, Category = General)
	int32 MaxWidth;

	// Maximum atlas page height (single pages might be smaller)
	UPROPERTY(EditAnywhere, Category = General)
	int32 MaxHeight;

	// List of generated atlas textures
	UPROPERTY(VisibleAnywhere, Category=General)
	TArray<UTexture*> GeneratedTextures;

	// The GUID of the atlas group, used to match up sprites that belong to this group even thru atlas renames
	UPROPERTY(VisibleAnywhere, Category=General)
	FGuid AtlasGUID;
#endif



public:
	// UObject interface
#if WITH_EDITORONLY_DATA
	virtual void PostInitProperties() override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
#endif
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	// End of UObject interface
};
