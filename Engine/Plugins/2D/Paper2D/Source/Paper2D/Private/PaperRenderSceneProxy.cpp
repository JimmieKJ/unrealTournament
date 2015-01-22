// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DPrivatePCH.h"
#include "PaperRenderSceneProxy.h"
#include "PhysicsEngine/BodySetup.h"
#include "Rendering/PaperBatchManager.h"
#include "Rendering/PaperBatchSceneProxy.h"
#include "PhysicsEngine/BodySetup2D.h"
#include "LocalVertexFactory.h"

//////////////////////////////////////////////////////////////////////////
// FPaperSpriteVertex

FPackedNormal FPaperSpriteVertex::PackedNormalX(FVector(1.0f, 0.0f, 0.0f));
FPackedNormal FPaperSpriteVertex::PackedNormalZ(FVector(0.0f, -1.0f, 0.0f));

void FPaperSpriteVertex::SetTangentsFromPaperAxes()
{
	PackedNormalX = PaperAxisX;
	PackedNormalZ = PaperAxisZ;
	// store determinant of basis in w component of normal vector
	PackedNormalZ.Vector.W = (GetBasisDeterminantSign(PaperAxisX, PaperAxisY, PaperAxisZ) < 0.0f) ? 0 : 255;
}

//////////////////////////////////////////////////////////////////////////
// FPaperSpriteVertexBuffer

/** A dummy vertex buffer used to give the FPaperSpriteVertexFactory something to reference as a stream source. */
class FPaperSpriteVertexBuffer : public FVertexBuffer
{
public:
	virtual void InitRHI() override
	{
		FRHIResourceCreateInfo CreateInfo;
		VertexBufferRHI = RHICreateVertexBuffer(sizeof(FPaperSpriteVertex), BUF_Static, CreateInfo);
	}
};
static TGlobalResource<FPaperSpriteVertexBuffer> GDummyMaterialSpriteVertexBuffer;

/** The vertex factory used to draw paper sprites. */
class FPaperSpriteVertexFactory : public FLocalVertexFactory
{
public:
	void AllocateStuff()
	{
		FLocalVertexFactory::DataType Data;

		Data.PositionComponent = FVertexStreamComponent(
			&GDummyMaterialSpriteVertexBuffer,
			STRUCT_OFFSET(FPaperSpriteVertex,Position),
			sizeof(FPaperSpriteVertex),
			VET_Float3
			);
		Data.TangentBasisComponents[0] = FVertexStreamComponent(
			&GDummyMaterialSpriteVertexBuffer,
			STRUCT_OFFSET(FPaperSpriteVertex,TangentX),
			sizeof(FPaperSpriteVertex),
			VET_PackedNormal
			);
		Data.TangentBasisComponents[1] = FVertexStreamComponent(
			&GDummyMaterialSpriteVertexBuffer,
			STRUCT_OFFSET(FPaperSpriteVertex,TangentZ),
			sizeof(FPaperSpriteVertex),
			VET_PackedNormal
			);
		Data.ColorComponent = FVertexStreamComponent(
			&GDummyMaterialSpriteVertexBuffer,
			STRUCT_OFFSET(FPaperSpriteVertex,Color),
			sizeof(FPaperSpriteVertex),
			VET_Color
			);
		Data.TextureCoordinates.Empty();
		Data.TextureCoordinates.Add(FVertexStreamComponent(
			&GDummyMaterialSpriteVertexBuffer,
			STRUCT_OFFSET(FPaperSpriteVertex,TexCoords),
			sizeof(FPaperSpriteVertex),
			VET_Float2
			));

		SetData(Data);
	}

	FPaperSpriteVertexFactory()
	{
		AllocateStuff();
	}
};

//@TODO: Figure out how to do this properly at module load time
//static TGlobalResource<FPaperSpriteVertexFactory> GPaperSpriteVertexFactory;

//////////////////////////////////////////////////////////////////////////
// FTextureOverrideRenderProxy

/**
 * A material render proxy which overrides a named texture parameter.
 */
