// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*==============================================================================
	Texture2DPreview.h: Definitions for previewing 2d textures.
==============================================================================*/

#pragma once

#include "BatchedElements.h"

/**
 * Batched element parameters for previewing 2d textures.
 */
class UNREALED_API FBatchedElementTexture2DPreviewParameters : public FBatchedElementParameters
{
public:
	FBatchedElementTexture2DPreviewParameters(float InMipLevel, bool bInIsNormalMap, bool bInIsSingleChannel)
		: MipLevel(InMipLevel)
		, bIsNormalMap( bInIsNormalMap )
		, bIsSingleChannelFormat( bInIsSingleChannel )
	{
	}

	/** Binds vertex and pixel shaders for this element */
	virtual void BindShaders(FRHICommandList& RHICmdList, ERHIFeatureLevel::Type InFeatureLevel, const FMatrix& InTransform, const float InGamma, const FMatrix& ColorWeights, const FTexture* Texture) override;

private:

	/** Parameters that need to be passed to the shader */
	float MipLevel;
	bool bIsNormalMap;
	bool bIsSingleChannelFormat;
};
