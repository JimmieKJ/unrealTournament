// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PreCullTriangles.cpp : implementation of offline visibility culling of static triangles
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "UniformBuffer.h"
#include "ShaderParameters.h"
#include "PostProcessing.h"
#include "SceneFilterRendering.h"
#include "DistanceFieldLightingShared.h"
#include "DistanceFieldSurfaceCacheLighting.h"
#include "DistanceFieldGlobalIllumination.h"
#include "RHICommandList.h"
#include "SceneUtils.h"
#include "DistanceFieldAtlas.h"
#include "StaticMeshResources.h"

TGlobalResource<FDistanceFieldObjectBufferResource> GPreCullTrianglesCulledObjectBuffers;

float GPreCullMaxDistance = 1000;

FAutoConsoleVariableRef CPreCullMaxDistance(
	TEXT("r.PreCullMaxDistance"),
	GPreCullMaxDistance,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

class FCullObjectsForBoundsCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCullObjectsForBoundsCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldGI(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("UPDATEOBJECTS_THREADGROUP_SIZE"), UpdateObjectsGroupSize);
		OutEnvironment.CompilerFlags.Add(CFLAG_StandardOptimization);
	}

	FCullObjectsForBoundsCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ObjectBufferParameters.Bind(Initializer.ParameterMap);
		ObjectIndirectArguments.Bind(Initializer.ParameterMap, TEXT("ObjectIndirectArguments"));
		CulledObjectBounds.Bind(Initializer.ParameterMap, TEXT("CulledObjectBounds"));
		CulledObjectData.Bind(Initializer.ParameterMap, TEXT("CulledObjectData"));
		AOParameters.Bind(Initializer.ParameterMap);
		ObjectBoundsCenter.Bind(Initializer.ParameterMap, TEXT("ObjectBoundsCenter"));
		ObjectBoundsExtent.Bind(Initializer.ParameterMap, TEXT("ObjectBoundsExtent"));
		ObjectBoundingGeometryIndexCount.Bind(Initializer.ParameterMap, TEXT("ObjectBoundingGeometryIndexCount"));
		PreCullMaxDistance.Bind(Initializer.ParameterMap, TEXT("PreCullMaxDistance"));
	}

	FCullObjectsForBoundsCS()
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, const FScene* Scene, const FSceneView& View, FVector ObjectBoundsCenterValue, FVector ObjectBoundsExtentValue, const FDistanceFieldAOParameters& Parameters)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);
		ObjectBufferParameters.Set(RHICmdList, ShaderRHI, *(Scene->DistanceFieldSceneData.ObjectBuffers), Scene->DistanceFieldSceneData.NumObjectsInBuffer);

		FUnorderedAccessViewRHIParamRef OutUAVs[3];
		OutUAVs[0] = GPreCullTrianglesCulledObjectBuffers.Buffers.ObjectIndirectArguments.UAV;
		OutUAVs[1] = GPreCullTrianglesCulledObjectBuffers.Buffers.Bounds.UAV;
		OutUAVs[2] = GPreCullTrianglesCulledObjectBuffers.Buffers.Data.UAV;
		RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));

		ObjectIndirectArguments.SetBuffer(RHICmdList, ShaderRHI, GPreCullTrianglesCulledObjectBuffers.Buffers.ObjectIndirectArguments);
		CulledObjectBounds.SetBuffer(RHICmdList, ShaderRHI, GPreCullTrianglesCulledObjectBuffers.Buffers.Bounds);
		CulledObjectData.SetBuffer(RHICmdList, ShaderRHI, GPreCullTrianglesCulledObjectBuffers.Buffers.Data);
		AOParameters.Set(RHICmdList, ShaderRHI, Parameters);

		SetShaderValue(RHICmdList, ShaderRHI, ObjectBoundsCenter, ObjectBoundsCenterValue);
		SetShaderValue(RHICmdList, ShaderRHI, ObjectBoundsExtent, ObjectBoundsExtentValue);

		SetShaderValue(RHICmdList, ShaderRHI, ObjectBoundingGeometryIndexCount, StencilingGeometry::GLowPolyStencilSphereIndexBuffer.GetIndexCount());
		SetShaderValue(RHICmdList, ShaderRHI, PreCullMaxDistance, GPreCullMaxDistance);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, const FScene* Scene)
	{
		ObjectBufferParameters.UnsetParameters(RHICmdList, GetComputeShader(), *(Scene->DistanceFieldSceneData.ObjectBuffers));
		ObjectIndirectArguments.UnsetUAV(RHICmdList, GetComputeShader());
		CulledObjectBounds.UnsetUAV(RHICmdList, GetComputeShader());
		CulledObjectData.UnsetUAV(RHICmdList, GetComputeShader());

		FUnorderedAccessViewRHIParamRef OutUAVs[3];
		OutUAVs[0] = GPreCullTrianglesCulledObjectBuffers.Buffers.ObjectIndirectArguments.UAV;
		OutUAVs[1] = GPreCullTrianglesCulledObjectBuffers.Buffers.Bounds.UAV;
		OutUAVs[2] = GPreCullTrianglesCulledObjectBuffers.Buffers.Data.UAV;	
		RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));
	}

	virtual bool Serialize(FArchive& Ar)
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ObjectBufferParameters;
		Ar << ObjectIndirectArguments;
		Ar << CulledObjectBounds;
		Ar << CulledObjectData;
		Ar << AOParameters;
		Ar << ObjectBoundsCenter;
		Ar << ObjectBoundsExtent;
		Ar << ObjectBoundingGeometryIndexCount;
		Ar << PreCullMaxDistance;
		return bShaderHasOutdatedParameters;
	}

