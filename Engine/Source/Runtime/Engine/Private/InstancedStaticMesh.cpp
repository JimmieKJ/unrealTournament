// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	InstancedStaticMesh.cpp: Static mesh rendering code.
=============================================================================*/

#include "EnginePrivate.h"
#include "NavigationSystemHelpers.h"
#include "AI/Navigation/NavCollision.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "InstancedStaticMesh.h"
#include "../../Renderer/Private/ScenePrivate.h"
#include "PhysicsSerializer.h"

const int32 InstancedStaticMeshMaxTexCoord = 8;

IMPLEMENT_HIT_PROXY(HInstancedStaticMeshInstance, HHitProxy);

TAutoConsoleVariable<int32> CVarMinLOD(
	TEXT("foliage.MinLOD"),
	-1,
	TEXT("Used to discard the top LODs for performance evaluation. -1: Disable all effects of this cvar."));


/** InstancedStaticMeshInstance hit proxy */
void HInstancedStaticMeshInstance::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(Component);
}

FStaticMeshInstanceBuffer::FStaticMeshInstanceBuffer(ERHIFeatureLevel::Type InFeatureLevel):
	InstanceData(NULL)
	, Stride(0)
	, NumInstances(0)
{
	SetFeatureLevel(InFeatureLevel);
}

FStaticMeshInstanceBuffer::~FStaticMeshInstanceBuffer()
{
	CleanUp();
}

/** Delete existing resources */
void FStaticMeshInstanceBuffer::CleanUp()
{
	if (InstanceData)
	{
		delete InstanceData;
		InstanceData = NULL;
	}
	NumInstances = 0;
}


void FStaticMeshInstanceBuffer::SetupCPUAccess(UInstancedStaticMeshComponent* InComponent)
{
	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.GenerateMeshDistanceFields"));

	const bool bNeedsCPUAccess = InComponent->CastShadow && InComponent->bAffectDistanceFieldLighting 
		// Distance field algorithms need access to instance data on the CPU
		&& CVar->GetValueOnGameThread() != 0;

	InstanceData->SetAllowCPUAccess(InstanceData->GetAllowCPUAccess() || bNeedsCPUAccess);
}

void FStaticMeshInstanceBuffer::Init(UInstancedStaticMeshComponent* InComponent, const TArray<TRefCountPtr<HHitProxy> >& InHitProxies)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FStaticMeshInstanceBuffer_Init);
	double StartTime = FPlatformTime::Seconds();

	bool bUseRemapTable = InComponent->PerInstanceSMData.Num() == InComponent->InstanceReorderTable.Num();

	int32 NumRealInstances = InComponent->PerInstanceSMData.Num();
	int32 NumRemoved = InComponent->RemovedInstances.Num();

	// Allocate the vertex data storage type.
	AllocateData();

	SetupCPUAccess(InComponent);

	NumInstances = NumRealInstances + NumRemoved;
	InstanceData->AllocateInstances(NumInstances);

	// Setup our random number generator such that random values are generated consistently for any
	// given instance index between reattaches
	FRandomStream RandomStream( InComponent->InstancingRandomSeed );

	for (int32 InstanceIndex = 0; InstanceIndex < NumRealInstances; InstanceIndex++)
	{
		FInstanceStream* InstanceRenderData = InstanceData->GetInstanceWriteAddress(bUseRemapTable ? InComponent->InstanceReorderTable[InstanceIndex] : InstanceIndex);
		const FInstancedStaticMeshInstanceData& Instance = InComponent->PerInstanceSMData[InstanceIndex];
						
		InstanceRenderData->SetInstance(Instance.Transform, RandomStream.GetFraction(), Instance.LightmapUVBias, Instance.ShadowmapUVBias);
	}

	SetPerInstanceEditorData(InComponent, InHitProxies);

	// Hide any removed instances
	if (NumRemoved)
	{
		check(bUseRemapTable);
		for (int32 InstanceIndex = 0; InstanceIndex < NumRemoved; InstanceIndex++)
		{
			FInstanceStream* InstanceRenderData = InstanceData->GetInstanceWriteAddress(InComponent->RemovedInstances[InstanceIndex]);
			InstanceRenderData->NullifyInstance();
		}
	}

	float ThisTime = (StartTime - FPlatformTime::Seconds()) * 1000.0f;
	if (ThisTime > 30.0f)
	{
		UE_LOG(LogStaticMesh, Display, TEXT("Took %6.2fms to set up instance buffer for %d instances for component %s."), ThisTime, NumRealInstances, *InComponent->GetFullName());
	}
}

void FStaticMeshInstanceBuffer::InitFromPreallocatedData(UInstancedStaticMeshComponent* InComponent, FStaticMeshInstanceData& Other)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FStaticMeshInstanceBuffer_InitFromPreallocatedData);
	const uint32 NewNumInstances = Other.Num();
	AllocateData(Other);
	NumInstances = NewNumInstances;
	SetupCPUAccess(InComponent);
	InstanceData->SetAllowCPUAccess(true);		// GDC demo hack!
}

void FStaticMeshInstanceBuffer::SetPerInstanceEditorData(UInstancedStaticMeshComponent* InComponent, const TArray<TRefCountPtr<HHitProxy>>& InHitProxies)
{
#if WITH_EDITOR
	if (GIsEditor)
	{
		FColor HitProxyColor(ForceInit);
		int32 NumRealInstances = InComponent->PerInstanceSMData.Num();
		bool bUseRemapTable = InComponent->PerInstanceSMData.Num() == InComponent->InstanceReorderTable.Num();
		
		for (int32 InstanceIndex = 0; InstanceIndex < NumRealInstances; InstanceIndex++)
		{
			if (InHitProxies.Num() == NumRealInstances)
			{
				HitProxyColor = InHitProxies[InstanceIndex]->Id.GetColor();
			}
			// Record if the instance is selected
			bool bSelected = InstanceIndex < InComponent->SelectedInstances.Num() && InComponent->SelectedInstances[InstanceIndex];
			FInstanceStream* InstanceRenderData = InstanceData->GetInstanceWriteAddress(bUseRemapTable ? InComponent->InstanceReorderTable[InstanceIndex] : InstanceIndex);
			InstanceRenderData->SetInstanceEditorData(HitProxyColor, bSelected);
		}
	}
#endif
}

/**
 * Specialized assignment operator, only used when importing LOD's.  
 */
void FStaticMeshInstanceBuffer::operator=(const FStaticMeshInstanceBuffer &Other)
{
	checkf(0, TEXT("Unexpected assignment call"));
}

void FStaticMeshInstanceBuffer::InitRHI()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FStaticMeshInstanceBuffer_InitRHI);
	check(InstanceData);
	FResourceArrayInterface* ResourceArray = InstanceData->GetResourceArray();
	if(ResourceArray->GetResourceDataSize())
	{
		// Create the vertex buffer.
		FRHIResourceCreateInfo CreateInfo(ResourceArray);
		VertexBufferRHI = RHICreateVertexBuffer(ResourceArray->GetResourceDataSize(),BUF_Static, CreateInfo);
	}
}

void FStaticMeshInstanceBuffer::AllocateData()
{
	// Clear any old VertexData before allocating.
	CleanUp();

	check(HasValidFeatureLevel());
	
	const bool bInstanced = RHISupportsInstancing(GetFeatureLevelShaderPlatform(GetFeatureLevel()));
	const bool bNeedsCPUAccess = !bInstanced;
	InstanceData = new FStaticMeshInstanceData(bNeedsCPUAccess);
	// Calculate the vertex stride.
	Stride = InstanceData->GetStride();
}

void FStaticMeshInstanceBuffer::AllocateData(FStaticMeshInstanceData& Other)
{
	AllocateData();
	Other.SetAllowCPUAccess(InstanceData->GetAllowCPUAccess());
	FMemory::Memswap(&Other, InstanceData, sizeof(FStaticMeshInstanceData));
}




/**
 * Should we cache the material's shadertype on this platform with this vertex factory? 
 */
bool FInstancedStaticMeshVertexFactory::ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
{
	return (Material->IsUsedWithInstancedStaticMeshes() || Material->IsSpecialEngineMaterial())
			&& FLocalVertexFactory::ShouldCache(Platform, Material, ShaderType);
}


/**
 * Copy the data from another vertex factory
 * @param Other - factory to copy from
 */
void FInstancedStaticMeshVertexFactory::Copy(const FInstancedStaticMeshVertexFactory& Other)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		FInstancedStaticMeshVertexFactoryCopyData,
		FInstancedStaticMeshVertexFactory*,VertexFactory,this,
		const DataType*,DataCopy,&Other.Data,
	{
		VertexFactory->Data = *DataCopy;
	});
	BeginUpdateResourceRHI(this);
}

