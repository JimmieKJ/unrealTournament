// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
		StaticMeshComponent->OnStaticMeshChanged().AddRaw(this, &FMeshPaintGeometryAdapterForStaticMeshes::OnStaticMeshChanged);

		if (StaticMeshComponent->StaticMesh != nullptr)
		{
			ReferencedStaticMesh = StaticMeshComponent->StaticMesh;
			PaintingMeshLODIndex = InPaintingMeshLODIndex;
			UVChannelIndex = InUVChannelIndex;
			ReferencedStaticMesh->OnPostMeshBuild().AddRaw(this, &FMeshPaintGeometryAdapterForStaticMeshes::OnPostMeshBuild);
			const bool bSuccess = Initialize();
			return bSuccess;
		}
	}

	return false;
}

FMeshPaintGeometryAdapterForStaticMeshes::~FMeshPaintGeometryAdapterForStaticMeshes()
{
	if (StaticMeshComponent != nullptr)
	{
		if (ReferencedStaticMesh != nullptr)
		{
			ReferencedStaticMesh->OnPostMeshBuild().RemoveAll(this);
		}

		StaticMeshComponent->OnStaticMeshChanged().RemoveAll(this);
	}
}

void FMeshPaintGeometryAdapterForStaticMeshes::OnPostMeshBuild(UStaticMesh* StaticMesh)
{
	check(StaticMesh == ReferencedStaticMesh);
	Initialize();
}

void FMeshPaintGeometryAdapterForStaticMeshes::OnStaticMeshChanged(UStaticMeshComponent* InStaticMeshComponent)
{
	check(StaticMeshComponent == InStaticMeshComponent);
	OnRemoved();
	ReferencedStaticMesh->OnPostMeshBuild().RemoveAll(this);
	ReferencedStaticMesh = InStaticMeshComponent->StaticMesh;
	if (ReferencedStaticMesh)
	{
		ReferencedStaticMesh->OnPostMeshBuild().AddRaw(this, &FMeshPaintGeometryAdapterForStaticMeshes::OnPostMeshBuild);
		Initialize();
		OnAdded();
	}
}

bool FMeshPaintGeometryAdapterForStaticMeshes::Initialize()
{
	check(ReferencedStaticMesh == StaticMeshComponent->StaticMesh);
	if (PaintingMeshLODIndex < ReferencedStaticMesh->GetNumLODs())
	{
		LODModel = &(ReferencedStaticMesh->RenderData->LODResources[PaintingMeshLODIndex]);
		InitializeMeshVertices();
		BuildOctree();
		return (UVChannelIndex >= 0) && (UVChannelIndex < (int32)LODModel->VertexBuffer.GetNumTexCoords());
	}

	return false;
}

void FMeshPaintGeometryAdapterForStaticMeshes::InitializeMeshVertices()
{
	const int32 NumVertices = LODModel->PositionVertexBuffer.GetNumVertices();
	MeshVertices.Reset();
	MeshVertices.AddDefaulted(NumVertices);
	for (int32 Index = 0; Index < NumVertices; Index++)
	{
		const FVector& Position = LODModel->PositionVertexBuffer.VertexPosition(Index);
		MeshVertices[Index] = Position;
	}
}

