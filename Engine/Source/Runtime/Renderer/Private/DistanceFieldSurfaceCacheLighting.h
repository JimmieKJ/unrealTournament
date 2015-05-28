// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DistanceFieldSurfaceCacheLighting.h
=============================================================================*/

#pragma once

#include "ScreenRendering.h"

const static int32 GAOMaxSupportedLevel = 6;
/** Number of cone traced directions. */
const int32 NumConeSampleDirections = 9;

class FDistanceFieldAOParameters
{
public:
	float OcclusionMaxDistance;
	float Contrast;

	FDistanceFieldAOParameters(float InOcclusionMaxDistance, float InContrast = 0)
	{
		Contrast = FMath::Clamp(InContrast, .01f, 2.0f);
		OcclusionMaxDistance = FMath::Clamp(InOcclusionMaxDistance, 200.0f, 3000.0f);
	}
};

/**  */
class FRefinementLevelResources
{
public:

	FRefinementLevelResources()
	{
		MaxIrradianceCacheSamples = 0;
	}

	void InitDynamicRHI()
	{
		if (MaxIrradianceCacheSamples > 0)
		{
			//@todo - handle max exceeded
			PositionAndRadius.Initialize(sizeof(float) * 4, MaxIrradianceCacheSamples, PF_A32B32G32R32F, BUF_Static);
			OccluderRadius.Initialize(sizeof(float), MaxIrradianceCacheSamples, PF_R32_FLOAT, BUF_Static);
			Normal.Initialize(sizeof(FFloat16Color), MaxIrradianceCacheSamples, PF_FloatRGBA, BUF_Static);
			BentNormal.Initialize(sizeof(FFloat16Color), MaxIrradianceCacheSamples, PF_FloatRGBA, BUF_Static);
			Irradiance.Initialize(sizeof(FFloat16Color), MaxIrradianceCacheSamples, PF_FloatRGBA, BUF_Static);
			ScatterDrawParameters.Initialize(sizeof(uint32), 4, PF_R32_UINT, BUF_Static | BUF_DrawIndirect);
			SavedStartIndex.Initialize(sizeof(uint32), 1, PF_R32_UINT, BUF_Static);
			TileCoordinate.Initialize(sizeof(uint16) * 2, MaxIrradianceCacheSamples, PF_R16G16_UINT, BUF_Static);
		}
	}

	void ReleaseDynamicRHI()
	{
		PositionAndRadius.Release();
		OccluderRadius.Release();
		Normal.Release();
		BentNormal.Release();
		Irradiance.Release();
		ScatterDrawParameters.Release();
		SavedStartIndex.Release();
		TileCoordinate.Release();
	}

	size_t GetSizeBytes() const
	{
		return PositionAndRadius.NumBytes + OccluderRadius.NumBytes + Normal.NumBytes + BentNormal.NumBytes
			+ Irradiance.NumBytes + ScatterDrawParameters.NumBytes + SavedStartIndex.NumBytes + TileCoordinate.NumBytes;
	}

	int32 MaxIrradianceCacheSamples;

	FRWBuffer PositionAndRadius;
	FRWBuffer OccluderRadius;
	FRWBuffer Normal;
	FRWBuffer BentNormal;
	FRWBuffer Irradiance;
	FRWBuffer ScatterDrawParameters;
	FRWBuffer SavedStartIndex;
	FRWBuffer TileCoordinate;
};

/**  */
class FSurfaceCacheResources : public FRenderResource
{
public:

	FSurfaceCacheResources()
	{
		for (int32 i = 0; i < ARRAY_COUNT(Level); i++)
		{
			Level[i] = new FRefinementLevelResources();
		}

		TempResources = new FRefinementLevelResources();

		MinLevel = 100;
		MaxLevel = 0;
		bHasIrradiance = false;
	}

	~FSurfaceCacheResources()
	{
		for (int32 i = 0; i < ARRAY_COUNT(Level); i++)
		{
			delete Level[i];
		}

		delete TempResources;
	}

