// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


// forward declarations
class FMediaSampleBuffer;
class UMediaTexture;


/**
 * FTextureResource type for movie textures.
 */
class FMediaTextureResource
	: public FTextureResource
	, public FRenderTarget
	, public FDeferredUpdateResource
{
public:

	/** 
	 * Creates and initializes a new instance.
	 *
	 * @param InOwner The Movie texture object to create a resource for (must not be nullptr).
	 * @param InVideoBuffer The video sample buffer to use.
	 */
	FMediaTextureResource(const class UMediaTexture* InOwner, const TSharedRef<FMediaSampleBuffer, ESPMode::ThreadSafe>& InVideoBuffer);

public:

	// FTextureResource overrides

	virtual void InitDynamicRHI() override;
	virtual void ReleaseDynamicRHI() override;

public:

	// FRenderTarget overrides

	virtual FIntPoint GetSizeXY() const override;

public:

	// FDeferredUpdateResource overrides

	virtual void UpdateDeferredResource( FRHICommandListImmediate& RHICmdList, bool bClearRenderTarget=true ) override;

private:

	/** Whether the resource was cleared last frame. */
	bool Cleared;

	/** The color that the resource was cleared with. */
	FLinearColor LastClearColor;

	/** The playback time of the last drawn video frame. */
	FTimespan LastFrameTime;

	/** The UTextureRenderTarget2D which this resource represents. */
	const UMediaTexture* Owner;

	/** Texture resource used for rendering with and resolving to. */
	FTexture2DRHIRef Texture2DRHI;

	/** Pointer to the video sample buffer. */
	TSharedRef<FMediaSampleBuffer, ESPMode::ThreadSafe> VideoBuffer;
};
