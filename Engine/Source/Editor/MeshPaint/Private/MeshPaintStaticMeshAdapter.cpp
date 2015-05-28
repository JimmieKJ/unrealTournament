// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MeshPaintPrivatePCH.h"
#include "MeshPaintStaticMeshAdapter.h"
// 
// #include "Materials/MaterialExpressionTextureBase.h"
// #include "Materials/MaterialExpressionTextureSample.h"
// #include "Materials/MaterialExpressionTextureCoordinate.h"
// #include "Materials/MaterialExpressionTextureSampleParameter.h"
// 
#include "StaticMeshResources.h"
#include "MeshPaintEdMode.h"
// #include "Factories.h"
// #include "ScopedTransaction.h"
// #include "MeshPaintRendering.h"
// #include "ImageUtils.h"
// #include "Editor/UnrealEd/Public/Toolkits/ToolkitManager.h"
// #include "RawMesh.h"
// #include "Editor/UnrealEd/Public/ObjectTools.h"
// #include "AssetToolsModule.h"
// #include "EditorSupportDelegates.h"
// 
// //Slate dependencies
// #include "Editor/LevelEditor/Public/LevelEditor.h"
// #include "Editor/LevelEditor/Public/SLevelViewport.h"
// #include "MessageLog.h"
// 
// 
// #include "Runtime/Engine/Classes/PhysicsEngine/BodySetup.h"
// #include "SMeshPaint.h"
// #include "ComponentReregisterContext.h"
// #include "CanvasTypes.h"
// #include "Engine/Selection.h"
// #include "Engine/TextureRenderTarget2D.h"
// #include "EngineUtils.h"
// #include "Engine/StaticMeshActor.h"
// #include "Materials/MaterialInstanceConstant.h"

//////////////////////////////////////////////////////////////////////////
// FMeshPaintGeometryAdapterForStaticMeshes

bool FMeshPaintGeometryAdapterForStaticMeshes::Construct(UMeshComponent* InComponent, int32 InPaintingMeshLODIndex, int32 InUVChannelIndex)
{
	StaticMeshComponent = Cast<UStaticMeshComponent>(InComponent);
	if (StaticMeshComponent != nullptr)
	{
		if (StaticMeshComponent->StaticMesh != nullptr)
		{
			if (InPaintingMeshLODIndex < StaticMeshComponent->StaticMesh->GetNumLODs())
			{
				LODModel = &(StaticMeshComponent->StaticMesh->RenderData->LODResources[InPaintingMeshLODIndex]);
				UVChannelIndex = InUVChannelIndex;
				Indices = LODModel->IndexBuffer.GetArrayView();

				return (UVChannelIndex >= 0) && (UVChannelIndex < (int32)LODModel->VertexBuffer.GetNumTexCoords());
			}
		}
	}

	return false;
}

int32 FMeshPaintGeometryAdapterForStaticMeshes::GetNumTexCoords() const
{
	return LODModel->VertexBuffer.GetNumTexCoords();
}

void FMeshPaintGeometryAdapterForStaticMeshes::GetTriangleInfo(int32 TriIndex, FTexturePaintTriangleInfo& OutTriInfo) const
{
	// Grab the vertex indices and points for this triangle
	for (int32 TriVertexNum = 0; TriVertexNum < 3; ++TriVertexNum)
	{
		const int32 VertexIndex = Indices[TriIndex * 3 + TriVertexNum];
		OutTriInfo.TriVertices[TriVertexNum] = LODModel->PositionVertexBuffer.VertexPosition(VertexIndex);
		OutTriInfo.TriUVs[TriVertexNum] = LODModel->VertexBuffer.GetVertexUV(VertexIndex, UVChannelIndex);
	}
}

