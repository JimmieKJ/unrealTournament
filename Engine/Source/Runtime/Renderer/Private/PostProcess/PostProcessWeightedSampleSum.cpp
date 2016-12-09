// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessWeightedSampleSum.cpp: Post processing weight sample sum implementation.
=============================================================================*/

#include "PostProcess/PostProcessWeightedSampleSum.h"
#include "ClearQuad.h"
#include "RendererModule.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "PostProcess/SceneRenderTargets.h"
#include "PostProcess/SceneFilterRendering.h"
#include "PostProcess/PostProcessing.h"

// maximum number of sample using the shader that has the dynamic loop
#define MAX_FILTER_SAMPLES	128

// maximum number of sample available using unrolled loop shaders
#define MAX_FILTER_COMPILE_TIME_SAMPLES 32
#define MAX_FILTER_COMPILE_TIME_SAMPLES_SM4 16
#define MAX_FILTER_COMPILE_TIME_SAMPLES_ES2 7

#define MAX_PACKED_SAMPLES_OFFSET ((MAX_FILTER_SAMPLES + 1) / 2)

static TAutoConsoleVariable<int32> CVarLoopMode(
	TEXT("r.Filter.LoopMode"),
	0,
	TEXT("Controles when to use either dynamic or unrolled loops to iterates over the Gaussian filtering.\n")
	TEXT("This passes is used for Gaussian Blur, Bloom and Depth of Field. The dynamic loop allows\n")
	TEXT("up to 128 samples versus the 32 samples of unrolled loops, but add an additonal cost for\n")
	TEXT("the loop's stop test at every iterations.\n")
	TEXT(" 0: Unrolled loop only (default; limited to 32 samples).\n")
	TEXT(" 1: Fall back to dynamic loop if needs more than 32 samples.\n")
	TEXT(" 2: Dynamic loop only."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarFilterSizeScale(
	TEXT("r.Filter.SizeScale"),
	1.0f,
	TEXT("Allows to scale down or up the sample count used for bloom and Gaussian depth of field (scale is clampled to give reasonable results).\n")
	TEXT("Values down to 0.6 are hard to notice\n")
	TEXT(" 1 full quality (default)\n")
	TEXT(" >1 more samples (slower)\n")
	TEXT(" <1 less samples (faster, artifacts with HDR content or boxy results with GaussianDOF)"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

// will be removed soon
static TAutoConsoleVariable<int32> FilterNewMethod(
	TEXT("r.Filter.NewMethod"),
	1,
	TEXT("Affects bloom and Gaussian depth of field.\n")
	TEXT(" 0: old method (doesn't scale linearly with size)\n")
	TEXT(" 1: new method, might need asset tweak (default)"),
	ECVF_RenderThreadSafe);

/**
 * A pixel shader which filters a texture.
 * @param CombineMethod 0:weighted filtering, 1: weighted filtering + additional texture, 2: max magnitude
 */
template<uint32 CompileTimeNumSamples, uint32 CombineMethodInt>
class TFilterPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TFilterPS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		if( IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) )
		{
			return true;
		}
		else if( IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4) )
		{
			return CompileTimeNumSamples <= MAX_FILTER_COMPILE_TIME_SAMPLES_SM4;
		}
		else
		{
			return CompileTimeNumSamples <= MAX_FILTER_COMPILE_TIME_SAMPLES_ES2;
		}
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("NUM_SAMPLES"), CompileTimeNumSamples);
		OutEnvironment.SetDefine(TEXT("COMBINE_METHOD"), CombineMethodInt);

		if (CompileTimeNumSamples == 0)
		{
			// CompileTimeNumSamples == 0 implies the dynamic loop, but we still needs to pass the number
			// of maximum number of samples for the uniform arraies.
			OutEnvironment.SetDefine(TEXT("MAX_NUM_SAMPLES"), MAX_FILTER_SAMPLES);
		}
	}

	/** Default constructor. */
	TFilterPS() {}

	/** Initialization constructor. */
	TFilterPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		FilterTexture.Bind(Initializer.ParameterMap,TEXT("FilterTexture"));
		FilterTextureSampler.Bind(Initializer.ParameterMap,TEXT("FilterTextureSampler"));
		AdditiveTexture.Bind(Initializer.ParameterMap,TEXT("AdditiveTexture"));
		AdditiveTextureSampler.Bind(Initializer.ParameterMap,TEXT("AdditiveTextureSampler"));
		SampleWeights.Bind(Initializer.ParameterMap,TEXT("SampleWeights"));
		
		if (CompileTimeNumSamples == 0)
		{
			// dynamic loop do UV offset in the pixel shader, and requires the number of samples.
			SampleOffsets.Bind(Initializer.ParameterMap,TEXT("SampleOffsets"));
			SampleCount.Bind(Initializer.ParameterMap,TEXT("SampleCount"));
		}
	}

	/** Serializer */
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << FilterTexture << FilterTextureSampler << AdditiveTexture << AdditiveTextureSampler << SampleWeights << SampleOffsets << SampleCount;
		return bShaderHasOutdatedParameters;
	}

	/** Sets shader parameter values */
	void SetParameters(
		FRHICommandList& RHICmdList, FSamplerStateRHIParamRef SamplerStateRHI, FTextureRHIParamRef FilterTextureRHI, FTextureRHIParamRef AdditiveTextureRHI, 
		const FLinearColor* SampleWeightValues, const FVector2D* SampleOffsetValues, uint32 NumSamples )
	{
		check(CompileTimeNumSamples == 0 && NumSamples > 0 && NumSamples <= MAX_FILTER_SAMPLES || CompileTimeNumSamples == NumSamples);
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		SetTextureParameter(RHICmdList, ShaderRHI, FilterTexture, FilterTextureSampler, SamplerStateRHI, FilterTextureRHI);
		SetTextureParameter(RHICmdList, ShaderRHI, AdditiveTexture, AdditiveTextureSampler, SamplerStateRHI, AdditiveTextureRHI);
		SetShaderValueArray(RHICmdList, ShaderRHI, SampleWeights, SampleWeightValues, NumSamples);

		if (CompileTimeNumSamples == 0)
		{
			// we needs additional setups for the dynamic loop
			FVector4 PackedSampleOffsetsValues[MAX_PACKED_SAMPLES_OFFSET];

			for(uint32 SampleIndex = 0;SampleIndex < NumSamples;SampleIndex += 2)
			{
				PackedSampleOffsetsValues[SampleIndex / 2].X = SampleOffsetValues[SampleIndex + 0].X;
				PackedSampleOffsetsValues[SampleIndex / 2].Y = SampleOffsetValues[SampleIndex + 0].Y;
				if(SampleIndex + 1 < NumSamples)
				{
					PackedSampleOffsetsValues[SampleIndex / 2].W = SampleOffsetValues[SampleIndex + 1].X;
					PackedSampleOffsetsValues[SampleIndex / 2].Z = SampleOffsetValues[SampleIndex + 1].Y;
				}
			}

			SetShaderValueArray(RHICmdList, ShaderRHI, SampleOffsets, PackedSampleOffsetsValues, MAX_PACKED_SAMPLES_OFFSET);
			SetShaderValue(RHICmdList, ShaderRHI, SampleCount, NumSamples);
		}
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("FilterPixelShader");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("Main");
	}