private:

	FDistanceFieldObjectBufferParameters ObjectBufferParameters;
	FRWShaderParameter ObjectIndirectArguments;
	FRWShaderParameter CulledObjectBounds;
	FRWShaderParameter CulledObjectData;
	FAOParameters AOParameters;
	FShaderParameter ObjectBoundsCenter;
	FShaderParameter ObjectBoundsExtent;
	FShaderParameter ObjectBoundingGeometryIndexCount;
	FShaderParameter PreCullMaxDistance;
};

IMPLEMENT_SHADER_TYPE(,FCullObjectsForBoundsCS,TEXT("PreCullTriangles"),TEXT("CullObjectsForBoundsCS"),SF_Compute);

class FCullVolumeBuffersResource : public FRenderResource
{
public:

	FCPUUpdatedBuffer CullVolumePlaneOffsets;
	FCPUUpdatedBuffer CullVolumePlaneData;

	FCullVolumeBuffersResource()
	{
		CullVolumePlaneOffsets.Format = PF_R32_UINT;
		CullVolumePlaneOffsets.Stride = 2;

		CullVolumePlaneData.Format = PF_A32B32G32R32F;
		CullVolumePlaneData.Stride = 1;
	}

	virtual void InitDynamicRHI()  override
	{
		CullVolumePlaneOffsets.Initialize();
		CullVolumePlaneData.Initialize();
	}

	virtual void ReleaseDynamicRHI() override
	{
		CullVolumePlaneOffsets.Release();
		CullVolumePlaneData.Release();
	}
};

TGlobalResource<FCullVolumeBuffersResource> GCullVolumeBuffers;

class FPreCulledTriangleBufferResource : public FRenderResource
{
public:
	FPreCulledTriangleBuffers Buffers;

	virtual void InitDynamicRHI()  override
	{
		Buffers.Initialize();
	}

	virtual void ReleaseDynamicRHI() override
	{
		Buffers.Release();
	}
};

class FPreCullTrianglesCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPreCullTrianglesCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldGI(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), GDistanceFieldAOTileSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), GDistanceFieldAOTileSizeY);
		OutEnvironment.CompilerFlags.Add(CFLAG_StandardOptimization);
	}

	FPreCullTrianglesCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ObjectParameters.Bind(Initializer.ParameterMap);
		TriangleVisibleMask.Bind(Initializer.ParameterMap, TEXT("TriangleVisibleMask"));
		StartIndex.Bind(Initializer.ParameterMap, TEXT("StartIndex"));
		NumTriangles.Bind(Initializer.ParameterMap, TEXT("NumTriangles"));
		TriangleVertexData.Bind(Initializer.ParameterMap, TEXT("TriangleVertexData"));
		PreCullMaxDistance.Bind(Initializer.ParameterMap, TEXT("PreCullMaxDistance"));
		NumCullVolumes.Bind(Initializer.ParameterMap, TEXT("NumCullVolumes"));
		CullVolumePlaneOffsets.Bind(Initializer.ParameterMap, TEXT("CullVolumePlaneOffsets"));
		CullVolumePlaneData.Bind(Initializer.ParameterMap, TEXT("CullVolumePlaneData"));
	}

	FPreCullTrianglesCS()
	{
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FSceneView& View, 
		FPreCulledTriangleBuffers& PreCulledTriangleBuffers,
		int32 StartIndexValue,
		int32 NumTrianglesValue,
		const FUniformMeshBuffers& UniformMeshBuffers,
		int32 NumCullVolumesValue)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);

		ObjectParameters.Set(RHICmdList, ShaderRHI, GPreCullTrianglesCulledObjectBuffers.Buffers);

		FUnorderedAccessViewRHIParamRef OutUAVs[1];
		OutUAVs[0] = PreCulledTriangleBuffers.TriangleVisibleMask.UAV;
		RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));

		TriangleVisibleMask.SetBuffer(RHICmdList, ShaderRHI, PreCulledTriangleBuffers.TriangleVisibleMask);

		SetShaderValue(RHICmdList, ShaderRHI, StartIndex, StartIndexValue);
		SetShaderValue(RHICmdList, ShaderRHI, NumTriangles, NumTrianglesValue);
		SetSRVParameter(RHICmdList, ShaderRHI, TriangleVertexData, UniformMeshBuffers.TriangleDataSRV);
		SetShaderValue(RHICmdList, ShaderRHI, PreCullMaxDistance, GPreCullMaxDistance);

		SetShaderValue(RHICmdList, ShaderRHI, NumCullVolumes, NumCullVolumesValue);
		SetSRVParameter(RHICmdList, ShaderRHI, CullVolumePlaneOffsets, GCullVolumeBuffers.CullVolumePlaneOffsets.BufferSRV);
		SetSRVParameter(RHICmdList, ShaderRHI, CullVolumePlaneData, GCullVolumeBuffers.CullVolumePlaneData.BufferSRV);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, FPreCulledTriangleBuffers& PreCulledTriangleBuffers)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		TriangleVisibleMask.UnsetUAV(RHICmdList, ShaderRHI);
		// RHISetStreamOutTargets doesn't unbind existing uses like render targets do 
		SetSRVParameter(RHICmdList, ShaderRHI, TriangleVertexData, NULL);

		FUnorderedAccessViewRHIParamRef OutUAVs[1];
		OutUAVs[0] = PreCulledTriangleBuffers.TriangleVisibleMask.UAV;
		RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ObjectParameters;
		Ar << TriangleVisibleMask;
		Ar << StartIndex;
		Ar << NumTriangles;
		Ar << TriangleVertexData;
		Ar << PreCullMaxDistance;
		Ar << NumCullVolumes;
		Ar << CullVolumePlaneOffsets;
		Ar << CullVolumePlaneData;
		return bShaderHasOutdatedParameters;
	}

