// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISlate3DRenderer.h"

class FSlate3DRenderer : public ISlate3DRenderer, public FDeferredCleanupInterface
{
public:
	FSlate3DRenderer( TSharedRef<FSlateFontServices> InSlateFontServices, TSharedRef<FSlateRHIResourceManager> InResourceManager, bool bUseGammaCorrection = false );

	void Cleanup();
	virtual void FinishCleanup() override;

	virtual FSlateDrawBuffer& GetDrawBuffer() override;
	virtual void DrawWindow_GameThread(FSlateDrawBuffer& DrawBuffer) override;
	virtual void DrawWindowToTarget_RenderThread(FRHICommandListImmediate& RHICmdList, FTextureRenderTarget2DResource* RenderTargetResource, FSlateDrawBuffer& InDrawBuffer, bool bInClearTarget) override;

private:

	/** Double buffered draw buffers so that the rendering thread can be rendering windows while the game thread is setting up for next frame */
	static const int32 NUM_DRAW_BUFFERS = 3;
	FSlateDrawBuffer DrawBuffers[NUM_DRAW_BUFFERS];

	/** The font services to use for rendering text */
	TSharedRef<FSlateFontServices> SlateFontServices;

	/** Texture manager for accessing textures on the game thread */
	TSharedRef<FSlateRHIResourceManager> ResourceManager;

	/** The rendering policy to use for drawing to the render target */
	TSharedPtr<class FSlateRHIRenderingPolicy> RenderTargetPolicy;

	/** Element batcher that renders draw elements */
	TSharedPtr<FSlateElementBatcher> ElementBatcher;

	/** The draw buffer that is currently free for use by the game thread */
	uint8 FreeBufferIndex;
};