void FInstancedStaticMeshVertexFactory::InitRHI()
{
	check(HasValidFeatureLevel());
	const bool bInstanced = RHISupportsInstancing(GetFeatureLevelShaderPlatform(GetFeatureLevel()));

#if !ALLOW_DITHERED_LOD_FOR_INSTANCED_STATIC_MESHES // position only shaders cannot work with dithered LOD
	// If the vertex buffer containing position is not the same vertex buffer containing the rest of the data,
	// then initialize PositionStream and PositionDeclaration.
	if(Data.PositionComponent.VertexBuffer != Data.TangentBasisComponents[0].VertexBuffer)
	{
		FVertexDeclarationElementList PositionOnlyStreamElements;
		PositionOnlyStreamElements.Add(AccessPositionStreamComponent(Data.PositionComponent,0));

		if (bInstanced)
		{
			// toss in the instanced location stream
			PositionOnlyStreamElements.Add(AccessPositionStreamComponent(Data.InstanceOriginComponent,8));
			PositionOnlyStreamElements.Add(AccessPositionStreamComponent(Data.InstanceTransformComponent[0],9));
			PositionOnlyStreamElements.Add(AccessPositionStreamComponent(Data.InstanceTransformComponent[1],10));
			PositionOnlyStreamElements.Add(AccessPositionStreamComponent(Data.InstanceTransformComponent[2],11));
		}
		InitPositionDeclaration(PositionOnlyStreamElements);
	}
#endif

	FVertexDeclarationElementList Elements;
	if(Data.PositionComponent.VertexBuffer != NULL)
	{
		Elements.Add(AccessStreamComponent(Data.PositionComponent,0));
	}

	// only tangent,normal are used by the stream. the binormal is derived in the shader
	uint8 TangentBasisAttributes[2] = { 1, 2 };
	for(int32 AxisIndex = 0;AxisIndex < 2;AxisIndex++)
	{
		if(Data.TangentBasisComponents[AxisIndex].VertexBuffer != NULL)
		{
			Elements.Add(AccessStreamComponent(Data.TangentBasisComponents[AxisIndex],TangentBasisAttributes[AxisIndex]));
		}
	}

	if(Data.ColorComponent.VertexBuffer)
	{
		Elements.Add(AccessStreamComponent(Data.ColorComponent,3));
	}
	else
	{
		//If the mesh has no color component, set the null color buffer on a new stream with a stride of 0.
		//This wastes 4 bytes of bandwidth per vertex, but prevents having to compile out twice the number of vertex factories.
		FVertexStreamComponent NullColorComponent(&GNullColorVertexBuffer, 0, 0, VET_Color);
		Elements.Add(AccessStreamComponent(NullColorComponent,3));
	}

	if(Data.TextureCoordinates.Num())
	{
		const int32 BaseTexCoordAttribute = 4;
		for(int32 CoordinateIndex = 0;CoordinateIndex < Data.TextureCoordinates.Num();CoordinateIndex++)
		{
			Elements.Add(AccessStreamComponent(
				Data.TextureCoordinates[CoordinateIndex],
				BaseTexCoordAttribute + CoordinateIndex
				));
		}

		for(int32 CoordinateIndex = Data.TextureCoordinates.Num(); CoordinateIndex < (InstancedStaticMeshMaxTexCoord + 1) / 2; CoordinateIndex++)
		{
			Elements.Add(AccessStreamComponent(
				Data.TextureCoordinates[Data.TextureCoordinates.Num() - 1],
				BaseTexCoordAttribute + CoordinateIndex
				));
		}
	}

	if(Data.LightMapCoordinateComponent.VertexBuffer)
	{
		Elements.Add(AccessStreamComponent(Data.LightMapCoordinateComponent,15));
	}
	else if(Data.TextureCoordinates.Num())
	{
		Elements.Add(AccessStreamComponent(Data.TextureCoordinates[0],15));
	}

	// toss in the instanced location stream
	if (bInstanced)
	{
		Elements.Add(AccessStreamComponent(Data.InstanceOriginComponent,8));
		Elements.Add(AccessStreamComponent(Data.InstanceTransformComponent[0],9));
		Elements.Add(AccessStreamComponent(Data.InstanceTransformComponent[1],10));
		Elements.Add(AccessStreamComponent(Data.InstanceTransformComponent[2],11));
		Elements.Add(AccessStreamComponent(Data.InstanceLightmapAndShadowMapUVBiasComponent,12));
	}

	// we don't need per-vertex shadow or lightmap rendering
	InitDeclaration(Elements,Data);
}


FVertexFactoryShaderParameters* FInstancedStaticMeshVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	return ShaderFrequency == SF_Vertex ? new FInstancedStaticMeshVertexFactoryShaderParameters() : NULL;
}

IMPLEMENT_VERTEX_FACTORY_TYPE(FInstancedStaticMeshVertexFactory,"LocalVertexFactory",true,true,true,true,true);





void FInstancedStaticMeshRenderData::InitStaticMeshVertexFactories(
		TArray<FInstancedStaticMeshVertexFactory>* VertexFactories,
		FInstancedStaticMeshRenderData* InstancedRenderData,
		UStaticMesh* Parent)
{
	const bool bInstanced = RHISupportsInstancing(GetFeatureLevelShaderPlatform(InstancedRenderData->FeatureLevel));

	for( int32 LODIndex=0;LODIndex<VertexFactories->Num(); LODIndex++ )
	{
		const FStaticMeshLODResources* RenderData = &InstancedRenderData->LODModels[LODIndex];
						
		FInstancedStaticMeshVertexFactory::DataType Data;
		Data.PositionComponent = FVertexStreamComponent(
			&RenderData->PositionVertexBuffer,
			STRUCT_OFFSET(FPositionVertex,Position),
			RenderData->PositionVertexBuffer.GetStride(),
			VET_Float3
			);
		Data.TangentBasisComponents[0] = FVertexStreamComponent(
			&RenderData->VertexBuffer,
			STRUCT_OFFSET(FStaticMeshFullVertex,TangentX),
			RenderData->VertexBuffer.GetStride(),
			VET_PackedNormal
			);
		Data.TangentBasisComponents[1] = FVertexStreamComponent(
			&RenderData->VertexBuffer,
			STRUCT_OFFSET(FStaticMeshFullVertex,TangentZ),
			RenderData->VertexBuffer.GetStride(),
			VET_PackedNormal
			);


		if( RenderData->ColorVertexBuffer.GetNumVertices() > 0 )
		{
			Data.ColorComponent = FVertexStreamComponent(
				&RenderData->ColorVertexBuffer,
				0,	// Struct offset to color
				RenderData->ColorVertexBuffer.GetStride(),
				VET_Color
				);
		}

		Data.TextureCoordinates.Empty();
		// Only bind InstancedStaticMeshMaxTexCoord, even if the mesh has more.
		int32 NumTexCoords = FMath::Min<int32>((int32)RenderData->VertexBuffer.GetNumTexCoords(), InstancedStaticMeshMaxTexCoord);
		if( !RenderData->VertexBuffer.GetUseFullPrecisionUVs() )
		{
			int32 UVIndex;
			for (UVIndex = 0; UVIndex < NumTexCoords - 1; UVIndex += 2)
			{
				Data.TextureCoordinates.Add(FVertexStreamComponent(
					&RenderData->VertexBuffer,
					STRUCT_OFFSET(TStaticMeshFullVertexFloat16UVs<MAX_STATIC_TEXCOORDS>, UVs) + sizeof(FVector2DHalf)* UVIndex,
					RenderData->VertexBuffer.GetStride(),
					VET_Half4
					));
			}
			// possible last UV channel if we have an odd number
			if( UVIndex < NumTexCoords )
			{
				Data.TextureCoordinates.Add(FVertexStreamComponent(
					&RenderData->VertexBuffer,
					STRUCT_OFFSET(TStaticMeshFullVertexFloat16UVs<MAX_STATIC_TEXCOORDS>,UVs) + sizeof(FVector2DHalf) * UVIndex,
					RenderData->VertexBuffer.GetStride(),
					VET_Half2
					));
			}

			if (Parent->LightMapCoordinateIndex >= 0 && Parent->LightMapCoordinateIndex < NumTexCoords)
			{
				Data.LightMapCoordinateComponent = FVertexStreamComponent(
					&RenderData->VertexBuffer,
					STRUCT_OFFSET(TStaticMeshFullVertexFloat16UVs<MAX_STATIC_TEXCOORDS>, UVs) + sizeof(FVector2DHalf)* Parent->LightMapCoordinateIndex,
					RenderData->VertexBuffer.GetStride(),
					VET_Half2
					);
			}
		}
		else
		{
			int32 UVIndex;
			for (UVIndex = 0; UVIndex < NumTexCoords - 1; UVIndex += 2)
			{
				Data.TextureCoordinates.Add(FVertexStreamComponent(
					&RenderData->VertexBuffer,
					STRUCT_OFFSET(TStaticMeshFullVertexFloat32UVs<MAX_STATIC_TEXCOORDS>, UVs) + sizeof(FVector2D)* UVIndex,
					RenderData->VertexBuffer.GetStride(),
					VET_Float4
					));
			}
			// possible last UV channel if we have an odd number
			if (UVIndex < NumTexCoords)
			{
				Data.TextureCoordinates.Add(FVertexStreamComponent(
					&RenderData->VertexBuffer,
					STRUCT_OFFSET(TStaticMeshFullVertexFloat32UVs<MAX_STATIC_TEXCOORDS>,UVs) + sizeof(FVector2D) * UVIndex,
					RenderData->VertexBuffer.GetStride(),
					VET_Float2
					));
			}

			if (Parent->LightMapCoordinateIndex >= 0 && Parent->LightMapCoordinateIndex < NumTexCoords)
			{
				Data.LightMapCoordinateComponent = FVertexStreamComponent(
					&RenderData->VertexBuffer,
					STRUCT_OFFSET(TStaticMeshFullVertexFloat32UVs<InstancedStaticMeshMaxTexCoord>, UVs) + sizeof(FVector2D)* Parent->LightMapCoordinateIndex,
					RenderData->VertexBuffer.GetStride(),
					VET_Float2
					);
			}
		}

		if (bInstanced)
		{
			const FStaticMeshInstanceBuffer& InstanceBuffer = InstancedRenderData->PerInstanceRenderData->InstanceBuffer;
			
			Data.InstanceOriginComponent = FVertexStreamComponent(
				&InstanceBuffer,
				STRUCT_OFFSET(FInstanceStream,InstanceOrigin), 
				InstanceBuffer.GetStride(),
				VET_Float4,
				true
				);
			Data.InstanceTransformComponent[0] = FVertexStreamComponent(
				&InstanceBuffer,
				STRUCT_OFFSET(FInstanceStream,InstanceTransform1), 
				InstanceBuffer.GetStride(),
				VET_Half4,
				true
				);
			Data.InstanceTransformComponent[1] = FVertexStreamComponent(
				&InstanceBuffer,
				STRUCT_OFFSET(FInstanceStream,InstanceTransform2), 
				InstanceBuffer.GetStride(),
				VET_Half4,
				true
				);
			Data.InstanceTransformComponent[2] = FVertexStreamComponent(
				&InstanceBuffer,
				STRUCT_OFFSET(FInstanceStream,InstanceTransform3), 
				InstanceBuffer.GetStride(),
				VET_Half4,
				true
				);

			Data.InstanceLightmapAndShadowMapUVBiasComponent = FVertexStreamComponent(
				&InstanceBuffer,
				STRUCT_OFFSET(FInstanceStream,InstanceLightmapAndShadowMapUVBias), 
				InstanceBuffer.GetStride(),
				VET_Short4N,
				true
				);
		}

		// Assign to the vertex factory for this LOD.
		FInstancedStaticMeshVertexFactory& VertexFactory = (*VertexFactories)[LODIndex];
		VertexFactory.SetData(Data);
	}
}


void FInstancedStaticMeshSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_InstancedStaticMeshSceneProxy_GetMeshElements);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	const bool bSelectionRenderEnabled = GIsEditor && ViewFamily.EngineShowFlags.Selection;

	// If the first pass rendered selected instances only, we need to render the deselected instances in a second pass
	const int32 NumSelectionGroups = (bSelectionRenderEnabled && bHasSelectedInstances) ? 2 : 1;

	const FInstancingUserData* PassUserData[2] =
	{
		bHasSelectedInstances && bSelectionRenderEnabled ? &UserData_SelectedInstances : &UserData_AllInstances,
		&UserData_DeselectedInstances
	};

	bool BatchRenderSelection[2] = 
	{
		bSelectionRenderEnabled && IsSelected(),
		false
	};

	const bool bIsWireframe = ViewFamily.EngineShowFlags.Wireframe;

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			const FSceneView* View = Views[ViewIndex];

			for (int32 SelectionGroupIndex = 0; SelectionGroupIndex < NumSelectionGroups; SelectionGroupIndex++)
			{
				const int32 LODIndex = GetLOD(View);
				const FStaticMeshLODResources& LODModel = StaticMesh->RenderData->LODResources[LODIndex];

				for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
				{
					const int32 NumBatches = GetNumMeshBatches();

					for (int32 BatchIndex = 0; BatchIndex < NumBatches; BatchIndex++)
					{
						FMeshBatch& MeshElement = Collector.AllocateMesh();

						if (GetMeshElement(LODIndex, BatchIndex, SectionIndex, GetDepthPriorityGroup(View), BatchRenderSelection[SelectionGroupIndex], IsHovered(), MeshElement))
						{
							//@todo-rco this is only supporting selection on the first element
							MeshElement.Elements[0].UserData = PassUserData[SelectionGroupIndex];
							MeshElement.bCanApplyViewModeOverrides = true;
							MeshElement.bUseSelectionOutline = BatchRenderSelection[SelectionGroupIndex];
							MeshElement.bUseWireframeSelectionColoring = BatchRenderSelection[SelectionGroupIndex];

							if (View->bRenderFirstInstanceOnly)
							{
								for (int32 ElementIndex = 0; ElementIndex < MeshElement.Elements.Num(); ElementIndex++)
								{
									MeshElement.Elements[ElementIndex].NumInstances = FMath::Min<uint32>(MeshElement.Elements[ElementIndex].NumInstances, 1);
								}
							}

							Collector.AddMesh(ViewIndex, MeshElement);
							INC_DWORD_STAT_BY(STAT_StaticMeshTriangles, MeshElement.GetNumPrimitives());
						}
					}
				}
			}
		}
	}
#endif
}

int32 FInstancedStaticMeshSceneProxy::GetNumMeshBatches() const
{
	const bool bInstanced = RHISupportsInstancing(GetFeatureLevelShaderPlatform(InstancedRenderData.FeatureLevel));

	if (bInstanced)
	{
		return 1;
	}
	else
	{
		const uint32 NumInstances = InstancedRenderData.PerInstanceRenderData->InstanceBuffer.GetNumInstances();
		const uint32 MaxInstancesPerBatch = FInstancedStaticMeshVertexFactory::NumBitsForVisibilityMask();
		const uint32 NumBatches = FMath::DivideAndRoundUp(NumInstances, MaxInstancesPerBatch);
		return NumBatches;
	}
}

void FInstancedStaticMeshSceneProxy::SetupInstancedMeshBatch(int32 LODIndex, int32 BatchIndex, FMeshBatch& OutMeshBatch) const
{
	const bool bInstanced = RHISupportsInstancing(GetFeatureLevelShaderPlatform(InstancedRenderData.FeatureLevel));
	OutMeshBatch.VertexFactory = &InstancedRenderData.VertexFactories[LODIndex];
	const uint32 NumInstances = InstancedRenderData.PerInstanceRenderData->InstanceBuffer.GetNumInstances();
	FMeshBatchElement& BatchElement0 = OutMeshBatch.Elements[0];
	BatchElement0.UserData = (void*)&UserData_AllInstances;
	BatchElement0.InstancedLODIndex = LODIndex;
	BatchElement0.UserIndex = 0;

	if (bInstanced)
	{
		BatchElement0.NumInstances = NumInstances;
	}
	else
	{
		const uint32 MaxInstancesPerBatch = FInstancedStaticMeshVertexFactory::NumBitsForVisibilityMask();
		const uint32 NumBatches = FMath::DivideAndRoundUp(NumInstances, MaxInstancesPerBatch);
		uint32 NumInstancesThisBatch = BatchIndex == NumBatches - 1 ? NumInstances % MaxInstancesPerBatch : MaxInstancesPerBatch;

		OutMeshBatch.Elements.Reserve(NumInstancesThisBatch);

		int32 InstanceIndex = BatchIndex * MaxInstancesPerBatch;
		if (NumInstancesThisBatch > 0)
		{
			// BatchElement0 is already inside the array; but Reserve() might have shifted it
			OutMeshBatch.Elements[0].UserIndex = InstanceIndex;
			--NumInstancesThisBatch;
			++InstanceIndex;

			// Add remaining BatchElements 1..n-1
			while (NumInstancesThisBatch > 0)
			{
				auto* NewBatchElement = new(OutMeshBatch.Elements) FMeshBatchElement();
				*NewBatchElement = BatchElement0;
				NewBatchElement->UserIndex = InstanceIndex;
				++InstanceIndex;
				--NumInstancesThisBatch;
			}
		}
	}
}

bool FInstancedStaticMeshSceneProxy::GetShadowMeshElement(int32 LODIndex, int32 BatchIndex, uint8 InDepthPriorityGroup, FMeshBatch& OutMeshBatch) const
{
	if (LODIndex < InstancedRenderData.VertexFactories.Num() && FStaticMeshSceneProxy::GetShadowMeshElement(LODIndex, BatchIndex, InDepthPriorityGroup, OutMeshBatch))
	{
		SetupInstancedMeshBatch(LODIndex, BatchIndex, OutMeshBatch);
		return true;
	}
	return false;
}

/** Sets up a FMeshBatch for a specific LOD and element. */
bool FInstancedStaticMeshSceneProxy::GetMeshElement(int32 LODIndex, int32 BatchIndex, int32 ElementIndex, uint8 InDepthPriorityGroup, const bool bUseSelectedMaterial, const bool bUseHoveredMaterial, FMeshBatch& OutMeshBatch) const
{
	if (LODIndex < InstancedRenderData.VertexFactories.Num() && FStaticMeshSceneProxy::GetMeshElement(LODIndex, BatchIndex, ElementIndex, InDepthPriorityGroup, bUseSelectedMaterial, bUseHoveredMaterial, OutMeshBatch))
	{
		SetupInstancedMeshBatch(LODIndex, BatchIndex, OutMeshBatch);
		return true;
	}
	return false;
};

/** Sets up a wireframe FMeshBatch for a specific LOD. */
bool FInstancedStaticMeshSceneProxy::GetWireframeMeshElement(int32 LODIndex, int32 BatchIndex, const FMaterialRenderProxy* WireframeRenderProxy, uint8 InDepthPriorityGroup, FMeshBatch& OutMeshBatch) const
{
	if (LODIndex < InstancedRenderData.VertexFactories.Num() && FStaticMeshSceneProxy::GetWireframeMeshElement(LODIndex, BatchIndex, WireframeRenderProxy, InDepthPriorityGroup, OutMeshBatch))
	{
		SetupInstancedMeshBatch(LODIndex, BatchIndex, OutMeshBatch);
		return true;
	}
	return false;
}

void FInstancedStaticMeshSceneProxy::GetDistancefieldAtlasData(FBox& LocalVolumeBounds, FIntVector& OutBlockMin, FIntVector& OutBlockSize, bool& bOutBuiltAsIfTwoSided, bool& bMeshWasPlane, TArray<FMatrix>& ObjectLocalToWorldTransforms) const
{
	FStaticMeshSceneProxy::GetDistancefieldAtlasData(LocalVolumeBounds, OutBlockMin, OutBlockSize, bOutBuiltAsIfTwoSided, bMeshWasPlane, ObjectLocalToWorldTransforms);

	ObjectLocalToWorldTransforms.Reset();

	const FInstanceStream* InstanceStream = InstancedRenderData.PerInstanceRenderData->InstanceBuffer.GetData();
	const FVector4 FourthVector(0, 0, 0, 1);

	const uint32 NumInstances = InstancedRenderData.PerInstanceRenderData->InstanceBuffer.GetNumInstances();
	for (uint32 InstanceIndex = 0; InstanceIndex < NumInstances; InstanceIndex++)
	{
		const FMatrix TransposedInstanceToLocal(
			FPlane(
				InstanceStream[InstanceIndex].InstanceTransform1[0].GetFloat(),
				InstanceStream[InstanceIndex].InstanceTransform1[1].GetFloat(),
				InstanceStream[InstanceIndex].InstanceTransform1[2].GetFloat(),
				InstanceStream[InstanceIndex].InstanceOrigin.X),
			FPlane(
				InstanceStream[InstanceIndex].InstanceTransform2[0].GetFloat(),
				InstanceStream[InstanceIndex].InstanceTransform2[1].GetFloat(),
				InstanceStream[InstanceIndex].InstanceTransform2[2].GetFloat(),
				InstanceStream[InstanceIndex].InstanceOrigin.Y),
			FPlane(
				InstanceStream[InstanceIndex].InstanceTransform3[0].GetFloat(),
				InstanceStream[InstanceIndex].InstanceTransform3[1].GetFloat(),
				InstanceStream[InstanceIndex].InstanceTransform3[2].GetFloat(),
				InstanceStream[InstanceIndex].InstanceOrigin.Z),
			(const FPlane&)FourthVector);

		ObjectLocalToWorldTransforms.Add(TransposedInstanceToLocal.GetTransposed() * GetLocalToWorld());
	}
}

