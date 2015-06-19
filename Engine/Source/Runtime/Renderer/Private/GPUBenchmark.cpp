// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GPUBenchmark.cpp: GPUBenchmark to compute performance index to set video options automatically
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "GPUBenchmark.h"
#include "SceneUtils.h"

static uint32 GBenchmarkResolution = 512;

DEFINE_LOG_CATEGORY_STATIC(LogSynthBenchmark, Log, All);


// todo: get rid of global
FRenderQueryPool GTimerQueryPool(RQT_AbsoluteTime);

/** Encapsulates the post processing down sample pixel shader. */
template <uint32 Method>
class FPostProcessBenchmarkPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessBenchmarkPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("METHOD"), Method);
	}

	/** Default constructor. */
	FPostProcessBenchmarkPS() {}

public:
	FShaderResourceParameter InputTexture;
	FShaderResourceParameter InputTextureSampler;

	/** Initialization constructor. */
	FPostProcessBenchmarkPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		InputTexture.Bind(Initializer.ParameterMap,TEXT("InputTexture"));
		InputTextureSampler.Bind(Initializer.ParameterMap,TEXT("InputTextureSampler"));
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << InputTexture << InputTextureSampler;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, TRefCountPtr<IPooledRenderTarget>& Src)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);

		SetTextureParameter(RHICmdList, ShaderRHI, InputTexture, InputTextureSampler, TStaticSamplerState<>::GetRHI(), Src->GetRenderTargetItem().ShaderResourceTexture);
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("GPUBenchmark");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("MainPS");
	}
};

// #define avoids a lot of code duplication
#define VARIATION1(A) typedef FPostProcessBenchmarkPS<A> FPostProcessBenchmarkPS##A; \
	IMPLEMENT_SHADER_TYPE2(FPostProcessBenchmarkPS##A, SF_Pixel);

VARIATION1(0)			VARIATION1(1)			VARIATION1(2)			VARIATION1(3)			VARIATION1(4)
#undef VARIATION1


/** Encapsulates the post processing down sample vertex shader. */
class FPostProcessBenchmarkVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessBenchmarkVS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}

	/** Default constructor. */
	FPostProcessBenchmarkVS() {}
	
	/** Initialization constructor. */
	FPostProcessBenchmarkVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
	}

	/** Serializer */
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);

		return bShaderHasOutdatedParameters;
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();
		
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessBenchmarkVS,TEXT("GPUBenchmark"),TEXT("MainBenchmarkVS"),SF_Vertex);


template <uint32 Method>
void RunBenchmarkShader(FRHICommandList& RHICmdList, const FSceneView& View, TRefCountPtr<IPooledRenderTarget>& Src, float WorkScale)
{
	auto ShaderMap = GetGlobalShaderMap(View.GetFeatureLevel());

	TShaderMapRef<FPostProcessBenchmarkVS> VertexShader(ShaderMap);
	TShaderMapRef<FPostProcessBenchmarkPS<Method> > PixelShader(ShaderMap);

	static FGlobalBoundShaderState BoundShaderState;
	

	SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	PixelShader->SetParameters(RHICmdList, View, Src);
	VertexShader->SetParameters(RHICmdList, View);

	// single pass was not fine grained enough so we reduce the pass size based on the fractional part of WorkScale
	float TotalHeight = GBenchmarkResolution * WorkScale;

	// rounds up
	uint32 PassCount = (uint32)FMath::CeilToFloat(TotalHeight / GBenchmarkResolution);

	for(uint32 i = 0; i < PassCount; ++i)
	{
		float Top = i * GBenchmarkResolution;
		float Bottom = FMath::Min(Top + GBenchmarkResolution, TotalHeight);
		float LocalHeight = Bottom - Top;

		DrawRectangle(
			RHICmdList,
			0, 0,
			GBenchmarkResolution, LocalHeight,
			0, 0,
			GBenchmarkResolution, LocalHeight,
			FIntPoint(GBenchmarkResolution, GBenchmarkResolution),
			FIntPoint(GBenchmarkResolution, GBenchmarkResolution),
			*VertexShader,
			EDRF_Default);
	}
}