	void AllocateFor(int32 InMinLevel, int32 InMaxLevel, int32 MaxSurfaceCacheSamples)
	{
		check(InMaxLevel < ARRAY_COUNT(Level));

		bool bReallocate = false;

		if (MinLevel > InMinLevel)
		{
			MinLevel = InMinLevel;
			bReallocate = true;
		}

		if (MaxLevel < InMaxLevel)
		{
			MaxLevel = InMaxLevel;
			bReallocate = true;
		}

		for (int32 LevelIndex = MinLevel; LevelIndex <= MaxLevel; LevelIndex++)
		{
			//@todo - would like to allocate each level based on its worst case, but not possible right now due to swapping with TempResources
			if (Level[LevelIndex]->MaxIrradianceCacheSamples < MaxSurfaceCacheSamples)
			{
				Level[LevelIndex]->MaxIrradianceCacheSamples = MaxSurfaceCacheSamples;
				bReallocate = true;
			}
		}

		if (TempResources->MaxIrradianceCacheSamples < MaxSurfaceCacheSamples)
		{
			bReallocate = true;
			TempResources->MaxIrradianceCacheSamples = MaxSurfaceCacheSamples;
		}

		if (!IsInitialized())
		{
			InitResource();
		}
		else if (bReallocate)
		{
			UpdateRHI();
		}
	}

	virtual void InitDynamicRHI() override
	{
		DispatchParameters.Initialize(sizeof(uint32), 3, PF_R32_UINT, BUF_Static | BUF_DrawIndirect);

		for (int32 i = MinLevel; i <= MaxLevel; i++)
		{
			Level[i]->InitDynamicRHI();
		}

		TempResources->InitDynamicRHI();

		bClearedResources = false;
	}

	virtual void ReleaseDynamicRHI() override
	{
		DispatchParameters.Release();

		for (int32 i = MinLevel; i <= MaxLevel; i++)
		{
			Level[i]->ReleaseDynamicRHI();
		}

		TempResources->ReleaseDynamicRHI();
	}

	size_t GetSizeBytes() const
	{
		size_t TotalSize = 0;

		for (int32 i = MinLevel; i <= MaxLevel; i++)
		{
			TotalSize += Level[i]->GetSizeBytes();
		}

		TotalSize += TempResources->GetSizeBytes();

		return TotalSize;
	}

	FRWBuffer DispatchParameters;

	FRefinementLevelResources* Level[GAOMaxSupportedLevel + 1];

	/** This is for temporary storage when copying from last frame's state */
	FRefinementLevelResources* TempResources;

	bool bClearedResources;
	bool bHasIrradiance;

private:

	int32 MinLevel;
	int32 MaxLevel;
};

inline bool DoesPlatformSupportDistanceFieldAO(EShaderPlatform Platform)
{
	return Platform == SP_PCD3D_SM5 || Platform == SP_PS4;
}

template<bool bOneGroupPerRecord>
class TSetupFinalGatherIndirectArgumentsCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TSetupFinalGatherIndirectArgumentsCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("ONE_GROUP_PER_RECORD"), bOneGroupPerRecord ? TEXT("1") : TEXT("0"));

		// To reduce shader compile time of compute shaders with shared memory, doesn't have an impact on generated code with current compiler (June 2010 DX SDK)
		OutEnvironment.CompilerFlags.Add(CFLAG_StandardOptimization);
	}

	TSetupFinalGatherIndirectArgumentsCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DrawParameters.Bind(Initializer.ParameterMap, TEXT("DrawParameters"));
		DispatchParameters.Bind(Initializer.ParameterMap, TEXT("DispatchParameters"));
		SavedStartIndex.Bind(Initializer.ParameterMap, TEXT("SavedStartIndex"));
	}

	TSetupFinalGatherIndirectArgumentsCS()
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, int32 DepthLevel)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);

		const FScene* Scene = (const FScene*)View.Family->Scene;
		FSurfaceCacheResources& SurfaceCacheResources = *Scene->SurfaceCacheResources;

		SetSRVParameter(RHICmdList, ShaderRHI, DrawParameters, SurfaceCacheResources.Level[DepthLevel]->ScatterDrawParameters.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, SavedStartIndex, SurfaceCacheResources.Level[DepthLevel]->SavedStartIndex.SRV);

		DispatchParameters.SetBuffer(RHICmdList, ShaderRHI, SurfaceCacheResources.DispatchParameters);
	}

	void UnsetParameters(FRHICommandList& RHICmdList)
	{
		DispatchParameters.UnsetUAV(RHICmdList, GetComputeShader());
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DrawParameters;
		Ar << SavedStartIndex;
		Ar << DispatchParameters;
		return bShaderHasOutdatedParameters;
	}