void FInstancedStaticMeshSceneProxy::GetDistanceFieldInstanceInfo(int32& NumInstances, float& BoundsSurfaceArea) const
{
	NumInstances = DistanceFieldData ? InstancedRenderData.PerInstanceRenderData->InstanceBuffer.GetNumInstances() : 0;

	if (NumInstances > 0)
	{
		const FInstanceStream* InstanceStream = InstancedRenderData.PerInstanceRenderData->InstanceBuffer.GetData();
		const FVector4 FourthVector(0, 0, 0, 1);
		const int32 InstanceIndex = 0;
		const FMatrix TransposedInstanceToLocal(
			FPlane(
				InstanceStream[InstanceIndex].InstanceTransform1[0].GetFloat(),
				InstanceStream[InstanceIndex].InstanceTransform1[1].GetFloat(),
				InstanceStream[InstanceIndex].InstanceTransform1[2].GetFloat(),
				InstanceStream[InstanceIndex].InstanceOrigin.X),
			FPlane(
				InstanceStream[InstanceIndex].InstanceTransform2[0].GetFloat(),
				InstanceStream[InstanceIndex].InstanceTransform2[1].GetFloat(),
				InstanceStream[InstanceIndex].InstanceTransform2[2].GetFloat(),
				InstanceStream[InstanceIndex].InstanceOrigin.Y),
			FPlane(
				InstanceStream[InstanceIndex].InstanceTransform3[0].GetFloat(),
				InstanceStream[InstanceIndex].InstanceTransform3[1].GetFloat(),
				InstanceStream[InstanceIndex].InstanceTransform3[2].GetFloat(),
				InstanceStream[InstanceIndex].InstanceOrigin.Z),
			(const FPlane&)FourthVector);

		const FMatrix InstanceTransform = TransposedInstanceToLocal.GetTransposed() * GetLocalToWorld();
		const FVector AxisScales = InstanceTransform.GetScaleVector();
		const FVector BoxDimensions = RenderData->Bounds.BoxExtent * AxisScales * 2;

		BoundsSurfaceArea = 2 * BoxDimensions.X * BoxDimensions.Y
			+ 2 * BoxDimensions.Z * BoxDimensions.Y
			+ 2 * BoxDimensions.X * BoxDimensions.Z;
	}
}

/*-----------------------------------------------------------------------------
	UInstancedStaticMeshComponent
-----------------------------------------------------------------------------*/

UInstancedStaticMeshComponent::UInstancedStaticMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bPerInstanceRenderDataWasPrebuilt(false)
	, SelectionStamp(0)
{
	Mobility = EComponentMobility::Movable;
	BodyInstance.bSimulatePhysics = false;

	PhysicsSerializer = ObjectInitializer.CreateDefaultSubobject<UPhysicsSerializer>(this, TEXT("PhysicsSerializer"));
}

#if WITH_EDITOR
/** Helper class used to preserve lighting/selection state across blueprint reinstancing */
class FInstancedStaticMeshComponentInstanceData : public FSceneComponentInstanceData
{
public:
	FInstancedStaticMeshComponentInstanceData(const UInstancedStaticMeshComponent& InComponent)
		: FSceneComponentInstanceData(&InComponent)
		, StaticMesh(InComponent.StaticMesh)
		, bHasCachedStaticLighting(false)
	{
	}

	virtual void ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase) override
	{
		FSceneComponentInstanceData::ApplyToComponent(Component, CacheApplyPhase);
		CastChecked<UInstancedStaticMeshComponent>(Component)->ApplyComponentInstanceData(this);
	}

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		FSceneComponentInstanceData::AddReferencedObjects(Collector);

		Collector.AddReferencedObject(StaticMesh);
		if (bHasCachedStaticLighting)
		{
			for (FLightMapRef& LightMapRef : CachedStaticLighting.LODDataLightMap)
			{
				if (LightMapRef != nullptr)
				{
					LightMapRef->AddReferencedObjects(Collector);
				}
			}
			for (FShadowMapRef& ShadowMapRef : CachedStaticLighting.LODDataShadowMap)
			{
				if (ShadowMapRef != nullptr)
				{
					ShadowMapRef->AddReferencedObjects(Collector);
				}
			}		
		}
	}

	/** Used to store lightmap data during RerunConstructionScripts */
	struct FLightMapInstanceData
	{
		/** Transform of component */
		FTransform Transform;

		/** Lightmaps from LODData */
		TArray<FLightMapRef>	LODDataLightMap;
		TArray<FShadowMapRef>	LODDataShadowMap;

		TArray<FGuid> IrrelevantLights;
	};

public:
	/** Mesh being used by component */
	UStaticMesh* StaticMesh;

	// Static lighting info
	bool bHasCachedStaticLighting;
	FLightMapInstanceData CachedStaticLighting;
	TArray<FInstancedStaticMeshInstanceData> PerInstanceSMData;

	/** The cached selected instances */
	TBitArray<> SelectedInstances;
};
#endif

FName UInstancedStaticMeshComponent::GetComponentInstanceDataType() const
{
	static const FName InstancedStaticMeshComponentInstanceDataName(TEXT("InstancedStaticMeshComponentInstanceData"));
	return InstancedStaticMeshComponentInstanceDataName;
}

FActorComponentInstanceData* UInstancedStaticMeshComponent::GetComponentInstanceData() const
{
#if WITH_EDITOR
	FActorComponentInstanceData* InstanceData = nullptr;
	FInstancedStaticMeshComponentInstanceData* StaticMeshInstanceData = nullptr;

	// Don't back up static lighting if there isn't any
	if (bHasCachedStaticLighting || SelectedInstances.Num() > 0)
	{
		InstanceData = StaticMeshInstanceData = new FInstancedStaticMeshComponentInstanceData(*this);	
	}

	// Don't back up static lighting if there isn't any
	if (bHasCachedStaticLighting)
	{
		// Fill in info (copied from UStaticMeshComponent::GetComponentInstanceData)
		StaticMeshInstanceData->bHasCachedStaticLighting = true;
		StaticMeshInstanceData->CachedStaticLighting.Transform = ComponentToWorld;
		StaticMeshInstanceData->CachedStaticLighting.IrrelevantLights = IrrelevantLights;
		StaticMeshInstanceData->CachedStaticLighting.LODDataLightMap.Empty(LODData.Num());
		for (const FStaticMeshComponentLODInfo& LODDataEntry : LODData)
		{
			StaticMeshInstanceData->CachedStaticLighting.LODDataLightMap.Add(LODDataEntry.LightMap);
			StaticMeshInstanceData->CachedStaticLighting.LODDataShadowMap.Add(LODDataEntry.ShadowMap);
		}

		// Back up per-instance lightmap/shadowmap info
		StaticMeshInstanceData->PerInstanceSMData = PerInstanceSMData;
	}

	// Back up instance selection
	if (SelectedInstances.Num() > 0)
	{
		StaticMeshInstanceData->SelectedInstances = SelectedInstances;
	}

	if (InstanceData == nullptr)
	{
		// Skip up the hierarchy
		InstanceData = UPrimitiveComponent::GetComponentInstanceData();
	}

	return InstanceData;
#else
	return nullptr;
#endif
}

void UInstancedStaticMeshComponent::ApplyComponentInstanceData(FInstancedStaticMeshComponentInstanceData* InstancedMeshData)
{
#if WITH_EDITOR
	check(InstancedMeshData);

	if (StaticMesh != InstancedMeshData->StaticMesh)
	{
		return;
	}

	// See if data matches current state
	if (InstancedMeshData->bHasCachedStaticLighting)
	{
		bool bMatch = false;

		// Check for any instance having moved as that would invalidate static lighting
		if (PerInstanceSMData.Num() == InstancedMeshData->PerInstanceSMData.Num() &&
			InstancedMeshData->CachedStaticLighting.Transform.Equals(ComponentToWorld))
		{
			bMatch = true;

			for (int32 InstanceIndex = 0; InstanceIndex < PerInstanceSMData.Num(); ++InstanceIndex)
			{
				if (PerInstanceSMData[InstanceIndex].Transform != InstancedMeshData->PerInstanceSMData[InstanceIndex].Transform)
				{
					bMatch = false;
					break;
				}
			}
		}

		// Restore static lighting if appropriate
		if (bMatch)
		{
			const int32 NumLODLightMaps = InstancedMeshData->CachedStaticLighting.LODDataLightMap.Num();
			SetLODDataCount(NumLODLightMaps, NumLODLightMaps);
			for (int32 i = 0; i < NumLODLightMaps; ++i)
			{
				LODData[i].LightMap = InstancedMeshData->CachedStaticLighting.LODDataLightMap[i];
				LODData[i].ShadowMap = InstancedMeshData->CachedStaticLighting.LODDataShadowMap[i];
			}
			IrrelevantLights = InstancedMeshData->CachedStaticLighting.IrrelevantLights;
			PerInstanceSMData = InstancedMeshData->PerInstanceSMData;
			bHasCachedStaticLighting = true;
		}
	}

	SelectedInstances = InstancedMeshData->SelectedInstances;

	ReleasePerInstanceRenderData();
	MarkRenderStateDirty();
#endif
}

FPrimitiveSceneProxy* UInstancedStaticMeshComponent::CreateSceneProxy()
{
	ProxySize = 0;
	// Verify that the mesh is valid before using it.
	const bool bMeshIsValid = 
		// make sure we have instances
		PerInstanceSMData.Num() > 0 &&
		// make sure we have an actual staticmesh
		StaticMesh &&
		StaticMesh->HasValidRenderData() &&
		// You really can't use hardware instancing on the consoles with multiple elements because they share the same index buffer. 
		// @todo: Level error or something to let LDs know this
		1;//StaticMesh->LODModels(0).Elements.Num() == 1;

	if(bMeshIsValid)
	{
		// If we don't have a random seed for this instanced static mesh component yet, then go ahead and
		// generate one now.  This will be saved with the static mesh component and used for future generation
		// of random numbers for this component's instances. (Used by the PerInstanceRandom material expression)
		while( InstancingRandomSeed == 0 )
		{
			InstancingRandomSeed = FMath::Rand();
		}

		ProxySize = FStaticMeshInstanceData::GetResourceSize(PerInstanceSMData.Num());
		return ::new FInstancedStaticMeshSceneProxy(this, GetWorld()->FeatureLevel);
	}
	else
	{
		return NULL;
	}
}