void FMeshPaintGeometryAdapterForStaticMeshes::BuildOctree()
{
	// First determine bounding box of mesh verts
	FBox Bounds;
	for (const FVector& Vertex : MeshVertices)
	{
		Bounds += Vertex;
	}

	MeshTriOctree = MakeUnique<FMeshTriOctree>(Bounds.GetCenter(), Bounds.GetExtent().GetMax());

	FIndexArrayView LocalIndices = LODModel->IndexBuffer.GetArrayView();
	const uint32 NumIndexBufferIndices = LocalIndices.Num();
	check(NumIndexBufferIndices % 3 == 0);
	const uint32 NumTriangles = NumIndexBufferIndices / 3;

	for (uint32 TriIndex = 0; TriIndex < NumTriangles; ++TriIndex)
	{
		// Grab the vertex indices and points for this triangle
		FMeshTriangle MeshTri;
		MeshTri.Vertices[0] = MeshVertices[LocalIndices[TriIndex * 3 + 0]];
		MeshTri.Vertices[1] = MeshVertices[LocalIndices[TriIndex * 3 + 1]];
		MeshTri.Vertices[2] = MeshVertices[LocalIndices[TriIndex * 3 + 2]];
		MeshTri.Normal = FVector::CrossProduct(MeshTri.Vertices[1] - MeshTri.Vertices[0], MeshTri.Vertices[2] - MeshTri.Vertices[0]).GetSafeNormal();
		MeshTri.Index = TriIndex;

		FBox TriBox;
		TriBox.Min.X = FMath::Min3(MeshTri.Vertices[0].X, MeshTri.Vertices[1].X, MeshTri.Vertices[2].X);
		TriBox.Min.Y = FMath::Min3(MeshTri.Vertices[0].Y, MeshTri.Vertices[1].Y, MeshTri.Vertices[2].Y);
		TriBox.Min.Z = FMath::Min3(MeshTri.Vertices[0].Z, MeshTri.Vertices[1].Z, MeshTri.Vertices[2].Z);

		TriBox.Max.X = FMath::Max3(MeshTri.Vertices[0].X, MeshTri.Vertices[1].X, MeshTri.Vertices[2].X);
		TriBox.Max.Y = FMath::Max3(MeshTri.Vertices[0].Y, MeshTri.Vertices[1].Y, MeshTri.Vertices[2].Y);
		TriBox.Max.Z = FMath::Max3(MeshTri.Vertices[0].Z, MeshTri.Vertices[1].Z, MeshTri.Vertices[2].Z);

		MeshTri.BoxCenterAndExtent = FBoxCenterAndExtent(TriBox);
		MeshTriOctree->AddElement(MeshTri);
	}
}

void FMeshPaintGeometryAdapterForStaticMeshes::InitializeAdapterGlobals()
{
	MeshToComponentMap.Empty();
}

