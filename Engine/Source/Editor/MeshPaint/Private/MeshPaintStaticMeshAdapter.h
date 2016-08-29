// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshPaintModule.h"
#include "GenericOctree.h"

//////////////////////////////////////////////////////////////////////////
// FMeshPaintGeometryAdapterForStaticMeshes

class FMeshPaintGeometryAdapterForStaticMeshes : public IMeshPaintGeometryAdapter
{
public:
	static void InitializeAdapterGlobals();

	virtual bool Construct(UMeshComponent* InComponent, int32 InPaintingMeshLODIndex, int32 InUVChannelIndex) override;
	virtual ~FMeshPaintGeometryAdapterForStaticMeshes();
	virtual bool Initialize() override;
	virtual void OnAdded() override;
	virtual void OnRemoved() override;
	virtual bool IsValid() const override { return StaticMeshComponent && StaticMeshComponent->StaticMesh == ReferencedStaticMesh; }
	virtual int32 GetNumTexCoords() const override;
	virtual void GetTriangleInfo(int32 TriIndex, struct FTexturePaintTriangleInfo& OutTriInfo) const override;
	virtual bool SupportsTexturePaint() const override { return true; }
	virtual bool SupportsVertexPaint() const override { return StaticMeshComponent && !StaticMeshComponent->bDisallowMeshPaintPerInstance; }
	virtual bool LineTraceComponent(struct FHitResult& OutHit, const FVector Start, const FVector End, const struct FCollisionQueryParams& Params) const override;
	virtual TArray<uint32> SphereIntersectTriangles(const float ComponentSpaceSquaredBrushRadius, const FVector& ComponentSpaceBrushPosition, const FVector& ComponentSpaceCameraPosition, const bool bOnlyFrontFacing) const override;
	virtual void QueryPaintableTextures(int32 MaterialIndex, int32& OutDefaultIndex, TArray<struct FPaintableTexture>& InOutTextureList) override;
	virtual void ApplyOrRemoveTextureOverride(UTexture* SourceTexture, UTexture* OverrideTexture) const override;
	virtual void SetCurrentUVChannelIndex(int32 InUVChannelIndex) override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual const TArray<FVector>& GetMeshVertices() const override { return MeshVertices; }

protected:

	virtual void InitializeMeshVertices();
	virtual void BuildOctree();

	void OnPostMeshBuild(UStaticMesh* StaticMesh);
	void OnStaticMeshChanged(UStaticMeshComponent* StaticMeshComponent);

	/** Triangle for use in Octree for mesh paint optimization */
	struct FMeshTriangle
	{
		uint32 Index;
		FVector Vertices[3];
		FVector Normal;
		FBoxCenterAndExtent BoxCenterAndExtent;
	};

	/** Semantics for the simple mesh paint octree */
	struct FMeshTriangleOctreeSemantics
	{
		enum { MaxElementsPerLeaf = 16 };
		enum { MinInclusiveElementsPerNode = 7 };
		enum { MaxNodeDepth = 12 };

		typedef TInlineAllocator<MaxElementsPerLeaf> ElementAllocator;

		/**
		* Get the bounding box of the provided octree element. In this case, the box
		* is merely the point specified by the element.
		*
		* @param	Element	Octree element to get the bounding box for
		*
		* @return	Bounding box of the provided octree element
		*/
		FORCEINLINE static FBoxCenterAndExtent GetBoundingBox(const FMeshTriangle& Element)
		{
			return Element.BoxCenterAndExtent;
		}

		/**
		* Determine if two octree elements are equal
		*
		* @param	A	First octree element to check
		* @param	B	Second octree element to check
		*
		* @return	true if both octree elements are equal, false if they are not
		*/
		FORCEINLINE static bool AreElementsEqual(const FMeshTriangle& A, const FMeshTriangle& B)
		{
			return (A.Index == B.Index);
		}

		/** Ignored for this implementation */
		FORCEINLINE static void SetElementId(const FMeshTriangle& Element, FOctreeElementId Id)
		{
		}
	};
	typedef TOctree<FMeshTriangle, FMeshTriangleOctreeSemantics> FMeshTriOctree;

	struct FStaticMeshReferencers
	{
		FStaticMeshReferencers()
			: RestoreBodySetup(nullptr)
		{}

		struct FReferencersInfo
		{
			FReferencersInfo(UStaticMeshComponent* InStaticMeshComponent, ECollisionEnabled::Type InCachedCollisionType)
				: StaticMeshComponent(InStaticMeshComponent)
				, CachedCollisionType(InCachedCollisionType)
			{}

			UStaticMeshComponent* StaticMeshComponent;
			ECollisionEnabled::Type CachedCollisionType;
		};

		TArray<FReferencersInfo> Referencers;
		UBodySetup* RestoreBodySetup;
	};

	typedef TMap<UStaticMesh*, FStaticMeshReferencers> FMeshToComponentMap;
	static FMeshToComponentMap MeshToComponentMap;

	UStaticMeshComponent* StaticMeshComponent;
	FStaticMeshLODResources* LODModel;
	int32 PaintingMeshLODIndex;
	int32 UVChannelIndex;
	FIndexArrayView Indices;
	TArray<FVector> MeshVertices;
	TUniquePtr<FMeshTriOctree> MeshTriOctree;
	UStaticMesh* ReferencedStaticMesh;
};

//////////////////////////////////////////////////////////////////////////
// FMeshPaintGeometryAdapterForStaticMeshesFactory

class FMeshPaintGeometryAdapterForStaticMeshesFactory : public IMeshPaintGeometryAdapterFactory
{
public:
	virtual TSharedPtr<IMeshPaintGeometryAdapter> Construct(class UMeshComponent* InComponent, int32 InPaintingMeshLODIndex, int32 InUVChannelIndex) const override;
	virtual void InitializeAdapterGlobals() override { FMeshPaintGeometryAdapterForStaticMeshes::InitializeAdapterGlobals(); }
};
