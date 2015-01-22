// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PaperSpriteSheet.generated.h"

UCLASS(BlueprintType, meta = (DisplayThumbnail = "true"))
class UPaperSpriteSheet : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	// The names of sprites during import
	UPROPERTY(VisibleAnywhere, Category=Data)
	TArray<FString> SpriteNames;

	UPROPERTY(VisibleAnywhere, Category = Data)
	TArray< TAssetPtr<class UPaperSprite> > Sprites;

	// The name of the texture during import
	UPROPERTY(VisibleAnywhere, Category = Data)
	FString TextureName;

	UPROPERTY(VisibleAnywhere, Category = Data)
	UTexture2D* Texture;

#if WITH_EDITORONLY_DATA
	// Import data for this 
	UPROPERTY(VisibleAnywhere, Instanced, Category=ImportSettings)
	class UAssetImportData* AssetImportData;
#endif
};
