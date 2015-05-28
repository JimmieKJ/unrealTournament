// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "EnginePrivate.h"
#include "Particles/ParticleResources.h"
#include "Engine/NiagaraEffectRenderer.h"

DECLARE_CYCLE_STAT(TEXT("Render Total"), STAT_NiagaraRender, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Render Sprites"), STAT_NiagaraRenderSprites, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Render Ribbons"), STAT_NiagaraRenderRibbons, STATGROUP_Niagara);

UNiagaraEffectRendererProperties::UNiagaraEffectRendererProperties(FObjectInitializer const &Initializer)
{}

UNiagaraSpriteRendererProperties::UNiagaraSpriteRendererProperties(FObjectInitializer const &Initializer)
{}

UNiagaraRibbonRendererProperties::UNiagaraRibbonRendererProperties(FObjectInitializer const &Initializer)
{}



struct FNiagaraDynamicDataSprites : public FNiagaraDynamicDataBase
{
	TArray<FParticleSpriteVertex> VertexData;
};

struct FNiagaraDynamicDataRibbon : public FNiagaraDynamicDataBase
{
	TArray<FParticleBeamTrailVertex> VertexData;
};



/* Mesh collector classes */
class FNiagaraMeshCollectorResourcesSprite : public FOneFrameResource
{
public:
	FParticleSpriteVertexFactory VertexFactory;
	FParticleSpriteUniformBufferRef UniformBuffer;

	virtual ~FNiagaraMeshCollectorResourcesSprite()
	{
		VertexFactory.ReleaseResource();
	}
};


class FNiagaraMeshCollectorResourcesRibbon : public FOneFrameResource
{
public:
	FParticleBeamTrailVertexFactory VertexFactory;
	FParticleBeamTrailUniformBufferRef UniformBuffer;

	virtual ~FNiagaraMeshCollectorResourcesRibbon()
	{
		VertexFactory.ReleaseResource();
	}
};





NiagaraEffectRendererSprites::NiagaraEffectRendererSprites(ERHIFeatureLevel::Type FeatureLevel, UNiagaraEffectRendererProperties *InProps) :
	NiagaraEffectRenderer(),
	DynamicDataRender(NULL)
{
	//check(InProps);
	VertexFactory = new FParticleSpriteVertexFactory(PVFT_Sprite, FeatureLevel);
	Properties = Cast<UNiagaraSpriteRendererProperties>(InProps);
}


void NiagaraEffectRendererSprites::ReleaseRenderThreadResources()
{
	VertexFactory->ReleaseResource();
	WorldSpacePrimitiveUniformBuffer.ReleaseResource();
}

void NiagaraEffectRendererSprites::CreateRenderThreadResources() 
{
	VertexFactory->InitResource();
}

