//! @file SubstanceImageInputThumbnailRenderer.h
//! @author Antoine Gonzalez - Allegorithmic
//! @date 20131010
//! @copyright Allegorithmic. All rights reserved.

#pragma once

#include "SubstanceImageInputThumbnailRenderer.generated.h"

UCLASS()
class USubstanceImageInputThumbnailRenderer : public UThumbnailRenderer
{
	GENERATED_UCLASS_BODY()

	// Begin UThumbnailRenderer Object
	virtual void Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget*, FCanvas* Canvas) override;
	// End UThumbnailRenderer Object
};
