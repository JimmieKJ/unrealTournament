// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class ISlate3DRenderer 
{
public:
	virtual ~ISlate3DRenderer() {};

	/** @return The free buffer for drawing */
	virtual FSlateDrawBuffer& GetDrawBuffer() = 0;

 	/** 
	 * Batches the draw elements in the buffer to prepare it for rendering.
	 * Call in the game thread before sending to the render thread.
	 *
	 * @param DrawBuffer The draw buffer to prepare
	 */
	virtual void DrawWindow_GameThread(FSlateDrawBuffer& DrawBuffer) = 0;

	/** 
	 * Renders the batched draw elements of the draw buffer to the given render target.
	 * Call after preparing the draw buffer and render target on the game thread.
	 * 
	 * @param RenderTarget The render target to render the contents of the draw buffer to
	 * @param InDrawBuffer The draw buffer containing the batched elements to render
	 */
	virtual void DrawWindowToTarget_RenderThread( FRHICommandListImmediate& RHICmdList, UTextureRenderTarget2D* RenderTarget, FSlateDrawBuffer& InDrawBuffer ) = 0;
};