void NiagaraEffectRendererSprites::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector, const FNiagaraSceneProxy *SceneProxy) const
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraRender);
	SCOPE_CYCLE_COUNTER(STAT_NiagaraRenderSprites);

	SimpleTimer MeshElementsTimer;

	check(DynamicDataRender)
		
	if (DynamicDataRender->VertexData.Num() == 0)
	{
		return;
	}

	const bool bIsWireframe = ViewFamily.EngineShowFlags.Wireframe;
	FMaterialRenderProxy* MaterialRenderProxy = Material->GetRenderProxy(SceneProxy->IsSelected(), SceneProxy->IsHovered());

	int32 SizeInBytes = DynamicDataRender->VertexData.GetTypeSize() * DynamicDataRender->VertexData.Num();
	FGlobalDynamicVertexBuffer::FAllocation LocalDynamicVertexAllocation = FGlobalDynamicVertexBuffer::Get().Allocate(SizeInBytes);

	if (LocalDynamicVertexAllocation.IsValid())
	{
		// Update the primitive uniform buffer if needed.
		if (!WorldSpacePrimitiveUniformBuffer.IsInitialized())
		{
			FPrimitiveUniformShaderParameters PrimitiveUniformShaderParameters = GetPrimitiveUniformShaderParameters(
				FMatrix::Identity,
				SceneProxy->GetActorPosition(),
				SceneProxy->GetBounds(),
				SceneProxy->GetLocalBounds(),
				SceneProxy->ReceivesDecals(),
				false,
				false,
				SceneProxy->UseEditorDepthTest()
				);
			WorldSpacePrimitiveUniformBuffer.SetContents(PrimitiveUniformShaderParameters);
			WorldSpacePrimitiveUniformBuffer.InitResource();
		}

		// Copy the vertex data over.
		FMemory::Memcpy(LocalDynamicVertexAllocation.Buffer, DynamicDataRender->VertexData.GetData(), SizeInBytes);

		// Compute the per-view uniform buffers.
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				const FSceneView* View = Views[ViewIndex];

				FNiagaraMeshCollectorResourcesSprite& CollectorResources = Collector.AllocateOneFrameResource<FNiagaraMeshCollectorResourcesSprite>();
				FParticleSpriteUniformParameters PerViewUniformParameters;// = UniformParameters;
				PerViewUniformParameters.AxisLockRight = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
				PerViewUniformParameters.AxisLockUp = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
				PerViewUniformParameters.RotationScale = 1.0f;
				PerViewUniformParameters.RotationBias = 0.0f;
				PerViewUniformParameters.TangentSelector = FVector4(0.0f, 0.0f, 0.0f, 1.0f);
				PerViewUniformParameters.InvDeltaSeconds = 0.0f;
				if (Properties)
				{
					PerViewUniformParameters.SubImageSize = FVector4(Properties->SubImageInfo.X, Properties->SubImageInfo.Y, 1.0f / Properties->SubImageInfo.X, 1.0f / Properties->SubImageInfo.Y);
				}
				PerViewUniformParameters.NormalsType = 0;
				PerViewUniformParameters.NormalsSphereCenter = FVector4(0.0f, 0.0f, 0.0f, 1.0f);
				PerViewUniformParameters.NormalsCylinderUnitDirection = FVector4(0.0f, 0.0f, 1.0f, 0.0f);
				PerViewUniformParameters.PivotOffset = FVector2D(-0.5f, -0.5f);
				PerViewUniformParameters.MacroUVParameters = FVector4(0.0f, 0.0f, 1.0f, 1.0f);

				// Collector.AllocateOneFrameResource uses default ctor, initialize the vertex factory
				CollectorResources.VertexFactory.SetFeatureLevel(ViewFamily.GetFeatureLevel());
				CollectorResources.VertexFactory.SetParticleFactoryType(PVFT_Sprite);

				CollectorResources.UniformBuffer = FParticleSpriteUniformBufferRef::CreateUniformBufferImmediate(PerViewUniformParameters, UniformBuffer_SingleFrame);

				CollectorResources.VertexFactory.InitResource();
				CollectorResources.VertexFactory.SetSpriteUniformBuffer(CollectorResources.UniformBuffer);
				CollectorResources.VertexFactory.SetInstanceBuffer(
					LocalDynamicVertexAllocation.VertexBuffer,
					LocalDynamicVertexAllocation.VertexOffset,
					sizeof(FParticleSpriteVertex),
					true
					);
				CollectorResources.VertexFactory.SetDynamicParameterBuffer(NULL, 0, 0, true);

				FMeshBatch& MeshBatch = Collector.AllocateMesh();
				MeshBatch.VertexFactory = &CollectorResources.VertexFactory;
				MeshBatch.CastShadow = SceneProxy->CastsDynamicShadow();
				MeshBatch.bUseAsOccluder = false;
				MeshBatch.ReverseCulling = SceneProxy->IsLocalToWorldDeterminantNegative();
				MeshBatch.Type = PT_TriangleList;
				MeshBatch.DepthPriorityGroup = SceneProxy->GetDepthPriorityGroup(View);
				MeshBatch.bCanApplyViewModeOverrides = true;
				MeshBatch.bUseWireframeSelectionColoring = SceneProxy->IsSelected();

				if (bIsWireframe)
				{
					MeshBatch.MaterialRenderProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy(SceneProxy->IsSelected(), SceneProxy->IsHovered());
				}
				else
				{
					MeshBatch.MaterialRenderProxy = MaterialRenderProxy;
				}

				FMeshBatchElement& MeshElement = MeshBatch.Elements[0];
				MeshElement.IndexBuffer = &GParticleIndexBuffer;
				MeshElement.FirstIndex = 0;
				MeshElement.NumPrimitives = 2;
				MeshElement.NumInstances = DynamicDataRender->VertexData.Num();
				MeshElement.MinVertexIndex = 0;
				MeshElement.MaxVertexIndex = MeshElement.NumInstances * 4 - 1;
				MeshElement.PrimitiveUniformBufferResource = &WorldSpacePrimitiveUniformBuffer;

				Collector.AddMesh(ViewIndex, MeshBatch);
			}
		}
	}

	CPUTimeMS += MeshElementsTimer.GetElapsedMilliseconds();
}

