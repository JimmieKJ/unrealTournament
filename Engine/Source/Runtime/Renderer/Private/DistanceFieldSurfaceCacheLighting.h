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

	FDistanceFieldAOParameters(float InOcclusionMaxDistance = 600.0f, float InContrast = 0)
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
			TileCoordinate.Initialize(sizeof(uint16)* 2, MaxIrradianceCacheSamples, PF_R16G16_UINT, BUF_Static);
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

	FRWBuffer DispatchParameters;

	FRefinementLevelResources* Level[GAOMaxSupportedLevel + 1];

	/** This is for temporary storage when copying from last frame's state */
	FRefinementLevelResources* TempResources;

	bool bClearedResources;

private:

	int32 MinLevel;
	int32 MaxLevel;
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
};


/** Generates unit length, stratified and uniformly distributed direction samples in a hemisphere. */
extern void GenerateStratifiedUniformHemisphereSamples2(int32 NumThetaSteps, int32 NumPhiSteps, FRandomStream& RandomStream, TArray<FVector4>& Samples);

extern void GetSpacedVectors(TArray<TInlineAllocator<9> >& OutVectors);

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

