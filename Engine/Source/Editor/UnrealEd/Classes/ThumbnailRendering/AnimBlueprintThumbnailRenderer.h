// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/**
 *
 * This thumbnail renderer displays a given AnimBlueprint
 */

#pragma once

#include "ThumbnailHelpers.h"
#include "AnimBlueprintThumbnailRenderer.generated.h"

UCLASS(config=Editor, MinimalAPI)
class UAnimBlueprintThumbnailRenderer : public UDefaultSizedThumbnailRenderer
{
	GENERATED_UCLASS_BODY()

	// Begin UThumbnailRenderer Object
	UNREALED_API virtual void Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget*, FCanvas* Canvas) override;
	// End UThumbnailRenderer Object

	// UObject Implementation
	UNREALED_API virtual void BeginDestroy() override;
	// End UObject Implementation

private:
	TClassInstanceThumbnailScene<FAnimBlueprintThumbnailScene, 400> ThumbnailScenes;
};