bool NiagaraEffectRendererSprites::SetMaterialUsage()
{
	//Causes deadlock :S Need to look at / rework the setting of materials and render modules.
	//return Material && Material->CheckMaterialUsage_Concurrent(MATUSAGE_ParticleSprites);
	return true;
}

/** Update render data buffer from attributes */
FNiagaraDynamicDataBase *NiagaraEffectRendererSprites::GenerateVertexData(const FNiagaraEmitterParticleData &Data)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraGenSpriteVertexData);

	SimpleTimer VertexDataTimer;
	FVector MaxSize;

	FNiagaraDynamicDataSprites *DynamicData = new FNiagaraDynamicDataSprites;
	TArray<FParticleSpriteVertex>& RenderData = DynamicData->VertexData;

	RenderData.Reset(Data.GetNumParticles());
	CachedBounds.Init();

	const FVector4 *PosPtr = Data.GetAttributeData(FNiagaraVariableInfo(FName(TEXT("Position")), ENiagaraDataType::Vector));
	const FVector4 *ColPtr = Data.GetAttributeData(FNiagaraVariableInfo(FName(TEXT("Color")), ENiagaraDataType::Vector));
	const FVector4 *AgePtr = Data.GetAttributeData(FNiagaraVariableInfo(FName(TEXT("Age")), ENiagaraDataType::Vector));
	const FVector4 *RotPtr = Data.GetAttributeData(FNiagaraVariableInfo(FName(TEXT("Rotation")), ENiagaraDataType::Vector));

	//Bail if we don't have the required attributes to render this emitter.
	if (!PosPtr || !ColPtr || !AgePtr || !RotPtr)
	{
		return DynamicData;
	}

	uint32 NumSubImages = 1;
	if (Properties)
	{
		NumSubImages = Properties->SubImageInfo.X*Properties->SubImageInfo.Y;
	}
	float ParticleId = 0.0f, IdInc = 1.0f / Data.GetNumParticles();
	RenderData.AddUninitialized(Data.GetNumParticles());
	for (uint32 ParticleIndex = 0; ParticleIndex < Data.GetNumParticles(); ParticleIndex++)
	{
		FParticleSpriteVertex& NewVertex = RenderData[ParticleIndex];
		NewVertex.Position = PosPtr[ParticleIndex];
		NewVertex.OldPosition = NewVertex.Position;
		NewVertex.Color = FLinearColor(ColPtr[ParticleIndex]);
		NewVertex.Color.A = ColPtr[ParticleIndex].W;
		NewVertex.ParticleId = ParticleId;
		ParticleId += IdInc;
		NewVertex.RelativeTime = AgePtr[ParticleIndex].X;
		NewVertex.Size = FVector2D(RotPtr[ParticleIndex].Y, RotPtr[ParticleIndex].Z);
		NewVertex.Rotation = RotPtr[ParticleIndex].X;
		NewVertex.SubImageIndex = RotPtr[ParticleIndex].W * NumSubImages;

		FPlatformMisc::Prefetch(PosPtr + ParticleIndex+1);
		FPlatformMisc::Prefetch(RotPtr + ParticleIndex + 1);
		FPlatformMisc::Prefetch(ColPtr + ParticleIndex + 1);		
		FPlatformMisc::Prefetch(AgePtr + ParticleIndex + 1);
		MaxSize = MaxSize.ComponentMax(FVector(NewVertex.Size.GetMax()));
		CachedBounds += NewVertex.Position;
	}
	CachedBounds.ExpandBy(MaxSize);

	CPUTimeMS = VertexDataTimer.GetElapsedMilliseconds();


	return DynamicData;
}



void NiagaraEffectRendererSprites::SetDynamicData_RenderThread(FNiagaraDynamicDataBase* NewDynamicData)
{
	check(IsInRenderingThread());

	if (DynamicDataRender)
	{
		delete DynamicDataRender;
		DynamicDataRender = NULL;
	}
	DynamicDataRender = static_cast<FNiagaraDynamicDataSprites*>(NewDynamicData);
}

int NiagaraEffectRendererSprites::GetDynamicDataSize()
{
	uint32 Size = sizeof(FNiagaraDynamicDataSprites);
	if (DynamicDataRender)
	{
		Size += DynamicDataRender->VertexData.GetAllocatedSize();
	}

	return Size;
}

bool NiagaraEffectRendererSprites::HasDynamicData()
{
	return DynamicDataRender && DynamicDataRender->VertexData.Num() > 0;
}