void RunBenchmarkShader(FRHICommandListImmediate& RHICmdList, const FSceneView& View, uint32 MethodId, TRefCountPtr<IPooledRenderTarget>& Src, float WorkScale)
{
	SCOPED_DRAW_EVENTF(RHICmdList, Benchmark, TEXT("Benchmark Method:%d"), MethodId);

	switch(MethodId)
	{
		case 0: RunBenchmarkShader<0>(RHICmdList, View, Src, WorkScale); return;
		case 1: RunBenchmarkShader<1>(RHICmdList, View, Src, WorkScale); return;
		case 2: RunBenchmarkShader<2>(RHICmdList, View, Src, WorkScale); return;
		case 3: RunBenchmarkShader<3>(RHICmdList, View, Src, WorkScale); return;
		case 4: RunBenchmarkShader<4>(RHICmdList, View, Src, WorkScale); return;

		default:
			check(0);
	}
}

// Many Benchmark timings stored in an array to allow to extract a good value dropping outliers
// We need to get rid of the bad samples.
class FTimingSeries
{
public:
	// @param ArraySize
	void Init(uint32 ArraySize)
	{
		check(ArraySize > 0);

		TimingValues.AddZeroed(ArraySize);
	}

	//
	void SetEntry(uint32 Index, float TimingValue)
	{
		check(Index < (uint32)TimingValues.Num());

		TimingValues[Index] = TimingValue;
	}

	//
	float GetEntry(uint32 Index) const
	{
		check(Index < (uint32)TimingValues.Num());

		return TimingValues[Index];
	}

	// @param OutConfidence
	float ComputeValue(float& OutConfidence) const
	{
		float Ret = 0.0f;

		TArray<float> SortedValues;
		{
			// a lot of values in the beginning are wrong, we cut off some part (1/4 of the samples area)
			uint32 StartIndex = TimingValues.Num() / 3;

			for(uint32 Index = StartIndex; Index < (uint32)TimingValues.Num(); ++Index)
			{
				SortedValues.Add(TimingValues[Index]);
			}
			SortedValues.Sort();
		}

		OutConfidence = 0.0f;
		
		uint32 Passes = 10;

		// slow but simple
		for(uint32 Pass = 0; Pass < Passes; ++Pass)
		{
			// 0..1, 0 not included
			float Alpha = (Pass + 1) / (float)Passes;

			int32 MidIndex = SortedValues.Num() / 2;
			int32 FromIndex = (int32)FMath::Lerp(MidIndex, 0, Alpha); 
			int32 ToIndex = (int32)FMath::Lerp(MidIndex, SortedValues.Num(), Alpha); 

			float Delta = 0.0f;
			float Confidence = 0.0f;

			float TimingValue = ComputeTimingFromSortedRange(FromIndex, ToIndex, SortedValues, Delta, Confidence);

			// aim for 5% delta and some samples
			if(Pass > 0 && Delta > TimingValue * 0.5f)
			{
				// it gets worse, take the best one we had so far
				break;
			}

			OutConfidence = Confidence;
			Ret = TimingValue;
		}

		return Ret;
	}

private:
	// @param FromIndex within SortedValues
	// @param ToIndex within SortedValues
	// @param OutDelta +/- 
	// @param OutConfidence 0..100 0=not at all, 100=fully, meaning how many samples are considered useful
	// @return TimingValue, smaller is better
	static float ComputeTimingFromSortedRange(int32 FromIndex, int32 ToIndex, const TArray<float>& SortedValues, float& OutDelta, float& OutConfidence)
	{
		float ValidSum = 0;
		uint32 ValidCount = 0;
		float Min = FLT_MAX;
		float Max = -FLT_MAX;
		{
			for(int32 Index = FromIndex; Index < ToIndex; ++Index)
			{
				float Value = SortedValues[Index];

				Min = FMath::Min(Min, Value);
				Max = FMath::Max(Max, Value);

				ValidSum += Value;
				++ValidCount;
			}
		}

		if(ValidCount)
		{
			OutDelta = (Max - Min) * 0.5f;

			OutConfidence = 100.0f * ValidCount / (float)SortedValues.Num();

			return ValidSum / ValidCount;
		}
		else
		{
			OutDelta = 0.0f;
			OutConfidence = 0.0f;
			return 0.0f;
		}
	}

