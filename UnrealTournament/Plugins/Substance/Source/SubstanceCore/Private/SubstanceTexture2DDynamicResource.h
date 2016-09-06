//! @file SubstanceTexture2D.cpp
//! @brief Implementation of the USubstanceTexture2D class
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#pragma once

#include "TextureResource.h"

/** A dynamic 2D texture resource. */
class FSubstanceTexture2DDynamicResource : public FTextureResource
{
public:
	/** Initialization constructor. */
	FSubstanceTexture2DDynamicResource(class USubstanceTexture2D* InOwner) : 
		SubstanceOwner(InOwner)
	{
		if (SubstanceOwner->Format == PF_G8 || 
			SubstanceOwner->Format == PF_G16)
		{
			this->bGreyScaleFormat = true;
		}
	}

	/** Returns the width of the texture in pixels. */
	virtual uint32 GetSizeX() const override
	{
		if (SubstanceOwner && SubstanceOwner->Mips.Num())
		{
			return SubstanceOwner->Mips[0].SizeX;
		}

		return 0;
	}

	/** Returns the height of the texture in pixels. */
	virtual uint32 GetSizeY() const override
	{
		if (SubstanceOwner && SubstanceOwner->Mips.Num())
		{
			return SubstanceOwner->Mips[0].SizeY;
		}

		return 0;
	}

	/** Create RHI sampler states. */
	void CreateSamplerStates(float MipMapBias) ;

	/** Called when the resource is initialized. This is only called by the rendering thread. */
	virtual void InitRHI() override;

	/** Called when the resource is released. This is only called by the rendering thread. */
	virtual void ReleaseRHI() override;

	/** Returns the Texture2DRHI, which can be used for locking/unlocking the mips. */
	FTexture2DRHIRef GetTexture2DRHI();

private:
	USubstanceTexture2D* SubstanceOwner;

	FTexture2DRHIRef Texture2DRHI;
};