class FTextureOverrideRenderProxy : public FDynamicPrimitiveResource, public FMaterialRenderProxy
{
public:
	const FMaterialRenderProxy* const Parent;
	const UTexture* Texture;
	FName TextureParameterName;

	/** Initialization constructor. */
	FTextureOverrideRenderProxy(const FMaterialRenderProxy* InParent, const UTexture* InTexture, const FName& InParameterName)
		: Parent(InParent)
		, Texture(InTexture)
		, TextureParameterName(InParameterName)
	{}

	virtual ~FTextureOverrideRenderProxy()
	{
	}

	// FDynamicPrimitiveResource interface.
	virtual void InitPrimitiveResource()
	{
	}
	virtual void ReleasePrimitiveResource()
	{
		delete this;
	}

	// FMaterialRenderProxy interface.
	virtual const FMaterial* GetMaterial(ERHIFeatureLevel::Type InFeatureLevel) const override
	{
		return Parent->GetMaterial(InFeatureLevel);
	}

	virtual bool GetVectorValue(const FName ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const override
	{
		return Parent->GetVectorValue(ParameterName, OutValue, Context);
	}

	virtual bool GetScalarValue(const FName ParameterName, float* OutValue, const FMaterialRenderContext& Context) const override
	{
		return Parent->GetScalarValue(ParameterName, OutValue, Context);
	}

	virtual bool GetTextureValue(const FName ParameterName,const UTexture** OutValue, const FMaterialRenderContext& Context) const override
	{
		if (ParameterName == TextureParameterName)
		{
			*OutValue = Texture;
			return true;
		}
		else
		{
			return Parent->GetTextureValue(ParameterName, OutValue, Context);
		}
	}
};

//////////////////////////////////////////////////////////////////////////
// FPaperRenderSceneProxy

FPaperRenderSceneProxy::FPaperRenderSceneProxy(const UPrimitiveComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
	, Material(nullptr)
	, Owner(InComponent->GetOwner())
	, BodySetup(const_cast<UPrimitiveComponent*>(InComponent)->GetBodySetup())
	, bCastShadow(InComponent->CastShadow)
	, CollisionResponse(InComponent->GetCollisionResponseToChannels())
{
	WireframeColor = FLinearColor::White;
}

FPaperRenderSceneProxy::~FPaperRenderSceneProxy()
{
#if TEST_BATCHING
	if (FPaperBatchSceneProxy* Batcher = FPaperBatchManager::GetBatcher(GetScene()))
	{
		Batcher->UnregisterManagedProxy(this);
	}
#endif
}

void FPaperRenderSceneProxy::CreateRenderThreadResources()
{
#if TEST_BATCHING
	if (FPaperBatchSceneProxy* Batcher = FPaperBatchManager::GetBatcher(GetScene()))
	{
		Batcher->RegisterManagedProxy(this);
	}
#endif
}

void FPaperRenderSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_PaperRenderSceneProxy_GetDynamicMeshElements);
	checkSlow(IsInRenderingThread());

	const FEngineShowFlags& EngineShowFlags = ViewFamily.EngineShowFlags;

	bool bDrawSimpleCollision = false;
	bool bDrawComplexCollision = false;
	const bool bInCollisionView = IsCollisionView(EngineShowFlags, bDrawSimpleCollision, bDrawComplexCollision);
	
	// Sprites don't distinguish between simple and complex collision; when viewing visibility we should still render simple collision
	bDrawSimpleCollision |= bDrawComplexCollision;

	// Draw simple collision as wireframe if 'show collision', collision is enabled
	const bool bDrawWireframeCollision = EngineShowFlags.Collision && IsCollisionEnabled();

	const bool bDrawSprite = !bInCollisionView;