protected:
	FShaderResourceParameter FilterTexture;
	FShaderResourceParameter FilterTextureSampler;
	FShaderResourceParameter AdditiveTexture;
	FShaderResourceParameter AdditiveTextureSampler;
	FShaderParameter SampleWeights;

	// parameters only for CompileTimeNumSamples == 0
	FShaderParameter SampleOffsets;
	FShaderParameter SampleCount;
};



// #define avoids a lot of code duplication
#define VARIATION1(A)		VARIATION2(A,0)			VARIATION2(A,1)			VARIATION2(A,2)
#define VARIATION2(A, B) typedef TFilterPS<A, B> TFilterPS##A##B; \
	IMPLEMENT_SHADER_TYPE2(TFilterPS##A##B, SF_Pixel);
	VARIATION1(0) // number of samples known at runtime
	VARIATION1(1) VARIATION1(2) VARIATION1(3) VARIATION1(4) VARIATION1(5) VARIATION1(6) VARIATION1(7) VARIATION1(8)
	VARIATION1(9) VARIATION1(10) VARIATION1(11) VARIATION1(12) VARIATION1(13) VARIATION1(14) VARIATION1(15) VARIATION1(16)
	VARIATION1(17) VARIATION1(18) VARIATION1(19) VARIATION1(20) VARIATION1(21) VARIATION1(22) VARIATION1(23) VARIATION1(24)
	VARIATION1(25) VARIATION1(26) VARIATION1(27) VARIATION1(28) VARIATION1(29) VARIATION1(30) VARIATION1(31) VARIATION1(32)
#undef VARIATION1
#undef VARIATION2


/** A vertex shader which filters a texture. Can re reused by other postprocessing pixel shaders. */
template<uint32 NumSamples>
class TFilterVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TFilterVS, Global);
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
			return NumSamples <= MAX_FILTER_COMPILE_TIME_SAMPLES_SM4;
		}
		else
		{
			return NumSamples <= MAX_FILTER_COMPILE_TIME_SAMPLES_ES2;
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
	TFilterVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		SampleOffsets.Bind(Initializer.ParameterMap, TEXT("SampleOffsets"));
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
		for (int32 SampleIndex = 0; SampleIndex < NumSamples; SampleIndex += 2)
		{
			PackedSampleOffsetsValue[SampleIndex / 2].X = SampleOffsetsValue[SampleIndex + 0].X;
			PackedSampleOffsetsValue[SampleIndex / 2].Y = SampleOffsetsValue[SampleIndex + 0].Y;
			if (SampleIndex + 1 < NumSamples)
			{
				PackedSampleOffsetsValue[SampleIndex / 2].W = SampleOffsetsValue[SampleIndex + 1].X;
				PackedSampleOffsetsValue[SampleIndex / 2].Z = SampleOffsetsValue[SampleIndex + 1].Y;
			}
		}
		SetShaderValueArray(RHICmdList, GetVertexShader(), SampleOffsets, PackedSampleOffsetsValue, NumSampleChunks);
	}