void UInstancedStaticMeshComponent::InitInstanceBody(int32 InstanceIdx, FBodyInstance* InstanceBodyInstance)
{
	if (!StaticMesh)
	{
		UE_LOG(LogStaticMesh, Warning, TEXT("Unabled to create a body instance for %s in Actor %s. No StaticMesh set."), *GetName(), GetOwner() ? *GetOwner()->GetName() : TEXT("?"));
		return;
	}

	check(InstanceIdx < PerInstanceSMData.Num());
	check(InstanceIdx < InstanceBodies.Num());
	check(InstanceBodyInstance);

	UBodySetup* BodySetup = GetBodySetup();
	check(BodySetup);

	// Get transform of the instance
	FTransform InstanceTransform = FTransform(PerInstanceSMData[InstanceIdx].Transform) * ComponentToWorld;
	
	InstanceBodyInstance->CopyBodyInstancePropertiesFrom(&BodyInstance);
	InstanceBodyInstance->InstanceBodyIndex = InstanceIdx; // Set body index 

	// make sure we never enable bSimulatePhysics for ISMComps
	InstanceBodyInstance->bSimulatePhysics = false;

#if WITH_PHYSX
	// Create physics body instance.
	// Aggregates aren't used for static objects
	auto* Aggregate = (Mobility == EComponentMobility::Movable) ? Aggregates[FMath::DivideAndRoundDown<int32>(InstanceIdx, AggregateMaxSize)] : nullptr;
	InstanceBodyInstance->bAutoWeld = false;	//We don't support this for instanced meshes.
	InstanceBodyInstance->InitBody(BodySetup, InstanceTransform, this, GetWorld()->GetPhysicsScene(), Aggregate);
#endif //WITH_PHYSX
}

void UInstancedStaticMeshComponent::CreateAllInstanceBodies()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_UInstancedStaticMeshComponent_CreateAllInstanceBodies);

	if (UBodySetup* BodySetup = GetBodySetup())
	{
		FPhysScene* PhysScene = GetWorld()->GetPhysicsScene();

	    const int32 NumBodies = PerInstanceSMData.Num();
	    InstanceBodies.SetNumUninitialized(NumBodies);
    
	    TArray<FTransform> Transforms;
	    Transforms.Reserve(NumBodies);
    
	    for (int32 i = 0; i < NumBodies; ++i)
	    {
		    InstanceBodies[i] = new FBodyInstance;
		    FBodyInstance* Instance = InstanceBodies[i];
			const FTransform InstanceTM = FTransform(PerInstanceSMData[i].Transform) * ComponentToWorld;   
		    Instance->CopyBodyInstancePropertiesFrom(&BodyInstance);
		    Instance->InstanceBodyIndex = i; // Set body index 
		    Instance->bAutoWeld = false;
    
		    // make sure we never enable bSimulatePhysics for ISMComps
		    Instance->bSimulatePhysics = false;

			if(Mobility == EComponentMobility::Movable)
			{
				Instance->InitBody(BodySetup, InstanceTM, this, PhysScene);
			}else
			{
				Transforms.Add(InstanceTM);
#if WITH_PHYSX
				Instance->RigidActorSyncId = i + 1;

				if (GetWorld()->GetPhysicsScene()->HasAsyncScene())
				{
					Instance->RigidActorAsyncId = Instance->RigidActorSyncId + NumBodies;
				}
#endif
			}
	    }

		if (NumBodies > 0 && Mobility == EComponentMobility::Static)
		{
			TArray<UBodySetup*> BodySetups;
			TArray<UPhysicalMaterial*> PhysicalMaterials;

			BodySetups.Add(BodySetup);
			TWeakObjectPtr<UPrimitiveComponent> WeakSelfPtr(this);
			FBodyInstance::GetComplexPhysicalMaterials(&BodyInstance, WeakSelfPtr, PhysicalMaterials);
			PhysicalMaterials.Add(FBodyInstance::GetSimplePhysicalMaterial(&BodyInstance, WeakSelfPtr, TWeakObjectPtr<UBodySetup>(BodySetup)));

			PhysicsSerializer->CreatePhysicsData(BodySetups, PhysicalMaterials);
			FBodyInstance::InitStaticBodies(InstanceBodies, Transforms, BodySetup, this, GetWorld()->GetPhysicsScene(), PhysicsSerializer);

			//Serialize physics data for fast path cooking
			PhysicsSerializer->SerializePhysics(InstanceBodies, BodySetups, PhysicalMaterials);
		}
	}
}

void UInstancedStaticMeshComponent::ClearAllInstanceBodies()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_UInstancedStaticMeshComponent_ClearAllInstanceBodies);
	for (int32 i = 0; i < InstanceBodies.Num(); i++)
	{
		check(InstanceBodies[i]);
		InstanceBodies[i]->TermBody();
		delete InstanceBodies[i];
	}

	InstanceBodies.Empty();
}


void UInstancedStaticMeshComponent::CreatePhysicsState()
{
	check(InstanceBodies.Num() == 0);

	FPhysScene* PhysScene = GetWorld()->GetPhysicsScene();

	if (!PhysScene) { return; }

#if WITH_PHYSX
	check(Aggregates.Num() == 0);

	const int32 NumBodies = PerInstanceSMData.Num();

	// Aggregates aren't used for static objects
	const int32 NumAggregates = (Mobility == EComponentMobility::Movable) ? FMath::DivideAndRoundUp<int32>(NumBodies, AggregateMaxSize) : 0;

	// Get the scene type from the main BodyInstance
	const uint32 SceneType = BodyInstance.UseAsyncScene(PhysScene) ? PST_Async : PST_Sync;

	for (int32 i = 0; i < NumAggregates; i++)
	{
		auto* Aggregate = GPhysXSDK->createAggregate(AggregateMaxSize, false);
		Aggregates.Add(Aggregate);
		PxScene* PScene = PhysScene->GetPhysXScene(SceneType);
		SCOPED_SCENE_WRITE_LOCK(PScene);
		PScene->addAggregate(*Aggregate);
	}
#endif

	// Create all the bodies.
	CreateAllInstanceBodies();

	USceneComponent::CreatePhysicsState();
}

void UInstancedStaticMeshComponent::DestroyPhysicsState()
{
	USceneComponent::DestroyPhysicsState();

	// Release all physics representations
	ClearAllInstanceBodies();

#if WITH_PHYSX
	// releasing Aggregates, they shouldn't contain any Bodies now, because they are released above
	for (auto* Aggregate : Aggregates)
	{
		Aggregate->release();
	}
	Aggregates.Empty();
#endif //WITH_PHYSX
}

bool UInstancedStaticMeshComponent::CanEditSimulatePhysics()
{
	// if instancedstaticmeshcomponent, we will never allow it
	return false;
}

FBoxSphereBounds UInstancedStaticMeshComponent::CalcBounds(const FTransform& BoundTransform) const
{
	if(StaticMesh && PerInstanceSMData.Num() > 0)
	{
		FMatrix BoundTransformMatrix = BoundTransform.ToMatrixWithScale();

		FBoxSphereBounds RenderBounds = StaticMesh->GetBounds();
		FBoxSphereBounds NewBounds = RenderBounds.TransformBy(PerInstanceSMData[0].Transform * BoundTransformMatrix);

		for (int32 InstanceIndex = 1; InstanceIndex < PerInstanceSMData.Num(); InstanceIndex++)
		{
			NewBounds = NewBounds + RenderBounds.TransformBy(PerInstanceSMData[InstanceIndex].Transform * BoundTransformMatrix);
		}

		return NewBounds;
	}
	else
	{
		return FBoxSphereBounds(BoundTransform.GetLocation(), FVector::ZeroVector, 0.f);
	}
}

#if WITH_EDITOR
void UInstancedStaticMeshComponent::GetStaticLightingInfo(FStaticLightingPrimitiveInfo& OutPrimitiveInfo, const TArray<ULightComponent*>& InRelevantLights, const FLightingBuildOptions& Options)
{
	if (HasValidSettingsForStaticLighting())
	{
		// create static lighting for LOD 0
		int32 LightMapWidth = 0;
		int32 LightMapHeight = 0;
		GetLightMapResolution(LightMapWidth, LightMapHeight);

		bool bFit = false;
		bool bReduced = false;
		while (1)
		{
			const int32 OneLessThanMaximumSupportedResolution = 1 << (GMaxTextureMipCount - 2);

			const int32 MaxInstancesInMaxSizeLightmap = (OneLessThanMaximumSupportedResolution / LightMapWidth) * ((OneLessThanMaximumSupportedResolution / 2) / LightMapHeight);
			if (PerInstanceSMData.Num() > MaxInstancesInMaxSizeLightmap)
			{
				if (LightMapWidth < 4 || LightMapHeight < 4)
				{
					break;
				}
				LightMapWidth /= 2;
				LightMapHeight /= 2;
				bReduced = true;
			}
			else
			{
				bFit = true;
				break;
			}
		}

		if (!bFit)
		{
			FMessageLog("LightingResults").Message(EMessageSeverity::Error)
				->AddToken(FUObjectToken::Create(this))
				->AddToken(FTextToken::Create(NSLOCTEXT("InstancedStaticMesh", "FailedStaticLightingWarning", "The total lightmap size for this InstancedStaticMeshComponent is too big no matter how much we reduce the per-instance size, the number of mesh instances in this component must be reduced")));
			return;
		}
		if (bReduced)
		{
			FMessageLog("LightingResults").Message(EMessageSeverity::Warning)
				->AddToken(FUObjectToken::Create(this))
				->AddToken(FTextToken::Create(NSLOCTEXT("InstancedStaticMesh", "ReducedStaticLightingWarning", "The total lightmap size for this InstancedStaticMeshComponent was too big and it was automatically reduced. Consider reducing the component's lightmap resolution or number of mesh instances in this component")));
		}

		const int32 LightMapSize = GetWorld()->GetWorldSettings()->PackedLightAndShadowMapTextureSize;
		const int32 MaxInstancesInDefaultSizeLightmap = (LightMapSize / LightMapWidth) * ((LightMapSize / 2) / LightMapHeight);
		if (PerInstanceSMData.Num() > MaxInstancesInDefaultSizeLightmap)
		{
			FMessageLog("LightingResults").Message(EMessageSeverity::Warning)
				->AddToken(FUObjectToken::Create(this))
				->AddToken(FTextToken::Create(NSLOCTEXT("InstancedStaticMesh", "LargeStaticLightingWarning", "The total lightmap size for this InstancedStaticMeshComponent is large, consider reducing the component's lightmap resolution or number of mesh instances in this component")));
		}

		bool bCanLODsShareStaticLighting = StaticMesh->CanLODsShareStaticLighting();

		// TODO: We currently only support one LOD of static lighting in instanced meshes
		// Need to create per-LOD instance data to fix that
		if (!bCanLODsShareStaticLighting)
		{
			UE_LOG(LogStaticMesh, Warning, TEXT("Instanced meshes don't yet support unique static lighting for each LOD, lighting on LOD 1+ may be incorrect"));
			bCanLODsShareStaticLighting = true;
		}

		int32 NumLODs = bCanLODsShareStaticLighting ? 1 : StaticMesh->RenderData->LODResources.Num();

		CachedMappings.Reset(PerInstanceSMData.Num() * NumLODs);
		CachedMappings.AddZeroed(PerInstanceSMData.Num() * NumLODs);

		for (int32 LODIndex = 0; LODIndex < NumLODs; LODIndex++)
		{
			const FStaticMeshLODResources& LODRenderData = StaticMesh->RenderData->LODResources[LODIndex];

			for (int32 InstanceIndex = 0; InstanceIndex < PerInstanceSMData.Num(); InstanceIndex++)
			{
				auto* StaticLightingMesh = new FStaticLightingMesh_InstancedStaticMesh(this, LODIndex, InstanceIndex, InRelevantLights);
				OutPrimitiveInfo.Meshes.Add(StaticLightingMesh);

				auto* InstancedMapping = new FStaticLightingTextureMapping_InstancedStaticMesh(this, LODIndex, InstanceIndex, StaticLightingMesh, LightMapWidth, LightMapHeight, StaticMesh->LightMapCoordinateIndex, true);
				OutPrimitiveInfo.Mappings.Add(InstancedMapping);

				CachedMappings[LODIndex * PerInstanceSMData.Num() + InstanceIndex].Mapping = InstancedMapping;
				NumPendingLightmaps++;
			}

			// Shrink LOD texture lightmaps by half for each LOD level (minimum 4x4 px)
			LightMapWidth  = FMath::Max(LightMapWidth  / 2, 4);
			LightMapHeight = FMath::Max(LightMapHeight / 2, 4);
		}
	}
}

