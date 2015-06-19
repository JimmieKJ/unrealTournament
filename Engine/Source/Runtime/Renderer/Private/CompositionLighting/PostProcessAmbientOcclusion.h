// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessAmbientOcclusion.h: Post processing ambient occlusion implementation.
=============================================================================*/

#pragma once

#include "RenderingCompositionGraph.h"


// ePId_Input0: SceneDepth
// ePId_Input1: optional from former downsampling pass
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessAmbientOcclusionSetup : public TRenderingCompositePassBase<2, 1>
{
public:

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

private:
	// otherwise this is a down sampling pass which takes two MRT inputs from the setup pass before
	bool IsInitialPass() const;

	// @return VertexShader
	template <uint32 bInitialSetup>
	FShader* SetShaderSetupTempl(const FRenderingCompositePassContext& Context);
};

// ePId_Input0: defines the resolution we compute AO and provides the normal
// ePId_Input1: setup in same resolution as ePId_Input1 for depth expect when running in full resolution, then it's half
// ePId_Input2: optional AO result one lower resolution
// ePId_Input3: optional HZB
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessAmbientOcclusion : public TRenderingCompositePassBase<4, 1>
{
public:
	// @param bInAOSetupAsInput true:use AO setup as input, false: use GBuffer normal and native z depth
	FRCPassPostProcessAmbientOcclusion(bool bInAOSetupAsInput = true) : bAOSetupAsInput(bInAOSetupAsInput)
	{
	}

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

private:
	
	template <uint32 bAOSetupAsInput, uint32 bDoUpsample, uint32 SampleSetQuality>
		void SetShaderTempl(const FRenderingCompositePassContext& Context);

	bool bAOSetupAsInput;
};

// apply the AO to the SceneColor (lightmapped object), extra pas that is not always needed
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessBasePassAO : public TRenderingCompositePassBase<0, 1>
{
public:
	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
};



/** Pixel shader parameters needed for screen space AmbientOcclusion and ScreenSpaceRreflection passes. */
class FScreenSpaceAOandSSRShaderParameters
{
public:

	void Bind(const FShaderParameterMap& ParameterMap)
	{
		ScreenSpaceAOandSSRShaderParams.Bind(ParameterMap, TEXT("ScreenSpaceAOandSSRShaderParams"));
	}

	template<typename ShaderRHIParamRef>
	void Set(FRHICommandList& RHICmdList, const FSceneView& View, const ShaderRHIParamRef ShaderRHI, FIntPoint InputTextureSize) const
	{
		const FFinalPostProcessSettings& Settings = View.FinalPostProcessSettings;

		FIntPoint SSAORandomizationSize = GSystemTextures.SSAORandomization->GetDesc().Extent;
		FVector2D ViewportUVToRandomUV(InputTextureSize.X / (float)SSAORandomizationSize.X, InputTextureSize.Y / (float)SSAORandomizationSize.Y);

		// e.g. 4 means the input texture is 4x smaller than the buffer size
		uint32 ScaleToFullRes = GSceneRenderTargets.GetBufferSizeXY().X / InputTextureSize.X;

		FIntRect ViewRect = FIntRect::DivideAndRoundUp(View.ViewRect, ScaleToFullRes);

		// Transform from NDC [-1, 1] to screen space so that positions can be used as texture coordinates to lookup into ambient occlusion buffers.
		// This handles the view size being smaller than the buffer size, and applies a half pixel offset on required platforms.
		// Scale in xy, Bias in zw.
		FVector4 AOScreenPositionScaleBiasValue = FVector4(
			ViewRect.Size().X / InputTextureSize.X / +2.0f,
			ViewRect.Size().Y / InputTextureSize.Y / -2.0f,
			(ViewRect.Size().X / 2.0f + ViewRect.Min.X) / InputTextureSize.X,
			(ViewRect.Size().Y / 2.0f + ViewRect.Min.Y) / InputTextureSize.Y);

		// Combining two scales into one parameter, Projection matrix scaling of x and y and scaling from clip to screen space.
		FVector2D ProjectionScaleValue = FVector2D(View.ViewMatrices.ProjMatrix.M[0][0], View.ViewMatrices.ProjMatrix.M[1][1])
			* FVector2D(AOScreenPositionScaleBiasValue.X, AOScreenPositionScaleBiasValue.Y);

		float AORadiusInShader = Settings.AmbientOcclusionRadius;
		float ScaleRadiusInWorldSpace = 1.0f;

		if(!Settings.AmbientOcclusionRadiusInWS)
		{
			// radius is defined in view space in 400 units
			AORadiusInShader /= 400.0f;
			ScaleRadiusInWorldSpace = 0.0f;
		}

		// /4 is an adjustment for usage with multiple mips
		float f = FMath::Log2(ScaleToFullRes);
		float g = pow(Settings.AmbientOcclusionMipScale, f);
		AORadiusInShader *= pow(Settings.AmbientOcclusionMipScale, FMath::Log2(ScaleToFullRes)) / 4.0f;

		float Ratio = View.UnscaledViewRect.Width() / (float)View.UnscaledViewRect.Height();

		// Grab this and pass into shader so we can negate the fov influence of projection on the screen pos.
		float InvTanHalfFov = View.ViewMatrices.ProjMatrix.M[0][0];

		FVector4 Value[5];

		float StaticFraction = FMath::Clamp(Settings.AmbientOcclusionStaticFraction, 0.0f, 1.0f);

		// clamp to prevent user error
		float FadeRadius = FMath::Max(1.0f, Settings.AmbientOcclusionFadeRadius);
		float InvFadeRadius = 1.0f / FadeRadius;

		// /1000 to be able to define the value in that distance
		Value[0] = FVector4(Settings.AmbientOcclusionPower, Settings.AmbientOcclusionBias / 1000.0f, 1.0f / Settings.AmbientOcclusionDistance_DEPRECATED, Settings.AmbientOcclusionIntensity);
		Value[1] = FVector4(ViewportUVToRandomUV.X, ViewportUVToRandomUV.Y, AORadiusInShader, Ratio);
		Value[2] = FVector4(ScaleToFullRes, Settings.AmbientOcclusionMipThreshold / ScaleToFullRes, ScaleRadiusInWorldSpace, Settings.AmbientOcclusionMipBlend);
		Value[3] = FVector4(0, 0, StaticFraction, InvTanHalfFov);
		Value[4] = FVector4(InvFadeRadius, -(Settings.AmbientOcclusionFadeDistance - FadeRadius) * InvFadeRadius, 0, 0);

		SetShaderValueArray(RHICmdList, ShaderRHI, ScreenSpaceAOandSSRShaderParams, Value, 5);
	}

