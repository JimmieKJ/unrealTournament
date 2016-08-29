// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ThumbnailHelpers.h"
#include "ClassThumbnailRenderer.generated.h"

UCLASS(config=Editor,MinimalAPI)
class UClassThumbnailRenderer : public UDefaultSizedThumbnailRenderer
{
	GENERATED_UCLASS_BODY()

	// Begin UThumbnailRenderer Object
	virtual bool CanVisualizeAsset(UObject* Object) override;
	UNREALED_API virtual void Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget*, FCanvas* Canvas) override;
	// End UThumbnailRenderer Object

	// UObject Implementation
	UNREALED_API virtual void BeginDestroy() override;
	// End UObject Implementation

private:
	TClassInstanceThumbnailScene<FClassThumbnailScene, 400> ThumbnailScenes;
};
