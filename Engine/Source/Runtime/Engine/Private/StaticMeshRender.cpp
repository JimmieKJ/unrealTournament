// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	StaticMeshRender.cpp: Static mesh rendering code.
=============================================================================*/

#include "EnginePrivate.h"
#include "StaticMeshResources.h"


#include "LevelUtils.h"
#include "TessellationRendering.h"
#include "LightMap.h"
#include "ShadowMap.h"
#include "DistanceFieldAtlas.h"
#include "ComponentReregisterContext.h"
#include "Components/BrushComponent.h"
#include "AI/Navigation/NavCollision.h"
#include "ComponentRecreateRenderStateContext.h"

/** If true, optimized depth-only index buffers are used for shadow rendering. */
static bool GUseShadowIndexBuffer = true;

/** If true, reversed index buffer are used for mesh with negative transform determinants. */
static bool GUseReversedIndexBuffer = true;

static void ToggleShadowIndexBuffers()
{
	FlushRenderingCommands();
	GUseShadowIndexBuffer = !GUseShadowIndexBuffer;
	UE_LOG(LogStaticMesh,Log,TEXT("Optimized shadow index buffers %s"),GUseShadowIndexBuffer ? TEXT("ENABLED") : TEXT("DISABLED"));
	FGlobalComponentReregisterContext ReregisterContext;
}

static void ToggleReversedIndexBuffers()
{
	FlushRenderingCommands();
	GUseReversedIndexBuffer = !GUseReversedIndexBuffer;
	UE_LOG(LogStaticMesh,Log,TEXT("Reversed index buffers %s"),GUseReversedIndexBuffer ? TEXT("ENABLED") : TEXT("DISABLED"));
	FGlobalComponentReregisterContext ReregisterContext;
}

static FAutoConsoleCommand GToggleShadowIndexBuffersCmd(
	TEXT("ToggleShadowIndexBuffers"),
	TEXT("Render static meshes with an optimized shadow index buffer that minimizes unique vertices."),
	FConsoleCommandDelegate::CreateStatic(ToggleShadowIndexBuffers)
	);

static bool GUsePreCulledIndexBuffer = true;

void TogglePreCulledIndexBuffers( UWorld* InWorld )
{
	FGlobalComponentRecreateRenderStateContext Context;
	FlushRenderingCommands();
	GUsePreCulledIndexBuffer = !GUsePreCulledIndexBuffer;
}

FAutoConsoleCommandWithWorld GToggleUsePreCulledIndexBuffersCmd(
	TEXT("r.TogglePreCulledIndexBuffers"),
	TEXT("Toggles use of preculled index buffers from the command 'PreCullIndexBuffers'"),
	FConsoleCommandWithWorldDelegate::CreateStatic(TogglePreCulledIndexBuffers));

static FAutoConsoleCommand GToggleReversedIndexBuffersCmd(
	TEXT("ToggleReversedIndexBuffers"),
	TEXT("Render static meshes with negative transform determinants using a reversed index buffer."),
	FConsoleCommandDelegate::CreateStatic(ToggleReversedIndexBuffers)
	);

bool GForceDefaultMaterial = false;

static void ToggleForceDefaultMaterial()
{
	FlushRenderingCommands();
	GForceDefaultMaterial = !GForceDefaultMaterial;
	UE_LOG(LogStaticMesh,Log,TEXT("Force default material %s"),GForceDefaultMaterial ? TEXT("ENABLED") : TEXT("DISABLED"));
	FGlobalComponentReregisterContext ReregisterContext;
}

static FAutoConsoleCommand GToggleForceDefaultMaterialCmd(
	TEXT("ToggleForceDefaultMaterial"),
	TEXT("Render all meshes with the default material."),
	FConsoleCommandDelegate::CreateStatic(ToggleForceDefaultMaterial)
	);

/** Initialization constructor. */
FStaticMeshSceneProxy::FStaticMeshSceneProxy(UStaticMeshComponent* InComponent):
	FPrimitiveSceneProxy(InComponent, InComponent->StaticMesh->GetFName())
	, Owner(InComponent->GetOwner())
	, StaticMesh(InComponent->StaticMesh)
	, BodySetup(InComponent->GetBodySetup())
	, RenderData(InComponent->StaticMesh->RenderData)
	, ForcedLodModel(InComponent->ForcedLodModel)
	, bCastShadow(InComponent->CastShadow)
	, CollisionTraceFlag(ECollisionTraceFlag::CTF_UseDefault)
	, MaterialRelevance(InComponent->GetMaterialRelevance(GetScene().GetFeatureLevel()))
	, CollisionResponse(InComponent->GetCollisionResponseToChannels())
#if WITH_EDITORONLY_DATA
	, SectionIndexPreview(InComponent->SectionIndexPreview)