private:

	FDistanceFieldCulledObjectBufferParameters ObjectParameters;
	FRWShaderParameter TriangleVisibleMask;
	FShaderParameter StartIndex;
	FShaderParameter NumTriangles;
	FShaderResourceParameter TriangleVertexData;
	FShaderParameter PreCullMaxDistance;
	FShaderParameter NumCullVolumes;
	FShaderResourceParameter CullVolumePlaneOffsets;
	FShaderResourceParameter CullVolumePlaneData;
};

IMPLEMENT_SHADER_TYPE(,FPreCullTrianglesCS,TEXT("PreCullTriangles"),TEXT("PreCullTrianglesCS"),SF_Compute);

TGlobalResource<FPreCulledTriangleBufferResource> GPreCulledTriangleBuffers;
TGlobalResource<FPreCulledTriangleBufferResource> GPreCulledTriangleBuffersFromHeightfields;

void FDeferredShadingSceneRenderer::PreCullStaticMeshes(FRHICommandListImmediate& RHICmdList, const TArray<UStaticMeshComponent*>& ComponentsToPreCull, const TArray<TArray<FPlane> >& CullVolumes)
{
	FViewInfo& View = Views[0];
	FDistanceFieldAOParameters Parameters(1000);

	if (SupportsDistanceFieldAO(Scene->GetFeatureLevel(), Scene->GetShaderPlatform()))
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_PreCullStaticMeshes);
		GDistanceFieldVolumeTextureAtlas.UpdateAllocations();
		UpdateGlobalDistanceFieldObjectBuffers(RHICmdList);

		FHeightfieldLightingViewInfo HeightfieldScene;
		HeightfieldScene.SetupHeightfieldsForScene(*Scene, RHICmdList);

		{
			int32 NumCullVolumePlanes = 0;

			for (int32 VolumeIndex = 0; VolumeIndex < CullVolumes.Num(); VolumeIndex++)
			{
				NumCullVolumePlanes += CullVolumes[VolumeIndex].Num();
			}

			if (CullVolumes.Num() > GCullVolumeBuffers.CullVolumePlaneOffsets.MaxElements)
			{
				GCullVolumeBuffers.CullVolumePlaneOffsets.MaxElements = CullVolumes.Num() * 5 / 4;
				GCullVolumeBuffers.CullVolumePlaneOffsets.Release();
				GCullVolumeBuffers.CullVolumePlaneOffsets.Initialize();
			}

			if (NumCullVolumePlanes > GCullVolumeBuffers.CullVolumePlaneData.MaxElements)
			{
				GCullVolumeBuffers.CullVolumePlaneData.MaxElements = NumCullVolumePlanes * 5 / 4;
				GCullVolumeBuffers.CullVolumePlaneData.Release();
				GCullVolumeBuffers.CullVolumePlaneData.Initialize();
			}

			if (NumCullVolumePlanes > 0)
			{
				uint32* CullVolumePlaneOffsetsData = (uint32*)RHILockVertexBuffer(GCullVolumeBuffers.CullVolumePlaneOffsets.Buffer, 0, GCullVolumeBuffers.CullVolumePlaneOffsets.Buffer->GetSize(), RLM_WriteOnly);
				FVector4* CullVolumePlaneData = (FVector4*)RHILockVertexBuffer(GCullVolumeBuffers.CullVolumePlaneData.Buffer, 0, GCullVolumeBuffers.CullVolumePlaneData.Buffer->GetSize(), RLM_WriteOnly);
			
				int32 StartOffset = 0;

				for (int32 VolumeIndex = 0; VolumeIndex < CullVolumes.Num(); VolumeIndex++)
				{
					const TArray<FPlane>& PlaneData = CullVolumes[VolumeIndex];
					CullVolumePlaneOffsetsData[VolumeIndex * 2 + 0] = PlaneData.Num();
					CullVolumePlaneOffsetsData[VolumeIndex * 2 + 1] = StartOffset;

					for (int32 PlaneIndex = 0; PlaneIndex < PlaneData.Num(); PlaneIndex++)
					{
						CullVolumePlaneData[StartOffset + PlaneIndex] = FVector4(PlaneData[PlaneIndex], PlaneData[PlaneIndex].W);
					}

					StartOffset += PlaneData.Num();
				}

				RHIUnlockVertexBuffer(GCullVolumeBuffers.CullVolumePlaneOffsets.Buffer);
				RHIUnlockVertexBuffer(GCullVolumeBuffers.CullVolumePlaneData.Buffer);
			}
		}

		{
			SCOPED_DRAW_EVENT(RHICmdList, ObjectFrustumCulling);

			if (GPreCullTrianglesCulledObjectBuffers.Buffers.MaxObjects < Scene->DistanceFieldSceneData.NumObjectsInBuffer
				|| GPreCullTrianglesCulledObjectBuffers.Buffers.MaxObjects > 3 * Scene->DistanceFieldSceneData.NumObjectsInBuffer)
			{
				GPreCullTrianglesCulledObjectBuffers.Buffers.MaxObjects = Scene->DistanceFieldSceneData.NumObjectsInBuffer * 5 / 4;
				GPreCullTrianglesCulledObjectBuffers.Buffers.Release();
				GPreCullTrianglesCulledObjectBuffers.Buffers.Initialize();
			}

			UE_LOG(LogShaders, Log, TEXT("Processing %u meshes"), ComponentsToPreCull.Num());
			int32 TotalNumTrianglesProcessed = 0;
			int32 TotalNumTrianglesVisible = 0;

			for (int32 ComponentIndex = 0; ComponentIndex < ComponentsToPreCull.Num(); ComponentIndex++)
			{
				if (ComponentsToPreCull.Num() > 200 && (ComponentIndex % FMath::Max(ComponentsToPreCull.Num() / 10, 1)) == 0)
				{
					UE_LOG(LogShaders, Log, TEXT("PreCulled %u percent"), (uint32)(100 * ComponentIndex / (float)ComponentsToPreCull.Num()));
				}

				UStaticMeshComponent* StaticMeshComponent = ComponentsToPreCull[ComponentIndex];
				FStaticMeshSceneProxy* StaticMeshProxy = (FStaticMeshSceneProxy*)StaticMeshComponent->SceneProxy;
				FBoxSphereBounds Bounds = StaticMeshProxy->GetBounds();

				for (int32 LODIndex = 0; LODIndex < StaticMeshComponent->StaticMesh->RenderData->LODResources.Num(); LODIndex++)
				{
					{
						uint32 ClearValues[4] = { 0 };
						RHICmdList.ClearUAV(GPreCullTrianglesCulledObjectBuffers.Buffers.ObjectIndirectArguments.UAV, ClearValues);

						TShaderMapRef<FCullObjectsForBoundsCS> ComputeShader(GetGlobalShaderMap(Scene->GetFeatureLevel()));
						RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
						ComputeShader->SetParameters(RHICmdList, Scene, View, Bounds.GetBox().GetCenter(), Bounds.GetBox().GetExtent(), Parameters);

						DispatchComputeShader(RHICmdList, *ComputeShader, FMath::DivideAndRoundUp<uint32>(Scene->DistanceFieldSceneData.NumObjectsInBuffer, UpdateObjectsGroupSize), 1, 1);
						ComputeShader->UnsetParameters(RHICmdList, Scene);
					}

					FUniformMeshBuffers* UniformMeshBuffers = NULL;
					const FMaterialRenderProxy* MaterialRenderProxy = NULL;
					FUniformBufferRHIParamRef PrimitiveUniformBuffer = NULL;
					const int32 NumUniformTriangles = FUniformMeshConverter::Convert(RHICmdList, *this, View, StaticMeshProxy->GetPrimitiveSceneInfo(), LODIndex, UniformMeshBuffers, MaterialRenderProxy, PrimitiveUniformBuffer);
			
					if (NumUniformTriangles > 0 && UniformMeshBuffers)
					{
						if (GPreCulledTriangleBuffers.Buffers.MaxIndices < NumUniformTriangles * 3)
						{
							GPreCulledTriangleBuffers.Buffers.MaxIndices = NumUniformTriangles * 3 * 5 / 4;
							GPreCulledTriangleBuffers.ReleaseResource();
							GPreCulledTriangleBuffers.InitResource();
						}

						uint32 MaskClearValues[4] = { 255 };
						RHICmdList.ClearUAV(GPreCulledTriangleBuffers.Buffers.TriangleVisibleMask.UAV, MaskClearValues);

						const FStaticMeshLODResources& StaticMeshLODResources = StaticMeshComponent->StaticMesh->RenderData->LODResources[LODIndex];

						for (int32 SectionIndex = 0; SectionIndex < StaticMeshLODResources.Sections.Num(); SectionIndex++)
						{
							const FStaticMeshSection& Section = StaticMeshLODResources.Sections[SectionIndex];

							{
								TShaderMapRef<FPreCullTrianglesCS> ComputeShader(GetGlobalShaderMap(Scene->GetFeatureLevel()));
								RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
								ComputeShader->SetParameters(RHICmdList, View, GPreCulledTriangleBuffers.Buffers, Section.FirstIndex, Section.NumTriangles, *UniformMeshBuffers, CullVolumes.Num());

								uint32 GroupSize = FMath::DivideAndRoundUp((int32)Section.NumTriangles, GDistanceFieldAOTileSizeX * GDistanceFieldAOTileSizeY);
								DispatchComputeShader(RHICmdList, *ComputeShader, GroupSize, 1, 1);
								ComputeShader->UnsetParameters(RHICmdList, GPreCulledTriangleBuffers.Buffers);
							}
						}

						if (GPreCulledTriangleBuffersFromHeightfields.Buffers.MaxIndices < NumUniformTriangles * 3)
						{
							GPreCulledTriangleBuffersFromHeightfields.Buffers.MaxIndices = NumUniformTriangles * 3 * 5 / 4;
							GPreCulledTriangleBuffersFromHeightfields.ReleaseResource();
							GPreCulledTriangleBuffersFromHeightfields.InitResource();
						}

						uint32 HeightfieldMaskClearValues[4] = { 1 };
						RHICmdList.ClearUAV(GPreCulledTriangleBuffersFromHeightfields.Buffers.TriangleVisibleMask.UAV, HeightfieldMaskClearValues);

						for (int32 SectionIndex = 0; SectionIndex < StaticMeshLODResources.Sections.Num(); SectionIndex++)
						{
							const FStaticMeshSection& Section = StaticMeshLODResources.Sections[SectionIndex];
							HeightfieldScene.PreCullTriangles(View, RHICmdList, GPreCulledTriangleBuffersFromHeightfields.Buffers, Section.FirstIndex, Section.NumTriangles, *UniformMeshBuffers);
						}

						{
							uint32* TriangleVisibleMaskData = (uint32*)RHILockVertexBuffer(GPreCulledTriangleBuffers.Buffers.TriangleVisibleMask.Buffer, 0, NumUniformTriangles * sizeof(uint32), RLM_ReadOnly);
							uint32* TriangleVisibleMaskDataFromHeightfields = (uint32*)RHILockVertexBuffer(GPreCulledTriangleBuffersFromHeightfields.Buffers.TriangleVisibleMask.Buffer, 0, NumUniformTriangles * sizeof(uint32), RLM_ReadOnly);

							FIndexArrayView IndexBufferView = StaticMeshLODResources.IndexBuffer.GetArrayView();
							int32 GlobalTriangleIndex = 0;

							TArray<int32> NumTrianglesPerSection;
							NumTrianglesPerSection.Empty(StaticMeshLODResources.Sections.Num());

							TArray<uint32> CopiedIndices;
							CopiedIndices.Empty(NumUniformTriangles * 3);

							for (int32 SectionIndex = 0; SectionIndex < StaticMeshLODResources.Sections.Num(); SectionIndex++)
							{
								const FStaticMeshSection& Section = StaticMeshLODResources.Sections[SectionIndex];
								int32 NumVisibleTrianglesInSection = 0;

								TotalNumTrianglesProcessed += Section.NumTriangles;

								for (uint32 TriangleIndex = 0; TriangleIndex < Section.NumTriangles; TriangleIndex++)
								{
									uint32 TriangleMask = TriangleVisibleMaskData[GlobalTriangleIndex];
									check(TriangleMask == 0 || TriangleMask == 1);

									uint32 TriangleMaskFromHeightfields = TriangleVisibleMaskDataFromHeightfields[GlobalTriangleIndex];
									check(TriangleMaskFromHeightfields == 0 || TriangleMaskFromHeightfields == 1);

									if (TriangleMask > 0 && TriangleMaskFromHeightfields > 0)
									{
										CopiedIndices.Add(IndexBufferView[GlobalTriangleIndex * 3 + 0]);
										CopiedIndices.Add(IndexBufferView[GlobalTriangleIndex * 3 + 1]);
										CopiedIndices.Add(IndexBufferView[GlobalTriangleIndex * 3 + 2]);
										NumVisibleTrianglesInSection++;
									}

									GlobalTriangleIndex++;
								}

								TotalNumTrianglesVisible += NumVisibleTrianglesInSection;
								NumTrianglesPerSection.Add(NumVisibleTrianglesInSection);
							}

							RHIUnlockVertexBuffer(GPreCulledTriangleBuffers.Buffers.TriangleVisibleMask.Buffer);
							RHIUnlockVertexBuffer(GPreCulledTriangleBuffersFromHeightfields.Buffers.TriangleVisibleMask.Buffer);

							StaticMeshComponent->UpdatePreCulledData(LODIndex, CopiedIndices, NumTrianglesPerSection);
						}
					}
				}
			}

			UE_LOG(LogShaders, Log, TEXT("%.3fm triangles visible out of %.3fm total (%u percent removed)"), TotalNumTrianglesVisible / 1000000.0f, TotalNumTrianglesProcessed / 1000000.0f, (int32)(100 * (1 - TotalNumTrianglesVisible / (float)TotalNumTrianglesProcessed)));
		}
	}
}

