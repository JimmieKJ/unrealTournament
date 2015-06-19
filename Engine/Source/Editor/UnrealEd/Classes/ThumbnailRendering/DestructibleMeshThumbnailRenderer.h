// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 *
 * This thumbnail renderer displays a given skeletal mesh
 */

#pragma once
#include "DestructibleMeshThumbnailRenderer.generated.h"

UCLASS(config=Editor, MinimalAPI)
class UDestructibleMeshThumbnailRenderer : public UDefaultSizedThumbnailRenderer
{
	GENERATED_UCLASS_BODY()


	// Begin UThumbnailRenderer Object
	UNREALED_API virtual void Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget*, FCanvas* Canvas) override;
	// End UThumbnailRenderer Object

	// UObject implementation
	UNREALED_API virtual void BeginDestroy() override;

private:
	class FDestructibleMeshThumbnailScene* ThumbnailScene;
};