	if (bDrawSprite)
	{
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				const FSceneView* View = Views[ViewIndex];

				// Set the selection/hover color from the current engine setting.
				FLinearColor OverrideColor;
				bool bUseOverrideColor = false;

				const bool bShowAsSelected = !(GIsEditor && EngineShowFlags.Selection) || IsSelected();
				if (bShowAsSelected || IsHovered())
				{
					bUseOverrideColor = true;
					OverrideColor = GetSelectionColor(FLinearColor::White, bShowAsSelected, IsHovered());
				}

				bUseOverrideColor = false;

				// Sprites of locked actors draw in red.
				//FLinearColor LevelColorToUse = IsSelected() ? ColorToUse : (FLinearColor)LevelColor;
				//FLinearColor PropertyColorToUse = PropertyColor;

#if TEST_BATCHING
#else
				GetDynamicMeshElementsForView(View, ViewIndex, bUseOverrideColor, OverrideColor, Collector);
#endif
			}
		}
	}


	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			if ((bDrawSimpleCollision || bDrawWireframeCollision) && AllowDebugViewmodes())
			{
				if (BodySetup)
				{
					if (FMath::Abs(GetLocalToWorld().Determinant()) < SMALL_NUMBER)
					{
						// Catch this here or otherwise GeomTransform below will assert
						// This spams so commented out
						//UE_LOG(LogStaticMesh, Log, TEXT("Zero scaling not supported (%s)"), *StaticMesh->GetPathName());
					}
					else
					{
						const bool bDrawSolid = !bDrawWireframeCollision;

						if (bDrawSolid)
						{
							// Make a material for drawing solid collision stuff
							auto SolidMaterialInstance = new FColoredMaterialRenderProxy(
								GEngine->ShadedLevelColorationUnlitMaterial->GetRenderProxy(IsSelected(), IsHovered()),
								WireframeColor
								);

							Collector.RegisterOneFrameMaterialProxy(SolidMaterialInstance);

							FTransform GeomTransform(GetLocalToWorld());
							BodySetup->AggGeom.GetAggGeom(GeomTransform, WireframeColor, SolidMaterialInstance, false, true, UseEditorDepthTest(), ViewIndex, Collector);
						}
						else
						{
							// wireframe
							FColor CollisionColor = FColor(157, 149, 223, 255);
							FTransform GeomTransform(GetLocalToWorld());
							BodySetup->AggGeom.GetAggGeom(GeomTransform, GetSelectionColor(CollisionColor, IsSelected(), IsHovered()), nullptr, (Owner == nullptr), false, UseEditorDepthTest(), ViewIndex, Collector);
						}
					}
				}
			}

			// Draw bounds
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if (EngineShowFlags.Paper2DSprites)
			{
				RenderBounds(Collector.GetPDI(ViewIndex), EngineShowFlags, GetBounds(), (Owner == nullptr) || IsSelected());
			}
#endif
		}
	}
}

FVertexFactory* FPaperRenderSceneProxy::GetPaperSpriteVertexFactory() const
{
	static bool bInitAxes = false;
	if (!bInitAxes)
	{
		bInitAxes = true;
		FPaperSpriteVertex::SetTangentsFromPaperAxes();
	}

	static TGlobalResource<FPaperSpriteVertexFactory> GPaperSpriteVertexFactory;
	return &GPaperSpriteVertexFactory;
}

void FPaperRenderSceneProxy::GetDynamicMeshElementsForView(const FSceneView* View, int32 ViewIndex, bool bUseOverrideColor, const FLinearColor& OverrideColor, FMeshElementCollector& Collector) const
{
	if (Material != nullptr)
	{
		GetBatchMesh(View, bUseOverrideColor, OverrideColor, Material, BatchedSprites, ViewIndex, Collector);
	}
	GetNewBatchMeshes(View, bUseOverrideColor, OverrideColor, ViewIndex, Collector);
}

