// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshPaintModule.h"

class UMeshComponent;
class UPaperSpriteComponent;

//////////////////////////////////////////////////////////////////////////
// FMeshPaintSpriteAdapter

class FMeshPaintSpriteAdapter : public IMeshPaintGeometryAdapter
{
public:
	virtual bool Construct(UMeshComponent* InComponent, int32 InPaintingMeshLODIndex, int32 InUVChannelIndex) override;
	virtual int32 GetNumTexCoords() const override;
	virtual void GetTriangleInfo(int32 TriIndex, struct FTexturePaintTriangleInfo& OutTriInfo) const override;
	virtual bool SupportsTexturePaint() const override { return true; }
	virtual bool SupportsVertexPaint() const override { return false; }
	virtual bool LineTraceComponent(struct FHitResult& OutHit, const FVector Start, const FVector End, const struct FCollisionQueryParams& Params) const override;
	virtual void SphereIntersectTriangles(TArray<int32>& OutTriangles, const float ComponentSpaceSquaredBrushRadius, const FVector& ComponentSpaceBrushPosition) const override;
	virtual void QueryPaintableTextures(int32 MaterialIndex, int32& OutDefaultIndex, TArray<struct FPaintableTexture>& InOutTextureList) override;
	virtual void ApplyOrRemoveTextureOverride(UTexture* SourceTexture, UTexture* OverrideTexture) const override;

protected:
	UPaperSpriteComponent* SpriteComponent;
};

//////////////////////////////////////////////////////////////////////////
// FMeshPaintSpriteAdapterFactory

class FMeshPaintSpriteAdapterFactory : public IMeshPaintGeometryAdapterFactory
{
public:
	virtual TSharedPtr<IMeshPaintGeometryAdapter> Construct(UMeshComponent* InComponent, int32 InPaintingMeshLODIndex, int32 InUVChannelIndex) const override;
};
