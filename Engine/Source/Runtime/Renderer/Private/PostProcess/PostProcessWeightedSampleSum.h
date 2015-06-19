// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessWeightedSampleSum.h: Post processing weight sample sum implementation.
=============================================================================*/

#pragma once

#include "RenderingCompositionGraph.h"
#include "PostProcessing.h"


/** A vertex shader which filters a texture. Can re reused by other postprocessing pixel shaders. */
template<uint32 NumSamples>
class TFilterVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TFilterVS,Global);
public:

	/** The number of 4D constant registers used to hold the packed 2D sample offsets. */
	enum { NumSampleChunks = (NumSamples + 1) / 2 };

	static bool ShouldCache(EShaderPlatform Platform)
	{
		if (IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5))
		{
			return true;
		}
		else if (IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4))
		{
			return NumSamples <= 16;
		}
		else
		{
			return NumSamples <= 7;
		}
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("NUM_SAMPLES"), NumSamples);
	}

	/** Default constructor. */
	TFilterVS() {}

	/** Initialization constructor. */
	TFilterVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
	FGlobalShader(Initializer)
	{
		SampleOffsets.Bind(Initializer.ParameterMap,TEXT("SampleOffsets"));
	}

	/** Serializer */
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << SampleOffsets;

		return bShaderHasOutdatedParameters;
	}

	/** Sets shader parameter values */
	void SetParameters(FRHICommandList& RHICmdList, const FVector2D* SampleOffsetsValue)
	{
		FVector4 PackedSampleOffsetsValue[NumSampleChunks];
		for(int32 SampleIndex = 0;SampleIndex < NumSamples;SampleIndex += 2)
		{
			PackedSampleOffsetsValue[SampleIndex / 2].X = SampleOffsetsValue[SampleIndex + 0].X;
			PackedSampleOffsetsValue[SampleIndex / 2].Y = SampleOffsetsValue[SampleIndex + 0].Y;
			if(SampleIndex + 1 < NumSamples)
			{
				PackedSampleOffsetsValue[SampleIndex / 2].W = SampleOffsetsValue[SampleIndex + 1].X;
				PackedSampleOffsetsValue[SampleIndex / 2].Z = SampleOffsetsValue[SampleIndex + 1].Y;
			}
		}
		SetShaderValueArray(RHICmdList, GetVertexShader(),SampleOffsets,PackedSampleOffsetsValue,NumSampleChunks);
	}

private:
	FShaderParameter SampleOffsets;
};

enum EFilterCombineMethod
{
	EFCM_Weighted,		// for Gaussian blur, e.g. bloom
	EFCM_MaxMagnitude	// useful for motion blur
};

// to trigger certain optimizations and to orient the filter kernel
enum EFilterShape
{
	EFS_Horiz,
	EFS_Vert
};


/**
 * ePId_Input0: main input texture (usually to blur)
 * ePId_Input1: optional additive input (usually half res bloom)
 * N samples are added together, each sample is weighted.
 * derives from TRenderingCompositePassBase<InputCount, OutputCount>
 */
class FRCPassPostProcessWeightedSampleSum : public TRenderingCompositePassBase<2, 1>
{
public:
	// constructor
	FRCPassPostProcessWeightedSampleSum(EFilterShape InFilterShape,
			EFilterCombineMethod InCombineMethod,
			float InSizeScale,
			const TCHAR* InDebugName = TEXT("WeightedSampleSum"),
			FLinearColor InAdditiveTintValue = FLinearColor::White);

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

	// retrieve runtime filter kernel properties.
	static float GetClampedKernelRadius(ERHIFeatureLevel::Type InFeatureLevel, float KernelRadius);
	static int GetIntegerKernelRadius(ERHIFeatureLevel::Type InFeatureLevel, float KernelRadius);

	// @param InCrossCenterWeight >=0
	void SetCrossCenterWeight(float InCrossCenterWeight) { check(InCrossCenterWeight >= 0.0f); CrossCenterWeight = InCrossCenterWeight; }

private:
	void DrawQuad(FRHICommandListImmediate& RHICmdList, bool bDoFastBlur, FIntRect SrcRect, FIntRect DestRect, bool bRequiresClear, FIntPoint DestSize, FIntPoint SrcSize, FShader* VertexShader) const;
	static uint32 GetMaxNumSamples(ERHIFeatureLevel::Type InFeatureLevel);

	// e.g. EFS_Horiz or EFS_Vert
	EFilterShape FilterShape;
	EFilterCombineMethod CombineMethod;
	float SizeScale;
	FLinearColor TintValue;
	const TCHAR* DebugName;
	// to give the center sample some special weight (see r.Bloom.Cross), >=0
	float CrossCenterWeight;

	// @return true: half x resolution for horizontal pass, vertical pass takes that as input, lower quality
	bool DoFastBlur() const;
};
