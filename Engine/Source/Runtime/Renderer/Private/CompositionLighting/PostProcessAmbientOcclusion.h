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

// ePId_Input0: defines the resolution we compute AO and provides the normal (only needed if bInAOSetupAsInput)
// ePId_Input1: setup in same resolution as ePId_Input1 for depth expect when running in full resolution, then it's half (only needed if bInAOSetupAsInput)
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
		FShader* SetShaderTempl(const FRenderingCompositePassContext& Context);

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
		uint32 ScaleToFullRes = FSceneRenderTargets::Get(RHICmdList).GetBufferSizeXY().X / InputTextureSize.X;

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



/** The uniform shader parameters needed for screen space TemporalAA and SSAO passes. */
BEGIN_UNIFORM_BUFFER_STRUCT(FCameraMotionParameters,)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FVector4, Value, [5])
END_UNIFORM_BUFFER_STRUCT(FCameraMotionParameters)

TUniformBufferRef<FCameraMotionParameters> CreateCameraMotionParametersUniformBuffer(const FSceneView& View);

// for render thread
// @return usually in 0..100 range but could be outside, combines the view with the cvar setting
float GetAmbientOcclusionQualityRT(const FSceneView& View);