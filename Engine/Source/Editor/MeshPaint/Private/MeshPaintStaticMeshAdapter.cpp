// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MeshPaintPrivatePCH.h"
#include "MeshPaintStaticMeshAdapter.h"
#include "StaticMeshResources.h"
#include "MeshPaintEdMode.h"
#include "PhysicsEngine/BodySetup.h"

//////////////////////////////////////////////////////////////////////////
// FMeshPaintGeometryAdapterForStaticMeshes

FMeshPaintGeometryAdapterForStaticMeshes::FMeshToComponentMap FMeshPaintGeometryAdapterForStaticMeshes::MeshToComponentMap;

bool FMeshPaintGeometryAdapterForStaticMeshes::Construct(UMeshComponent* InComponent, int32 InPaintingMeshLODIndex, int32 InUVChannelIndex)
{
	StaticMeshComponent = Cast<UStaticMeshComponent>(InComponent);
	if (StaticMeshComponent != nullptr)
	{
		if (StaticMeshComponent->StaticMesh != nullptr)
		{
			PaintingMeshLODIndex = InPaintingMeshLODIndex;
			UVChannelIndex = InUVChannelIndex;
			StaticMeshComponent->StaticMesh->OnPostMeshBuild().AddRaw(this, &FMeshPaintGeometryAdapterForStaticMeshes::OnPostMeshBuild);
			const bool bSuccess = InitializeMeshData();
			return bSuccess;
		}
	}

	return false;
}

FMeshPaintGeometryAdapterForStaticMeshes::~FMeshPaintGeometryAdapterForStaticMeshes()
{
	if (StaticMeshComponent != nullptr && StaticMeshComponent->StaticMesh != nullptr)
	{
		StaticMeshComponent->StaticMesh->OnPostMeshBuild().RemoveAll(this);
	}
}

void FMeshPaintGeometryAdapterForStaticMeshes::OnPostMeshBuild(UStaticMesh* StaticMesh)
{
	InitializeMeshData();
}

bool FMeshPaintGeometryAdapterForStaticMeshes::InitializeMeshData()
{
	if (PaintingMeshLODIndex < StaticMeshComponent->StaticMesh->GetNumLODs())
	{
		LODModel = &(StaticMeshComponent->StaticMesh->RenderData->LODResources[PaintingMeshLODIndex]);
		Indices = LODModel->IndexBuffer.GetArrayView();

		return (UVChannelIndex >= 0) && (UVChannelIndex < (int32)LODModel->VertexBuffer.GetNumTexCoords());
	}

	return false;
}

void FMeshPaintGeometryAdapterForStaticMeshes::InitializeAdapterGlobals()
{
	MeshToComponentMap.Empty();
}

void FMeshPaintGeometryAdapterForStaticMeshes::OnAdded()
{
	check(StaticMeshComponent);
	UStaticMesh* CurStaticMesh = StaticMeshComponent->StaticMesh;

	if (CurStaticMesh != nullptr)
	{
		FStaticMeshReferencers& StaticMeshReferencers = MeshToComponentMap.FindOrAdd(CurStaticMesh);

		check(!StaticMeshReferencers.Referencers.ContainsByPredicate(
			[=](const FStaticMeshReferencers::FReferencersInfo& Info)
			{
				return Info.StaticMeshComponent == this->StaticMeshComponent;
			}
		));

		// If this is the first attempt to add a temporary body setup to the mesh, do it
		if (StaticMeshReferencers.NumReferencers == 0)
		{
			// Remember the old body setup (this will be added as a GC reference so that it doesn't get destroyed)
			StaticMeshReferencers.RestoreBodySetup = CurStaticMesh->BodySetup;

			// Create a new body setup from the mesh's main body setup. This has tp have the static mesh as its outer,
			// otherwise the body instance will not be created correctly.
			UBodySetup* TempBodySetupRaw = DuplicateObject<UBodySetup>(CurStaticMesh->BodySetup, CurStaticMesh);

			// Set collide all flag so that the body creates physics meshes using ALL elements from the mesh not just the collision mesh.
			TempBodySetupRaw->bMeshCollideAll = true;

			// This forces it to recreate the physics mesh.
			TempBodySetupRaw->InvalidatePhysicsData();

			// Force it to use high detail tri-mesh for collisions.
			TempBodySetupRaw->CollisionTraceFlag = CTF_UseComplexAsSimple;
			TempBodySetupRaw->AggGeom.ConvexElems.Empty();

			// Set as new body setup
			CurStaticMesh->BodySetup = TempBodySetupRaw;
		}

		StaticMeshReferencers.NumReferencers++;

		ECollisionEnabled::Type CachedCollisionType = StaticMeshComponent->BodyInstance.GetCollisionEnabled();
		StaticMeshReferencers.Referencers.Emplace(StaticMeshComponent, CachedCollisionType);

		// Force the collision type to not be 'NoCollision' without it the line trace will always fail. 
		if (CachedCollisionType == ECollisionEnabled::NoCollision)
		{
			StaticMeshComponent->BodyInstance.SetCollisionEnabled(ECollisionEnabled::QueryOnly, false);
		}

		// Set new physics state for the component
		StaticMeshComponent->RecreatePhysicsState();
	}
}

void FMeshPaintGeometryAdapterForStaticMeshes::OnRemoved()
{
	check(StaticMeshComponent);
	UStaticMesh* CurStaticMesh = StaticMeshComponent->StaticMesh;

	if (CurStaticMesh != nullptr)
	{
		// Remove a reference from the static mesh map
		FStaticMeshReferencers* StaticMeshReferencers = MeshToComponentMap.Find(CurStaticMesh);
		check(StaticMeshReferencers);
		check(StaticMeshReferencers->Referencers.Num() > 0);
		check(StaticMeshReferencers->NumReferencers > 0);
		StaticMeshReferencers->NumReferencers--;

		// If the last reference was removed, restore the body setup for all the mesh components which referenced it.
		if (StaticMeshReferencers->NumReferencers == 0)
		{
			CurStaticMesh->BodySetup = StaticMeshReferencers->RestoreBodySetup;

			for (const auto& Info : StaticMeshReferencers->Referencers)
			{
				Info.StaticMeshComponent->BodyInstance.SetCollisionEnabled(Info.CachedCollisionType, false);
				Info.StaticMeshComponent->RecreatePhysicsState();
			}

			StaticMeshReferencers->Referencers.Reset();
		}
	}
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
	// Ray trace
	return StaticMeshComponent->LineTraceComponent(OutHit, Start, End, Params);
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

void FMeshPaintGeometryAdapterForStaticMeshes::SetCurrentUVChannelIndex(int32 InUVChannelIndex)
{
	check(UVChannelIndex >= 0 && UVChannelIndex < (int32)LODModel->VertexBuffer.GetNumTexCoords());
	UVChannelIndex = InUVChannelIndex;
}

void FMeshPaintGeometryAdapterForStaticMeshes::AddReferencedObjects(FReferenceCollector& Collector)
{
	check(StaticMeshComponent);
	UStaticMesh* CurStaticMesh = StaticMeshComponent->StaticMesh;

	if (CurStaticMesh != nullptr)
	{
		FStaticMeshReferencers* StaticMeshReferencers = MeshToComponentMap.Find(CurStaticMesh);
		check(StaticMeshReferencers);
		Collector.AddReferencedObject(StaticMeshReferencers->RestoreBodySetup);
	}
}

FVector FMeshPaintGeometryAdapterForStaticMeshes::GetMeshVertex(int32 Index) const
{
	const FVector OutPosition = LODModel->PositionVertexBuffer.VertexPosition(Index);
	return OutPosition;
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
