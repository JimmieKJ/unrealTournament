//! @file SubstanceTextureThumbnailRenderer.h
//! @author Antoine Gonzalez - Allegorithmic
//! @date 20140620
//! @copyright Allegorithmic. All rights reserved.

#pragma once

#include "SubstanceTextureThumbnailRenderer.generated.h"

UCLASS()
class USubstanceTextureThumbnailRenderer : public UTextureThumbnailRenderer
{
	GENERATED_UCLASS_BODY()

	// Begin UThumbnailRenderer Object
	virtual void Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget*, FCanvas* Canvas) override;
	// End UThumbnailRenderer Object
};
