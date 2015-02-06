// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ClassThumbnailRenderer.generated.h"

UCLASS(config=Editor,MinimalAPI)
class UClassThumbnailRenderer : public UDefaultSizedThumbnailRenderer
{
	GENERATED_UCLASS_BODY()


	// Begin UThumbnailRenderer Object
	virtual bool CanVisualizeAsset(UObject* Object) override;
	UNREALED_API virtual void Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget*, FCanvas* Canvas) override;
	// End UThumbnailRenderer Object

	// UObject implementation
	UNREALED_API virtual void BeginDestroy() override;
	// End UObject implementation

private:
	class FClassThumbnailScene* ThumbnailScene;
};
