// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	StereoRendering.h: Abstract stereoscopic rendering interface
=============================================================================*/

#pragma once

class IStereoRendering
{
public:
	/** 
	 * Whether or not stereo rendering is on.
	 */
	virtual bool IsStereoEnabled() const = 0;

	/** 
	 * Switches stereo rendering on / off. Returns current state of stereo.
	 * @return Returns current state of stereo (true / false).
	 */
	virtual bool EnableStereo(bool stereo = true) = 0;

    /**
     * Adjusts the viewport rectangle for stereo, based on which eye pass is being rendered.
     */
    virtual void AdjustViewRect(enum EStereoscopicPass StereoPass, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const = 0;

    /**
	 * Calculates the offset for the camera position, given the specified position, rotation, and world scale
	 */
	virtual void CalculateStereoViewOffset(const enum EStereoscopicPass StereoPassType, const FRotator& ViewRotation, const float WorldToMeters, FVector& ViewLocation) = 0;

	/**
	 * Gets a projection matrix for the device, given the specified eye setup
	 */
	virtual FMatrix GetStereoProjectionMatrix(const enum EStereoscopicPass StereoPassType, const float FOV) const = 0;

	/**
	 * Sets view-specific params (such as view projection matrix) for the canvas.
	 */
	virtual void InitCanvasFromView(class FSceneView* InView, class UCanvas* Canvas) = 0;

	/**
	 * Pushes transformations based on specified viewport into canvas. Necessary to call PopTransform on
	 * FCanvas object afterwards.
	 */
	virtual void PushViewportCanvas(enum EStereoscopicPass StereoPass, class FCanvas *InCanvas, class UCanvas *InCanvasObject, class FViewport *InViewport) const = 0;

	/**
	 * Pushes transformations based on specified view into canvas. Necessary to call PopTransform on FCanvas 
	 * object afterwards.
	 */
	virtual void PushViewCanvas(enum EStereoscopicPass StereoPass, class FCanvas *InCanvas, class UCanvas *InCanvasObject, class FSceneView *InView) const = 0;

	/**
	 * Returns eye render params, used from PostProcessHMD, RenderThread.
	 */
	virtual void GetEyeRenderParams_RenderThread(enum EStereoscopicPass StereoPass, FVector2D& EyeToSrcUVScaleValue, FVector2D& EyeToSrcUVOffsetValue) const {}

	/**
	 * Returns timewarp matrices, used from PostProcessHMD, RenderThread.
	 */
	virtual void GetTimewarpMatrices_RenderThread(enum EStereoscopicPass StereoPass, FMatrix& EyeRotationStart, FMatrix& EyeRotationEnd) const {}

	// Optional methods to support rendering into a texture.
	/**
	 * Updates viewport for direct rendering of distortion. Should be called on a game thread.
	 * Optional SViewport* parameter can be used to access SWindow object.
	 */
	virtual void UpdateViewport(bool bUseSeparateRenderTarget, const class FViewport& Viewport, class SViewport* = nullptr) {}

	/**
	 * Calculates dimensions of the render target texture for direct rendering of distortion.
	 */
	virtual void CalculateRenderTargetSize(uint32& InOutSizeX, uint32& InOutSizeY) const {}

	/**
	 * Returns true, if render target texture must be re-calculated. 
	 */
	virtual bool NeedReAllocateViewportRenderTarget(const class FViewport& Viewport) const { return false; }

	// Whether separate render target should be used or not.
	virtual bool ShouldUseSeparateRenderTarget() const { return false; }

	// Renders texture into a backbuffer. Could be empty if no rendertarget texture is used, or if direct-rendering 
	// through RHI bridge is implemented. 
	virtual void RenderTexture_RenderThread(class FRHICommandListImmediate& RHICmdList, class FRHITexture2D* BackBuffer, class FRHITexture2D* SrcTexture) const {}

	/**
	 * Called after Present is called.
	 */
	virtual void FinishRenderingFrame_RenderThread(class FRHICommandListImmediate& RHICmdList) {}
};