void FPaperRenderSceneProxy::GetNewBatchMeshes(const FSceneView* View, bool bUseOverrideColor, const FLinearColor& OverrideColor, int32 ViewIndex, FMeshElementCollector& Collector) const
{
	//@TODO: Doesn't support OverrideColor yet
	if (BatchedSections.Num() == 0)
	{
		return;
	}

	const uint8 DPG = GetDepthPriorityGroup(View);
	FVertexFactory* VertexFactory = GetPaperSpriteVertexFactory();
	const bool bIsWireframeView = View->Family->EngineShowFlags.Wireframe;

	for (const FSpriteRenderSection& Batch : BatchedSections)
	{
		const FLinearColor EffectiveWireframeColor = (Batch.Material->GetBlendMode() != BLEND_Opaque) ? WireframeColor : FLinearColor::Green;
		FMaterialRenderProxy* ParentMaterialProxy = Batch.Material->GetRenderProxy((View->Family->EngineShowFlags.Selection) && IsSelected(), IsHovered());
		FTexture* TextureResource = (Batch.Texture != nullptr) ? Batch.Texture->Resource : nullptr;

		if ((TextureResource != nullptr) && (Batch.NumVertices > 0))
		{
			FMeshBatch& Mesh = Collector.AllocateMesh();

			Mesh.bCanApplyViewModeOverrides = true;
			Mesh.bUseWireframeSelectionColoring = IsSelected();

			// Implementing our own wireframe coloring as the automatic one (controlled by Mesh.bCanApplyViewModeOverrides) only supports per-FPrimitiveSceneProxy WireframeColor
			if (bIsWireframeView)
			{
				auto WireframeMaterialInstance = new FColoredMaterialRenderProxy(
					GEngine->WireframeMaterial->GetRenderProxy(IsSelected(), IsHovered()),
					GetSelectionColor(EffectiveWireframeColor, IsSelected(), IsHovered(), false)
					);

				Collector.RegisterOneFrameMaterialProxy(WireframeMaterialInstance);

				ParentMaterialProxy = WireframeMaterialInstance;

				Mesh.bWireframe = true;
				// We are applying our own wireframe override
				Mesh.bCanApplyViewModeOverrides = false;
			}

			// Create a texture override material proxy and register it as a dynamic resource so that it won't be deleted until the rendering thread has finished with it
			FTextureOverrideRenderProxy* TextureOverrideMaterialProxy = new FTextureOverrideRenderProxy(ParentMaterialProxy, Batch.Texture, TEXT("SpriteTexture"));
			Collector.RegisterOneFrameMaterialProxy(TextureOverrideMaterialProxy);

			Mesh.UseDynamicData = true;
			// Note: memory referenced by Collector must be valid as long as the Collector is around, see Collector.AllocateOneFrameResource
			Mesh.DynamicVertexData = BatchedVertices.GetData();
			Mesh.DynamicVertexStride = sizeof(FPaperSpriteVertex);

			Mesh.VertexFactory = VertexFactory;
			Mesh.LCI = nullptr;
			Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative() ? true : false;
			Mesh.CastShadow = bCastShadow;
			Mesh.DepthPriorityGroup = DPG;
			Mesh.Type = PT_TriangleList;
			Mesh.bDisableBackfaceCulling = true;
			Mesh.MaterialRenderProxy = TextureOverrideMaterialProxy;

			// Set up the FMeshBatchElement.
			FMeshBatchElement& BatchElement = Mesh.Elements[0];
			BatchElement.IndexBuffer = nullptr;
			BatchElement.DynamicIndexData = nullptr;
			BatchElement.DynamicIndexStride = 0;
			BatchElement.PrimitiveUniformBuffer = GetUniformBuffer();
			BatchElement.FirstIndex = Batch.VertexOffset;
			BatchElement.MinVertexIndex = Batch.VertexOffset;
			BatchElement.MaxVertexIndex = Batch.VertexOffset + Batch.NumVertices;
			BatchElement.NumPrimitives = Batch.NumVertices / 3;

			Collector.AddMesh(ViewIndex, Mesh);
		}
	}
}

class FPaperVertexArray : public FOneFrameResource
{
public:
	TArray<FPaperSpriteVertex, TInlineAllocator<6> > Vertices;
};