void FMeshPaintGeometryAdapterForStaticMeshes::OnAdded()
{
	check(StaticMeshComponent);
	check(ReferencedStaticMesh);
	check(ReferencedStaticMesh == StaticMeshComponent->StaticMesh);

	FStaticMeshReferencers& StaticMeshReferencers = MeshToComponentMap.FindOrAdd(ReferencedStaticMesh);

	check(!StaticMeshReferencers.Referencers.ContainsByPredicate(
		[=](const FStaticMeshReferencers::FReferencersInfo& Info)
		{
			return Info.StaticMeshComponent == this->StaticMeshComponent;
		}
	));

	// If this is the first attempt to add a temporary body setup to the mesh, do it
	if (StaticMeshReferencers.Referencers.Num() == 0)
	{
		// Remember the old body setup (this will be added as a GC reference so that it doesn't get destroyed)
		StaticMeshReferencers.RestoreBodySetup = ReferencedStaticMesh->BodySetup;

		// Create a new body setup from the mesh's main body setup. This has to have the static mesh as its outer,
		// otherwise the body instance will not be created correctly.
		UBodySetup* TempBodySetupRaw = DuplicateObject<UBodySetup>(ReferencedStaticMesh->BodySetup, ReferencedStaticMesh);
		TempBodySetupRaw->ClearFlags(RF_Transactional);

		// Set collide all flag so that the body creates physics meshes using ALL elements from the mesh not just the collision mesh.
		TempBodySetupRaw->bMeshCollideAll = true;

		// This forces it to recreate the physics mesh.
		TempBodySetupRaw->InvalidatePhysicsData();

		// Force it to use high detail tri-mesh for collisions.
		TempBodySetupRaw->CollisionTraceFlag = CTF_UseComplexAsSimple;
		TempBodySetupRaw->AggGeom.ConvexElems.Empty();

		// Set as new body setup
		ReferencedStaticMesh->BodySetup = TempBodySetupRaw;
	}

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

void FMeshPaintGeometryAdapterForStaticMeshes::OnRemoved()
{
	check(StaticMeshComponent);
	
	// If the referenced static mesh has been destroyed (and nulled by GC), don't try to do anything more.
	// It should be in the process of removing all global geometry adapters if it gets here in this situation.
	if (!ReferencedStaticMesh)
	{
		return;
	}

	// Remove a reference from the static mesh map
	FStaticMeshReferencers* StaticMeshReferencers = MeshToComponentMap.Find(ReferencedStaticMesh);
	check(StaticMeshReferencers);
	check(StaticMeshReferencers->Referencers.Num() > 0);

	int32 Index = StaticMeshReferencers->Referencers.IndexOfByPredicate(
		[=](const FStaticMeshReferencers::FReferencersInfo& Info)
		{
			return Info.StaticMeshComponent == this->StaticMeshComponent;
		}
	);
	check(Index != INDEX_NONE);

	StaticMeshComponent->BodyInstance.SetCollisionEnabled(StaticMeshReferencers->Referencers[Index].CachedCollisionType, false);
	StaticMeshComponent->RecreatePhysicsState();

	StaticMeshReferencers->Referencers.RemoveAtSwap(Index);

	// If the last reference was removed, restore the body setup for the static mesh
	if (StaticMeshReferencers->Referencers.Num() == 0)
	{
		ReferencedStaticMesh->BodySetup = StaticMeshReferencers->RestoreBodySetup;
		verify(MeshToComponentMap.Remove(ReferencedStaticMesh) == 1);
	}
}

int32 FMeshPaintGeometryAdapterForStaticMeshes::GetNumTexCoords() const
{
	return LODModel->VertexBuffer.GetNumTexCoords();
}

void FMeshPaintGeometryAdapterForStaticMeshes::GetTriangleInfo(int32 TriIndex, FTexturePaintTriangleInfo& OutTriInfo) const
{
	FIndexArrayView VertexIndices = LODModel->IndexBuffer.GetArrayView();

	// Grab the vertex indices and points for this triangle
	for (int32 TriVertexNum = 0; TriVertexNum < 3; ++TriVertexNum)
	{
		const int32 VertexIndex = VertexIndices[TriIndex * 3 + TriVertexNum];
		OutTriInfo.TriVertices[TriVertexNum] = MeshVertices[VertexIndex];
		OutTriInfo.TriUVs[TriVertexNum] = LODModel->VertexBuffer.GetVertexUV(VertexIndex, UVChannelIndex);
	}
}

bool FMeshPaintGeometryAdapterForStaticMeshes::LineTraceComponent(struct FHitResult& OutHit, const FVector Start, const FVector End, const struct FCollisionQueryParams& Params) const
{
	// Ray trace
	return StaticMeshComponent->LineTraceComponent(OutHit, Start, End, Params);
}

TArray<uint32> FMeshPaintGeometryAdapterForStaticMeshes::SphereIntersectTriangles(const float ComponentSpaceSquaredBrushRadius, const FVector& ComponentSpaceBrushPosition, const FVector& ComponentSpaceCameraPosition, const bool bOnlyFrontFacing) const
{
	TArray<uint32> OutTriangles;

	// Use a bit of distance bias to make sure that we get all of the overlapping triangles.  We
	// definitely don't want our brush to be cut off by a hard triangle edge
	const float SquaredRadiusBias = ComponentSpaceSquaredBrushRadius * 0.025f;

	for (FMeshTriOctree::TConstElementBoxIterator<> TriIt(*MeshTriOctree, FBoxCenterAndExtent(ComponentSpaceBrushPosition, FVector(FMath::Sqrt(ComponentSpaceSquaredBrushRadius + SquaredRadiusBias)))); TriIt.HasPendingElements(); TriIt.Advance())
	{
		// Check to see if the triangle is front facing
		FMeshTriangle const& CurrentTri = TriIt.GetCurrentElement();
		const float SignedPlaneDist = FVector::PointPlaneDist(ComponentSpaceCameraPosition, CurrentTri.Vertices[0], CurrentTri.Normal);
		if (!bOnlyFrontFacing || SignedPlaneDist < 0.0f)
		{
			OutTriangles.Add(CurrentTri.Index);
		}
	}

	return OutTriangles;
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
	if (!ReferencedStaticMesh)
	{
		return;
	}

	FStaticMeshReferencers* StaticMeshReferencers = MeshToComponentMap.Find(ReferencedStaticMesh);
	check(StaticMeshReferencers);
	Collector.AddReferencedObject(StaticMeshReferencers->RestoreBodySetup);

	for (auto& Info : StaticMeshReferencers->Referencers)
	{
		Collector.AddReferencedObject(Info.StaticMeshComponent);
	}
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