void UInstancedStaticMeshComponent::ApplyLightMapping(FStaticLightingTextureMapping_InstancedStaticMesh* InMapping)
{
	NumPendingLightmaps--;

	if (NumPendingLightmaps == 0)
	{
		// Calculate the range of each coefficient in this light-map and repack the data to have the same scale factor and bias across all instances
		// TODO: Per instance scale?

		// generate the final lightmaps for all the mappings for this component
		TArray<TUniquePtr<FQuantizedLightmapData>> AllQuantizedData;
		for (auto& MappingInfo : CachedMappings)
		{
			FStaticLightingTextureMapping_InstancedStaticMesh* Mapping = MappingInfo.Mapping;
			AllQuantizedData.Add(MoveTemp(Mapping->QuantizedData));
		}

		bool bNeedsShadowMap = false;
		TArray<TMap<ULightComponent*, TUniquePtr<FShadowMapData2D>>> AllShadowMapData;
		for (auto& MappingInfo : CachedMappings)
		{
			FStaticLightingTextureMapping_InstancedStaticMesh* Mapping = MappingInfo.Mapping;
			bNeedsShadowMap = bNeedsShadowMap || (Mapping->ShadowMapData.Num() > 0);
			AllShadowMapData.Add(MoveTemp(Mapping->ShadowMapData));
		}

		// Create a light-map for the primitive.
		const ELightMapPaddingType PaddingType = GAllowLightmapPadding ? LMPT_NormalPadding : LMPT_NoPadding;
		FLightMap2D* NewLightMap = FLightMap2D::AllocateInstancedLightMap(this, MoveTemp(AllQuantizedData), Bounds, PaddingType, LMF_Streamed);

		// Create a shadow-map for the primitive.
		FShadowMap2D* NewShadowMap = bNeedsShadowMap ? FShadowMap2D::AllocateInstancedShadowMap(this, MoveTemp(AllShadowMapData), Bounds, PaddingType, SMF_Streamed) : nullptr;

		// Ensure LODData has enough entries in it, free not required.
		SetLODDataCount(StaticMesh->GetNumLODs(), StaticMesh->GetNumLODs());

		// Share lightmap/shadowmap over all LODs
		for (FStaticMeshComponentLODInfo& ComponentLODInfo : LODData)
		{
			ComponentLODInfo.LightMap = NewLightMap;
			ComponentLODInfo.ShadowMap = NewShadowMap;
		}

		// Build the list of statically irrelevant lights.
		// TODO: This should be stored per LOD.
		TSet<FGuid> RelevantLights;
		TSet<FGuid> PossiblyIrrelevantLights;
		for (auto& MappingInfo : CachedMappings)
		{
			for (const ULightComponent* Light : MappingInfo.Mapping->Mesh->RelevantLights)
			{
				// Check if the light is stored in the light-map.
				const bool bIsInLightMap = LODData[0].LightMap && LODData[0].LightMap->LightGuids.Contains(Light->LightGuid);

				// Check if the light is stored in the shadow-map.
				const bool bIsInShadowMap = LODData[0].ShadowMap && LODData[0].ShadowMap->LightGuids.Contains(Light->LightGuid);

				// If the light isn't already relevant to another mapping, add it to the potentially irrelevant list
				if (!bIsInLightMap && !bIsInShadowMap && !RelevantLights.Contains(Light->LightGuid))
				{
					PossiblyIrrelevantLights.Add(Light->LightGuid);
				}

				// Light is relevant
				if (bIsInLightMap || bIsInShadowMap)
				{
					RelevantLights.Add(Light->LightGuid);
					PossiblyIrrelevantLights.Remove(Light->LightGuid);
				}
			}
		}

		IrrelevantLights = PossiblyIrrelevantLights.Array();

		bHasCachedStaticLighting = true;

		ReleasePerInstanceRenderData();
		MarkRenderStateDirty();

		MarkPackageDirty();
	}
}
#endif

void UInstancedStaticMeshComponent::ReleasePerInstanceRenderData()
{
	if (PerInstanceRenderData.IsValid() && !bPerInstanceRenderDataWasPrebuilt)
	{
		typedef TSharedPtr<FPerInstanceRenderData, ESPMode::ThreadSafe> FPerInstanceRenderDataPtr;

		PerInstanceRenderData->HitProxies.Empty();

		// Make shared pointer object on the heap
		FPerInstanceRenderDataPtr* CleanupRenderDataPtr = new FPerInstanceRenderDataPtr(PerInstanceRenderData);
		PerInstanceRenderData.Reset();

		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			FReleasePerInstanceRenderData,
			FPerInstanceRenderDataPtr*, InCleanupRenderDataPtr, CleanupRenderDataPtr,
		{
			// Destroy the shared pointer object we allocated on the heap.
			// Resource will either be released here or by scene proxy on the render thread, whoever gets executed last
			delete InCleanupRenderDataPtr;
		});
	}
}

void UInstancedStaticMeshComponent::GetLightAndShadowMapMemoryUsage( int32& LightMapMemoryUsage, int32& ShadowMapMemoryUsage ) const
{
	Super::GetLightAndShadowMapMemoryUsage(LightMapMemoryUsage, ShadowMapMemoryUsage);

	int32 NumInstances = PerInstanceSMData.Num();

	// Scale lighting demo by number of instances
	LightMapMemoryUsage *= NumInstances;
	ShadowMapMemoryUsage *= NumInstances;
}


void UInstancedStaticMeshComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	PerInstanceSMData.BulkSerialize(Ar);

#if WITH_EDITOR
	if( Ar.IsTransacting() )
	{
		Ar << SelectedInstances;
	}
#endif
}

int32 UInstancedStaticMeshComponent::AddInstance(const FTransform& InstanceTransform)
{
	int InstanceIdx = PerInstanceSMData.Num();

	FInstancedStaticMeshInstanceData* NewInstanceData = new(PerInstanceSMData) FInstancedStaticMeshInstanceData();
	SetupNewInstanceData(*NewInstanceData, InstanceIdx, InstanceTransform);

#if WITH_EDITOR
	if (SelectedInstances.Num())
	{
		SelectedInstances.Add(false);
	}
#endif

	ReleasePerInstanceRenderData();
	MarkRenderStateDirty();

	PartialNavigationUpdate(InstanceIdx);

	return InstanceIdx;
}

int32 UInstancedStaticMeshComponent::AddInstanceWorldSpace(const FTransform& WorldTransform)
 {
	// Transform from world space to local space
	FTransform RelativeTM = WorldTransform.GetRelativeTransform(ComponentToWorld);
	return AddInstance(RelativeTM);
}

bool UInstancedStaticMeshComponent::RemoveInstance(int32 InstanceIndex)
{
	if (!PerInstanceSMData.IsValidIndex(InstanceIndex))
	{
		return false;
	}

	// Request navigation update
	PartialNavigationUpdate(InstanceIndex);

	// remove instance
	PerInstanceSMData.RemoveAt(InstanceIndex);

#if WITH_EDITOR
	// remove selection flag if array is filled in
	if (SelectedInstances.IsValidIndex(InstanceIndex))
	{
		SelectedInstances.RemoveAt(InstanceIndex);
	}
#endif

	// update the physics state
	if (bPhysicsStateCreated)
	{
		// TODO: it may be possible to instead just update the BodyInstanceIndex for all bodies after the removed instance. 
		ClearAllInstanceBodies();
		CreateAllInstanceBodies();
	}

	// Indicate we need to update render state to reflect changes
	ReleasePerInstanceRenderData();
	MarkRenderStateDirty();

	return true;
}

bool UInstancedStaticMeshComponent::GetInstanceTransform(int32 InstanceIndex, FTransform& OutInstanceTransform, bool bWorldSpace) const
{
	if (!PerInstanceSMData.IsValidIndex(InstanceIndex))
	{
		return false;
	}

	const FInstancedStaticMeshInstanceData& InstanceData = PerInstanceSMData[InstanceIndex];

	OutInstanceTransform = FTransform(InstanceData.Transform);
	if (bWorldSpace)
	{
		OutInstanceTransform = OutInstanceTransform * ComponentToWorld;
	}

	return true;
}