void FPaperRenderSceneProxy::GetBatchMesh(const FSceneView* View, bool bUseOverrideColor, const FLinearColor& OverrideColor, class UMaterialInterface* BatchMaterial, const TArray<FSpriteDrawCallRecord>& Batch, int32 ViewIndex, FMeshElementCollector& Collector) const
{
	const uint8 DPG = GetDepthPriorityGroup(View);
	FVertexFactory* VertexFactory = GetPaperSpriteVertexFactory();

	const bool bIsWireframeView = View->Family->EngineShowFlags.Wireframe;
	const FLinearColor EffectiveWireframeColor = (BatchMaterial->GetBlendMode() != BLEND_Opaque) ? WireframeColor : FLinearColor::Green;

	for (const FSpriteDrawCallRecord& Record : Batch)
	{
		FTexture* TextureResource = (Record.Texture != nullptr) ? Record.Texture->Resource : nullptr;
		if ((TextureResource != nullptr) && (Record.RenderVerts.Num() > 0))
		{
			const FLinearColor SpriteColor = bUseOverrideColor ? OverrideColor : Record.Color;
			const FVector EffectiveOrigin = Record.Destination;

			FPaperVertexArray& VertexArray = Collector.AllocateOneFrameResource<FPaperVertexArray>();
			VertexArray.Vertices.Empty(Record.RenderVerts.Num());

			for (int32 SVI = 0; SVI < Record.RenderVerts.Num(); ++SVI)
			{
				const FVector4& SourceVert = Record.RenderVerts[SVI];
				const FVector Pos((PaperAxisX * SourceVert.X) + (PaperAxisY * SourceVert.Y) + EffectiveOrigin);
				const FVector2D UV(SourceVert.Z, SourceVert.W);

				new (VertexArray.Vertices) FPaperSpriteVertex(Pos, UV, SpriteColor);
			}

			// Set up the FMeshElement.
			FMeshBatch& Mesh = Collector.AllocateMesh();

			Mesh.UseDynamicData = true;
			Mesh.DynamicVertexData = VertexArray.Vertices.GetData();
			Mesh.DynamicVertexStride = sizeof(FPaperSpriteVertex);
			Mesh.VertexFactory = VertexFactory;
			Mesh.bCanApplyViewModeOverrides = true;
			Mesh.bUseWireframeSelectionColoring = IsSelected();

			FMaterialRenderProxy* ParentMaterialProxy = BatchMaterial->GetRenderProxy((View->Family->EngineShowFlags.Selection) && IsSelected(), IsHovered());

			// Implementing our own wireframe coloring as the automatic one (controlled by Mesh.bCanApplyViewModeOverrides) only supports per-FPrimitiveSceneProxy WireframeColor
			if (bIsWireframeView)
			{
				auto WireframeMaterialInstance = new FColoredMaterialRenderProxy(
					GEngine->WireframeMaterial->GetRenderProxy(IsSelected(), IsHovered()),
					GetSelectionColor(EffectiveWireframeColor, IsSelected(), IsHovered(), false)
					);

				Collector.RegisterOneFrameMaterialProxy(WireframeMaterialInstance);

				ParentMaterialProxy = WireframeMaterialInstance;

				Mesh.bWireframe = true;
				// We are applying our own wireframe override
				Mesh.bCanApplyViewModeOverrides = false;
			}

			// Create a texture override material proxy and register it as a dynamic resource so that it won't be deleted until the rendering thread has finished with it
			FTextureOverrideRenderProxy* TextureOverrideMaterialProxy = new FTextureOverrideRenderProxy(ParentMaterialProxy, Record.Texture, TEXT("SpriteTexture"));
			Collector.RegisterOneFrameMaterialProxy(TextureOverrideMaterialProxy);

			Mesh.MaterialRenderProxy = TextureOverrideMaterialProxy;
			Mesh.LCI = nullptr;
			Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative() ? true : false;
			Mesh.CastShadow = false;
			Mesh.DepthPriorityGroup = DPG;
			Mesh.Type = PT_TriangleList;
			Mesh.bDisableBackfaceCulling = true;

			// Set up the FMeshBatchElement.
			FMeshBatchElement& BatchElement = Mesh.Elements[0];
			BatchElement.IndexBuffer = nullptr;
			BatchElement.DynamicIndexData = nullptr;
			BatchElement.DynamicIndexStride = 0;
			BatchElement.FirstIndex = 0;
			BatchElement.MinVertexIndex = 0;
			BatchElement.MaxVertexIndex = VertexArray.Vertices.Num();
			BatchElement.PrimitiveUniformBuffer = GetUniformBuffer();
			BatchElement.NumPrimitives = VertexArray.Vertices.Num() / 3;

			const bool bIsWireframeView = View->Family->EngineShowFlags.Wireframe;

			Collector.AddMesh(ViewIndex, Mesh);
		}
	}
}

