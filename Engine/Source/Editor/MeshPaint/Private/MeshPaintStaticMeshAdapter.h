// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshPaintModule.h"

//////////////////////////////////////////////////////////////////////////
// FMeshPaintGeometryAdapterForStaticMeshes

class FMeshPaintGeometryAdapterForStaticMeshes : public IMeshPaintGeometryAdapter
{
public:
	static void InitializeAdapterGlobals();

	virtual bool Construct(UMeshComponent* InComponent, int32 InPaintingMeshLODIndex, int32 InUVChannelIndex) override;
	virtual ~FMeshPaintGeometryAdapterForStaticMeshes();
	virtual bool InitializeMeshData() override;
	virtual void OnAdded() override;
	virtual void OnRemoved() override;
	virtual int32 GetNumTexCoords() const override;
	virtual void GetTriangleInfo(int32 TriIndex, struct FTexturePaintTriangleInfo& OutTriInfo) const override;
	virtual bool SupportsTexturePaint() const override { return true; }
	virtual bool SupportsVertexPaint() const override { return StaticMeshComponent && !StaticMeshComponent->bDisallowMeshPaintPerInstance; }
	virtual bool LineTraceComponent(struct FHitResult& OutHit, const FVector Start, const FVector End, const struct FCollisionQueryParams& Params) const override;
	virtual void SphereIntersectTriangles(TArray<int32>& OutTriangles, const float ComponentSpaceSquaredBrushRadius, const FVector& ComponentSpaceBrushPosition) const override;
	virtual void QueryPaintableTextures(int32 MaterialIndex, int32& OutDefaultIndex, TArray<struct FPaintableTexture>& InOutTextureList) override;
	virtual void ApplyOrRemoveTextureOverride(UTexture* SourceTexture, UTexture* OverrideTexture) const override;
	virtual void SetCurrentUVChannelIndex(int32 InUVChannelIndex) override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FVector GetMeshVertex(int32 Index) const override;

protected:

	void OnPostMeshBuild(UStaticMesh* StaticMesh);

	struct FStaticMeshReferencers
	{
		FStaticMeshReferencers()
			: NumReferencers(0)
			, RestoreBodySetup(nullptr)
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

		int32 NumReferencers;
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
};

//////////////////////////////////////////////////////////////////////////
// FMeshPaintGeometryAdapterForStaticMeshesFactory

class FMeshPaintGeometryAdapterForStaticMeshesFactory : public IMeshPaintGeometryAdapterFactory
{
public:
	virtual TSharedPtr<IMeshPaintGeometryAdapter> Construct(class UMeshComponent* InComponent, int32 InPaintingMeshLODIndex, int32 InUVChannelIndex) const override;
	virtual void InitializeAdapterGlobals() override { FMeshPaintGeometryAdapterForStaticMeshes::InitializeAdapterGlobals(); }
};