const TArray<FNiagaraVariableInfo>& NiagaraEffectRendererSprites::GetRequiredAttributes()
{
	static TArray<FNiagaraVariableInfo> Attrs;
	
	if (Attrs.Num() == 0)
	{
		Attrs.Add(FNiagaraVariableInfo(FName(TEXT("Position")), ENiagaraDataType::Vector));
		Attrs.Add(FNiagaraVariableInfo(FName(TEXT("Color")), ENiagaraDataType::Vector));
		Attrs.Add(FNiagaraVariableInfo(FName(TEXT("Rotation")), ENiagaraDataType::Vector));
		Attrs.Add(FNiagaraVariableInfo(FName(TEXT("Age")), ENiagaraDataType::Vector));
	}

	return Attrs;
}



NiagaraEffectRendererRibbon::NiagaraEffectRendererRibbon(ERHIFeatureLevel::Type FeatureLevel, UNiagaraEffectRendererProperties *InProps) :
NiagaraEffectRenderer(),
DynamicDataRender(NULL)
{
	VertexFactory = new FParticleBeamTrailVertexFactory(PVFT_BeamTrail, FeatureLevel);
	Properties = Cast<UNiagaraRibbonRendererProperties>(InProps);
}


void NiagaraEffectRendererRibbon::ReleaseRenderThreadResources()
{
	VertexFactory->ReleaseResource();
	WorldSpacePrimitiveUniformBuffer.ReleaseResource();
}

// FPrimitiveSceneProxy interface.
void NiagaraEffectRendererRibbon::CreateRenderThreadResources()
{
	VertexFactory->InitResource();
}




void NiagaraEffectRendererRibbon::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector, const FNiagaraSceneProxy *SceneProxy) const
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraRender);
	SCOPE_CYCLE_COUNTER(STAT_NiagaraRenderRibbons);

	SimpleTimer MeshElementsTimer;

	check(DynamicDataRender)
	if (DynamicDataRender->VertexData.Num() == 0)
	{
		return;
	}

	const bool bIsWireframe = ViewFamily.EngineShowFlags.Wireframe;
	FMaterialRenderProxy* MaterialRenderProxy = Material->GetRenderProxy(SceneProxy->IsSelected(), SceneProxy->IsHovered());

	int32 SizeInBytes = DynamicDataRender->VertexData.GetTypeSize() * DynamicDataRender->VertexData.Num();
	FGlobalDynamicVertexBuffer::FAllocation LocalDynamicVertexAllocation = FGlobalDynamicVertexBuffer::Get().Allocate(SizeInBytes);

	if (LocalDynamicVertexAllocation.IsValid())
	{
		// Update the primitive uniform buffer if needed.
		if (!WorldSpacePrimitiveUniformBuffer.IsInitialized())
		{
			FPrimitiveUniformShaderParameters PrimitiveUniformShaderParameters = GetPrimitiveUniformShaderParameters(
				FMatrix::Identity,
				SceneProxy->GetActorPosition(),
				SceneProxy->GetBounds(),
				SceneProxy->GetLocalBounds(),
				SceneProxy->ReceivesDecals(),
				false,
				false,
				SceneProxy->UseEditorDepthTest()
				);
			WorldSpacePrimitiveUniformBuffer.SetContents(PrimitiveUniformShaderParameters);
			WorldSpacePrimitiveUniformBuffer.InitResource();
		}

		// Copy the vertex data over.
		FMemory::Memcpy(LocalDynamicVertexAllocation.Buffer, DynamicDataRender->VertexData.GetData(), SizeInBytes);

		// Compute the per-view uniform buffers.
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				const FSceneView* View = Views[ViewIndex];

				FNiagaraMeshCollectorResourcesRibbon& CollectorResources = Collector.AllocateOneFrameResource<FNiagaraMeshCollectorResourcesRibbon>();
				FParticleBeamTrailUniformParameters PerViewUniformParameters;// = UniformParameters;
				PerViewUniformParameters.CameraUp = FVector4(0.0f, 0.0f, 1.0f, 0.0f);
				PerViewUniformParameters.CameraRight = FVector4(1.0f, 0.0f, 0.0f, 0.0f);
				PerViewUniformParameters.ScreenAlignment = FVector4(0.0f, 0.0f, 0.0f, 0.0f);

				// Collector.AllocateOneFrameResource uses default ctor, initialize the vertex factory
				CollectorResources.VertexFactory.SetFeatureLevel(ViewFamily.GetFeatureLevel());
				CollectorResources.VertexFactory.SetParticleFactoryType(PVFT_BeamTrail);

				CollectorResources.UniformBuffer = FParticleBeamTrailUniformBufferRef::CreateUniformBufferImmediate(PerViewUniformParameters, UniformBuffer_SingleFrame);

				CollectorResources.VertexFactory.InitResource();
				CollectorResources.VertexFactory.SetBeamTrailUniformBuffer(CollectorResources.UniformBuffer);
				CollectorResources.VertexFactory.SetVertexBuffer(LocalDynamicVertexAllocation.VertexBuffer, LocalDynamicVertexAllocation.VertexOffset, sizeof(FParticleBeamTrailVertex));
				CollectorResources.VertexFactory.SetDynamicParameterBuffer(NULL, 0, 0);

				FMeshBatch& MeshBatch = Collector.AllocateMesh();
				MeshBatch.VertexFactory = &CollectorResources.VertexFactory;
				MeshBatch.CastShadow = SceneProxy->CastsDynamicShadow();
				MeshBatch.bUseAsOccluder = false;
				MeshBatch.ReverseCulling = SceneProxy->IsLocalToWorldDeterminantNegative();
				MeshBatch.bDisableBackfaceCulling = true;
				MeshBatch.Type = PT_TriangleStrip;
				MeshBatch.DepthPriorityGroup = SceneProxy->GetDepthPriorityGroup(View);
				MeshBatch.bCanApplyViewModeOverrides = true;
				MeshBatch.bUseWireframeSelectionColoring = SceneProxy->IsSelected();

				if (bIsWireframe)
				{
					MeshBatch.MaterialRenderProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy(SceneProxy->IsSelected(), SceneProxy->IsHovered());
				}
				else
				{
					MeshBatch.MaterialRenderProxy = MaterialRenderProxy;
				}

				FMeshBatchElement& MeshElement = MeshBatch.Elements[0];
				MeshElement.IndexBuffer = &GParticleIndexBuffer;
				MeshElement.FirstIndex = 0;
				MeshElement.NumPrimitives = (DynamicDataRender->VertexData.Num() - 2);
				MeshElement.NumInstances = 1;
				MeshElement.MinVertexIndex = 0;
				MeshElement.MaxVertexIndex = DynamicDataRender->VertexData.Num() - 1;
				MeshElement.PrimitiveUniformBufferResource = &WorldSpacePrimitiveUniformBuffer;

				Collector.AddMesh(ViewIndex, MeshBatch);
			}
		}
	}

	CPUTimeMS += MeshElementsTimer.GetElapsedMilliseconds();
}