private:
	FShaderParameter SampleOffsets;
};

/** A macro to declaring a filter shader type for a specific number of samples. */
#define IMPLEMENT_FILTER_SHADER_TYPE(NumSamples) \
	IMPLEMENT_SHADER_TYPE(template<>,TFilterVS<NumSamples>,TEXT("FilterVertexShader"),TEXT("Main"),SF_Vertex);
	/** The filter shader types for 1-MAX_FILTER_SAMPLES samples. */
	IMPLEMENT_FILTER_SHADER_TYPE(1);
	IMPLEMENT_FILTER_SHADER_TYPE(2);
	IMPLEMENT_FILTER_SHADER_TYPE(3);
	IMPLEMENT_FILTER_SHADER_TYPE(4);
	IMPLEMENT_FILTER_SHADER_TYPE(5);
	IMPLEMENT_FILTER_SHADER_TYPE(6);
	IMPLEMENT_FILTER_SHADER_TYPE(7);
	IMPLEMENT_FILTER_SHADER_TYPE(8);
	IMPLEMENT_FILTER_SHADER_TYPE(9);
	IMPLEMENT_FILTER_SHADER_TYPE(10);
	IMPLEMENT_FILTER_SHADER_TYPE(11);
	IMPLEMENT_FILTER_SHADER_TYPE(12);
	IMPLEMENT_FILTER_SHADER_TYPE(13);
	IMPLEMENT_FILTER_SHADER_TYPE(14);
	IMPLEMENT_FILTER_SHADER_TYPE(15);
	IMPLEMENT_FILTER_SHADER_TYPE(16);
	IMPLEMENT_FILTER_SHADER_TYPE(17);
	IMPLEMENT_FILTER_SHADER_TYPE(18);
	IMPLEMENT_FILTER_SHADER_TYPE(19);
	IMPLEMENT_FILTER_SHADER_TYPE(20);
	IMPLEMENT_FILTER_SHADER_TYPE(21);
	IMPLEMENT_FILTER_SHADER_TYPE(22);
	IMPLEMENT_FILTER_SHADER_TYPE(23);
	IMPLEMENT_FILTER_SHADER_TYPE(24);
	IMPLEMENT_FILTER_SHADER_TYPE(25);
	IMPLEMENT_FILTER_SHADER_TYPE(26);
	IMPLEMENT_FILTER_SHADER_TYPE(27);
	IMPLEMENT_FILTER_SHADER_TYPE(28);
	IMPLEMENT_FILTER_SHADER_TYPE(29);
	IMPLEMENT_FILTER_SHADER_TYPE(30);
	IMPLEMENT_FILTER_SHADER_TYPE(31);
	IMPLEMENT_FILTER_SHADER_TYPE(32);
#undef IMPLEMENT_FILTER_SHADER_TYPE



/**
 * Sets the filter shaders with the provided filter samples.
 * @param SamplerStateRHI - The sampler state to use for the source texture.
 * @param FilterTextureRHI - The source texture.
 * @param AdditiveTextureRHI - The additional source texture, used in CombineMethod=1
 * @param CombineMethodInt 0:weighted filtering, 1: weighted filtering + additional texture, 2: max magnitude
 * @param SampleOffsets - A pointer to an array of NumSamples UV offsets
 * @param SampleWeights - A pointer to an array of NumSamples 4-vector weights
 * @param NumSamples - The number of samples used by the filter.
 * @param OutVertexShader - The vertex shader used for the filter
 */