void PreCullStaticMeshesOuter(FRHICommandListImmediate& RHICmdList, FDeferredShadingSceneRenderer* SceneRenderer, const TArray<UStaticMeshComponent*>& ComponentsToPreCull, const TArray<TArray<FPlane> >& CullVolumes)
{
	FMemMark MemStackMark(FMemStack::Get());
	
	SceneRenderer->PreCullStaticMeshes(RHICmdList, ComponentsToPreCull, CullVolumes);

	check(&RHICmdList == &FRHICommandListExecutor::GetImmediateCommandList());
	check(IsInRenderingThread());
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_CaptureSceneToScratchCubemap_Flush);
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThread);
	}

	FSceneRenderer::WaitForTasksClearSnapshotsAndDeleteSceneRenderer(RHICmdList, SceneRenderer);
}

/** 
 * Render target class required for rendering the scene.
 */
class FPreCullTrianglesRenderTarget : public FRenderResource, public FRenderTarget
{
public:

	FPreCullTrianglesRenderTarget() :
		Size(128)
	{}

	virtual const FTexture2DRHIRef& GetRenderTargetTexture() const 
	{
		static FTexture2DRHIRef DummyTexture;
		return DummyTexture;
	}

	virtual FIntPoint GetSizeXY() const { return FIntPoint(Size, Size); }
	virtual float GetDisplayGamma() const { return 1.0f; }

private:

	int32 Size;
};

TGlobalResource<FPreCullTrianglesRenderTarget> GPreCullTrianglesRenderTarget;

void FScene::PreCullStaticMeshes(const TArray<UStaticMeshComponent*>& ComponentsToPreCull, const TArray<TArray<FPlane> >& CullVolumes)
{
	// Alert the RHI that we're rendering a new frame
	// Not really a new frame, but it will allow pooling mechanisms to update, like the uniform buffer pool
	ENQUEUE_UNIQUE_RENDER_COMMAND(
		BeginFrame,
	{
		GFrameNumberRenderThread++;
		RHICmdList.BeginFrame();
	})

	FSceneViewFamilyContext ViewFamily( FSceneViewFamily::ConstructionValues(
		&GPreCullTrianglesRenderTarget,
		this,
		FEngineShowFlags(ESFIM_Game))
		.SetWorldTimes( 0.0f, 0.0f, 0.0f )				
		.SetResolveScene(false) );

	// Disable features that are not desired when capturing the scene
	ViewFamily.EngineShowFlags.PostProcessing = 0;
	ViewFamily.EngineShowFlags.MotionBlur = 0;
	ViewFamily.EngineShowFlags.SetOnScreenDebug(false);
	ViewFamily.EngineShowFlags.HMDDistortion = 0;
	// Exclude particles and light functions as they are usually dynamic, and can't be captured well
	ViewFamily.EngineShowFlags.Particles = 0;
	ViewFamily.EngineShowFlags.LightFunctions = 0;
	ViewFamily.EngineShowFlags.SetCompositeEditorPrimitives(false);
	// These are highly dynamic and can't be captured effectively
	ViewFamily.EngineShowFlags.LightShafts = 0;

	FSceneViewInitOptions ViewInitOptions;
	ViewInitOptions.ViewFamily = &ViewFamily;
	ViewInitOptions.BackgroundColor = FLinearColor::Black;
	ViewInitOptions.OverlayColor = FLinearColor::Black;
	ViewInitOptions.SetViewRectangle(FIntRect(0, 0, 128 * 1, 128 * 1));

	// Projection matrix based on the fov, near / far clip settings
	// Each face always uses a 90 degree field of view
	if ((bool)ERHIZBuffer::IsInverted)
	{
		ViewInitOptions.ProjectionMatrix = FReversedZPerspectiveMatrix(
			90.0f * (float)PI / 360.0f,
			(float)128 * 1,
			(float)128 * 1,
			10
			);
	}
	else
	{
		ViewInitOptions.ProjectionMatrix = FPerspectiveMatrix(
			90.0f * (float)PI / 360.0f,
			(float)128 * 1,
			(float)128 * 1,
			10
			);
	}

	ViewInitOptions.ViewRotationMatrix = FMatrix::Identity;
	ViewInitOptions.ViewOrigin = FVector::ZeroVector;

	FSceneView* View = new FSceneView(ViewInitOptions);

	View->StartFinalPostprocessSettings(FVector::ZeroVector);
	View->EndFinalPostprocessSettings(ViewInitOptions);

	ViewFamily.Views.Add(View);

	FSceneRenderer* SceneRenderer = FSceneRenderer::CreateSceneRenderer(&ViewFamily, NULL);

	ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER( 
		PreCullTrianglesCommand,
		FSceneRenderer*, SceneRenderer, SceneRenderer,
		// Note: passing these arrays which are on the GT stack, only because this is an offline operation and the game thread will flush rendering commands before exiting the scope
		const TArray<UStaticMeshComponent*>&, ComponentsToPreCull, ComponentsToPreCull,
		const TArray<TArray<FPlane> >&, CullVolumes, CullVolumes,
	{
		PreCullStaticMeshesOuter(RHICmdList, (FDeferredShadingSceneRenderer*)SceneRenderer, ComponentsToPreCull, CullVolumes);

		RHICmdList.EndFrame();
	});

	FlushRenderingCommands();
}