void NiagaraEffectRendererRibbon::SetDynamicData_RenderThread(FNiagaraDynamicDataBase* NewDynamicData)
{
	check(IsInRenderingThread());

	if (DynamicDataRender)
	{
		delete DynamicDataRender;
		DynamicDataRender = NULL;
	}
	DynamicDataRender = static_cast<FNiagaraDynamicDataRibbon*>(NewDynamicData);
}

int NiagaraEffectRendererRibbon::GetDynamicDataSize()
{
	uint32 Size = sizeof(FNiagaraDynamicDataRibbon);
	if (DynamicDataRender)
	{
		Size += DynamicDataRender->VertexData.GetAllocatedSize();
	}

	return Size;
}

bool NiagaraEffectRendererRibbon::HasDynamicData()
{
	return DynamicDataRender && DynamicDataRender->VertexData.Num() > 0;
}

const TArray<FNiagaraVariableInfo>& NiagaraEffectRendererRibbon::GetRequiredAttributes()
{
	static TArray<FNiagaraVariableInfo> Attrs;

	if (Attrs.Num() == 0)
	{
		Attrs.Add(FNiagaraVariableInfo(FName(TEXT("Position")), ENiagaraDataType::Vector));
		Attrs.Add(FNiagaraVariableInfo(FName(TEXT("Color")), ENiagaraDataType::Vector));
		Attrs.Add(FNiagaraVariableInfo(FName(TEXT("Rotation")), ENiagaraDataType::Vector));
		Attrs.Add(FNiagaraVariableInfo(FName(TEXT("Age")), ENiagaraDataType::Vector));
	}

	return Attrs;
}

bool NiagaraEffectRendererRibbon::SetMaterialUsage()
{
	//Causes deadlock :S Need to look at / rework the setting of materials and render modules.
	//return Material && Material->CheckMaterialUsage_Concurrent(MATUSAGE_BeamTrails);
	return true;
}

