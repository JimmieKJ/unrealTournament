// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshPaintModule.h"

class UMeshComponent;
class UPaperSpriteComponent;

//////////////////////////////////////////////////////////////////////////
// FMeshPaintSpriteAdapter

class FMeshPaintSpriteAdapter : public IMeshPaintGeometryAdapter
{
public:
	static void InitializeAdapterGlobals() {}

	virtual bool Construct(UMeshComponent* InComponent, int32 InPaintingMeshLODIndex, int32 InUVChannelIndex) override;
	virtual bool Initialize() override { return true; }
	virtual void OnAdded() override {}
	virtual void OnRemoved() override {}
	virtual bool IsValid() const override { return true; }
	virtual int32 GetNumTexCoords() const override;
	virtual void GetTriangleInfo(int32 TriIndex, struct FTexturePaintTriangleInfo& OutTriInfo) const override;
	virtual bool SupportsTexturePaint() const override { return true; }
	virtual bool SupportsVertexPaint() const override { return false; }
	virtual bool LineTraceComponent(struct FHitResult& OutHit, const FVector Start, const FVector End, const struct FCollisionQueryParams& Params) const override;
	virtual TArray<uint32> SphereIntersectTriangles(const float ComponentSpaceSquaredBrushRadius, const FVector& ComponentSpaceBrushPosition, const FVector& ComponentSpaceCameraPosition, const bool bOnlyFrontFacing) const override;
	virtual void QueryPaintableTextures(int32 MaterialIndex, int32& OutDefaultIndex, TArray<struct FPaintableTexture>& InOutTextureList) override;
	virtual void ApplyOrRemoveTextureOverride(UTexture* SourceTexture, UTexture* OverrideTexture) const override;
	virtual void SetCurrentUVChannelIndex(int32 InUVChannelIndex) override {}
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override {}
	virtual const TArray<FVector>& GetMeshVertices() const override { return MeshVertices; }

protected:
	UPaperSpriteComponent* SpriteComponent;
	TArray<FVector> MeshVertices;
};

//////////////////////////////////////////////////////////////////////////
// FMeshPaintSpriteAdapterFactory

class FMeshPaintSpriteAdapterFactory : public IMeshPaintGeometryAdapterFactory
{
public:
	virtual TSharedPtr<IMeshPaintGeometryAdapter> Construct(UMeshComponent* InComponent, int32 InPaintingMeshLODIndex, int32 InUVChannelIndex) const override;
	virtual void InitializeAdapterGlobals() override { FMeshPaintSpriteAdapter::InitializeAdapterGlobals(); }
};