bool UInstancedStaticMeshComponent::UpdateInstanceTransform(int32 InstanceIndex, const FTransform& NewInstanceTransform, bool bWorldSpace, bool bMarkRenderStateDirty)
{
	if (!PerInstanceSMData.IsValidIndex(InstanceIndex))
	{
		return false;
	}

	// Request navigation update
	PartialNavigationUpdate(InstanceIndex);

	FInstancedStaticMeshInstanceData& InstanceData = PerInstanceSMData[InstanceIndex];

	// Render data uses local transform of the instance
	FTransform LocalTransform = bWorldSpace ? NewInstanceTransform.GetRelativeTransform(ComponentToWorld) : NewInstanceTransform;
	InstanceData.Transform = LocalTransform.ToMatrixWithScale();

	if (bPhysicsStateCreated)
	{
		// Physics uses world transform of the instance
		FTransform WorldTransform = bWorldSpace ? NewInstanceTransform : (LocalTransform * ComponentToWorld);
		FBodyInstance* InstanceBodyInstance = InstanceBodies[InstanceIndex];
#if WITH_PHYSX
		// Update transform.
		InstanceBodyInstance->SetBodyTransform(WorldTransform, false);
		InstanceBodyInstance->UpdateBodyScale(WorldTransform.GetScale3D());
#endif //WITH_PHYSX
	}

	// Request navigation update
	PartialNavigationUpdate(InstanceIndex);

	ReleasePerInstanceRenderData();
	if (bMarkRenderStateDirty)
	{
		MarkRenderStateDirty();
	}

	return true;
}

bool UInstancedStaticMeshComponent::ShouldCreatePhysicsState() const
{
	return IsRegistered() && (bAlwaysCreatePhysicsState || IsCollisionEnabled());
}

void UInstancedStaticMeshComponent::ClearInstances()
{
	// Clear all the per-instance data
	PerInstanceSMData.Empty();
	InstanceReorderTable.Empty();
	RemovedInstances.Empty();

	ProxySize = 0;

	// Release any physics representations
	ClearAllInstanceBodies();

	// Indicate we need to update render state to reflect changes
	bPerInstanceRenderDataWasPrebuilt = false;
	ReleasePerInstanceRenderData();
	MarkRenderStateDirty();

	UNavigationSystem::UpdateNavOctree(this);
}

int32 UInstancedStaticMeshComponent::GetInstanceCount() const
{
	return PerInstanceSMData.Num();
}

void UInstancedStaticMeshComponent::SetCullDistances(int32 StartCullDistance, int32 EndCullDistance)
{
	InstanceStartCullDistance = StartCullDistance;
	InstanceEndCullDistance = EndCullDistance;
	MarkRenderStateDirty();
}

void UInstancedStaticMeshComponent::SetupNewInstanceData(FInstancedStaticMeshInstanceData& InOutNewInstanceData, int32 InInstanceIndex, const FTransform& InInstanceTransform)
{
	InOutNewInstanceData.Transform = InInstanceTransform.ToMatrixWithScale();
	InOutNewInstanceData.LightmapUVBias = FVector2D( -1.0f, -1.0f );
	InOutNewInstanceData.ShadowmapUVBias = FVector2D( -1.0f, -1.0f );

	if (bPhysicsStateCreated)
	{
		// Add another aggregate if needed
		// Aggregates aren't used for static objects
		if (Mobility == EComponentMobility::Movable)
		{
			const int32 AggregateIndex = FMath::DivideAndRoundDown<int32>(InInstanceIndex, AggregateMaxSize);
			if (AggregateIndex >= Aggregates.Num())
			{
				// Get the scene type from the main BodyInstance
				FPhysScene* PhysScene = GetWorld()->GetPhysicsScene();
				check(PhysScene);

				const uint32 SceneType = BodyInstance.UseAsyncScene(PhysScene) ? PST_Async : PST_Sync;

				auto* Aggregate = GPhysXSDK->createAggregate(AggregateMaxSize, false);
				const int32 AddedIndex = Aggregates.Add(Aggregate);
				check(AddedIndex == AggregateIndex);
				PhysScene->GetPhysXScene(SceneType)->addAggregate(*Aggregate);
			}
		}

		FBodyInstance* NewBodyInstance = new FBodyInstance();
		int32 BodyIndex = InstanceBodies.Insert(NewBodyInstance, InInstanceIndex);
		check(InInstanceIndex == BodyIndex);
		InitInstanceBody(BodyIndex, NewBodyInstance);
	}
}

void UInstancedStaticMeshComponent::PartialNavigationUpdate(int32 InstanceIdx)
{
	// Just update everything
	UNavigationSystem::UpdateNavOctree(this);
}

bool UInstancedStaticMeshComponent::DoCustomNavigableGeometryExport(FNavigableGeometryExport& GeomExport) const
{
	if (StaticMesh && StaticMesh->NavCollision)
	{
		UNavCollision* NavCollision = StaticMesh->NavCollision;
		if (StaticMesh->NavCollision->bIsDynamicObstacle)
		{
			return false;
		}
		
		if (NavCollision->bHasConvexGeometry)
		{
			GeomExport.ExportCustomMesh(NavCollision->ConvexCollision.VertexBuffer.GetData(), NavCollision->ConvexCollision.VertexBuffer.Num(),
				NavCollision->ConvexCollision.IndexBuffer.GetData(), NavCollision->ConvexCollision.IndexBuffer.Num(), FTransform::Identity);

			GeomExport.ExportCustomMesh(NavCollision->TriMeshCollision.VertexBuffer.GetData(), NavCollision->TriMeshCollision.VertexBuffer.Num(),
				NavCollision->TriMeshCollision.IndexBuffer.GetData(), NavCollision->TriMeshCollision.IndexBuffer.Num(), FTransform::Identity);
		}
		else
		{
			UBodySetup* BodySetup = StaticMesh->BodySetup;
			if (BodySetup)
			{
				GeomExport.ExportRigidBodySetup(*BodySetup, FTransform::Identity);
			}
		}

		// Hook per instance transform delegate
		GeomExport.SetNavDataPerInstanceTransformDelegate(FNavDataPerInstanceTransformDelegate::CreateUObject(this, &UInstancedStaticMeshComponent::GetNavigationPerInstanceTransforms));
	}

	// we don't want "regular" collision export for this component
	return false;
}

void UInstancedStaticMeshComponent::GetNavigationData(FNavigationRelevantData& Data) const
{
	if (StaticMesh && StaticMesh->NavCollision)	
	{
		UNavCollision* NavCollision = StaticMesh->NavCollision;
		if (NavCollision->bIsDynamicObstacle)
		{
			NavCollision->GetNavigationModifier(Data.Modifiers, FTransform::Identity);

			// Hook per instance transform delegate
			Data.NavDataPerInstanceTransformDelegate = FNavDataPerInstanceTransformDelegate::CreateUObject(this, &UInstancedStaticMeshComponent::GetNavigationPerInstanceTransforms);
		}
	}
}

void UInstancedStaticMeshComponent::GetNavigationPerInstanceTransforms(const FBox& AreaBox, TArray<FTransform>& InstanceData) const
{
	for (const auto& InstancedData : PerInstanceSMData)
	{
		//TODO: Is it worth doing per instance bounds check here ?
		const FTransform InstanceToComponent(InstancedData.Transform);
		if (!InstanceToComponent.GetScale3D().IsZero())
		{
			InstanceData.Add(InstanceToComponent*ComponentToWorld);
		}
	}
}

SIZE_T UInstancedStaticMeshComponent::GetResourceSize( EResourceSizeMode::Type Mode )
{
	SIZE_T ResSize = Super::GetResourceSize(Mode);

	// proxy stuff
	ResSize += ProxySize; 

	// component stuff
	ResSize += InstanceBodies.GetAllocatedSize();
	for (int32 i=0; i < InstanceBodies.Num(); ++i)
	{
		if (InstanceBodies[i] != NULL && InstanceBodies[i]->IsValidBodyInstance())
		{
			ResSize += InstanceBodies[i]->GetBodyInstanceResourceSize(Mode);
		}
	}
	ResSize += InstanceReorderTable.GetAllocatedSize();
	ResSize += RemovedInstances.GetAllocatedSize();

#if WITH_EDITOR
	ResSize += SelectedInstances.GetAllocatedSize();
#endif

#if WITH_PHYSX
	ResSize += Aggregates.GetAllocatedSize();
#endif

	return ResSize;
}

void UInstancedStaticMeshComponent::BeginDestroy()
{
	bPerInstanceRenderDataWasPrebuilt = false;
	Super::BeginDestroy();
	ReleasePerInstanceRenderData();
}

#if WITH_EDITOR
void UInstancedStaticMeshComponent::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	if(PropertyChangedEvent.Property != NULL && PropertyChangedEvent.Property->GetFName() == "PerInstanceSMData")
	{
		if(PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayAdd)
		{
			int32 AddedAtIndex = PropertyChangedEvent.GetArrayIndex(PropertyChangedEvent.Property->GetFName().ToString());
			check(AddedAtIndex != INDEX_NONE);
			SetupNewInstanceData(PerInstanceSMData[AddedAtIndex], AddedAtIndex, FTransform::Identity);

			// added via the property editor, so we will want to interactively work with instances
			bHasPerInstanceHitProxies = true;
		}

		ReleasePerInstanceRenderData();
		MarkRenderStateDirty();
	}

	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}

void UInstancedStaticMeshComponent::PostEditUndo()
{
	Super::PostEditUndo();

	UNavigationSystem::UpdateNavOctree(this);
}
#endif

bool UInstancedStaticMeshComponent::IsInstanceSelected(int32 InInstanceIndex) const
{
#if WITH_EDITOR
	if(SelectedInstances.IsValidIndex(InInstanceIndex))
	{
		return SelectedInstances[InInstanceIndex];
	}
#endif

	return false;
}

void UInstancedStaticMeshComponent::SelectInstance(bool bInSelected, int32 InInstanceIndex, int32 InInstanceCount)
{
#if WITH_EDITOR
	if(PerInstanceSMData.Num() != SelectedInstances.Num())
	{
		SelectedInstances.Init(false, PerInstanceSMData.Num());
	}

	check(SelectedInstances.IsValidIndex(InInstanceIndex));
	check(SelectedInstances.IsValidIndex(InInstanceIndex + (InInstanceCount - 1)));

	for (int32 InstanceIndex = InInstanceIndex; InstanceIndex < InInstanceIndex + InInstanceCount; InstanceIndex++)
	{
		SelectedInstances[InstanceIndex] = bInSelected;
	}

	SelectionStamp++;
#endif
}

