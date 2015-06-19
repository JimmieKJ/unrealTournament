// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DEditorPrivatePCH.h"
#include "MeshPaintSpriteAdapter.h"
#include "MeshPaintEdMode.h"
#include "PaperSpriteComponent.h"

//////////////////////////////////////////////////////////////////////////
// FMeshPaintSpriteAdapter

bool FMeshPaintSpriteAdapter::Construct(UMeshComponent* InComponent, int32 InPaintingMeshLODIndex, int32 InUVChannelIndex)
{
	SpriteComponent = CastChecked<UPaperSpriteComponent>(InComponent);
	return SpriteComponent->GetSprite() != nullptr;
}

int32 FMeshPaintSpriteAdapter::GetNumTexCoords() const
{
	return 1;
}

void FMeshPaintSpriteAdapter::GetTriangleInfo(int32 TriIndex, struct FTexturePaintTriangleInfo& OutTriInfo) const
{
	UPaperSprite* Sprite = SpriteComponent->GetSprite();
	checkSlow(Sprite != nullptr);

	// Grab the vertex indices and points for this triangle
	const TArray<FVector4>& BakedPoints = Sprite->BakedRenderData;

	for (int32 TriVertexNum = 0; TriVertexNum < 3; ++TriVertexNum)
	{
		const int32 VertexIndex = (TriIndex * 3) + TriVertexNum;
		const FVector4& XYUV = BakedPoints[VertexIndex];

		OutTriInfo.TriVertices[TriVertexNum] = (PaperAxisX * XYUV.X) + (PaperAxisY * XYUV.Y);
		OutTriInfo.TriUVs[TriVertexNum] = FVector2D(XYUV.Z, XYUV.W);
	}
}


bool FMeshPaintSpriteAdapter::LineTraceComponent(struct FHitResult& OutHit, const FVector Start, const FVector End, const struct FCollisionQueryParams& Params) const
{
	UPaperSprite* Sprite = SpriteComponent->GetSprite();
	checkSlow(Sprite != nullptr);

	const FTransform& ComponentToWorld = SpriteComponent->ComponentToWorld;

	// Can we possibly intersect with the sprite?
	const FBoxSphereBounds& Bounds = SpriteComponent->Bounds;
	if (FMath::PointDistToSegment(Bounds.Origin, Start, End) <= Bounds.SphereRadius)
	{
		const FVector LocalStart = ComponentToWorld.InverseTransformPosition(Start);
		const FVector LocalEnd = ComponentToWorld.InverseTransformPosition(End);

		FPlane LocalSpacePlane(FVector::ZeroVector, PaperAxisX, PaperAxisY);

		FVector Intersection;
		if (FMath::SegmentPlaneIntersection(LocalStart, LocalEnd, LocalSpacePlane, /*out*/ Intersection))
		{
			const float LocalX = FVector::DotProduct(Intersection, PaperAxisX);
			const float LocalY = FVector::DotProduct(Intersection, PaperAxisY);
			const FVector LocalPoint(LocalX, LocalY, 0.0f);

			const TArray<FVector4>& BakedPoints = Sprite->BakedRenderData;
			check((BakedPoints.Num() % 3) == 0);

			for (int32 VertexIndex = 0; VertexIndex < BakedPoints.Num(); VertexIndex += 3)
			{
				const FVector& A = BakedPoints[VertexIndex + 0];
				const FVector& B = BakedPoints[VertexIndex + 1];
				const FVector& C = BakedPoints[VertexIndex + 2];
				const FVector Q = FMath::GetBaryCentric2D(LocalPoint, A, B, C);

				if ((Q.X >= 0.0f) && (Q.Y >= 0.0f) && (Q.Z >= 0.0f) && FMath::IsNearlyEqual(Q.X + Q.Y + Q.Z, 1.0f))
				{
					const FVector WorldIntersection = ComponentToWorld.TransformPosition(Intersection);

					const FVector WorldNormalFront = ComponentToWorld.TransformVectorNoScale(PaperAxisZ);
					const FVector WorldNormal = (LocalSpacePlane.PlaneDot(LocalStart) >= 0.0f) ? WorldNormalFront : -WorldNormalFront;

					OutHit.bBlockingHit = true;
					OutHit.Time = (WorldIntersection - Start).Size() / (End - Start).Size();
					OutHit.Location = WorldIntersection;
					OutHit.Normal = WorldNormal;
					OutHit.ImpactPoint = WorldIntersection;
					OutHit.ImpactNormal = WorldNormal;
					OutHit.TraceStart = Start;
					OutHit.TraceEnd = End;
					OutHit.Actor = SpriteComponent->GetOwner();
					OutHit.Component = SpriteComponent;
					OutHit.FaceIndex = VertexIndex / 3;

					return true;
				}
			}
		}
	}

	return false;
}