FNiagaraDynamicDataBase *NiagaraEffectRendererRibbon::GenerateVertexData(const FNiagaraEmitterParticleData &Data)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraGenRibbonVertexData);

	SimpleTimer VertexDataTimer;

	FNiagaraDynamicDataRibbon *DynamicData = new FNiagaraDynamicDataRibbon;
	TArray<FParticleBeamTrailVertex>& RenderData = DynamicData->VertexData;

	RenderData.Reset(Data.GetNumParticles() * 2);
	//CachedBounds.Init();

	// build a sorted list by age, so we always get particles in order 
	// regardless of them being moved around due to dieing and spawning
	TArray<int32> SortedIndices;
	for (uint32 Idx = 0; Idx < Data.GetNumParticles(); Idx++)
	{
		SortedIndices.Add(Idx);
	}

	const FVector4 *PosPtr = Data.GetAttributeData(FNiagaraVariableInfo(FName(TEXT("Position")), ENiagaraDataType::Vector));
	const FVector4 *ColorPtr = Data.GetAttributeData(FNiagaraVariableInfo(FName(TEXT("Color")), ENiagaraDataType::Vector));
	const FVector4 *RotPtr = Data.GetAttributeData(FNiagaraVariableInfo(FName(TEXT("Rotation")), ENiagaraDataType::Vector));
	const FVector4 *AgePtr = Data.GetAttributeData(FNiagaraVariableInfo(FName(TEXT("Age")), ENiagaraDataType::Vector));

	//Bail if we don't have the required attributes to render this emitter.
	if (!PosPtr || !ColorPtr || !AgePtr || !RotPtr)
	{
		return DynamicData;
	}

	SortedIndices.Sort(
		[&AgePtr](const int32& A, const int32& B) {
		return AgePtr[A].X < AgePtr[B].X;
	}
	);


	FVector2D UVs[4] = { FVector2D(0.0f, 0.0f), FVector2D(1.0f, 0.0f), FVector2D(1.0f, 1.0f), FVector2D(0.0f, 1.0f) };

	FVector PrevPos, PrevPos2, PrevDir(0.0f, 0.0f, 0.1f);
	for (int32 i = 0; i < SortedIndices.Num() - 1; i++)
	{
		uint32 Index1 = SortedIndices[i];
		uint32 Index2 = SortedIndices[i + 1];

		const FVector ParticlePos = PosPtr[Index1];
		FVector ParticleDir = PosPtr[Index2] - ParticlePos;
		if (ParticleDir.Size() <= SMALL_NUMBER)
		{
			ParticleDir = PrevDir*0.1f;
		}
		FVector NormDir = ParticleDir.GetSafeNormal();

		FVector ParticleRight = FVector::CrossProduct(NormDir, FVector(0.0f, 0.0f, 1.0f));
		ParticleRight *= RotPtr[Index1].Y;
		FVector ParticleRightRot = ParticleRight.RotateAngleAxis(RotPtr[Index1].X, NormDir);

		if (i == 0)
		{
			AddRibbonVert(RenderData, ParticlePos + ParticleRightRot, Data, UVs[0], ColorPtr[Index1], AgePtr[Index1], RotPtr[i]);
			AddRibbonVert(RenderData, ParticlePos - ParticleRightRot, Data, UVs[1], ColorPtr[Index1], AgePtr[Index1], RotPtr[i]);
		}
		else
		{
			AddRibbonVert(RenderData, PrevPos2, Data, UVs[0], ColorPtr[Index1], AgePtr[Index1], RotPtr[i]);
			AddRibbonVert(RenderData, PrevPos, Data, UVs[1], ColorPtr[Index1], AgePtr[Index1], RotPtr[i]);
		}

		ParticleRightRot = ParticleRight.RotateAngleAxis(RotPtr[Index2].X, NormDir);
		AddRibbonVert(RenderData, ParticlePos - ParticleRightRot + ParticleDir, Data, UVs[2], ColorPtr[Index2], AgePtr[Index2], RotPtr[i]);
		AddRibbonVert(RenderData, ParticlePos + ParticleRightRot + ParticleDir, Data, UVs[3], ColorPtr[Index2], AgePtr[Index2], RotPtr[i]);
		PrevPos = ParticlePos - ParticleRightRot + ParticleDir;
		PrevPos2 = ParticlePos + ParticleRightRot + ParticleDir;
		PrevDir = ParticleDir;
	}

	CPUTimeMS = VertexDataTimer.GetElapsedMilliseconds();

	return DynamicData;
}