void UInstancedStaticMeshComponent::ClearInstanceSelection()
{
#if WITH_EDITOR	
	SelectedInstances.Empty();
	SelectionStamp++;
#endif
}

static TAutoConsoleVariable<int32> CVarCullAllInVertexShader(
	TEXT("foliage.CullAllInVertexShader"),
	0,
	TEXT("Debugging, if this is greater than 0, cull all instances in the vertex shader."));

void FInstancedStaticMeshVertexFactoryShaderParameters::SetMesh( FRHICommandList& RHICmdList, FShader* VertexShader,const class FVertexFactory* VertexFactory,const class FSceneView& View,const struct FMeshBatchElement& BatchElement,uint32 DataFlags ) const 
{
	FLocalVertexFactoryShaderParameters::SetMesh(RHICmdList, VertexShader, VertexFactory, View, BatchElement, DataFlags);

	FRHIVertexShader* VS = VertexShader->GetVertexShader();
	const FInstancingUserData* InstancingUserData = (const FInstancingUserData*)BatchElement.UserData;
	if( InstancingWorldViewOriginOneParameter.IsBound() )
	{
		FVector4 InstancingViewZCompareZero(MIN_flt, MIN_flt, MAX_flt, 1.0f);
		FVector4 InstancingViewZCompareOne(MIN_flt, MIN_flt, MAX_flt, 0.0f);
		FVector4 InstancingViewZConstant(ForceInit);
		FVector4 InstancingWorldViewOriginZero(ForceInit);
		FVector4 InstancingWorldViewOriginOne(ForceInit);
		InstancingWorldViewOriginOne.W = 1.0f;
		if (InstancingUserData && BatchElement.InstancedLODRange)
		{
			int32 FirstLOD = InstancingUserData->MinLOD;

			int32 DebugMin = FMath::Min(CVarMinLOD.GetValueOnRenderThread(), InstancingUserData->MeshRenderData->LODResources.Num() - 1);
			if (DebugMin >= 0)
			{
				FirstLOD = FMath::Max(FirstLOD, DebugMin);
			}


			float SphereRadius = InstancingUserData->MeshRenderData->Bounds.SphereRadius;
			float MinSize = View.ViewMatrices.IsPerspectiveProjection() ? CVarFoliageMinimumScreenSize.GetValueOnRenderThread() : 0.0f;
			float LODScale = CVarFoliageLODDistanceScale.GetValueOnRenderThread();
			float LODRandom = CVarRandomLODRange.GetValueOnRenderThread();
			float MaxDrawDistanceScale = GetCachedScalabilityCVars().ViewDistanceScale;

			if (BatchElement.InstancedLODIndex)
			{
				InstancingViewZConstant.X = -1.0f;
			}
			else
			{
				// this is the first LOD, so we don't have a fade-in region
				InstancingViewZConstant.X = 0.0f;
			}
			InstancingViewZConstant.Y = 0.0f;
			InstancingViewZConstant.Z = 1.0f;

			// now we subtract off the lower segments, since they will be incorporated 
			InstancingViewZConstant.Y -= InstancingViewZConstant.X;
			InstancingViewZConstant.Z -= InstancingViewZConstant.X + InstancingViewZConstant.Y;
			//not using W InstancingViewZConstant.W -= InstancingViewZConstant.X + InstancingViewZConstant.Y + InstancingViewZConstant.Z;

			for (int32 SampleIndex = 0; SampleIndex < 2; SampleIndex++)
			{
				FVector4& InstancingViewZCompare(SampleIndex ? InstancingViewZCompareOne : InstancingViewZCompareZero);
				float Fac = View.GetTemporalLODDistanceFactor(SampleIndex) * SphereRadius * SphereRadius * LODScale * LODScale;

				float FinalCull = MAX_flt;
				if (MinSize > 0.0)
				{
					FinalCull = FMath::Sqrt(Fac / MinSize);
				}
				if (InstancingUserData->EndCullDistance > 0.0f && InstancingUserData->EndCullDistance < FinalCull)
				{
					FinalCull = InstancingUserData->EndCullDistance;
				}
				FinalCull *= MaxDrawDistanceScale;

				InstancingViewZCompare.Z = FinalCull;
				if (BatchElement.InstancedLODIndex < InstancingUserData->MeshRenderData->LODResources.Num() - 1)
				{
					float NextCut = InstancingUserData->MeshRenderData->ScreenSize[BatchElement.InstancedLODIndex + 1];
					InstancingViewZCompare.Z = FMath::Min(FMath::Sqrt(Fac / NextCut), FinalCull);
				}

				InstancingViewZCompare.X = MIN_flt;
				if (BatchElement.InstancedLODIndex > FirstLOD)
				{
					float CurCut = FMath::Sqrt(Fac / InstancingUserData->MeshRenderData->ScreenSize[BatchElement.InstancedLODIndex]);
					if (CurCut < FinalCull)
					{
						InstancingViewZCompare.Y = CurCut;

					}
					else
					{
						// this LOD is completely removed by one of the other two factors
						InstancingViewZCompare.Y = MIN_flt;
						InstancingViewZCompare.Z = MIN_flt;
					}
				}
				else
				{
					// this is the first LOD, so we don't have a fade-in region
					InstancingViewZCompare.Y = MIN_flt;
				}
			}


			InstancingWorldViewOriginZero = View.GetTemporalLODOrigin(0);
			InstancingWorldViewOriginOne = View.GetTemporalLODOrigin(1);

			float Alpha = View.GetTemporalLODTransition();
			InstancingWorldViewOriginZero.W = 1.0f - Alpha;
			InstancingWorldViewOriginOne.W = Alpha;

			InstancingViewZCompareZero.W = LODRandom;
		}
		SetShaderValue(RHICmdList, VS, InstancingViewZCompareZeroParameter, InstancingViewZCompareZero );
		SetShaderValue(RHICmdList, VS, InstancingViewZCompareOneParameter, InstancingViewZCompareOne );
		SetShaderValue(RHICmdList, VS, InstancingViewZConstantParameter, InstancingViewZConstant );
		SetShaderValue(RHICmdList, VS, InstancingWorldViewOriginZeroParameter, InstancingWorldViewOriginZero );
		SetShaderValue(RHICmdList, VS, InstancingWorldViewOriginOneParameter, InstancingWorldViewOriginOne );
	}
	if( InstancingFadeOutParamsParameter.IsBound() )
	{
		FVector4 InstancingFadeOutParams(MAX_flt,0.f,1.f,1.f);
		if (InstancingUserData)
		{
			InstancingFadeOutParams.X = InstancingUserData->StartCullDistance;
			if( InstancingUserData->EndCullDistance > 0 )
			{
				if( InstancingUserData->EndCullDistance > InstancingUserData->StartCullDistance )
				{
					InstancingFadeOutParams.Y = 1.f / (float)(InstancingUserData->EndCullDistance - InstancingUserData->StartCullDistance);
				}
				else
				{
					InstancingFadeOutParams.Y = 1.f;
				}
			}
			else
			{
				InstancingFadeOutParams.Y = 0.f;
			}
			if (CVarCullAllInVertexShader.GetValueOnRenderThread() > 0)
			{
				InstancingFadeOutParams.Z = 0.0f;
				InstancingFadeOutParams.W = 0.0f;
			}
			else
			{
			InstancingFadeOutParams.Z = InstancingUserData->bRenderSelected ? 1.f : 0.f;
			InstancingFadeOutParams.W = InstancingUserData->bRenderUnselected ? 1.f : 0.f;
		}
		}
		SetShaderValue(RHICmdList, VS, InstancingFadeOutParamsParameter, InstancingFadeOutParams );
	}

	auto ShaderPlatform = GetFeatureLevelShaderPlatform(View.GetFeatureLevel());
	const bool bInstanced = RHISupportsInstancing(ShaderPlatform);
	if (!bInstanced)
	{
		if (CPUInstanceOrigin.IsBound())
		{
			const float ShortScale = 1.0f / 32767.0f;
			auto* InstancingData = (const FInstancingUserData*)BatchElement.UserData;
			const FInstanceStream* InstanceStream = InstancingData->RenderData->PerInstanceRenderData->InstanceBuffer.GetData() + BatchElement.UserIndex;

			FVector4 InstanceLightmapAndShadowMapUVBias(
				float(InstanceStream->InstanceLightmapAndShadowMapUVBias[0]) * ShortScale,
				float(InstanceStream->InstanceLightmapAndShadowMapUVBias[1]) * ShortScale,
				float(InstanceStream->InstanceLightmapAndShadowMapUVBias[2]) * ShortScale,
				float(InstanceStream->InstanceLightmapAndShadowMapUVBias[3]) * ShortScale
				);

			FVector4 InstanceTransform[3] = 
			{
				FVector4(
					InstanceStream->InstanceTransform1[0].GetFloat(),
					InstanceStream->InstanceTransform1[1].GetFloat(),
					InstanceStream->InstanceTransform1[2].GetFloat(),
					InstanceStream->InstanceTransform1[3].GetFloat()),
				FVector4(
					InstanceStream->InstanceTransform2[0].GetFloat(),
					InstanceStream->InstanceTransform2[1].GetFloat(),
					InstanceStream->InstanceTransform2[2].GetFloat(),
					InstanceStream->InstanceTransform2[3].GetFloat()),
				FVector4(
					InstanceStream->InstanceTransform3[0].GetFloat(),
					InstanceStream->InstanceTransform3[1].GetFloat(),
					InstanceStream->InstanceTransform3[2].GetFloat(),
					InstanceStream->InstanceTransform3[3].GetFloat())
			};

			SetShaderValue(RHICmdList, VS, CPUInstanceOrigin, InstanceStream->InstanceOrigin);
			SetShaderValueArray(RHICmdList, VS, CPUInstanceTransform, InstanceTransform, 3);
			SetShaderValue(RHICmdList, VS, CPUInstanceLightmapAndShadowMapBias, InstanceLightmapAndShadowMapUVBias); 
		}
	}
}
