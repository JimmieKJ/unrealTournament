// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 * Thumbnail information for assets that need a scene and a primitive
 */

#pragma once

#include "ThumbnailManager.h"

#include "SceneThumbnailInfoWithPrimitive.generated.h"


UCLASS(MinimalAPI)
class USceneThumbnailInfoWithPrimitive : public USceneThumbnailInfo
{
	GENERATED_UCLASS_BODY()

	/** The type of primitive used in this thumbnail */
	UPROPERTY(EditAnywhere, Category=Thumbnail)
	TEnumAsByte<EThumbnailPrimType> PrimitiveType;

	/** The custom mesh used when the primitive type is TPT_None */
	UPROPERTY(EditAnywhere, Category=Thumbnail)
	FStringAssetReference PreviewMesh;
};