FPrimitiveViewRelevance FPaperRenderSceneProxy::GetViewRelevance(const FSceneView* View)
{
	const FEngineShowFlags& EngineShowFlags = View->Family->EngineShowFlags;

	checkSlow(IsInRenderingThread());

	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View) && EngineShowFlags.Paper2DSprites;
	Result.bRenderCustomDepth = ShouldRenderCustomDepth();
	Result.bRenderInMainPass = ShouldRenderInMainPass();

	Result.bShadowRelevance = IsShadowCast(View);

	MaterialRelevance.SetPrimitiveViewRelevance(Result);

#undef SUPPORT_EXTRA_RENDERING
#define SUPPORT_EXTRA_RENDERING !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || WITH_EDITOR
	

#if SUPPORT_EXTRA_RENDERING
	bool bDrawSimpleCollision = false;
	bool bDrawComplexCollision = false;
	const bool bInCollisionView = IsCollisionView(EngineShowFlags, bDrawSimpleCollision, bDrawComplexCollision);
#endif

	Result.bDynamicRelevance = true;

	if (!EngineShowFlags.Materials
#if SUPPORT_EXTRA_RENDERING
		|| bInCollisionView
#endif
		)
	{
		Result.bOpaqueRelevance = true;
	}

	return Result;
}

void FPaperRenderSceneProxy::OnTransformChanged()
{
	Origin = GetLocalToWorld().GetOrigin();
}

uint32 FPaperRenderSceneProxy::GetMemoryFootprint() const
{
	return sizeof(*this) + GetAllocatedSize();
}

bool FPaperRenderSceneProxy::CanBeOccluded() const
{
	return !MaterialRelevance.bDisableDepthTest;
}

void FPaperRenderSceneProxy::SetDrawCall_RenderThread(const FSpriteDrawCallRecord& NewDynamicData)
{
	BatchedSprites.Empty();

	FSpriteDrawCallRecord& Record = *new (BatchedSprites) FSpriteDrawCallRecord;
	Record = NewDynamicData;
}

void FPaperRenderSceneProxy::SetBodySetup_RenderThread(UBodySetup* NewSetup)
{
	BodySetup = NewSetup;
}

bool FPaperRenderSceneProxy::IsCollisionView(const FEngineShowFlags& EngineShowFlags, bool& bDrawSimpleCollision, bool& bDrawComplexCollision) const
{
	bDrawSimpleCollision = false;
	bDrawComplexCollision = false;

	// If in a 'collision view' and collision is enabled
	const bool bInCollisionView = EngineShowFlags.CollisionVisibility || EngineShowFlags.CollisionPawn;
	if (bInCollisionView && IsCollisionEnabled())
	{
		// See if we have a response to the interested channel
		bool bHasResponse = EngineShowFlags.CollisionPawn && (CollisionResponse.GetResponse(ECC_Pawn) != ECR_Ignore);
		bHasResponse |= EngineShowFlags.CollisionVisibility && (CollisionResponse.GetResponse(ECC_Visibility) != ECR_Ignore);

		if (bHasResponse)
		{
			bDrawComplexCollision = EngineShowFlags.CollisionVisibility;
			bDrawSimpleCollision = EngineShowFlags.CollisionPawn;
		}
	}

	return bInCollisionView;
}