private:

	FShaderResourceParameter DrawParameters;
	FShaderResourceParameter SavedStartIndex;
	FRWShaderParameter DispatchParameters;
};


/**  */
class FTileIntersectionResources : public FRenderResource
{
public:
	virtual void InitDynamicRHI() override;

	virtual void ReleaseDynamicRHI() override
	{
		TileConeAxisAndCos.Release();
		TileConeDepthRanges.Release();
		TileHeadDataUnpacked.Release();
		TileHeadData.Release();
		TileArrayData.Release();
		TileArrayNextAllocation.Release();
	}

	FIntPoint TileDimensions;

	FRWBuffer TileConeAxisAndCos;
	FRWBuffer TileConeDepthRanges;
	FRWBuffer TileHeadDataUnpacked;
	FRWBuffer TileHeadData;
	FRWBuffer TileArrayData;
	FRWBuffer TileArrayNextAllocation;

	size_t GetSizeBytes() const
	{
		return TileConeAxisAndCos.NumBytes + TileConeDepthRanges.NumBytes + TileHeadDataUnpacked.NumBytes + TileHeadData.NumBytes + TileArrayData.NumBytes + TileArrayNextAllocation.NumBytes;
	}
};

extern void GetSpacedVectors(TArray<FVector, TInlineAllocator<9> >& OutVectors);

BEGIN_UNIFORM_BUFFER_STRUCT(FAOSampleData2,)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FVector4,SampleDirections,[NumConeSampleDirections])
END_UNIFORM_BUFFER_STRUCT(FAOSampleData2)

class FAOParameters
{
public:
	void Bind(const FShaderParameterMap& ParameterMap)
	{
		AOMaxDistance.Bind(ParameterMap,TEXT("AOMaxDistance"));
		AOStepScale.Bind(ParameterMap,TEXT("AOStepScale"));
		AOStepExponentScale.Bind(ParameterMap,TEXT("AOStepExponentScale"));
		AOMaxViewDistance.Bind(ParameterMap,TEXT("AOMaxViewDistance"));
	}

	friend FArchive& operator<<(FArchive& Ar,FAOParameters& Parameters)
	{
		Ar << Parameters.AOMaxDistance;
		Ar << Parameters.AOStepScale;
		Ar << Parameters.AOStepExponentScale;
		Ar << Parameters.AOMaxViewDistance;
		return Ar;
	}

	template<typename ShaderRHIParamRef>
	void Set(FRHICommandList& RHICmdList, const ShaderRHIParamRef ShaderRHI, const FDistanceFieldAOParameters& Parameters)
	{
		SetShaderValue(RHICmdList, ShaderRHI, AOMaxDistance, Parameters.OcclusionMaxDistance);

		extern float GAOConeHalfAngle;
		const float AOLargestSampleOffset = Parameters.OcclusionMaxDistance / (1 + FMath::Tan(GAOConeHalfAngle));

		extern float GAOStepExponentScale;
		extern uint32 GAONumConeSteps;
		float AOStepScaleValue = AOLargestSampleOffset / FMath::Pow(2.0f, GAOStepExponentScale * (GAONumConeSteps - 1));
		SetShaderValue(RHICmdList, ShaderRHI, AOStepScale, AOStepScaleValue);

		SetShaderValue(RHICmdList, ShaderRHI, AOStepExponentScale, GAOStepExponentScale);

		extern float GetMaxAOViewDistance();
		SetShaderValue(RHICmdList, ShaderRHI, AOMaxViewDistance, GetMaxAOViewDistance());
	}

private:
	FShaderParameter AOMaxDistance;
	FShaderParameter AOStepScale;
	FShaderParameter AOStepExponentScale;
	FShaderParameter AOMaxViewDistance;
};