void SetFilterShaders(
	FRHICommandListImmediate& RHICmdList,
	ERHIFeatureLevel::Type FeatureLevel,
	FSamplerStateRHIParamRef SamplerStateRHI,
	FTextureRHIParamRef FilterTextureRHI,
	FTextureRHIParamRef AdditiveTextureRHI,
	uint32 CombineMethodInt,
	FVector2D* SampleOffsets,
	FLinearColor* SampleWeights,
	uint32 NumSamples,
	FShader** OutVertexShader
	)
{
	check(CombineMethodInt <= 2);
	check(NumSamples <= MAX_FILTER_SAMPLES && NumSamples > 0);

	auto ShaderMap = GetGlobalShaderMap(FeatureLevel);
	const auto DynamicNumSample = CVarLoopMode.GetValueOnRenderThread();
	
	if ((NumSamples > MAX_FILTER_COMPILE_TIME_SAMPLES && DynamicNumSample != 0) || (DynamicNumSample == 2))
	{
		// there is to many samples, so we use the dynamic sample count shader
		
		TShaderMapRef<FPostProcessVS> VertexShader(ShaderMap);
		*OutVertexShader = *VertexShader;
		if(CombineMethodInt == 0)
		{
			TShaderMapRef<TFilterPS<0, 0> > PixelShader(ShaderMap);
			{
				static FGlobalBoundShaderState BoundShaderState;
				SetGlobalBoundShaderState(RHICmdList, FeatureLevel, BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
			}
			PixelShader->SetParameters(RHICmdList, SamplerStateRHI, FilterTextureRHI, AdditiveTextureRHI, SampleWeights, SampleOffsets, NumSamples);
		}
		else if(CombineMethodInt == 1)
		{
			TShaderMapRef<TFilterPS<0, 1> > PixelShader(ShaderMap);
			{
				static FGlobalBoundShaderState BoundShaderState;
				SetGlobalBoundShaderState(RHICmdList, FeatureLevel, BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
			}
			PixelShader->SetParameters(RHICmdList, SamplerStateRHI, FilterTextureRHI, AdditiveTextureRHI, SampleWeights, SampleOffsets, NumSamples);
		}
		else\
		{
			TShaderMapRef<TFilterPS<0, 2> > PixelShader(ShaderMap);
			{
				static FGlobalBoundShaderState BoundShaderState;
				SetGlobalBoundShaderState(RHICmdList, FeatureLevel, BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
			}
			PixelShader->SetParameters(RHICmdList, SamplerStateRHI, FilterTextureRHI, AdditiveTextureRHI, SampleWeights, SampleOffsets, NumSamples);
		}
		return;
	}

	// A macro to handle setting the filter shader for a specific number of samples.
#define SET_FILTER_SHADER_TYPE(CompileTimeNumSamples) \
	case CompileTimeNumSamples: \
	{ \
		TShaderMapRef<TFilterVS<CompileTimeNumSamples> > VertexShader(ShaderMap); \
		*OutVertexShader = *VertexShader; \
		if(CombineMethodInt == 0) \
		{ \
			TShaderMapRef<TFilterPS<CompileTimeNumSamples, 0> > PixelShader(ShaderMap); \
			{ \
				static FGlobalBoundShaderState BoundShaderState; \
				SetGlobalBoundShaderState(RHICmdList, FeatureLevel, BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader); \
			} \
			PixelShader->SetParameters(RHICmdList, SamplerStateRHI, FilterTextureRHI, AdditiveTextureRHI, SampleWeights, SampleOffsets, NumSamples); \
		} \
		else if(CombineMethodInt == 1) \
		{ \
			TShaderMapRef<TFilterPS<CompileTimeNumSamples, 1> > PixelShader(ShaderMap); \
			{ \
				static FGlobalBoundShaderState BoundShaderState; \
				SetGlobalBoundShaderState(RHICmdList, FeatureLevel, BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader); \
			} \
			PixelShader->SetParameters(RHICmdList, SamplerStateRHI, FilterTextureRHI, AdditiveTextureRHI, SampleWeights, SampleOffsets, NumSamples); \
		} \
		else\
		{ \
			TShaderMapRef<TFilterPS<CompileTimeNumSamples, 2> > PixelShader(ShaderMap); \
			{ \
				static FGlobalBoundShaderState BoundShaderState; \
				SetGlobalBoundShaderState(RHICmdList, FeatureLevel, BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader); \
			} \
			PixelShader->SetParameters(RHICmdList, SamplerStateRHI, FilterTextureRHI, AdditiveTextureRHI, SampleWeights, SampleOffsets, NumSamples); \
		} \
		VertexShader->SetParameters(RHICmdList, SampleOffsets); \
		break; \
	};
	
	check(NumSamples <= MAX_FILTER_COMPILE_TIME_SAMPLES);

	// Set the appropriate filter shader for the given number of samples.
	switch(NumSamples)
	{
	SET_FILTER_SHADER_TYPE(1);
	SET_FILTER_SHADER_TYPE(2);
	SET_FILTER_SHADER_TYPE(3);
	SET_FILTER_SHADER_TYPE(4);
	SET_FILTER_SHADER_TYPE(5);
	SET_FILTER_SHADER_TYPE(6);
	SET_FILTER_SHADER_TYPE(7);
	SET_FILTER_SHADER_TYPE(8);
	SET_FILTER_SHADER_TYPE(9);
	SET_FILTER_SHADER_TYPE(10);
	SET_FILTER_SHADER_TYPE(11);
	SET_FILTER_SHADER_TYPE(12);
	SET_FILTER_SHADER_TYPE(13);
	SET_FILTER_SHADER_TYPE(14);
	SET_FILTER_SHADER_TYPE(15);
	SET_FILTER_SHADER_TYPE(16);
	SET_FILTER_SHADER_TYPE(17);
	SET_FILTER_SHADER_TYPE(18);
	SET_FILTER_SHADER_TYPE(19);
	SET_FILTER_SHADER_TYPE(20);
	SET_FILTER_SHADER_TYPE(21);
	SET_FILTER_SHADER_TYPE(22);
	SET_FILTER_SHADER_TYPE(23);
	SET_FILTER_SHADER_TYPE(24);
	SET_FILTER_SHADER_TYPE(25);
	SET_FILTER_SHADER_TYPE(26);
	SET_FILTER_SHADER_TYPE(27);
	SET_FILTER_SHADER_TYPE(28);
	SET_FILTER_SHADER_TYPE(29);
	SET_FILTER_SHADER_TYPE(30);
	SET_FILTER_SHADER_TYPE(31);
	SET_FILTER_SHADER_TYPE(32);
	default:
		UE_LOG(LogRenderer, Fatal,TEXT("Invalid number of samples: %u"),NumSamples);
	}

#undef SET_FILTER_SHADER_TYPE
}

/**
 * Evaluates a normal distribution PDF (around 0) at given X.
 * This function misses the math for scaling the result (faster, not needed if the resulting values are renormalized).
 * @param X - The X to evaluate the PDF at.
 * @param Scale - The normal distribution's variance.
 * @return The value of the normal distribution at X. (unscaled)
 */
static float NormalDistributionUnscaled(float X,float Scale, EFilterShape FilterShape, float CrossCenterWeight)
{
	float Ret;

	if(FilterNewMethod.GetValueOnRenderThread())
	{
		float dxUnScaled = FMath::Abs(X);
		float dxScaled = dxUnScaled * Scale;

		// Constant is tweaked give a similar look to UE4 before we fix the scale bug (Some content tweaking might be needed).
		// The value defines how much of the Gaussian clipped by the sample window. 
		// r.Filter.SizeScale allows to tweak that for performance/quality.
		Ret = FMath::Exp(-16.7f * FMath::Square(dxScaled));

		// tweak the gaussian shape e.g. "r.Bloom.Cross 3.5"
		if (CrossCenterWeight > 1.0f)
		{
			Ret = FMath::Max(0.0f, 1.0f - dxUnScaled);
			Ret = FMath::Pow(Ret, CrossCenterWeight);
		}
		else
		{
			Ret = FMath::Lerp(Ret, FMath::Max(0.0f, 1.0f - dxUnScaled), CrossCenterWeight);
		}
	}
	else
	{
		// will be removed soon
		float OldVariance = 1.0f / Scale;

		float dx = FMath::Abs(X);

		Ret = FMath::Exp(-FMath::Square(dx) / (2.0f * OldVariance));

		// tweak the gaussian shape e.g. "r.Bloom.Cross 3.5"
		if(CrossCenterWeight > 1.0f)
		{
			Ret = FMath::Max(0.0f, 1.0f - dx / OldVariance);
			Ret = FMath::Pow(Ret, CrossCenterWeight);
		}
		else
		{
			Ret = FMath::Lerp(Ret, FMath::Max(0.0f, 1.0f - dx / OldVariance), CrossCenterWeight);
		}
	}

	return Ret;
}

/**
 * @return NumSamples >0
 */

static uint32 Compute1DGaussianFilterKernel(ERHIFeatureLevel::Type InFeatureLevel, float KernelRadius, FVector2D OutOffsetAndWeight[MAX_FILTER_SAMPLES], uint32 MaxFilterSamples, EFilterShape FilterShape, float CrossCenterWeight)
{
	float FilterSizeScale = FMath::Clamp(CVarFilterSizeScale.GetValueOnRenderThread(), 0.1f, 10.0f);

	float ClampedKernelRadius = FRCPassPostProcessWeightedSampleSum::GetClampedKernelRadius(InFeatureLevel, KernelRadius);
	int32 IntegerKernelRadius = FRCPassPostProcessWeightedSampleSum::GetIntegerKernelRadius(InFeatureLevel, KernelRadius * FilterSizeScale);

	float Scale = 1.0f / ClampedKernelRadius;

	// smallest IntegerKernelRadius will be 1

	uint32 NumSamples = 0;
	float WeightSum = 0.0f;
	for(int32 SampleIndex = -IntegerKernelRadius; SampleIndex <= IntegerKernelRadius; SampleIndex += 2)
	{
		float Weight0 = NormalDistributionUnscaled(SampleIndex, Scale, FilterShape, CrossCenterWeight);
		float Weight1 = 0.0f;

		// Because we use bilinear filtering we only require half the sample count.
		// But we need to fix the last weight.
		// Example (radius one texel, c is center, a left, b right):
		//    a b c (a is left texel, b center and c right) becomes two lookups one with a*.. + b **, the other with
		//    c * .. but another texel to the right would accidentially leak into this computation.
		if(SampleIndex != IntegerKernelRadius)
		{
			Weight1 = NormalDistributionUnscaled(SampleIndex + 1, Scale, FilterShape, CrossCenterWeight);
		}

		float TotalWeight = Weight0 + Weight1;
		OutOffsetAndWeight[NumSamples].X = (SampleIndex + Weight1 / TotalWeight);
		OutOffsetAndWeight[NumSamples].Y = TotalWeight;
		WeightSum += TotalWeight;
		NumSamples++;
	}

	// Normalize blur weights.
	float InvWeightSum = 1.0f / WeightSum;
	for(uint32 SampleIndex = 0;SampleIndex < NumSamples; ++SampleIndex)
	{
		OutOffsetAndWeight[SampleIndex].Y *= InvWeightSum;
	}

	return NumSamples;
}

FRCPassPostProcessWeightedSampleSum::FRCPassPostProcessWeightedSampleSum(EFilterShape InFilterShape, EFilterCombineMethod InCombineMethod, float InSizeScale, const TCHAR* InDebugName, FLinearColor InTintValue)
	: FilterShape(InFilterShape)
	, CombineMethod(InCombineMethod)
	, SizeScale(InSizeScale)
	, TintValue(InTintValue)
	, DebugName(InDebugName)
	, CrossCenterWeight(0.0f)
{
}

void FRCPassPostProcessWeightedSampleSum::Process(FRenderingCompositePassContext& Context)
{
	const FSceneView& View = Context.View;

	FRenderingCompositeOutput *Input = GetInput(ePId_Input0)->GetOutput();

	// input is not hooked up correctly
	check(Input);

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	// input is not hooked up correctly
	check(InputDesc);

	const FSceneViewFamily& ViewFamily = *(View.Family);

	FIntPoint SrcSize = InputDesc->Extent;
	FIntPoint DestSize = PassOutputs[0].RenderTargetDesc.Extent;
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(Context.RHICmdList);

	// e.g. 4 means the input texture is 4x smaller than the buffer size
	const FIntPoint BufferSize = SceneContext.GetBufferSizeXY();

	const uint32 SrcScaleFactorX = FMath::DivideAndRoundUp(BufferSize.X, SrcSize.X);
	const uint32 SrcScaleFactorY = FMath::DivideAndRoundUp(BufferSize.Y, SrcSize.Y);
	const FIntPoint SrcScaleFactor(SrcScaleFactorX, SrcScaleFactorY);

	const uint32 DstScaleFactorX = FMath::DivideAndRoundUp(BufferSize.X, DestSize.X);
	const uint32 DstScaleFactorY = FMath::DivideAndRoundUp(BufferSize.Y, DestSize.Y);
	const FIntPoint DstScaleFactor(DstScaleFactorX, DstScaleFactorY);

	TRefCountPtr<IPooledRenderTarget> InputPooledElement = Input->RequestInput();

	check(!InputPooledElement->IsFree());

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	bool bDoFastBlur = DoFastBlur();

	FVector2D InvSrcSize(1.0f / SrcSize.X, 1.0f / SrcSize.Y);
	// we scale by width because FOV is defined horizontally
	float SrcSizeForThisAxis = View.ViewRect.Width() / (float)SrcScaleFactor.X;

	if(bDoFastBlur && FilterShape == EFS_Vert)
	{
		SrcSizeForThisAxis *= 2.0f;
	}

	// in texel (input resolution), /2 as we use the diameter, 100 as we use percent
	float EffectiveBlurRadius = SizeScale * SrcSizeForThisAxis  / 2 / 100.0f;

	FVector2D BlurOffsets[MAX_FILTER_SAMPLES];
	FLinearColor BlurWeights[MAX_FILTER_SAMPLES];
	FVector2D OffsetAndWeight[MAX_FILTER_SAMPLES];

	const auto FeatureLevel = Context.View.GetFeatureLevel();

	// compute 1D filtered samples
	uint32 MaxNumSamples = GetMaxNumSamples(FeatureLevel);

	uint32 NumSamples = Compute1DGaussianFilterKernel(FeatureLevel, EffectiveBlurRadius, OffsetAndWeight, MaxNumSamples, FilterShape, CrossCenterWeight);

	FIntRect DestRect = FIntRect::DivideAndRoundUp(View.ViewRect, DstScaleFactor);

	SCOPED_DRAW_EVENTF(Context.RHICmdList, PostProcessWeightedSampleSum, TEXT("PostProcessWeightedSampleSum#%d %dx%d in %dx%d"),
		NumSamples, DestRect.Width(), DestRect.Height(), DestSize.X, DestSize.Y);

	// compute weights as weighted contributions of the TintValue
	for(uint32 i = 0; i < NumSamples; ++i)
	{
		BlurWeights[i] = TintValue * OffsetAndWeight[i].Y;
	}


	bool bRequiresClear = true;
	// check if we have to clear the whole surface.
	// Otherwise perform the clear when the dest rectangle has been computed.
	if (FeatureLevel == ERHIFeatureLevel::ES2 || FeatureLevel == ERHIFeatureLevel::ES3_1)
	{
		bRequiresClear = false;
		SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef(), ESimpleRenderTargetMode::EClearColorAndDepth);
	}
	else
	{
		SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef(), ESimpleRenderTargetMode::EExistingColorAndDepth);	
	}

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DestSize.X, DestSize.Y, 1.0f);

	FIntRect SrcRect = FIntRect::DivideAndRoundUp(View.ViewRect, SrcScaleFactor);
	if (bRequiresClear)
	{
		DrawClear(Context.RHICmdList, FeatureLevel, bDoFastBlur, SrcRect, DestRect, DestSize);
	}

	// set the state
	Context.RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	const FTextureRHIRef& FilterTexture = InputPooledElement->GetRenderTargetItem().ShaderResourceTexture;

	FRenderingCompositeOutputRef* NodeInput1 = GetInput(ePId_Input1);

	FRenderingCompositeOutput *Input1 = NodeInput1 ? NodeInput1->GetOutput() : 0;

	uint32 CombineMethodInt = 0;

	if(CombineMethod == EFCM_MaxMagnitude)
	{
		CombineMethodInt = 2;
	}

	// can be optimized
	FTextureRHIRef AdditiveTexture;

	if(Input1)
	{
		TRefCountPtr<IPooledRenderTarget> InputPooledElement1 = Input1->RequestInput();
		AdditiveTexture = InputPooledElement1->GetRenderTargetItem().ShaderResourceTexture;

		check(CombineMethod == EFCM_Weighted);

		CombineMethodInt = 1;
	}

	if (FilterShape == EFS_Horiz)
	{
		float YOffset = bDoFastBlur ? (InvSrcSize.Y * 0.5f) : 0.0f;
		for (uint32 i = 0; i < NumSamples; ++i)
		{
			BlurOffsets[i] = FVector2D(InvSrcSize.X * OffsetAndWeight[i].X, YOffset);
		}
	}
	else
	{
		float YOffset = bDoFastBlur ? -(InvSrcSize.Y * 0.5f) : 0.0f;
		for (uint32 i = 0; i < NumSamples; ++i)
		{
			BlurOffsets[i] = FVector2D(0, InvSrcSize.Y * OffsetAndWeight[i].X + YOffset);
		}
	}

	FShader* VertexShader = nullptr;
	SetFilterShaders(
		Context.RHICmdList,
		FeatureLevel,
		TStaticSamplerState<SF_Bilinear,AM_Border,AM_Border,AM_Clamp>::GetRHI(),
		FilterTexture,
		AdditiveTexture,
		CombineMethodInt,
		BlurOffsets,
		BlurWeights,
		NumSamples,
		&VertexShader
		);	

	DrawQuad(Context.RHICmdList, FeatureLevel, bDoFastBlur, SrcRect, DestRect, DestSize, SrcSize, VertexShader);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessWeightedSampleSum::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	if(DoFastBlur())
	{
		if(FilterShape == EFS_Horiz)
		{
			Ret.Extent.X = FMath::DivideAndRoundUp(Ret.Extent.X, 2);
		}
		else
		{
			// not perfect - we might get a RT one texel larger
			Ret.Extent.X *= 2;
		}
	}

	Ret.Reset();
	Ret.DebugName = DebugName;
	Ret.AutoWritable = false;

	Ret.ClearValue = FClearValueBinding(FLinearColor(0, 0, 0, 0));

	return Ret;
}