#endif
{
	check(RenderData);

	const int32 EffectiveMinLOD = InComponent->bOverrideMinLOD ? InComponent->MinLOD : InComponent->StaticMesh->MinLOD;
	ClampedMinLOD = FMath::Clamp(EffectiveMinLOD, 0, RenderData->LODResources.Num() - 1);

	WireframeColor = InComponent->GetWireframeColor();
	LevelColor = FLinearColor(1,1,1);
	PropertyColor = FLinearColor(1,1,1);
	bSupportsDistanceFieldRepresentation = true;

	const auto FeatureLevel = GetScene().GetFeatureLevel();

	// Copy the pointer to the volume data, async building of the data may modify the one on FStaticMeshLODResources while we are rendering
	DistanceFieldData = RenderData->LODResources[0].DistanceFieldData;

	if (GForceDefaultMaterial)
	{
		MaterialRelevance |= UMaterial::GetDefaultMaterial(MD_Surface)->GetRelevance(FeatureLevel);
	}

	// Build the proxy's LOD data.
	bool bAnySectionCastsShadows = false;
	LODs.Empty(RenderData->LODResources.Num());
	for(int32 LODIndex = 0;LODIndex < RenderData->LODResources.Num();LODIndex++)
	{
		FLODInfo* NewLODInfo = new(LODs) FLODInfo(InComponent,LODIndex);

		// Under certain error conditions an LOD's material will be set to 
		// DefaultMaterial. Ensure our material view relevance is set properly.
		const int32 NumSections = NewLODInfo->Sections.Num();
		for (int32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
		{
			const FLODInfo::FSectionInfo& SectionInfo = NewLODInfo->Sections[SectionIndex];
			bAnySectionCastsShadows |= RenderData->LODResources[LODIndex].Sections[SectionIndex].bCastShadow;
			if (SectionInfo.Material == UMaterial::GetDefaultMaterial(MD_Surface))
			{
				MaterialRelevance |= UMaterial::GetDefaultMaterial(MD_Surface)->GetRelevance(FeatureLevel);
			}
		}
	}

	// Disable shadow casting if no section has it enabled.
	bCastShadow = bCastShadow && bAnySectionCastsShadows;
	bCastDynamicShadow = bCastDynamicShadow && bCastShadow;

	bStaticElementsAlwaysUseProxyPrimitiveUniformBuffer = true;

	LpvBiasMultiplier = FMath::Min( InComponent->StaticMesh->LpvBiasMultiplier * InComponent->LpvBiasMultiplier, 3.0f );

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || WITH_EDITOR
	if( GIsEditor )
	{
		// Try to find a color for level coloration.
		if ( Owner )
		{
			ULevel* Level = Owner->GetLevel();
			ULevelStreaming* LevelStreaming = FLevelUtils::FindStreamingLevel( Level );
			if ( LevelStreaming )
			{
				LevelColor = LevelStreaming->LevelColor;
			}
		}

		// Get a color for property coloration.
		FColor TempPropertyColor;
		if (GEngine->GetPropertyColorationColor( (UObject*)InComponent, TempPropertyColor ))
		{
			PropertyColor = TempPropertyColor;
		}
	}
#endif

	if (BodySetup)
	{
		CollisionTraceFlag = BodySetup->GetCollisionTraceFlag();
	}
}

void UStaticMeshComponent::SetLODDataCount( const uint32 MinSize, const uint32 MaxSize )
{
	check(MaxSize <= MAX_STATIC_MESH_LODS);
	if (MaxSize < (uint32)LODData.Num())
	{
		// FStaticMeshComponentLODInfo can't be deleted directly as it has rendering resources
		for (int32 Index = MaxSize; Index < LODData.Num(); Index++)
		{
			LODData[Index].ReleaseOverrideVertexColorsAndBlock();
		}

		// call destructors
		LODData.RemoveAt(MaxSize, LODData.Num() - MaxSize);
	}
	
	if(MinSize > (uint32)LODData.Num())
	{
		// call constructors
		LODData.Reserve(MinSize);

		// TArray doesn't have a function for constructing n items
		uint32 ItemCountToAdd = MinSize - LODData.Num();
		for(uint32 i = 0; i < ItemCountToAdd; ++i)
		{
			// call constructor
			new (LODData)FStaticMeshComponentLODInfo();
		}
	}
}

bool FStaticMeshSceneProxy::GetShadowMeshElement(int32 LODIndex, int32 BatchIndex, uint8 InDepthPriorityGroup, FMeshBatch& OutMeshBatch, bool bDitheredLODTransition) const
{
	const FStaticMeshLODResources& LOD = RenderData->LODResources[LODIndex];
	const FLODInfo& ProxyLODInfo = LODs[LODIndex];

	const bool bUseReversedIndices = GUseReversedIndexBuffer && IsLocalToWorldDeterminantNegative() && LOD.bHasReversedDepthOnlyIndices;

	FMeshBatchElement& OutBatchElement = OutMeshBatch.Elements[0];
	OutMeshBatch.MaterialRenderProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy(false, false);
	OutMeshBatch.VertexFactory = &LOD.VertexFactory;
	OutBatchElement.IndexBuffer = bUseReversedIndices ? &LOD.ReversedDepthOnlyIndexBuffer : &LOD.DepthOnlyIndexBuffer;
	OutMeshBatch.Type = PT_TriangleList;
	OutBatchElement.FirstIndex = 0;
	OutBatchElement.NumPrimitives = LOD.DepthOnlyNumTriangles;
	OutBatchElement.PrimitiveUniformBufferResource = &GetUniformBuffer();
	OutBatchElement.MinVertexIndex = 0;
	OutBatchElement.MaxVertexIndex = LOD.PositionVertexBuffer.GetNumVertices() - 1;
	OutMeshBatch.DepthPriorityGroup = InDepthPriorityGroup;
	OutMeshBatch.ReverseCulling = IsLocalToWorldDeterminantNegative() && !bUseReversedIndices;
	OutMeshBatch.LODIndex = LODIndex;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	OutMeshBatch.VisualizeLODIndex = LODIndex;
#endif
	OutMeshBatch.LCI = &ProxyLODInfo;
	if (ForcedLodModel > 0) 
	{
		OutBatchElement.MaxScreenSize = 0.0f;
		OutBatchElement.MinScreenSize = -1.0f;
	}
	else
	{
		OutMeshBatch.bDitheredLODTransition = bDitheredLODTransition;
		OutBatchElement.MaxScreenSize = GetScreenSize(LODIndex);
		OutBatchElement.MinScreenSize = 0.0f;
		if (LODIndex < MAX_STATIC_MESH_LODS - 1)
		{
			OutBatchElement.MinScreenSize = GetScreenSize(LODIndex + 1);
		}
	}

	// By default this will be a shadow only mesh.
	OutMeshBatch.bUseAsOccluder = false;
	OutMeshBatch.bUseForMaterial = false;

	return true;
}

/** Sets up a FMeshBatch for a specific LOD and element. */
bool FStaticMeshSceneProxy::GetMeshElement(
	int32 LODIndex, 
	int32 BatchIndex, 
	int32 SectionIndex, 
	uint8 InDepthPriorityGroup, 
	bool bUseSelectedMaterial, 
	bool bUseHoveredMaterial, 
	bool bAllowPreCulledIndices, 
	FMeshBatch& OutMeshBatch) const
{
	const FStaticMeshLODResources& LOD = RenderData->LODResources[LODIndex];
	const FStaticMeshSection& Section = LOD.Sections[SectionIndex];

	const FLODInfo& ProxyLODInfo = LODs[LODIndex];
	UMaterialInterface* Material = ProxyLODInfo.Sections[SectionIndex].Material;
	OutMeshBatch.MaterialRenderProxy = Material->GetRenderProxy(bUseSelectedMaterial,bUseHoveredMaterial);
	OutMeshBatch.VertexFactory = &LOD.VertexFactory;

#if WITH_EDITORONLY_DATA
	// If section is hidden, then skip the draw.
	if ((SectionIndexPreview >= 0) && (SectionIndexPreview != SectionIndex))
	{
		return false;
	}
#endif

	const bool bWireframe = false;
	const bool bRequiresAdjacencyInformation = RequiresAdjacencyInformation( Material, OutMeshBatch.VertexFactory->GetType(), GetScene().GetFeatureLevel() );
	
	// Two sided material use bIsFrontFace which is wrong with Reversed Indices. AdjacencyInformation use another index buffer.
	const bool bUseReversedIndices = GUseReversedIndexBuffer && IsLocalToWorldDeterminantNegative() && LOD.bHasReversedIndices && !bWireframe && !bRequiresAdjacencyInformation && !Material->IsTwoSided();

	SetIndexSource(LODIndex, SectionIndex, OutMeshBatch, bWireframe, bRequiresAdjacencyInformation, bUseReversedIndices, bAllowPreCulledIndices);

	FMeshBatchElement& OutBatchElement = OutMeshBatch.Elements[0];

	// Has the mesh component overridden the vertex color stream for this mesh LOD?
	if (ProxyLODInfo.OverrideColorVertexBuffer != NULL)
	{
		// Make sure the indices are accessing data within the vertex buffer's
		check(Section.MaxVertexIndex < ProxyLODInfo.OverrideColorVertexBuffer->GetNumVertices());
		// Switch out the stock mesh vertex factory with the instanced colors one
		OutMeshBatch.VertexFactory = &LOD.VertexFactoryOverrideColorVertexBuffer;
		OutBatchElement.UserData = ProxyLODInfo.OverrideColorVertexBuffer;
		OutBatchElement.bUserDataIsColorVertexBuffer = true;
	}

	if(OutBatchElement.NumPrimitives > 0)
	{
		OutMeshBatch.DynamicVertexData = NULL;
		OutMeshBatch.LCI = &ProxyLODInfo;
		OutBatchElement.PrimitiveUniformBufferResource = &GetUniformBuffer();
		OutBatchElement.MinVertexIndex = Section.MinVertexIndex;
		OutBatchElement.MaxVertexIndex = Section.MaxVertexIndex;
		OutMeshBatch.LODIndex = LODIndex;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		OutMeshBatch.VisualizeLODIndex = LODIndex;
#endif
		OutMeshBatch.UseDynamicData = false;
		OutMeshBatch.ReverseCulling = IsLocalToWorldDeterminantNegative() && !bUseReversedIndices;
		OutMeshBatch.CastShadow = bCastShadow && Section.bCastShadow;
		OutMeshBatch.DepthPriorityGroup = (ESceneDepthPriorityGroup)InDepthPriorityGroup;
		if (ForcedLodModel > 0) 
		{
			OutBatchElement.MaxScreenSize = 0.0f;
			OutBatchElement.MinScreenSize = -1.0f;
		}
		else
		{
			// no support for stateless dithered LOD transitions for movable meshes
			OutMeshBatch.bDitheredLODTransition = !IsMovable() && Material->IsDitheredLODTransition(false);

			OutBatchElement.MaxScreenSize = GetScreenSize(LODIndex);
			OutBatchElement.MinScreenSize = 0.0f;
			if (LODIndex < MAX_STATIC_MESH_LODS - 1)
			{
				OutBatchElement.MinScreenSize = GetScreenSize(LODIndex + 1);
			}
		}
			
		return true;
	}
	else
	{
		return false;
	}
}

/** Sets up a wireframe FMeshBatch for a specific LOD. */
bool FStaticMeshSceneProxy::GetWireframeMeshElement(int32 LODIndex, int32 BatchIndex, const FMaterialRenderProxy* WireframeRenderProxy, uint8 InDepthPriorityGroup, bool bAllowPreCulledIndices, FMeshBatch& OutMeshBatch) const
{
	const FStaticMeshLODResources& LODModel = RenderData->LODResources[LODIndex];
	
	FMeshBatchElement& OutBatchElement = OutMeshBatch.Elements[0];

	OutMeshBatch.VertexFactory = &LODModel.VertexFactory;
	OutMeshBatch.MaterialRenderProxy = WireframeRenderProxy;
	OutBatchElement.PrimitiveUniformBufferResource = &GetUniformBuffer();
	OutBatchElement.MinVertexIndex = 0;
	OutBatchElement.MaxVertexIndex = LODModel.GetNumVertices() - 1;
	OutMeshBatch.ReverseCulling = IsLocalToWorldDeterminantNegative();
	OutMeshBatch.CastShadow = bCastShadow;
	OutMeshBatch.DepthPriorityGroup = (ESceneDepthPriorityGroup)InDepthPriorityGroup;
	if (ForcedLodModel > 0) 
	{
		OutBatchElement.MaxScreenSize = 0.0f;
		OutBatchElement.MinScreenSize = -1.0f;
	}
	else
	{
		OutBatchElement.MaxScreenSize = GetScreenSize(LODIndex);
		OutBatchElement.MinScreenSize = 0.0f;
		if (LODIndex < MAX_STATIC_MESH_LODS - 1)
		{
			OutBatchElement.MinScreenSize = GetScreenSize(LODIndex + 1);
		}
	}

	const bool bWireframe = true;
	const bool bRequiresAdjacencyInformation = false;
	const bool bUseReversedIndices = false;

	SetIndexSource(LODIndex, 0, OutMeshBatch, bWireframe, bRequiresAdjacencyInformation, bUseReversedIndices, bAllowPreCulledIndices);

	return OutBatchElement.NumPrimitives > 0;
}

/**
 * Sets IndexBuffer, FirstIndex and NumPrimitives of OutMeshElement.
 */
void FStaticMeshSceneProxy::SetIndexSource(int32 LODIndex, int32 SectionIndex, FMeshBatch& OutMeshElement, bool bWireframe, bool bRequiresAdjacencyInformation, bool bUseReversedIndices, bool bAllowPreCulledIndices) const
{
	FMeshBatchElement& OutElement = OutMeshElement.Elements[0];
	const FStaticMeshLODResources& LODModel = RenderData->LODResources[LODIndex];
	if (bWireframe)
	{
		if( LODModel.WireframeIndexBuffer.IsInitialized()
			&& !(RHISupportsTessellation(GetScene().GetShaderPlatform()) && OutMeshElement.VertexFactory->GetType()->SupportsTessellationShaders())
			)
		{
			OutMeshElement.Type = PT_LineList;
			OutElement.FirstIndex = 0;
			OutElement.IndexBuffer = &LODModel.WireframeIndexBuffer;
			OutElement.NumPrimitives = LODModel.WireframeIndexBuffer.GetNumIndices() / 2;
		}
		else
		{
			OutMeshElement.Type = PT_TriangleList;

			if (bAllowPreCulledIndices
				&& GUsePreCulledIndexBuffer 
				&& LODs[LODIndex].Sections[SectionIndex].NumPreCulledTriangles >= 0)
			{
				OutElement.IndexBuffer = LODs[LODIndex].PreCulledIndexBuffer;
				OutElement.FirstIndex = 0;
				OutElement.NumPrimitives = LODs[LODIndex].PreCulledIndexBuffer->GetNumIndices() / 3;
			}
			else
			{
				OutElement.FirstIndex = 0;
				OutElement.IndexBuffer = &LODModel.IndexBuffer;
				OutElement.NumPrimitives = LODModel.IndexBuffer.GetNumIndices() / 3;
			}

			OutMeshElement.bWireframe = true;
			OutMeshElement.bDisableBackfaceCulling = true;
		}
	}
	else
	{
		const FStaticMeshSection& Section = LODModel.Sections[SectionIndex];
		OutMeshElement.Type = PT_TriangleList;

		if (bAllowPreCulledIndices
			&& GUsePreCulledIndexBuffer 
			&& LODs[LODIndex].Sections[SectionIndex].NumPreCulledTriangles >= 0)
		{
			OutElement.IndexBuffer = LODs[LODIndex].PreCulledIndexBuffer;
			OutElement.FirstIndex = LODs[LODIndex].Sections[SectionIndex].FirstPreCulledIndex;
			OutElement.NumPrimitives = LODs[LODIndex].Sections[SectionIndex].NumPreCulledTriangles;
		}
		else
		{
			OutElement.IndexBuffer = bUseReversedIndices ? &LODModel.ReversedIndexBuffer : &LODModel.IndexBuffer;
			OutElement.FirstIndex = Section.FirstIndex;
			OutElement.NumPrimitives = Section.NumTriangles;
		}
	}

	if ( bRequiresAdjacencyInformation )
	{
		check( LODModel.bHasAdjacencyInfo );
		OutElement.IndexBuffer = &LODModel.AdjacencyIndexBuffer;
		OutMeshElement.Type = PT_12_ControlPointPatchList;
		OutElement.FirstIndex *= 4;
	}
}

// FPrimitiveSceneProxy interface.
#if WITH_EDITOR
HHitProxy* FStaticMeshSceneProxy::CreateHitProxies(UPrimitiveComponent* Component, TArray<TRefCountPtr<HHitProxy> >& OutHitProxies)
{
	// In order to be able to click on static meshes when they're batched up, we need to have catch all default
	// hit proxy to return.
	HHitProxy* DefaultHitProxy = FPrimitiveSceneProxy::CreateHitProxies(Component, OutHitProxies);

	if ( Component->GetOwner() )
	{
		// Generate separate hit proxies for each sub mesh, so that we can perform hit tests against each section for applying materials
		// to each one.
		for ( int32 LODIndex = 0; LODIndex < RenderData->LODResources.Num(); LODIndex++ )
		{
			const FStaticMeshLODResources& LODModel = RenderData->LODResources[LODIndex];

			check(LODs[LODIndex].Sections.Num() == LODModel.Sections.Num());

			for ( int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++ )
			{
				HHitProxy* ActorHitProxy;

				if ( Component->GetOwner()->IsA(ABrush::StaticClass()) && Component->IsA(UBrushComponent::StaticClass()) )
				{
					ActorHitProxy = new HActor(Component->GetOwner(), Component, HPP_Wireframe, SectionIndex);
				}
				else
				{
					ActorHitProxy = new HActor(Component->GetOwner(), Component, SectionIndex);
				}

				FLODInfo::FSectionInfo& Section = LODs[LODIndex].Sections[SectionIndex];

				// Set the hitproxy.
				check(Section.HitProxy == NULL);
				Section.HitProxy = ActorHitProxy;

				OutHitProxies.Add(ActorHitProxy);
			}
		}
	}
	
	return DefaultHitProxy;
}
#endif // WITH_EDITOR

// use for render thread only
bool UseLightPropagationVolumeRT2(ERHIFeatureLevel::Type InFeatureLevel)
{
	if (InFeatureLevel < ERHIFeatureLevel::SM5)
	{
		return false;
	}

	// todo: better we get the engine LPV state not the cvar (later we want to make it changeable at runtime)
	static const auto* CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.LightPropagationVolume"));
	check(CVar);

	int32 Value = CVar->GetValueOnRenderThread();

	return Value != 0;
}

inline bool AllowShadowOnlyMesh(ERHIFeatureLevel::Type InFeatureLevel)
{
	// todo: later we should refine that (only if occlusion feature in LPV is on, only if inside a cascade, if shadow casting is disabled it should look at bUseEmissiveForDynamicAreaLighting)
	return !UseLightPropagationVolumeRT2(InFeatureLevel);
}

void FStaticMeshSceneProxy::DrawStaticElements(FStaticPrimitiveDrawInterface* PDI)
{
	checkSlow(IsInParallelRenderingThread());
	if (!HasViewDependentDPG())
	{
		// Determine the DPG the primitive should be drawn in.
		uint8 PrimitiveDPG = GetStaticDepthPriorityGroup();
		int32 NumLODs = RenderData->LODResources.Num();
		//Never use the dynamic path in this path, because only unselected elements will use DrawStaticElements
		bool bUseSelectedMaterial = false;
		const bool bUseHoveredMaterial = false;
		const auto FeatureLevel = GetScene().GetFeatureLevel();

		//check if a LOD is being forced
		if (ForcedLodModel > 0) 
		{
			int32 LODIndex = FMath::Clamp(ForcedLodModel, 1, NumLODs) - 1;
			const FStaticMeshLODResources& LODModel = RenderData->LODResources[LODIndex];
			// Draw the static mesh elements.
			for(int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
			{
#if WITH_EDITOR
				if( GIsEditor )
				{
					const FLODInfo::FSectionInfo& Section = LODs[LODIndex].Sections[SectionIndex];

					bUseSelectedMaterial = Section.bSelected;
					PDI->SetHitProxy(Section.HitProxy);
				}
#endif // WITH_EDITOR

				const int32 NumBatches = GetNumMeshBatches();

				for (int32 BatchIndex = 0; BatchIndex < NumBatches; BatchIndex++)
				{
					FMeshBatch MeshBatch;

					if (GetMeshElement(LODIndex, BatchIndex, SectionIndex, PrimitiveDPG, bUseSelectedMaterial, bUseHoveredMaterial, true, MeshBatch))
					{
						PDI->DrawMesh(MeshBatch, FLT_MAX);
					}
				}
			}
		} 
		else //no LOD is being forced, submit them all with appropriate cull distances
		{
			for(int32 LODIndex = ClampedMinLOD; LODIndex < NumLODs; LODIndex++)
			{
				const FStaticMeshLODResources& LODModel = RenderData->LODResources[LODIndex];
				float ScreenSize = GetScreenSize(LODIndex);

				bool bUseUnifiedMeshForShadow = false;
				bool bUseUnifiedMeshForDepth = false;

				if (GUseShadowIndexBuffer && LODModel.bHasDepthOnlyIndices)
				{
					const FLODInfo& ProxyLODInfo = LODs[LODIndex];

					// The shadow-only mesh can be used only if all elements cast shadows and use opaque materials with no vertex modification.
					// In some cases (e.g. LPV) we don't want the optimization
					bool bSafeToUseUnifiedMesh = AllowShadowOnlyMesh(FeatureLevel);

					bool bAnySectionUsesDitheredLODTransition = false;
					bool bAllSectionsUseDitheredLODTransition = true;
					bool bIsMovable = IsMovable();
					bool bAllSectionsCastShadow = bCastShadow;

					for (int32 SectionIndex = 0; bSafeToUseUnifiedMesh && SectionIndex < LODModel.Sections.Num(); SectionIndex++)
					{
						const FMaterial* Material = ProxyLODInfo.Sections[SectionIndex].Material->GetRenderProxy(false)->GetMaterial(FeatureLevel);
						// no support for stateless dithered LOD transitions for movable meshes
						bAnySectionUsesDitheredLODTransition = bAnySectionUsesDitheredLODTransition || (!bIsMovable && Material->IsDitheredLODTransition());
						bAllSectionsUseDitheredLODTransition = bAllSectionsUseDitheredLODTransition && (!bIsMovable && Material->IsDitheredLODTransition());
						const FStaticMeshSection& Section = LODModel.Sections[SectionIndex];

						bSafeToUseUnifiedMesh =
							!(bAnySectionUsesDitheredLODTransition && !bAllSectionsUseDitheredLODTransition) // can't use a single section if they are not homogeneous
							&& Material->WritesEveryPixel()
							&& !Material->IsTwoSided()
							&& !IsTranslucentBlendMode(Material->GetBlendMode())
							&& !Material->MaterialModifiesMeshPosition_RenderThread();

						bAllSectionsCastShadow &= Section.bCastShadow;
					}

					if (bSafeToUseUnifiedMesh)
					{
						bUseUnifiedMeshForShadow = bAllSectionsCastShadow;

						// Depth pass is only used for deferred renderer. The other conditions are meant to match the logic in FStaticMesh::AddToDrawLists.
						// Could not link to "GEarlyZPassMovable" so moveable are ignored.
						bUseUnifiedMeshForDepth = ShouldUseAsOccluder() && GetScene().ShouldUseDeferredRenderer() && !IsMovable();

						if (bUseUnifiedMeshForShadow || bUseUnifiedMeshForDepth)
						{
							const int32 NumBatches = GetNumMeshBatches();

							for (int32 BatchIndex = 0; BatchIndex < NumBatches; BatchIndex++)
							{
								FMeshBatch MeshBatch;

								if (GetShadowMeshElement(LODIndex, BatchIndex, PrimitiveDPG, MeshBatch, bAllSectionsUseDitheredLODTransition))
								{
									bUseUnifiedMeshForShadow = bAllSectionsCastShadow;

									MeshBatch.CastShadow = bUseUnifiedMeshForShadow;
									MeshBatch.bUseAsOccluder = bUseUnifiedMeshForDepth;
									MeshBatch.bUseForMaterial = false;

									PDI->DrawMesh(MeshBatch, ScreenSize);
								}
							}
						}
					}
				}

				// Draw the static mesh elements.
				for(int32 SectionIndex = 0;SectionIndex < LODModel.Sections.Num();SectionIndex++)
				{
#if WITH_EDITOR
					if( GIsEditor )
					{
						const FLODInfo::FSectionInfo& Section = LODs[LODIndex].Sections[SectionIndex];

						bUseSelectedMaterial = Section.bSelected;
						PDI->SetHitProxy(Section.HitProxy);
					}
#endif // WITH_EDITOR

					const int32 NumBatches = GetNumMeshBatches();

					for (int32 BatchIndex = 0; BatchIndex < NumBatches; BatchIndex++)
					{
						FMeshBatch MeshBatch;

						if (GetMeshElement(LODIndex, BatchIndex, SectionIndex, PrimitiveDPG, bUseSelectedMaterial, bUseHoveredMaterial, true, MeshBatch))
						{
							// If we have submitted an optimized shadow-only mesh, remaining mesh elements must not cast shadows.
							MeshBatch.CastShadow &= !bUseUnifiedMeshForShadow;
							MeshBatch.bUseAsOccluder &= !bUseUnifiedMeshForDepth;

							PDI->DrawMesh(MeshBatch, ScreenSize);
						}
					}
				}
			}
		}
	}
}

bool FStaticMeshSceneProxy::IsCollisionView(const FEngineShowFlags& EngineShowFlags, bool& bDrawSimpleCollision, bool& bDrawComplexCollision) const
{
	bDrawSimpleCollision = bDrawComplexCollision = false;

	const bool bInCollisionView = EngineShowFlags.CollisionVisibility || EngineShowFlags.CollisionPawn;
	// If in a 'collision view' and collision is enabled
	if (bInCollisionView && IsCollisionEnabled())
	{
		// See if we have a response to the interested channel
		bool bHasResponse = EngineShowFlags.CollisionPawn && CollisionResponse.GetResponse(ECC_Pawn) != ECR_Ignore;
		bHasResponse |= EngineShowFlags.CollisionVisibility && CollisionResponse.GetResponse(ECC_Visibility) != ECR_Ignore;

		if(bHasResponse)
		{
			const ECollisionTraceFlag TraceFlag = BodySetup ? BodySetup->GetCollisionTraceFlag().GetValue() : ECollisionTraceFlag::CTF_UseDefault;
			
			//Visiblity uses complex and pawn uses simple. However, if UseSimpleAsComplex or UseComplexAsSimple is used we need to adjust accordingly
			bDrawComplexCollision = (EngineShowFlags.CollisionVisibility && TraceFlag != ECollisionTraceFlag::CTF_UseSimpleAsComplex) || (EngineShowFlags.CollisionPawn && TraceFlag == ECollisionTraceFlag::CTF_UseComplexAsSimple);
			bDrawSimpleCollision  = (EngineShowFlags.CollisionPawn && TraceFlag != ECollisionTraceFlag::CTF_UseComplexAsSimple) || (EngineShowFlags.CollisionVisibility && TraceFlag == ECollisionTraceFlag::CTF_UseSimpleAsComplex);
		}
	}

	return bInCollisionView;
}

void FStaticMeshSceneProxy::GetMeshDescription(int32 LODIndex, TArray<FMeshBatch>& OutMeshElements) const
{
	const FStaticMeshLODResources& LODModel = RenderData->LODResources[LODIndex];
	const FLODInfo& ProxyLODInfo = LODs[LODIndex];

	for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
	{
		const int32 NumBatches = GetNumMeshBatches();

		for (int32 BatchIndex = 0; BatchIndex < NumBatches; BatchIndex++)
		{
			FMeshBatch MeshElement; 

			if (GetMeshElement(LODIndex, BatchIndex, SectionIndex, SDPG_World, false, false, false, MeshElement))
			{
				OutMeshElements.Add(MeshElement);
			}
		}
	}
}

void FStaticMeshSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_StaticMeshSceneProxy_GetMeshElements);
	checkSlow(IsInRenderingThread());

	const bool bIsLightmapSettingError = HasStaticLighting() && !HasValidSettingsForStaticLighting();
	const bool bProxyIsSelected = IsSelected();
	const FEngineShowFlags& EngineShowFlags = ViewFamily.EngineShowFlags;

	bool bDrawSimpleCollision = false, bDrawComplexCollision = false;
	const bool bInCollisionView = IsCollisionView(EngineShowFlags, bDrawSimpleCollision, bDrawComplexCollision);
	
	// Should we draw the mesh wireframe to indicate we are using the mesh as collision
	const bool bDrawComplexWireframeCollision = (EngineShowFlags.Collision && IsCollisionEnabled() && CollisionTraceFlag == ECollisionTraceFlag::CTF_UseComplexAsSimple);

	const bool bDrawMesh = (bInCollisionView) ? (bDrawComplexCollision) : 
	(	IsRichView(ViewFamily) || HasViewDependentDPG()
		|| EngineShowFlags.Collision
		|| EngineShowFlags.Bounds
		|| bProxyIsSelected 
		|| IsHovered()
		|| bIsLightmapSettingError 
		|| !IsStaticPathAvailable() );

	// Draw polygon mesh if we are either not in a collision view, or are drawing it as collision.
	if (EngineShowFlags.StaticMeshes && bDrawMesh)
	{
		// how we should draw the collision for this mesh.
		const bool bIsWireframeView = EngineShowFlags.Wireframe;
		const bool bLevelColorationEnabled = EngineShowFlags.LevelColoration;
		const bool bPropertyColorationEnabled = EngineShowFlags.PropertyColoration;
		const ERHIFeatureLevel::Type FeatureLevel = ViewFamily.GetFeatureLevel();

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				const FSceneView* View = Views[ViewIndex];
				FLODMask LODMask = GetLODMask(View);

				for (int32 LODIndex = 0; LODIndex < RenderData->LODResources.Num(); LODIndex++)
				{
					if (LODMask.ContainsLOD(LODIndex))
					{
						const FStaticMeshLODResources& LODModel = RenderData->LODResources[LODIndex];
						const FLODInfo& ProxyLODInfo = LODs[LODIndex];

						if (AllowDebugViewmodes() && bIsWireframeView && !EngineShowFlags.Materials
							// If any of the materials are mesh-modifying, we can't use the single merged mesh element of GetWireframeMeshElement()
							&& !ProxyLODInfo.UsesMeshModifyingMaterials())
						{
							FLinearColor ViewWireframeColor( bLevelColorationEnabled ? LevelColor : WireframeColor );
							if ( bPropertyColorationEnabled )
							{
								ViewWireframeColor = PropertyColor;
							}

							auto WireframeMaterialInstance = new FColoredMaterialRenderProxy(
								GEngine->WireframeMaterial->GetRenderProxy(false),
								GetSelectionColor(ViewWireframeColor,!(GIsEditor && EngineShowFlags.Selection) || bProxyIsSelected, IsHovered(), false)
								);

							Collector.RegisterOneFrameMaterialProxy(WireframeMaterialInstance);

							const int32 NumBatches = GetNumMeshBatches();

							for (int32 BatchIndex = 0; BatchIndex < NumBatches; BatchIndex++)
							{
								FMeshBatch& Mesh = Collector.AllocateMesh();

								if (GetWireframeMeshElement(LODIndex, BatchIndex, WireframeMaterialInstance, SDPG_World, true, Mesh))
								{
									// We implemented our own wireframe
									Mesh.bCanApplyViewModeOverrides = false;
									Collector.AddMesh(ViewIndex, Mesh);
									INC_DWORD_STAT_BY(STAT_StaticMeshTriangles,Mesh.GetNumPrimitives());
								}
							}
						}
						else
						{
							const FLinearColor UtilColor( LevelColor );

							// Draw the static mesh sections.
							for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
							{
								const int32 NumBatches = GetNumMeshBatches();

								for (int32 BatchIndex = 0; BatchIndex < NumBatches; BatchIndex++)
								{
									bool bSectionIsSelected = false;
									FMeshBatch& MeshElement = Collector.AllocateMesh();

	#if WITH_EDITOR
									if (GIsEditor)
									{
										const FLODInfo::FSectionInfo& Section = LODs[LODIndex].Sections[SectionIndex];

										bSectionIsSelected = Section.bSelected;
										MeshElement.BatchHitProxyId = Section.HitProxy ? Section.HitProxy->Id : FHitProxyId();
									}
	#endif // WITH_EDITOR
								
									if (GetMeshElement(LODIndex, BatchIndex, SectionIndex, SDPG_World, bSectionIsSelected, IsHovered(), true, MeshElement))
									{
										bool bAddMesh = true;

	#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
										if (EngineShowFlags.VertexColors && AllowDebugViewmodes())
										{
											// Override the mesh's material with our material that draws the vertex colors
											UMaterial* VertexColorVisualizationMaterial = NULL;
											switch( GVertexColorViewMode )
											{
											case EVertexColorViewMode::Color:
												VertexColorVisualizationMaterial = GEngine->VertexColorViewModeMaterial_ColorOnly;
												break;

											case EVertexColorViewMode::Alpha:
												VertexColorVisualizationMaterial = GEngine->VertexColorViewModeMaterial_AlphaAsColor;
												break;

											case EVertexColorViewMode::Red:
												VertexColorVisualizationMaterial = GEngine->VertexColorViewModeMaterial_RedOnly;
												break;

											case EVertexColorViewMode::Green:
												VertexColorVisualizationMaterial = GEngine->VertexColorViewModeMaterial_GreenOnly;
												break;

											case EVertexColorViewMode::Blue:
												VertexColorVisualizationMaterial = GEngine->VertexColorViewModeMaterial_BlueOnly;
												break;
											}
											check( VertexColorVisualizationMaterial != NULL );

											auto VertexColorVisualizationMaterialInstance = new FColoredMaterialRenderProxy(
												VertexColorVisualizationMaterial->GetRenderProxy( MeshElement.MaterialRenderProxy->IsSelected(),MeshElement.MaterialRenderProxy->IsHovered() ),
												GetSelectionColor( FLinearColor::White, bSectionIsSelected, IsHovered() )
												);

											Collector.RegisterOneFrameMaterialProxy(VertexColorVisualizationMaterialInstance);
											MeshElement.MaterialRenderProxy = VertexColorVisualizationMaterialInstance;
										}
										else if (AllowDebugViewmodes() && (bDrawComplexWireframeCollision || (bInCollisionView && bDrawComplexCollision)))
										{
											if (LODModel.Sections[SectionIndex].bEnableCollision)
											{
												UMaterial* MaterialToUse = (bDrawComplexWireframeCollision) ? GEngine->WireframeMaterial : GEngine->ShadedLevelColorationUnlitMaterial;

												if (MaterialToUse)
												{
													// Override the mesh's material with our material that draws the collision color
													auto CollisionMaterialInstance = new FColoredMaterialRenderProxy(
														MaterialToUse ? MaterialToUse->GetRenderProxy(bProxyIsSelected, IsHovered()) : NULL,
														WireframeColor
														);

													Collector.RegisterOneFrameMaterialProxy(CollisionMaterialInstance);

													// If drawing the complex collision in wireframe, we do that in _addition_ to the normal mesh
													if (bDrawComplexWireframeCollision)
													{
														FMeshBatch& CollisionElement = Collector.AllocateMesh();
														GetMeshElement(LODIndex, BatchIndex, SectionIndex, SDPG_World, bSectionIsSelected, IsHovered(), true, CollisionElement);
														CollisionElement.MaterialRenderProxy = CollisionMaterialInstance;
														Collector.AddMesh(ViewIndex, CollisionElement);
														INC_DWORD_STAT_BY(STAT_StaticMeshTriangles, CollisionElement.GetNumPrimitives());
													}
													// Otherwise just change material on existing mesh
													else
													{
														MeshElement.MaterialRenderProxy = CollisionMaterialInstance;
													}
												}
											}
											else
											{
												bAddMesh = false;
											}
										}
										else
	#endif
	#if WITH_EDITOR
										if (bSectionIsSelected)
										{
											// Override the mesh's material with our material that draws the collision color
											auto SelectedMaterialInstance = new FOverrideSelectionColorMaterialRenderProxy(
												GEngine->ShadedLevelColorationUnlitMaterial->GetRenderProxy(bSectionIsSelected, IsHovered()),
												GetSelectionColor(GEngine->GetSelectedMaterialColor(), bSectionIsSelected, IsHovered())
												);

											MeshElement.MaterialRenderProxy = SelectedMaterialInstance;
										}
	#endif
										if (MeshElement.bDitheredLODTransition && LODMask.IsDithered())
										{
											if (LODIndex == LODMask.DitheredLODIndices[0])
											{
												MeshElement.DitheredLODTransitionAlpha = View->GetTemporalLODTransition();
											}
											else
											{
												MeshElement.DitheredLODTransitionAlpha = View->GetTemporalLODTransition() - 1.0f;
											}
										}
										else
										{
											MeshElement.bDitheredLODTransition = false;
										}
								
										MeshElement.bCanApplyViewModeOverrides = true;
										MeshElement.bUseWireframeSelectionColoring = bSectionIsSelected;

										if (bAddMesh)
										{
											Collector.AddMesh(ViewIndex, MeshElement);
											INC_DWORD_STAT_BY(STAT_StaticMeshTriangles,MeshElement.GetNumPrimitives());
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
	
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			// Draw simple collision as wireframe if 'show collision', collision is enabled, and we are not using the complex as the simple
			const bool bDrawSimpleWireframeCollision = (EngineShowFlags.Collision && IsCollisionEnabled() && CollisionTraceFlag != ECollisionTraceFlag::CTF_UseComplexAsSimple);


			if((bDrawSimpleCollision || bDrawSimpleWireframeCollision) && AllowDebugViewmodes())
			{
				if(BodySetup)
				{
					if(FMath::Abs(GetLocalToWorld().Determinant()) < SMALL_NUMBER)
					{
						// Catch this here or otherwise GeomTransform below will assert
						// This spams so commented out
						//UE_LOG(LogStaticMesh, Log, TEXT("Zero scaling not supported (%s)"), *StaticMesh->GetPathName());
					}
					else
					{
						const bool bDrawSolid = !bDrawSimpleWireframeCollision;

						if(bDrawSolid)
						{
							// Make a material for drawing solid collision stuff
							auto SolidMaterialInstance = new FColoredMaterialRenderProxy(
								GEngine->ShadedLevelColorationUnlitMaterial->GetRenderProxy(IsSelected(), IsHovered()),
								WireframeColor
								);

							Collector.RegisterOneFrameMaterialProxy(SolidMaterialInstance);

							FTransform GeomTransform(GetLocalToWorld());
							BodySetup->AggGeom.GetAggGeom(GeomTransform, WireframeColor.ToFColor(true), SolidMaterialInstance, false, true, UseEditorDepthTest(), ViewIndex, Collector);
						}
						// wireframe
						else
						{
							FColor CollisionColor = FColor(157,149,223,255);
							FTransform GeomTransform(GetLocalToWorld());
							BodySetup->AggGeom.GetAggGeom(GeomTransform, GetSelectionColor(CollisionColor, bProxyIsSelected, IsHovered()).ToFColor(true), NULL, ( Owner == NULL ), false, UseEditorDepthTest(), ViewIndex, Collector);
						}


						// The simple nav geometry is only used by dynamic obstacles for now
						if (StaticMesh->NavCollision && StaticMesh->NavCollision->bIsDynamicObstacle)
						{
							// Draw the static mesh's body setup (simple collision)
							FTransform GeomTransform(GetLocalToWorld());
							FColor NavCollisionColor = FColor(118,84,255,255);
							StaticMesh->NavCollision->DrawSimpleGeom(Collector.GetPDI(ViewIndex), GeomTransform, GetSelectionColor(NavCollisionColor, bProxyIsSelected, IsHovered()).ToFColor(true));
						}
					}
				}
			}
	
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if (EngineShowFlags.StaticMeshes)
			{
				RenderBounds(Collector.GetPDI(ViewIndex), EngineShowFlags, GetBounds(), !Owner || IsSelected());
			}
#endif
		}
	}
}

void FStaticMeshSceneProxy::GetLCIs(FLCIArray& LCIs)
{
	for (int32 LODIndex = 0; LODIndex < LODs.Num(); ++LODIndex)
	{
		FLightCacheInterface* LCI = &LODs[LODIndex];
		LCIs.Push(LCI);
	}
}

void FStaticMeshSceneProxy::OnTransformChanged()
{
	// Update the cached scaling.
	const FMatrix& LocalToWorld = GetLocalToWorld();
	TotalScale3D.X = FVector(LocalToWorld.TransformVector(FVector(1,0,0))).Size();
	TotalScale3D.Y = FVector(LocalToWorld.TransformVector(FVector(0,1,0))).Size();
	TotalScale3D.Z = FVector(LocalToWorld.TransformVector(FVector(0,0,1))).Size();
}

bool FStaticMeshSceneProxy::CanBeOccluded() const
{
	return !MaterialRelevance.bDisableDepthTest && !ShouldRenderCustomDepth();
}

FPrimitiveViewRelevance FStaticMeshSceneProxy::GetViewRelevance(const FSceneView* View) const
{   
	checkSlow(IsInParallelRenderingThread());

	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View) && View->Family->EngineShowFlags.StaticMeshes;
	Result.bRenderCustomDepth = ShouldRenderCustomDepth();
	Result.bRenderInMainPass = ShouldRenderInMainPass();
	Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || WITH_EDITOR
	bool bDrawSimpleCollision = false, bDrawComplexCollision = false;
	const bool bInCollisionView = IsCollisionView(View->Family->EngineShowFlags, bDrawSimpleCollision, bDrawComplexCollision);
#endif

	if(
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || WITH_EDITOR
		IsRichView(*View->Family) || 
		View->Family->EngineShowFlags.Collision ||
		bInCollisionView ||
		View->Family->EngineShowFlags.Bounds ||
#endif
		// Force down dynamic rendering path if invalid lightmap settings, so we can apply an error material in DrawRichMesh
		(HasStaticLighting() && !HasValidSettingsForStaticLighting()) ||
		HasViewDependentDPG() ||
		 !IsStaticPathAvailable()
#if WITH_EDITOR
		//only check these in the editor
		|| IsSelected() || IsHovered()
#endif
		)
	{
		Result.bDynamicRelevance = true;
	}
	else
	{
		Result.bStaticRelevance = true;
	}

	Result.bShadowRelevance = IsShadowCast(View);

	MaterialRelevance.SetPrimitiveViewRelevance(Result);

	if (!View->Family->EngineShowFlags.Materials 
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || WITH_EDITOR
		|| bInCollisionView
#endif
		)
	{
		Result.bOpaqueRelevance = true;
	}
	return Result;
}

void FStaticMeshSceneProxy::GetLightRelevance(const FLightSceneProxy* LightSceneProxy, bool& bDynamic, bool& bRelevant, bool& bLightMapped, bool& bShadowMapped) const
{
	// Attach the light to the primitive's static meshes.
	bDynamic = true;
	bRelevant = false;
	bLightMapped = true;
	bShadowMapped = true;

	if (LODs.Num() > 0)
	{
		for (int32 LODIndex = 0; LODIndex < LODs.Num(); LODIndex++)
		{
			const FLODInfo* LCI = &LODs[LODIndex];

			if (LCI)
			{
				ELightInteractionType InteractionType = LCI->GetInteraction(LightSceneProxy).GetType();

				if (InteractionType != LIT_CachedIrrelevant)
				{
					bRelevant = true;
				}

				if (InteractionType != LIT_CachedLightMap && InteractionType != LIT_CachedIrrelevant)
				{
					bLightMapped = false;
				}

				if (InteractionType != LIT_Dynamic)
				{
					bDynamic = false;
				}

				if (InteractionType != LIT_CachedSignedDistanceFieldShadowMap2D)
				{
					bShadowMapped = false;
				}
			}
		}
	}
	else
	{
		bRelevant = true;
		bLightMapped = false;
		bShadowMapped = false;
	}
}

void FStaticMeshSceneProxy::GetDistancefieldAtlasData(FBox& LocalVolumeBounds, FIntVector& OutBlockMin, FIntVector& OutBlockSize, bool& bOutBuiltAsIfTwoSided, bool& bMeshWasPlane, TArray<FMatrix>& ObjectLocalToWorldTransforms) const
{
	if (DistanceFieldData)
	{
		LocalVolumeBounds = DistanceFieldData->LocalBoundingBox;
		OutBlockMin = DistanceFieldData->VolumeTexture.GetAllocationMin();
		OutBlockSize = DistanceFieldData->VolumeTexture.GetAllocationSize();
		bOutBuiltAsIfTwoSided = DistanceFieldData->bBuiltAsIfTwoSided;
		bMeshWasPlane = DistanceFieldData->bMeshWasPlane;
		ObjectLocalToWorldTransforms.Add(GetLocalToWorld());
	}
	else
	{
		LocalVolumeBounds = FBox(0);
		OutBlockMin = FIntVector(-1, -1, -1);
		OutBlockSize = FIntVector(0, 0, 0);
		bOutBuiltAsIfTwoSided = false;
		bMeshWasPlane = false;
	}
}

void FStaticMeshSceneProxy::GetDistanceFieldInstanceInfo(int32& NumInstances, float& BoundsSurfaceArea) const
{
	NumInstances = DistanceFieldData ? 1 : 0;
	const FVector AxisScales = GetLocalToWorld().GetScaleVector();
	const FVector BoxDimensions = RenderData->Bounds.BoxExtent * AxisScales * 2;

	BoundsSurfaceArea = 2 * BoxDimensions.X * BoxDimensions.Y
		+ 2 * BoxDimensions.Z * BoxDimensions.Y
		+ 2 * BoxDimensions.X * BoxDimensions.Z;
}

bool FStaticMeshSceneProxy::HasDistanceFieldRepresentation() const
{
	return CastsDynamicShadow() && AffectsDistanceFieldLighting() && DistanceFieldData && DistanceFieldData->VolumeTexture.IsValidDistanceFieldVolume();
}

/** Initialization constructor. */
FStaticMeshSceneProxy::FLODInfo::FLODInfo(const UStaticMeshComponent* InComponent,int32 LODIndex):
	OverrideColorVertexBuffer(0),
	PreCulledIndexBuffer(NULL),
	LightMap(NULL),
	ShadowMap(NULL),
	bUsesMeshModifyingMaterials(false)
{
	const auto FeatureLevel = InComponent->GetWorld()->FeatureLevel;

	FStaticMeshRenderData* MeshRenderData = InComponent->StaticMesh->RenderData;
	FStaticMeshLODResources& LODModel = MeshRenderData->LODResources[LODIndex];
	if (LODIndex < InComponent->LODData.Num())
	{
		const FStaticMeshComponentLODInfo& ComponentLODInfo = InComponent->LODData[LODIndex];

		// Determine if the LOD has static lighting.
		LightMap = ComponentLODInfo.LightMap;
		ShadowMap = ComponentLODInfo.ShadowMap;
		IrrelevantLights = InComponent->IrrelevantLights;
		PreCulledIndexBuffer = &ComponentLODInfo.PreCulledIndexBuffer;

		// Initialize this LOD's overridden vertex colors, if it has any
		if( ComponentLODInfo.OverrideVertexColors )
		{
			bool bBroken = false;
			for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
			{
				const FStaticMeshSection& Section = LODModel.Sections[SectionIndex];
				if (Section.MaxVertexIndex >= ComponentLODInfo.OverrideVertexColors->GetNumVertices())
				{
					bBroken = true;
					break;
				}
			}
			if (!bBroken)
			{
				// the instance should point to the loaded data to avoid copy and memory waste
				OverrideColorVertexBuffer = ComponentLODInfo.OverrideVertexColors;
				check(OverrideColorVertexBuffer->GetStride() == sizeof(FColor)); //assumed when we set up the stream
			}
		}
	}

	if (MeshRenderData->bLODsShareStaticLighting && InComponent->LODData.IsValidIndex(0))
	{
		const FStaticMeshComponentLODInfo& ComponentLODInfo = InComponent->LODData[0];
		LightMap = ComponentLODInfo.LightMap;
		ShadowMap = ComponentLODInfo.ShadowMap;
	}

	bool bHasStaticLighting = LightMap != NULL || ShadowMap != NULL;

	// Gather the materials applied to the LOD.
	Sections.Empty(MeshRenderData->LODResources[LODIndex].Sections.Num());
	for(int32 SectionIndex = 0;SectionIndex < LODModel.Sections.Num();SectionIndex++)
	{
		const FStaticMeshSection& Section = LODModel.Sections[SectionIndex];
		FSectionInfo SectionInfo;

		// Determine the material applied to this element of the LOD.
		SectionInfo.Material = InComponent->GetMaterial(Section.MaterialIndex);
		if (GForceDefaultMaterial && SectionInfo.Material && !IsTranslucentBlendMode(SectionInfo.Material->GetBlendMode()))
		{
			SectionInfo.Material = UMaterial::GetDefaultMaterial(MD_Surface);
		}

		// If there isn't an applied material, or if we need static lighting and it doesn't support it, fall back to the default material.
		if(!SectionInfo.Material || (bHasStaticLighting && !SectionInfo.Material->CheckMaterialUsage_Concurrent(MATUSAGE_StaticLighting)))
		{
			SectionInfo.Material = UMaterial::GetDefaultMaterial(MD_Surface);
		}

		const bool bRequiresAdjacencyInformation = RequiresAdjacencyInformation(SectionInfo.Material, LODModel.VertexFactory.GetType(), FeatureLevel);
		if ( bRequiresAdjacencyInformation && !LODModel.bHasAdjacencyInfo )
		{
			UE_LOG(LogStaticMesh, Warning, TEXT("Adjacency information not built for static mesh with a material that requires it. Using default material instead.\n\tMaterial: %s\n\tStaticMesh: %s"),
				*SectionInfo.Material->GetPathName(), 
				*InComponent->StaticMesh->GetPathName() );
			SectionInfo.Material = UMaterial::GetDefaultMaterial(MD_Surface);
		}

		// Per-section selection for the editor.
#if WITH_EDITORONLY_DATA
		if (GIsEditor)
		{
			SectionInfo.bSelected = (InComponent->SelectedEditorSection == SectionIndex);
		}
#endif

		if (LODIndex < InComponent->LODData.Num())
		{
			const FStaticMeshComponentLODInfo& ComponentLODInfo = InComponent->LODData[LODIndex];

			if (SectionIndex < ComponentLODInfo.PreCulledSections.Num())
			{
				SectionInfo.FirstPreCulledIndex = ComponentLODInfo.PreCulledSections[SectionIndex].FirstIndex;
				SectionInfo.NumPreCulledTriangles = ComponentLODInfo.PreCulledSections[SectionIndex].NumTriangles;
			}
		}

		// Store the element info.
		Sections.Add(SectionInfo);

		// Flag the entire LOD if any material modifies its mesh
		UMaterialInterface::TMicRecursionGuard RecursionGuard;
		FMaterialResource const* MaterialResource = const_cast<UMaterialInterface const*>(SectionInfo.Material)->GetMaterial_Concurrent(RecursionGuard)->GetMaterialResource(FeatureLevel);
		if(MaterialResource)
		{
			if (IsInGameThread())
			{
				if (MaterialResource->MaterialModifiesMeshPosition_GameThread())
				{
					bUsesMeshModifyingMaterials = true;
				}
			}
			else
			{
				if (MaterialResource->MaterialModifiesMeshPosition_RenderThread())
				{
					bUsesMeshModifyingMaterials = true;
				}
			}
		}
	}

}

// FLightCacheInterface.
FLightInteraction FStaticMeshSceneProxy::FLODInfo::GetInteraction(const FLightSceneProxy* LightSceneProxy) const
{
	// Check if the light has static lighting or shadowing.
	// This directly accesses the component's static lighting with the assumption that it won't be changed without synchronizing with the rendering thread.
	if (LightSceneProxy->HasStaticShadowing())
	{
		const FGuid LightGuid = LightSceneProxy->GetLightGuid();
		
		if (LightMap && LightMap->ContainsLight(LightGuid))
		{
			return FLightInteraction::LightMap();
		}

		if (ShadowMap && ShadowMap->ContainsLight(LightGuid))
		{
			return FLightInteraction::ShadowMap2D();
		}

		if (IrrelevantLights.Contains(LightGuid))
		{
			return FLightInteraction::Irrelevant();
		}
	}

	// Use dynamic lighting if the light doesn't have static lighting.
	return FLightInteraction::Dynamic();
}

FLightMapInteraction FStaticMeshSceneProxy::FLODInfo::GetLightMapInteraction(ERHIFeatureLevel::Type InFeatureLevel) const
{
	return LightMap ? LightMap->GetInteraction(InFeatureLevel) : FLightMapInteraction();
}

FShadowMapInteraction FStaticMeshSceneProxy::FLODInfo::GetShadowMapInteraction() const
{
	return ShadowMap ? ShadowMap->GetInteraction() : FShadowMapInteraction();
}

float FStaticMeshSceneProxy::GetScreenSize( int32 LODIndex ) const
{
	return RenderData->ScreenSize[LODIndex];
}

/**
 * Returns the LOD that the primitive will render at for this view. 
 *
 * @param Distance - distance from the current view to the component's bound origin
 */
int32 FStaticMeshSceneProxy::GetLOD(const FSceneView* View) const 
{
	int32 CVarForcedLODLevel = GetCVarForceLOD();

	//If a LOD is being forced, use that one
	if (CVarForcedLODLevel >= 0)
	{
		return FMath::Clamp<int32>(CVarForcedLODLevel, 0, RenderData->LODResources.Num() - 1);
	}

	if (ForcedLodModel > 0)
	{
		return FMath::Clamp(ForcedLodModel, 1, RenderData->LODResources.Num()) - 1;
	}

#if WITH_EDITOR
	if (View->Family && View->Family->EngineShowFlags.LOD == 0)
	{
		return 0;
	}
#endif

	const FBoxSphereBounds& Bounds = GetBounds();
	return ComputeStaticMeshLOD(RenderData, Bounds.Origin, Bounds.SphereRadius, *View, ClampedMinLOD);
}

FLODMask FStaticMeshSceneProxy::GetLODMask(const FSceneView* View) const
{
	FLODMask Result;
	int32 CVarForcedLODLevel = GetCVarForceLOD();

	//If a LOD is being forced, use that one
	if (CVarForcedLODLevel >= 0)
	{
		Result.SetLOD(FMath::Clamp<int32>(CVarForcedLODLevel, 0, RenderData->LODResources.Num() - 1));
	}
	else if (View->DrawDynamicFlags & EDrawDynamicFlags::ForceLowestLOD)
	{
		Result.SetLOD(RenderData->LODResources.Num() - 1);
	}
	else if (ForcedLodModel > 0)
	{
		Result.SetLOD(FMath::Clamp(ForcedLodModel, 1, RenderData->LODResources.Num()) - 1);
	}
#if WITH_EDITOR
	else if (View->Family && View->Family->EngineShowFlags.LOD == 0)
	{
		Result.SetLOD(0);
	}
#endif
	else
	{
		const FBoxSphereBounds& Bounds = GetBounds();
		bool bUseDithered = false;
		if (LODs.Num())
		{
			// only dither if at least one section in LOD0 is dithered. Mixed dithering on sections won't work very well, but it makes an attempt
			const FLODInfo& ProxyLODInfo = LODs[0];
			const FStaticMeshLODResources& LODModel = RenderData->LODResources[0];
			// Draw the static mesh elements.
			for(int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
			{
				UMaterialInterface* Material = ProxyLODInfo.Sections[SectionIndex].Material;
				if (Material->IsDitheredLODTransition())
				{
					bUseDithered = true;
					break;
				}
			}

		}
		if (bUseDithered)
		{
			for (int32 Sample = 0; Sample < 2; Sample++)
			{
				Result.SetLODSample(ComputeTemporalStaticMeshLOD(RenderData, Bounds.Origin, Bounds.SphereRadius, *View, ClampedMinLOD, 1.0f, Sample), Sample);
			}
		}
		else
		{
			Result.SetLOD(ComputeStaticMeshLOD(RenderData, Bounds.Origin, Bounds.SphereRadius, *View, ClampedMinLOD));
		}
	}
	
	return Result;
}

FPrimitiveSceneProxy* UStaticMeshComponent::CreateSceneProxy()
{
	if(StaticMesh == NULL
		|| StaticMesh->RenderData == NULL
		|| StaticMesh->RenderData->LODResources.Num() == 0
		|| StaticMesh->RenderData->LODResources[0].VertexBuffer.GetNumVertices() == 0)
	{
		return NULL;
	}

	FPrimitiveSceneProxy* Proxy = ::new FStaticMeshSceneProxy(this);
	return Proxy;
}

bool UStaticMeshComponent::ShouldRecreateProxyOnUpdateTransform() const
{
	return (Mobility != EComponentMobility::Movable);
}