bool FMeshPaintGeometryAdapterForStaticMeshes::LineTraceComponent(struct FHitResult& OutHit, const FVector Start, const FVector End, const struct FCollisionQueryParams& Params) const
{
	check(StaticMeshComponent);
	check(StaticMeshComponent->StaticMesh);
	UStaticMesh* CurStaticMesh = StaticMeshComponent->StaticMesh;

	ECollisionEnabled::Type CachedCollisionType = ECollisionEnabled::NoCollision;
	UBodySetup* RestoreBodySetup = nullptr;

	//@TODO: This shouldn't be static
	/** A mapping of static meshes to body setups that were built for static meshes */
	static TMap<TWeakObjectPtr<UStaticMesh>, TWeakObjectPtr<UBodySetup>> StaticMeshToTempBodySetup;


	if (CurStaticMesh != nullptr)
	{
		// Get a temporary body setup the has fully detailed collision for the line traces below.
		TWeakObjectPtr<UBodySetup>* FindBodySetupPtr = StaticMeshToTempBodySetup.Find(CurStaticMesh);
		TWeakObjectPtr<UBodySetup> CollideAllBodySetup;
		if (FindBodySetupPtr && FindBodySetupPtr->IsValid())
		{
			// Existing temporary body setup for this mesh.
			CollideAllBodySetup = *FindBodySetupPtr;
		}
		else
		{
			// No existing body setup in the cache map - create one from the mesh's main body setup.
			UBodySetup* TempBodySetupRaw = DuplicateObject<UBodySetup>(CurStaticMesh->BodySetup, CurStaticMesh);

			// Set collide all flag so that the body creates physics meshes using ALL elements from the mesh not just the collision mesh.
			TempBodySetupRaw->bMeshCollideAll = true;

			// This forces it to recreate the physics mesh.
			TempBodySetupRaw->InvalidatePhysicsData();

			// Force it to use high detail tri-mesh for collisions.
			TempBodySetupRaw->CollisionTraceFlag = CTF_UseComplexAsSimple;
			TempBodySetupRaw->AggGeom.ConvexElems.Empty();

			CollideAllBodySetup = TempBodySetupRaw;

			// cache the body setup (remove existing entry for this mesh if is one - it must be an invalid weak ptr).
			StaticMeshToTempBodySetup.Remove(CurStaticMesh);
			StaticMeshToTempBodySetup.Add(CurStaticMesh, CollideAllBodySetup);
		}

		// Force the collision type to not be 'NoCollision' without it the line trace will always fail. 
		CachedCollisionType = StaticMeshComponent->BodyInstance.GetCollisionEnabled();
		if (CachedCollisionType == ECollisionEnabled::NoCollision)
		{
			StaticMeshComponent->BodyInstance.SetCollisionEnabled(ECollisionEnabled::QueryOnly, false);
		}

		// Swap the main and temp body setup on the mesh and recreate the physics state to update the body instance on the component.
		RestoreBodySetup = CurStaticMesh->BodySetup;
		CurStaticMesh->BodySetup = CollideAllBodySetup.Get();
		StaticMeshComponent->RecreatePhysicsState();
	}

	// Ray trace
	bool bHitSomething = StaticMeshComponent->LineTraceComponent(OutHit, Start, End, Params);

	if (CurStaticMesh != nullptr)
	{
		// Reset the original collision type if we reset it. 
		if (CachedCollisionType == ECollisionEnabled::NoCollision)
		{
			StaticMeshComponent->BodyInstance.SetCollisionEnabled(CachedCollisionType, false);
		}

		// Restore the main body setup on the mesh and recreate the physics state to update the body instance on the component.
		CurStaticMesh->BodySetup = RestoreBodySetup;
		StaticMeshComponent->RecreatePhysicsState();
	}

	return bHitSomething;
}

void FMeshPaintGeometryAdapterForStaticMeshes::SphereIntersectTriangles(TArray<int32>& OutTriangles, const float ComponentSpaceSquaredBrushRadius, const FVector& ComponentSpaceBrushPosition) const
{
	//@TODO: The code for static mesh components is still inside of the mesh painting mode due to the octree
}

void FMeshPaintGeometryAdapterForStaticMeshes::QueryPaintableTextures(int32 MaterialIndex, int32& OutDefaultIndex, TArray<struct FPaintableTexture>& InOutTextureList)
{
	DefaultQueryPaintableTextures(MaterialIndex, StaticMeshComponent, OutDefaultIndex, InOutTextureList);
}

void FMeshPaintGeometryAdapterForStaticMeshes::ApplyOrRemoveTextureOverride(UTexture* SourceTexture, UTexture* OverrideTexture) const
{
	DefaultApplyOrRemoveTextureOverride(StaticMeshComponent, SourceTexture, OverrideTexture);
}

//////////////////////////////////////////////////////////////////////////
// FMeshPaintGeometryAdapterForStaticMeshesFactory

TSharedPtr<IMeshPaintGeometryAdapter> FMeshPaintGeometryAdapterForStaticMeshesFactory::Construct(class UMeshComponent* InComponent, int32 InPaintingMeshLODIndex, int32 InUVChannelIndex) const
{
	if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(InComponent))
	{
		if (StaticMeshComponent->StaticMesh != nullptr)
		{
			TSharedRef<FMeshPaintGeometryAdapterForStaticMeshes> Result = MakeShareable(new FMeshPaintGeometryAdapterForStaticMeshes());
			if (Result->Construct(InComponent, InPaintingMeshLODIndex, InUVChannelIndex))
			{
				return Result;
			}
		}
	}

	return nullptr;
}