inline float GetMaxAOViewDistance()
{
	extern float GAOMaxViewDistance;
	// Scene depth stored in fp16 alpha, must fade out before it runs out of range
	// The fade extends past GAOMaxViewDistance a bit
	return FMath::Min(GAOMaxViewDistance, 65000.0f);
}

class FMaxSizedRWBuffers : public FRenderResource
{
public:
	FMaxSizedRWBuffers()
	{
		MaxSize = 0;
	}

	virtual void InitDynamicRHI()
	{
		check(0);
	}

	virtual void ReleaseDynamicRHI()
	{
		check(0);
	}

	void AllocateFor(int32 InMaxSize)
	{
		bool bReallocate = false;

		if (InMaxSize > MaxSize)
		{
			MaxSize = InMaxSize;
			bReallocate = true;
		}

		if (!IsInitialized())
		{
			InitResource();
		}
		else if (bReallocate)
		{
			UpdateRHI();
		}
	}

	int32 GetMaxSize() const { return MaxSize; }

protected:
	int32 MaxSize;
};

// Must match usf
const int32 RecordConeDataStride = 10;
// In float4s, must match usf
const int32 NumVisibilitySteps = 10;

/**  */
class FTemporaryIrradianceCacheResources : public FMaxSizedRWBuffers
{
public:

	virtual void InitDynamicRHI()
	{
		if (MaxSize > 0)
		{
			ConeVisibility.Initialize(sizeof(float), MaxSize * NumConeSampleDirections, PF_R32_FLOAT, BUF_Static);
			ConeData.Initialize(sizeof(float), MaxSize * NumConeSampleDirections * RecordConeDataStride, PF_R32_FLOAT, BUF_Static);
			StepBentNormal.Initialize(sizeof(float) * 4, MaxSize * NumVisibilitySteps, PF_A32B32G32R32F, BUF_Static);
			SurfelIrradiance.Initialize(sizeof(FFloat16Color), MaxSize, PF_FloatRGBA, BUF_Static);
			HeightfieldIrradiance.Initialize(sizeof(FFloat16Color), MaxSize, PF_FloatRGBA, BUF_Static);
		}
	}

	virtual void ReleaseDynamicRHI()
	{
		ConeVisibility.Release();
		ConeData.Release();
		StepBentNormal.Release();
		SurfelIrradiance.Release();
		HeightfieldIrradiance.Release();
	}

	size_t GetSizeBytes() const
	{
		return ConeVisibility.NumBytes + ConeData.NumBytes + StepBentNormal.NumBytes;
	}

	FRWBuffer ConeVisibility;
	FRWBuffer ConeData;
	FRWBuffer StepBentNormal;
	FRWBuffer SurfelIrradiance;
	FRWBuffer HeightfieldIrradiance;
};

extern FRWBuffer* GDebugBuffer2;

class FTrackGPUProgressCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FTrackGPUProgressCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	FTrackGPUProgressCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DebugBuffer.Bind(Initializer.ParameterMap, TEXT("DebugBuffer"));
		DebugId.Bind(Initializer.ParameterMap, TEXT("DebugId"));
	}

	FTrackGPUProgressCS()
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, uint32 DebugIdValue)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		DebugBuffer.SetBuffer(RHICmdList, ShaderRHI, *GDebugBuffer2);
		SetShaderValue(RHICmdList, ShaderRHI, DebugId, DebugIdValue);
	}

	void UnsetParameters(FRHICommandListImmediate& RHICmdList)
	{
		DebugBuffer.UnsetUAV(RHICmdList, GetComputeShader());
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DebugBuffer;
		Ar << DebugId;
		return bShaderHasOutdatedParameters;
	}

private:

	FRWShaderParameter DebugBuffer;
	FShaderParameter DebugId;
};

extern void TrackGPUProgress(FRHICommandListImmediate& RHICmdList, uint32 DebugId);

extern bool ShouldRenderDynamicSkyLight(const FScene* Scene, const FSceneViewFamily& ViewFamily);