void FMeshPaintSpriteAdapter::SphereIntersectTriangles(TArray<int32>& OutTriangles, const float ComponentSpaceSquaredBrushRadius, const FVector& ComponentSpaceBrushPosition) const
{
	UPaperSprite* Sprite = SpriteComponent->GetSprite();
	checkSlow(Sprite != nullptr);

	//@TODO: MESHPAINT: This isn't very precise..., but since the sprite is planar it shouldn't cause any actual issues with paint UX other than being suboptimal
	const int32 NumTriangles = Sprite->BakedRenderData.Num() / 3;
	OutTriangles.Reserve(OutTriangles.Num() + NumTriangles);
	for (int32 TriIndex = 0; TriIndex < NumTriangles; ++TriIndex)
	{
		OutTriangles.Add(TriIndex);
	}
}

void FMeshPaintSpriteAdapter::QueryPaintableTextures(int32 MaterialIndex, int32& OutDefaultIndex, TArray<struct FPaintableTexture>& InOutTextureList)
{
	UPaperSprite* Sprite = SpriteComponent->GetSprite();
	checkSlow(Sprite != nullptr);

	// Grab the sprite texture first
	int32 ForceIndex = INDEX_NONE;
	if (UTexture2D* SourceTexture = Sprite->GetBakedTexture())
	{
		FPaintableTexture PaintableTexture(SourceTexture, 0);
		ForceIndex = InOutTextureList.AddUnique(PaintableTexture);
	}

	// Grab the additional textures next
	FAdditionalSpriteTextureArray AdditionalTextureList;
	Sprite->GetBakedAdditionalSourceTextures(/*out*/ AdditionalTextureList);
	for (UTexture* AdditionalTexture : AdditionalTextureList)
	{
		if (AdditionalTexture != nullptr)
		{
			FPaintableTexture PaintableTexture(AdditionalTexture, 0);
			InOutTextureList.AddUnique(AdditionalTexture);
		}
	}

	// Now ask the material
	DefaultQueryPaintableTextures(MaterialIndex, SpriteComponent, OutDefaultIndex, InOutTextureList);

	if (ForceIndex != INDEX_NONE)
	{
		OutDefaultIndex = ForceIndex;
	}
}

void FMeshPaintSpriteAdapter::ApplyOrRemoveTextureOverride(UTexture* SourceTexture, UTexture* OverrideTexture) const
{
	// Apply it to the sprite component
	SpriteComponent->SetTransientTextureOverride(SourceTexture, OverrideTexture);

	// Make sure we swap it out on any textures that aren't part of the sprite as well
	DefaultApplyOrRemoveTextureOverride(SpriteComponent, SourceTexture, OverrideTexture);
}

//////////////////////////////////////////////////////////////////////////
// FMeshPaintSpriteAdapterFactory

TSharedPtr<IMeshPaintGeometryAdapter> FMeshPaintSpriteAdapterFactory::Construct(class UMeshComponent* InComponent, int32 InPaintingMeshLODIndex, int32 InUVChannelIndex) const
{
	if (UPaperSpriteComponent* SpriteComponent = Cast<UPaperSpriteComponent>(InComponent))
	{
		if (SpriteComponent->GetSprite() != nullptr)
		{
			TSharedRef<FMeshPaintSpriteAdapter> Result = MakeShareable(new FMeshPaintSpriteAdapter());
			if (Result->Construct(InComponent, InPaintingMeshLODIndex, InUVChannelIndex))
			{
				return Result;
			}
		}
	}

	return nullptr;
}