	TArray<float> TimingValues;
};

void RendererGPUBenchmark(FRHICommandListImmediate& RHICmdList, FSynthBenchmarkResults& InOut, const FSceneView& View, float WorkScale, bool bDebugOut)
{
	check(IsInRenderingThread());

	// two RT to ping pong so we force the GPU to flush it's pipeline
	TRefCountPtr<IPooledRenderTarget> RTItems[3];
	{
		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(FIntPoint(GBenchmarkResolution, GBenchmarkResolution), PF_B8G8R8A8, TexCreate_None, TexCreate_RenderTargetable | TexCreate_ShaderResource, false));
		GRenderTargetPool.FindFreeElement(Desc, RTItems[0], TEXT("Benchmark0"));
		GRenderTargetPool.FindFreeElement(Desc, RTItems[1], TEXT("Benchmark1"));

		Desc.Extent = FIntPoint(1, 1);
		Desc.Flags = TexCreate_CPUReadback;	// needs TexCreate_ResolveTargetable?
		Desc.TargetableFlags = TexCreate_None;

		GRenderTargetPool.FindFreeElement(Desc, RTItems[2], TEXT("BenchmarkReadback"));
	}

	// set the state
	RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false,CF_Always>::GetRHI());

	{
		// larger number means more accuracy but slower, some slower GPUs might timeout with a number to large
		const uint32 IterationCount = 70;
		const uint32 MethodCount = ARRAY_COUNT(InOut.GPUStats);

		// 0 / 1
		uint32 DestRTIndex = 0;

		const uint32 TimerSampleCount = IterationCount * MethodCount + 1;

		static FRenderQueryRHIRef TimerQueries[TimerSampleCount];
		static float LocalWorkScale[IterationCount];

		for(uint32  i = 0; i < TimerSampleCount; ++i)
		{
			TimerQueries[i] = GTimerQueryPool.AllocateQuery();
		}

		const bool bSupportsTimerQueries = (TimerQueries[0] != NULL);
		if(!bSupportsTimerQueries)
		{
			UE_LOG(LogSynthBenchmark, Warning, TEXT("GPU driver does not support timer queries."));

			// Temporary workaround for GL_TIMESTAMP being unavailable and GL_TIME_ELAPSED workaround breaking drivers
#if PLATFORM_MAC
			GLint RendererID = 0;
			float PerfScale = 1.0f;
			[[NSOpenGLContext currentContext] getValues:&RendererID forParameter:NSOpenGLCPCurrentRendererID];
			{
				switch((RendererID & kCGLRendererIDMatchingMask))
				{
					case kCGLRendererATIRadeonX4000ID: // AMD 7xx0 & Dx00 series - should be pretty beefy
						PerfScale = 1.2f;
						break;
					case kCGLRendererATIRadeonX3000ID: // AMD 5xx0, 6xx0 series - mostly OK
					case kCGLRendererGeForceID: // Nvidia 6x0 & 7x0 series - mostly OK
						PerfScale = 2.0f;
						break;
					case kCGLRendererIntelHD5000ID: // Intel HD 5000, Iris, Iris Pro - not dreadful
						PerfScale = 4.2f;
						break;
					case kCGLRendererIntelHD4000ID: // Intel HD 4000 - quite slow
						PerfScale = 7.5f;
						break;
					case kCGLRendererATIRadeonX2000ID: // ATi 4xx0, 3xx0, 2xx0 - almost all very slow and drivers are now very buggy
					case kCGLRendererGeForce8xxxID: // Nvidia 3x0, 2x0, 1x0, 9xx0, 8xx0 - almost all very slow
					case kCGLRendererIntelHDID: // Intel HD 3000 - very, very slow and very buggy driver
					default:
						PerfScale = 10.0f;
						break;
				}
			}
			
			InOut.GPUStats[0] = FSynthBenchmarkStat(TEXT("ALUHeavyNoise"), 1.0f / 4.601f, TEXT("s/GigaPix"));
			InOut.GPUStats[1] = FSynthBenchmarkStat(TEXT("TexHeavy"), 1.0f / 7.447f, TEXT("s/GigaPix"));
			InOut.GPUStats[2] = FSynthBenchmarkStat(TEXT("DepTexHeavy"), 1.0f / 3.847f, TEXT("s/GigaPix"));
			InOut.GPUStats[3] = FSynthBenchmarkStat(TEXT("FillOnly"), 1.0f / 25.463f, TEXT("s/GigaPix"));
			InOut.GPUStats[4] = FSynthBenchmarkStat(TEXT("Bandwidth"), 1.0f / 1.072f, TEXT("s/GigaPix"));
			InOut.GPUStats[0].SetMeasuredTime( FTimeSample(PerfScale, PerfScale * (1.0f / 4.601f)) );
			InOut.GPUStats[1].SetMeasuredTime( FTimeSample(PerfScale, PerfScale * (1.0f / 7.447f)) );
			InOut.GPUStats[2].SetMeasuredTime( FTimeSample(PerfScale, PerfScale * (1.0f / 3.847f)) );
			InOut.GPUStats[3].SetMeasuredTime( FTimeSample(PerfScale, PerfScale * (1.0f / 25.463f)) );
			InOut.GPUStats[4].SetMeasuredTime( FTimeSample(PerfScale, PerfScale * (1.0f / 1.072f)) );
#endif
			return;
		}

		// TimingValues are in Seconds
		FTimingSeries TimingSeries[MethodCount];
		// in 1/1000000 Seconds
		uint64 TotalTimes[MethodCount];
		
		for(uint32 MethodIterator = 0; MethodIterator < MethodCount; ++MethodIterator)
		{
			TotalTimes[MethodIterator] = 0;
			TimingSeries[MethodIterator].Init(IterationCount);
		}

		check(MethodCount == 5);
		InOut.GPUStats[0] = FSynthBenchmarkStat(TEXT("ALUHeavyNoise"), 1.0f / 4.601f, TEXT("s/GigaPix"));
		InOut.GPUStats[1] = FSynthBenchmarkStat(TEXT("TexHeavy"), 1.0f / 7.447f, TEXT("s/GigaPix"));
		InOut.GPUStats[2] = FSynthBenchmarkStat(TEXT("DepTexHeavy"), 1.0f / 3.847f, TEXT("s/GigaPix"));
		InOut.GPUStats[3] = FSynthBenchmarkStat(TEXT("FillOnly"), 1.0f / 25.463f, TEXT("s/GigaPix"));
		InOut.GPUStats[4] = FSynthBenchmarkStat(TEXT("Bandwidth"), 1.0f / 1.072f, TEXT("s/GigaPix"));

		// e.g. on NV670: Method3 (mostly fill rate )-> 26GP/s (seems realistic)
		// reference: http://en.wikipedia.org/wiki/Comparison_of_Nvidia_graphics_processing_units theoretical: 29.3G/s

		RHICmdList.EndRenderQuery(TimerQueries[0]);

		// multiple iterations to see how trust able the values are
		for(uint32 Iteration = 0; Iteration < IterationCount; ++Iteration)
		{
			for(uint32 MethodIterator = 0; MethodIterator < MethodCount; ++MethodIterator)
			{
				// alternate between forward and backward (should give the same number)
				//			uint32 MethodId = (Iteration % 2) ? MethodIterator : (MethodCount - 1 - MethodIterator);
				uint32 MethodId = MethodIterator;

				uint32 QueryIndex = 1 + Iteration * MethodCount + MethodId;

				// 0 / 1
				const uint32 SrcRTIndex = 1 - DestRTIndex;

				GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, RTItems[DestRTIndex]);

				SetRenderTarget(RHICmdList, RTItems[DestRTIndex]->GetRenderTargetItem().TargetableTexture, FTextureRHIRef());	

				// decide how much work we do in this pass
				LocalWorkScale[Iteration] = (Iteration / 10.f + 1.f) * WorkScale;

				RunBenchmarkShader(RHICmdList, View, MethodId, RTItems[SrcRTIndex], LocalWorkScale[Iteration]);

				RHICmdList.CopyToResolveTarget(RTItems[DestRTIndex]->GetRenderTargetItem().TargetableTexture, RTItems[DestRTIndex]->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());

				/*if(bGPUCPUSync)
				{
					// more consistent timing but strangely much faster to the level that is unrealistic

					FResolveParams Param;

					Param.Rect = FResolveRect(0, 0, 1, 1);
					RHICmdList.CopyToResolveTarget(
						RTItems[DestRTIndex]->GetRenderTargetItem().TargetableTexture,
						RTItems[2]->GetRenderTargetItem().ShaderResourceTexture,
						false,
						Param);

					void* Data = 0;
					int Width = 0;
					int Height = 0;

					RHIMapStagingSurface(RTItems[2]->GetRenderTargetItem().ShaderResourceTexture, Data, Width, Height);
					RHIUnmapStagingSurface(RTItems[2]->GetRenderTargetItem().ShaderResourceTexture);
				}*/

				RHICmdList.EndRenderQuery(TimerQueries[QueryIndex]);

				// ping pong
				DestRTIndex = 1 - DestRTIndex;
			}
		}

		{
			uint64 OldAbsTime = 0;
			// flushes the RHI thread to make sure all RHICmdList.EndRenderQuery() commands got executed.
			RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
			RHICmdList.GetRenderQueryResult(TimerQueries[0], OldAbsTime, true);
			GTimerQueryPool.ReleaseQuery(RHICmdList, TimerQueries[0]);

			for(uint32 Iteration = 0; Iteration < IterationCount; ++Iteration)
			{
				uint32 Results[MethodCount];

				for(uint32 MethodId = 0; MethodId < MethodCount; ++MethodId)
				{
					uint32 QueryIndex = 1 + Iteration * MethodCount + MethodId;

					uint64 AbsTime;
					RHICmdList.GetRenderQueryResult(TimerQueries[QueryIndex], AbsTime, true);
					GTimerQueryPool.ReleaseQuery(RHICmdList, TimerQueries[QueryIndex]);

					uint64 RelTime = AbsTime - OldAbsTime; 

					TotalTimes[MethodId] += RelTime;
					Results[MethodId] = RelTime;

					OldAbsTime = AbsTime;
				}

				for(uint32 MethodId = 0; MethodId < MethodCount; ++MethodId)
				{
					float TimeInSec = Results[MethodId] / 1000000.0f;

					// to normalize from seconds to seconds per GPixel
					float SamplesInGPix = LocalWorkScale[Iteration] * GBenchmarkResolution * GBenchmarkResolution / 1000000000.0f;

					// TimingValue in Seconds per GPixel
					TimingSeries[MethodId].SetEntry(Iteration, TimeInSec / SamplesInGPix);
				}
			}

			if(bSupportsTimerQueries)
			{
				for(uint32 MethodId = 0; MethodId < MethodCount; ++MethodId)
				{
					float Confidence = 0.0f;
					// in seconds per GPixel
					float NormalizedTime = TimingSeries[MethodId].ComputeValue(Confidence);

					if(Confidence > 0)
					{
						FTimeSample TimeSample(TotalTimes[MethodId] / 1000000.0f, NormalizedTime);

						InOut.GPUStats[MethodId].SetMeasuredTime(TimeSample, Confidence);
					}
				}
			}
		}
	}
}