static TAutoConsoleVariable<float> CVarFastBlurThreshold(
	TEXT("r.FastBlurThreshold"),
	7.0f,
	TEXT("Defines at what radius the Gaussian blur optimization kicks in (estimated 25% - 40% faster).\n")
	TEXT("The optimzation uses slightly less memory and has a quality loss on smallblur radius.\n")
	TEXT("  0: use the optimization always (fastest, lowest quality)\n")
	TEXT("  3: use the optimization starting at a 3 pixel radius (quite fast)\n")
	TEXT("  7: use the optimization starting at a 7 pixel radius (default)\n")
	TEXT(">15: barely ever use the optimization (high quality)"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

bool FRCPassPostProcessWeightedSampleSum::DoFastBlur() const
{
	bool bRet = false;

	// only do the fast blur only with bilinear filtering
	if(CombineMethod == EFCM_Weighted)
	{
		const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

		// input is not hooked up correctly
		check(InputDesc);

		if(FilterShape == EFS_Horiz)
		{
			FIntPoint SrcSize = InputDesc->Extent;

			int32 SrcSizeForThisAxis = SrcSize.X;

			// in texel (input resolution), *2 as we use the diameter
			// we scale by width because FOV is defined horizontally
			float EffectiveBlurRadius = SizeScale * SrcSizeForThisAxis  * 2 / 100.0f;

#if PLATFORM_HTML5
			float FastBlurThreshold = CVarFastBlurThreshold.GetValueOnGameThread();
#else
			float FastBlurThreshold = CVarFastBlurThreshold.GetValueOnRenderThread();
#endif

			// small radius look too different with this optimization so we only to it for larger radius
			bRet = EffectiveBlurRadius >= FastBlurThreshold;
		}
		else
		{
			FIntPoint SrcSize = InputDesc->Extent;
			FIntPoint BufferSize = FSceneRenderTargets::Get_FrameConstantsOnly().GetBufferSizeXY();

			float InputRatio = SrcSize.X / (float)SrcSize.Y;
			float BufferRatio = BufferSize.X / (float)BufferSize.Y;

			// Half res input detected
			bRet = InputRatio < BufferRatio * 0.75f;
		}
	}

	return bRet;
}

void FRCPassPostProcessWeightedSampleSum::AdjustRectsForFastBlur(FIntRect& SrcRect, FIntRect& DestRect) const
{	
	if (FilterShape == EFS_Horiz)
	{
		SrcRect.Min.X = DestRect.Min.X * 2;
		SrcRect.Max.X = DestRect.Max.X * 2;
	}
	else
	{
		DestRect.Min.X = SrcRect.Min.X * 2;
		DestRect.Max.X = SrcRect.Max.X * 2;
	}	
}

void FRCPassPostProcessWeightedSampleSum::DrawClear(FRHICommandListImmediate& RHICmdList, ERHIFeatureLevel::Type FeatureLevel, bool bDoFastBlur, FIntRect SrcRect, FIntRect DestRect, FIntPoint DestSize) const
{
	if (bDoFastBlur)
	{
		AdjustRectsForFastBlur(SrcRect, DestRect);
	}

	DrawClearQuad(RHICmdList, FeatureLevel, true, FLinearColor(0, 0, 0, 0), false, 1.0f, false, 0, DestSize, DestRect);	
}

void FRCPassPostProcessWeightedSampleSum::DrawQuad(FRHICommandListImmediate& RHICmdList, ERHIFeatureLevel::Type FeatureLevel, bool bDoFastBlur, FIntRect SrcRect, FIntRect DestRect, FIntPoint DestSize, FIntPoint SrcSize, FShader* VertexShader) const
{	
	if (bDoFastBlur)
	{
		AdjustRectsForFastBlur(SrcRect, DestRect);
	}	

	// Draw a quad mapping scene color to the view's render target
	DrawRectangle(
		RHICmdList,
		DestRect.Min.X, DestRect.Min.Y,
		DestRect.Width(), DestRect.Height(),
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width(), SrcRect.Height(),
		DestSize,
		SrcSize,
		VertexShader,
		EDRF_UseTriangleOptimization);
}

uint32 FRCPassPostProcessWeightedSampleSum::GetMaxNumSamples(ERHIFeatureLevel::Type InFeatureLevel)
{
	if (CVarLoopMode.GetValueOnRenderThread() != 0)
	{
		return MAX_FILTER_SAMPLES;
	}
	
	uint32 MaxNumSamples = MAX_FILTER_COMPILE_TIME_SAMPLES;

	if (InFeatureLevel == ERHIFeatureLevel::SM4)
	{
		MaxNumSamples = MAX_FILTER_COMPILE_TIME_SAMPLES_SM4;
	}
	else if (InFeatureLevel < ERHIFeatureLevel::SM4)
	{
		MaxNumSamples = MAX_FILTER_COMPILE_TIME_SAMPLES_ES2;
	}
	return MaxNumSamples;
}

float FRCPassPostProcessWeightedSampleSum::GetClampedKernelRadius(ERHIFeatureLevel::Type InFeatureLevel, float KernelRadius)
{
	return FMath::Clamp<float>(KernelRadius, DELTA, GetMaxNumSamples(InFeatureLevel) - 1);
}

int FRCPassPostProcessWeightedSampleSum::GetIntegerKernelRadius(ERHIFeatureLevel::Type InFeatureLevel, float KernelRadius)
{
	return FMath::Min<int32>(FMath::CeilToInt(GetClampedKernelRadius(InFeatureLevel, KernelRadius)), GetMaxNumSamples(InFeatureLevel) - 1);
}