	friend FArchive& operator<<(FArchive& Ar, FScreenSpaceAOandSSRShaderParameters& This);

private:
	FShaderParameter ScreenSpaceAOandSSRShaderParams;
};

/** Pixel shader parameters needed for screen space TemporalAA and SSAO passes. */
class FCameraMotionParameters
{
public:

	void Bind(const FShaderParameterMap& ParameterMap);

	template< typename ShaderRHIParamRef >
	void Set(FRHICommandList& RHICmdList, const FSceneView& View, const ShaderRHIParamRef ShaderRHI) const;

	friend FArchive& operator<<(FArchive& Ar, FCameraMotionParameters& This);

private:
	FShaderParameter CameraMotion;
};


template< typename ShaderRHIParamRef >
void FCameraMotionParameters::Set(FRHICommandList& RHICmdList, const FSceneView& View, const ShaderRHIParamRef ShaderRHI) const
{
	FSceneViewState* ViewState = (FSceneViewState*)View.State;

	FMatrix Proj = View.ViewMatrices.ProjMatrix;
	FMatrix PrevProj = ViewState->PrevViewMatrices.ProjMatrix;

	// Remove jitter
	Proj.M[2][0] -= View.ViewMatrices.TemporalAASample.X * 2.0f / View.ViewRect.Width();
	Proj.M[2][1] -= View.ViewMatrices.TemporalAASample.Y * 2.0f / View.ViewRect.Height();
	PrevProj.M[2][0] -= ViewState->PrevViewMatrices.TemporalAASample.X * 2.0f / View.ViewRect.Width();
	PrevProj.M[2][1] -= ViewState->PrevViewMatrices.TemporalAASample.Y * 2.0f / View.ViewRect.Height();

	FVector DeltaTranslation = ViewState->PrevViewMatrices.PreViewTranslation - View.ViewMatrices.PreViewTranslation;
	FMatrix ViewProj = ( View.ViewMatrices.TranslatedViewMatrix * Proj ).GetTransposed();
	FMatrix PrevViewProj = ( FTranslationMatrix(DeltaTranslation) * ViewState->PrevViewMatrices.TranslatedViewMatrix * PrevProj ).GetTransposed();

	double InvViewProj[16];
	Inverse4x4( InvViewProj, (float*)ViewProj.M );

	const float* p = (float*)PrevViewProj.M;

	const double cxx = InvViewProj[ 0]; const double cxy = InvViewProj[ 1]; const double cxz = InvViewProj[ 2]; const double cxw = InvViewProj[ 3];
	const double cyx = InvViewProj[ 4]; const double cyy = InvViewProj[ 5]; const double cyz = InvViewProj[ 6]; const double cyw = InvViewProj[ 7];
	const double czx = InvViewProj[ 8]; const double czy = InvViewProj[ 9]; const double czz = InvViewProj[10]; const double czw = InvViewProj[11];
	const double cwx = InvViewProj[12]; const double cwy = InvViewProj[13]; const double cwz = InvViewProj[14]; const double cww = InvViewProj[15];

	const double pxx = (double)(p[ 0]); const double pxy = (double)(p[ 1]); const double pxz = (double)(p[ 2]); const double pxw = (double)(p[ 3]);
	const double pyx = (double)(p[ 4]); const double pyy = (double)(p[ 5]); const double pyz = (double)(p[ 6]); const double pyw = (double)(p[ 7]);
	const double pwx = (double)(p[12]); const double pwy = (double)(p[13]); const double pwz = (double)(p[14]); const double pww = (double)(p[15]);

	FVector4 CameraMotionValue[5];

	CameraMotionValue[0] = FVector4(
		(float)(4.0*(cwx*pww + cxx*pwx + cyx*pwy + czx*pwz)),
		(float)((-4.0)*(cwy*pww + cxy*pwx + cyy*pwy + czy*pwz)),
		(float)(2.0*(cwz*pww + cxz*pwx + cyz*pwy + czz*pwz)),
		(float)(2.0*(cww*pww - cwx*pww + cwy*pww + (cxw - cxx + cxy)*pwx + (cyw - cyx + cyy)*pwy + (czw - czx + czy)*pwz)));

	CameraMotionValue[1] = FVector4(
		(float)(( 4.0)*(cwy*pww + cxy*pwx + cyy*pwy + czy*pwz)),
		(float)((-2.0)*(cwz*pww + cxz*pwx + cyz*pwy + czz*pwz)),
		(float)((-2.0)*(cww*pww + cwy*pww + cxw*pwx - 2.0*cxx*pwx + cxy*pwx + cyw*pwy - 2.0*cyx*pwy + cyy*pwy + czw*pwz - 2.0*czx*pwz + czy*pwz - cwx*(2.0*pww + pxw) - cxx*pxx - cyx*pxy - czx*pxz)),
		(float)(-2.0*(cyy*pwy + czy*pwz + cwy*(pww + pxw) + cxy*(pwx + pxx) + cyy*pxy + czy*pxz)));

	CameraMotionValue[2] = FVector4(
		(float)((-4.0)*(cwx*pww + cxx*pwx + cyx*pwy + czx*pwz)),
		(float)(cyz*pwy + czz*pwz + cwz*(pww + pxw) + cxz*(pwx + pxx) + cyz*pxy + czz*pxz),
		(float)(cwy*pww + cwy*pxw + cww*(pww + pxw) - cwx*(pww + pxw) + (cxw - cxx + cxy)*(pwx + pxx) + (cyw - cyx + cyy)*(pwy + pxy) + (czw - czx + czy)*(pwz + pxz)),
		(float)(0));

	CameraMotionValue[3] = FVector4(
		(float)((-4.0)*(cwx*pww + cxx*pwx + cyx*pwy + czx*pwz)),
		(float)((-2.0)*(cwz*pww + cxz*pwx + cyz*pwy + czz*pwz)),
		(float)(2.0*((-cww)*pww + cwx*pww - 2.0*cwy*pww - cxw*pwx + cxx*pwx - 2.0*cxy*pwx - cyw*pwy + cyx*pwy - 2.0*cyy*pwy - czw*pwz + czx*pwz - 2.0*czy*pwz + cwy*pyw + cxy*pyx + cyy*pyy + czy*pyz)),
		(float)(2.0*(cyx*pwy + czx*pwz + cwx*(pww - pyw) + cxx*(pwx - pyx) - cyx*pyy - czx*pyz)));

	CameraMotionValue[4] = FVector4(
		(float)(4.0*(cwy*pww + cxy*pwx + cyy*pwy + czy*pwz)),
		(float)(cyz*pwy + czz*pwz + cwz*(pww - pyw) + cxz*(pwx - pyx) - cyz*pyy - czz*pyz),
		(float)(cwy*pww + cww*(pww - pyw) - cwy*pyw + cwx*((-pww) + pyw) + (cxw - cxx + cxy)*(pwx - pyx) + (cyw - cyx + cyy)*(pwy - pyy) + (czw - czx + czy)*(pwz - pyz)),
		(float)(0));

	SetShaderValueArray(RHICmdList, ShaderRHI, CameraMotion, CameraMotionValue, 5);
}