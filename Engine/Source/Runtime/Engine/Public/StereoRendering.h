// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	StereoRendering.h: Abstract stereoscopic rendering interface
=============================================================================*/

#pragma once

class IStereoRendering
{
public:
	/** 
	 * Whether or not stereo rendering is on this frame.
	 */
	virtual bool IsStereoEnabled() const = 0;

	/** 
	 * Whether or not stereo rendering is on on next frame. Useful to determine if some preparation work
	 * should be done before stereo got enabled in next frame. 
	 */
	virtual bool IsStereoEnabledOnNextFrame() const { return IsStereoEnabled(); }

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
	 * Gets the percentage bounds of the safe region to draw in.  This allows things like stat rendering to appear within the readable portion of the stereo view.
	 * @return	The centered percentage of the view that is safe to draw readable text in
	 */
	virtual FVector2D GetTextSafeRegionBounds() const { return FVector2D(0.75f, 0.75f); }

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
	 * Returns eye render params, used from PostProcessHMD, RenderThread.
	 */
	virtual void GetEyeRenderParams_RenderThread(const struct FRenderingCompositePassContext& Context, FVector2D& EyeToSrcUVScaleValue, FVector2D& EyeToSrcUVOffsetValue) const {}

	/**
	 * Returns timewarp matrices, used from PostProcessHMD, RenderThread.
	 */
	virtual void GetTimewarpMatrices_RenderThread(const struct FRenderingCompositePassContext& Context, FMatrix& EyeRotationStart, FMatrix& EyeRotationEnd) const {}

	// Optional methods to support rendering into a texture.
	/**
	 * Updates viewport for direct rendering of distortion. Should be called on a game thread.
	 * Optional SViewport* parameter can be used to access SWindow object.
	 */
	virtual void UpdateViewport(bool bUseSeparateRenderTarget, const class FViewport& Viewport, class SViewport* = nullptr) {}

	/**
	 * Calculates dimensions of the render target texture for direct rendering of distortion.
	 */
	virtual void CalculateRenderTargetSize(const class FViewport& Viewport, uint32& InOutSizeX, uint32& InOutSizeY) {}

	/**
	 * Returns true, if render target texture must be re-calculated. 
	 */
	virtual bool NeedReAllocateViewportRenderTarget(const class FViewport& Viewport) { return false; }

	// Whether separate render target should be used or not.
	virtual bool ShouldUseSeparateRenderTarget() const { return false; }

	// Renders texture into a backbuffer. Could be empty if no rendertarget texture is used, or if direct-rendering 
	// through RHI bridge is implemented. 
	virtual void RenderTexture_RenderThread(class FRHICommandListImmediate& RHICmdList, class FRHITexture2D* BackBuffer, class FRHITexture2D* SrcTexture) const {}

	/**
	 * Called after Present is called.
	 */
	virtual void FinishRenderingFrame_RenderThread(class FRHICommandListImmediate& RHICmdList) {}

	/**
	 * Returns orthographic projection , used from Canvas::DrawItem.
	 */
	virtual void GetOrthoProjection(int32 RTWidth, int32 RTHeight, float OrthoDistance, FMatrix OrthoProjection[2]) const
	{
		OrthoProjection[0] = OrthoProjection[1] = FMatrix::Identity;
		OrthoProjection[1] = FTranslationMatrix(FVector(OrthoProjection[1].M[0][3] * RTWidth * .25 + RTWidth * .5, 0, 0));
	}

	/**
	 * Sets screen percentage to be used for stereo rendering.
	 *
	 * @param ScreenPercentage	(in) Specifies the screen percentage to be used in VR mode. Use 0.0f value to reset to default value.
	 */
	virtual void SetScreenPercentage(float InScreenPercentage) {}
	
	/** 
	 * Returns screen percentage to be used for stereo rendering.
	 *
	 * @return (float)	The screen percentage to be used in stereo mode. 0.0f, if default value is used.
	 */
	virtual float GetScreenPercentage() const { return 0.0f; }

	/** 
	 * Sets near and far clipping planes (NCP and FCP) for stereo rendering. Similar to 'stereo ncp= fcp' console command, but NCP and FCP set by this
	 * call won't be saved in .ini file.
	 *
	 * @param NCP				(in) Near clipping plane, in centimeters
	 * @param FCP				(in) Far clipping plane, in centimeters
	 */
	virtual void SetClippingPlanes(float NCP, float FCP) {}

	/**
	 * Returns currently active custom present. 
	 */
	virtual FRHICustomPresent* GetCustomPresent() { return nullptr; }

	/**
	 * Returns number of required buffered frames.
	 */
	virtual uint32 GetNumberOfBufferedFrames() const { return 1;  }

	/**
	 * Allocates a render target texture. 
	 *
	 * @param Index			(in) index of the buffer, changing from 0 to GetNumberOfBufferedFrames()
	 * @return				true, if texture was allocated; false, if the default texture allocation should be used.
	 */
	virtual bool AllocateRenderTargetTexture(uint32 Index, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 Flags, uint32 TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples = 1)
	{
		return false;
	}
};
