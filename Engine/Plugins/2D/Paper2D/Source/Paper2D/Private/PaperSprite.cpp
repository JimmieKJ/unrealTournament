// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DPrivatePCH.h"
#include "PaperSprite.h"
#include "PhysicsEngine/BodySetup.h"
#include "PaperCustomVersion.h"

#if WITH_EDITOR
#include "UnrealEd.h"
#endif

#if WITH_EDITOR

#include "PhysicsEngine/BodySetup2D.h"
#include "PaperSpriteAtlas.h"
#include "GeomTools.h"
#include "PaperGeomTools.h"
#include "BitmapUtils.h"
#include "ComponentReregisterContext.h"


//////////////////////////////////////////////////////////////////////////
// maf

void RemoveCollinearPoints(TArray<FIntPoint>& PointList)
{
	if (PointList.Num() < 3)
	{
		return;
	}

	for (int32 VertexIndex = 1; VertexIndex < PointList.Num(); )
	{
		const FVector2D A(PointList[VertexIndex-1]);
		const FVector2D B(PointList[VertexIndex]);
		const FVector2D C(PointList[(VertexIndex+1) % PointList.Num()]);

		// Determine if the area of the triangle ABC is zero (if so, they're collinear)
		const float AreaABC = (A.X * (B.Y - C.Y)) + (B.X * (C.Y - A.Y)) + (C.X * (A.Y - B.Y));

		if (FMath::Abs(AreaABC) < KINDA_SMALL_NUMBER)
		{
			// Remove B
			PointList.RemoveAt(VertexIndex);
		}
		else
		{
			// Continue onwards
			++VertexIndex;
		}
	}
}

float DotPoints(const FIntPoint& V1, const FIntPoint& V2)
{
	return (V1.X * V2.X) + (V1.Y * V2.Y);
}

struct FDouglasPeuckerSimplifier
{
	FDouglasPeuckerSimplifier(const TArray<FIntPoint>& InSourcePoints, float Epsilon)
		: SourcePoints(InSourcePoints)
		, EpsilonSquared(Epsilon * Epsilon)
		, NumRemoved(0)
	{
		OmitPoints.AddZeroed(SourcePoints.Num());
	}

	TArray<FIntPoint> SourcePoints;
	TArray<bool> OmitPoints;
	const float EpsilonSquared;
	int32 NumRemoved;

	// Removes all points between Index1 and Index2, not including them
	void RemovePoints(int32 Index1, int32 Index2)
	{
		for (int32 Index = Index1 + 1; Index < Index2; ++Index)
		{
			OmitPoints[Index] = true;
			++NumRemoved;
		}
	}

	void SimplifyPointsInner(int Index1, int Index2)
	{
		if (Index2 - Index1 < 2)
		{
			return;
		}

		// Find furthest point from the V1..V2 line
		const FVector V1(SourcePoints[Index1].X, 0.0f, SourcePoints[Index1].Y);
		const FVector V2(SourcePoints[Index2].X, 0.0f, SourcePoints[Index2].Y);
		const FVector V1V2 = V2 - V1;
		const float LineScale = 1.0f / V1V2.SizeSquared();

		float FarthestDistanceSquared = -1.0f;
		int32 FarthestIndex = INDEX_NONE;

		for (int32 Index = Index1; Index < Index2; ++Index)
		{
			const FVector VTest(SourcePoints[Index].X, 0.0f, SourcePoints[Index].Y);
			const FVector V1VTest = VTest - V1;

			const float t = FMath::Clamp(FVector::DotProduct(V1VTest, V1V2) * LineScale, 0.0f, 1.0f);
			const FVector ClosestPointOnV1V2 = V1 + t * V1V2;

			const float DistanceToLineSquared = FVector::DistSquared(ClosestPointOnV1V2, VTest);
			if (DistanceToLineSquared > FarthestDistanceSquared)
			{
				FarthestDistanceSquared = DistanceToLineSquared;
				FarthestIndex = Index;
			}
		}

		if (FarthestDistanceSquared > EpsilonSquared)
		{
			// Too far, subdivide further
			SimplifyPointsInner(Index1, FarthestIndex);
			SimplifyPointsInner(FarthestIndex, Index2);
		}
		else
		{
			// The farthest point wasn't too far, so omit all the points in between
			RemovePoints(Index1, Index2);
		}
	}

	void Execute(TArray<FIntPoint>& Result)
	{
		SimplifyPointsInner(0, SourcePoints.Num() - 1);

		Result.Empty(SourcePoints.Num() - NumRemoved);
		for (int32 Index = 0; Index < SourcePoints.Num(); ++Index)
		{
			if (!OmitPoints[Index])
			{
				Result.Add(SourcePoints[Index]);
			}
		}
	}
};

void SimplifyPoints(TArray<FIntPoint>& Points, float Epsilon)
{
	FDouglasPeuckerSimplifier Simplifier(Points, Epsilon);
	Simplifier.Execute(Points);
}

	
//////////////////////////////////////////////////////////////////////////
// FBoundaryImage

struct FBoundaryImage
{
	TArray<int8> Pixels;

	// Value to return out of bounds
	int8 OutOfBoundsValue;

	int32 X0;
	int32 Y0;
	int32 Width;
	int32 Height;

	FBoundaryImage(const FIntPoint& Pos, const FIntPoint& Size)
	{
		OutOfBoundsValue = 0;

		X0 = Pos.X - 1;
		Y0 = Pos.Y - 1;
		Width = Size.X + 2;
		Height = Size.Y + 2;

		Pixels.AddZeroed(Width * Height);
	}

	int32 GetIndex(int32 X, int32 Y) const
	{
		const int32 LocalX = X - X0;
		const int32 LocalY = Y - Y0;

		if ((LocalX >= 0) && (LocalX < Width) && (LocalY >= 0) && (LocalY < Height))
		{
			return LocalX + (LocalY * Width);
		}
		else
		{
			return INDEX_NONE;
		}
	}

	int8 GetPixel(int32 X, int32 Y) const
	{
		const int32 Index = GetIndex(X, Y);
		if (Index != INDEX_NONE)
		{
			return Pixels[Index];
		}
		else
		{
			return OutOfBoundsValue;
		}
	}

	void SetPixel(int32 X, int32 Y, int8 Value)
	{
		const int32 Index = GetIndex(X, Y);
		if (Index != INDEX_NONE)
		{
			Pixels[Index] = Value;
		}
	}
};

void UPaperSprite::ExtractSourceRegionFromTexturePoint(const FVector2D& SourcePoint)
{
	FIntPoint SourceIntPoint(FMath::RoundToInt(SourcePoint.X), FMath::RoundToInt(SourcePoint.Y));
	FIntPoint ClosestValidPoint;

	FBitmap Bitmap(SourceTexture, 0, 0);
	if (Bitmap.IsValid() && Bitmap.FoundClosestValidPoint(SourceIntPoint.X, SourceIntPoint.Y, 10, /*out*/ ClosestValidPoint))
	{
		FIntPoint Origin;
		FIntPoint Dimension;
		if (Bitmap.HasConnectedRect(ClosestValidPoint.X, ClosestValidPoint.Y, false, /*out*/ Origin, /*out*/ Dimension))
		{
			if (Dimension.X > 0 && Dimension.Y > 0)
			{
				SourceUV = FVector2D(Origin.X, Origin.Y);
				SourceDimension = FVector2D(Dimension.X, Dimension.Y);
				PostEditChange();
			}
		}
	}
}

#endif

//////////////////////////////////////////////////////////////////////////
// FSpriteDrawCallRecord

void FSpriteDrawCallRecord::BuildFromSprite(const UPaperSprite* Sprite)
{
	if (Sprite != nullptr)
	{
		Destination = FVector::ZeroVector;
		Texture = Sprite->GetBakedTexture();
		Color = FLinearColor::White;

		RenderVerts = Sprite->BakedRenderData;
	}
}

//////////////////////////////////////////////////////////////////////////
// UPaperSprite

UPaperSprite::UPaperSprite(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Default to using physics
	SpriteCollisionDomain = ESpriteCollisionMode::Use3DPhysics;
	
	AlternateMaterialSplitIndex = INDEX_NONE;

#if WITH_EDITOR
	PivotMode = ESpritePivotMode::Center_Center;
	bSnapPivotToPixelGrid = true;

	CollisionGeometry.GeometryType = ESpritePolygonMode::TightBoundingBox;
	CollisionThickness = 10.0f;

	PixelsPerUnrealUnit = 2.56f;

	bTrimmedInSourceImage = false;
	bRotatedInSourceImage = false;

	SourceTextureDimension.Set(0, 0);
#endif

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaskedMaterialRef(TEXT("/Paper2D/MaskedUnlitSpriteMaterial"));
	DefaultMaterial = MaskedMaterialRef.Object;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> OpaqueMaterialRef(TEXT("/Paper2D/OpaqueUnlitSpriteMaterial"));
	AlternateMaterial = OpaqueMaterialRef.Object;

#if WITH_EDITOR
	// Hook into notifications for object re-imports so that the gameplay tag tree can be reconstructed if the table changes
	if (GIsEditor && !bRegisteredObjectReimport)
	{
		bRegisteredObjectReimport = true;
		FEditorDelegates::OnAssetReimport.AddUObject(this, &UPaperSprite::OnObjectReimported);
	}
#endif
}

#if WITH_EDITOR

void UPaperSprite::OnObjectReimported(UObject* InObject)
{
	// Check if its our source texture, and if its dimensions have changed
	// If SourceTetxureDimension == 0, we don't have a previous dimension to work off, so can't
	// rescale sensibly
	UTexture2D* Texture = Cast<UTexture2D>(InObject);
	if (Texture != nullptr && Texture == GetSourceTexture() && NeedRescaleSpriteData())
	{
		RescaleSpriteData(GetSourceTexture());
		PostEditChange();
	}
}

#endif


#if WITH_EDITOR

/** Removes all components that use the specified sprite asset from their scenes for the lifetime of the class. */
class FSpriteReregisterContext
{
public:
	/** Initialization constructor. */
	FSpriteReregisterContext(UPaperSprite* TargetAsset)
	{
		// Look at sprite components
		for (TObjectIterator<UPaperSpriteComponent> SpriteIt; SpriteIt; ++SpriteIt)
		{
			if (UPaperSpriteComponent* TestComponent = *SpriteIt)
			{
				if (TestComponent->GetSprite() == TargetAsset)
				{
					AddComponentToRefresh(TestComponent);
				}
			}
		}

		// Look at flipbook components
		for (TObjectIterator<UPaperFlipbookComponent> FlipbookIt; FlipbookIt; ++FlipbookIt)
		{
			if (UPaperFlipbookComponent* TestComponent = *FlipbookIt)
			{
				if (UPaperFlipbook* Flipbook = TestComponent->GetFlipbook())
				{
					if (Flipbook->ContainsSprite(TargetAsset))
					{
						AddComponentToRefresh(TestComponent);
					}
				}
			}
		}
	}

protected:
	void AddComponentToRefresh(UActorComponent* Component)
	{
		if (ComponentContexts.Num() == 0)
		{
			// wait until resources are released
			FlushRenderingCommands();
		}

		new (ComponentContexts) FComponentReregisterContext(Component);
	}

private:
	/** The recreate contexts for the individual components. */
	TIndirectArray<FComponentReregisterContext> ComponentContexts;
};

void UPaperSprite::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	//@TODO: Determine when this is really needed, as it is seriously expensive!
	FSpriteReregisterContext ReregisterExistingComponents(this);

	// Look for changed properties
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	const FName MemberPropertyName = (PropertyChangedEvent.MemberProperty != nullptr) ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;

	if (PixelsPerUnrealUnit <= 0.0f)
	{
		PixelsPerUnrealUnit = 1.0f;
	}

	if (CollisionGeometry.GeometryType == ESpritePolygonMode::Diced)
	{
		// Disallow dicing on collision geometry for now
		CollisionGeometry.GeometryType = ESpritePolygonMode::SourceBoundingBox;
	}
	RenderGeometry.PixelsPerSubdivisionX = FMath::Max(RenderGeometry.PixelsPerSubdivisionX, 4);
	RenderGeometry.PixelsPerSubdivisionY = FMath::Max(RenderGeometry.PixelsPerSubdivisionY, 4);

	if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(UPaperSprite, SourceUV))
	{
		SourceUV.X = FMath::Max(FMath::RoundToFloat(SourceUV.X), 0.0f);
		SourceUV.Y = FMath::Max(FMath::RoundToFloat(SourceUV.Y), 0.0f);
	}
	else if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(UPaperSprite, SourceDimension))
	{
		SourceDimension.X = FMath::Max(FMath::RoundToFloat(SourceDimension.X), 0.0f);
		SourceDimension.Y = FMath::Max(FMath::RoundToFloat(SourceDimension.Y), 0.0f);
	}

	// Update the pivot (roundtripping thru the function will round to a pixel position if that option is enabled)
	CustomPivotPoint = GetPivotPosition();

	bool bRenderDataModified = false;
	bool bCollisionDataModified = false;
	bool bBothModified = false;

	if ((PropertyName == GET_MEMBER_NAME_CHECKED(UPaperSprite, SpriteCollisionDomain)) ||
		(PropertyName == GET_MEMBER_NAME_CHECKED(UPaperSprite, BodySetup)) ||
		(PropertyName == GET_MEMBER_NAME_CHECKED(UPaperSprite, CollisionGeometry)) )
	{
		bCollisionDataModified = true;
	}

	// Properties inside one of the geom structures (we don't know which one)
// 	if ((PropertyName == GET_MEMBER_NAME_CHECKED(UPaperSprite, GeometryType)) ||
// 		(PropertyName == GET_MEMBER_NAME_CHECKED(UPaperSprite, AlphaThreshold)) ||
// 		)
// 		BoxSize
// 		BoxPosition
// 		Vertices
// 		VertexCount
// 		Polygons
// 	{
		bBothModified = true;
//	}

	if ((PropertyName == GET_MEMBER_NAME_CHECKED(UPaperSprite, SourceUV)) ||
		(PropertyName == GET_MEMBER_NAME_CHECKED(UPaperSprite, SourceDimension)) ||
		(PropertyName == GET_MEMBER_NAME_CHECKED(UPaperSprite, CustomPivotPoint)) ||
		(PropertyName == GET_MEMBER_NAME_CHECKED(UPaperSprite, PivotMode)) )
	{
		bBothModified = true;
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UPaperSprite, SourceTexture))
	{
		if ((SourceTexture != nullptr) && SourceDimension.IsNearlyZero())
		{
			// If this is a brand new sprite that didn't have a texture set previously, act like we were factoried with the texture
			SourceUV = FVector2D::ZeroVector;
			SourceDimension = FVector2D(SourceTexture->GetSizeX(), SourceTexture->GetSizeY());
			SourceTextureDimension = FVector2D(SourceTexture->GetSizeX(), SourceTexture->GetSizeY());
		}
		bBothModified = true;
	}

	// The texture dimensions have changed
	//if (NeedRescaleSpriteData())
	//{
		// TMP: Disabled, not sure if we want this here
		// RescaleSpriteData(GetSourceTexture());
		// bBothModified = true;
	//}


	if (bCollisionDataModified || bBothModified)
	{
		RebuildCollisionData();
	}

	if (bRenderDataModified || bBothModified)
	{
		RebuildRenderData();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UPaperSprite::RescaleSpriteData(const UTexture2D* Texture)
{
	FVector2D PreviousTextureDimension = SourceTextureDimension;
	FVector2D NewTextureDimension(Texture->GetImportedSize().X, Texture->GetImportedSize().Y);

	// Don't ever divby0 (no previously stored texture dimensions)
	// or scale to 0, should be covered by NeedRescaleSpriteData
	if (NewTextureDimension.X == 0 || NewTextureDimension.Y == 0 ||
		PreviousTextureDimension.X == 0 || PreviousTextureDimension.Y == 0)
	{
		return;
	}

	const FVector2D& S = NewTextureDimension;
	const FVector2D& D = PreviousTextureDimension;

	struct Local
	{
		static float NoSnap(const float Value, const float Scale, const float Divisor)
		{
			return (Value * Scale) / Divisor;
		}

		static FVector2D RescaleNeverSnap(const FVector2D& Value, const FVector2D& Scale, const FVector2D& Divisor)
		{
			return FVector2D(NoSnap(Value.X, Scale.X, Divisor.X), NoSnap(Value.Y, Scale.Y, Divisor.Y));
		}

		static FVector2D Rescale(const FVector2D& Value, const FVector2D& Scale, const FVector2D& Divisor)
		{
			// Never snap, want to be able to return to original values when rescaled back
			return RescaleNeverSnap(Value, Scale, Divisor);
			//return FVector2D(FMath::FloorToFloat(NoSnap(Value.X, Scale.X, Divisor.X)), FMath::FloorToFloat(NoSnap(Value.Y, Scale.Y, Divisor.Y)));
		}
	};

	// Sockets are in pivot space, convert these to texture space to apply later
	TArray<FVector2D> RescaledTextureSpaceSocketPositions;
	for (int32 SocketIndex = 0; SocketIndex < Sockets.Num(); ++SocketIndex)
	{
		FPaperSpriteSocket& Socket = Sockets[SocketIndex];
		FVector Translation = Socket.LocalTransform.GetTranslation();
		FVector2D TextureSpaceSocketPosition = ConvertPivotSpaceToTextureSpace(FVector2D(Translation.X, Translation.Z));
		RescaledTextureSpaceSocketPositions.Add(Local::RescaleNeverSnap(TextureSpaceSocketPosition, S, D));
	}

	SourceUV = Local::Rescale(SourceUV, S, D);
	SourceDimension = Local::Rescale(SourceDimension, S, D);
	SourceImageDimensionBeforeTrimming = Local::Rescale(SourceImageDimensionBeforeTrimming, S, D);
	SourceTextureDimension = NewTextureDimension;

	if (bSnapPivotToPixelGrid)
	{
		CustomPivotPoint = Local::Rescale(CustomPivotPoint, S, D);
	}
	else
	{
		CustomPivotPoint = Local::RescaleNeverSnap(CustomPivotPoint, S, D);
	}

	for (int32 GeomtryIndex = 0; GeomtryIndex < 2; ++GeomtryIndex)
	{
		FSpritePolygonCollection& Geometry = (GeomtryIndex == 0) ? CollisionGeometry : RenderGeometry;
		for (int32 PolygonIndex = 0; PolygonIndex < Geometry.Polygons.Num(); ++PolygonIndex)
		{
			FSpritePolygon& Polygon = Geometry.Polygons[PolygonIndex];
			Polygon.BoxPosition = Local::Rescale(Polygon.BoxPosition, S, D);
			Polygon.BoxSize = Local::Rescale(Polygon.BoxSize, S, D);

			for (int32 VertexIndex = 0; VertexIndex < Polygon.Vertices.Num(); ++VertexIndex)
			{
				Polygon.Vertices[VertexIndex] = Local::Rescale(Polygon.Vertices[VertexIndex], S, D);
			}
		}
	}

	// Apply texture space pivot positions now that pivot space is correctly defined
	for (int32 SocketIndex = 0; SocketIndex < Sockets.Num(); ++SocketIndex)
	{
		FPaperSpriteSocket& Socket = Sockets[SocketIndex];
		FVector2D PivotSpaceSocketPosition = ConvertTextureSpaceToPivotSpace(RescaledTextureSpaceSocketPositions[SocketIndex]);
		FVector Translation = Socket.LocalTransform.GetTranslation();
		Translation.X = PivotSpaceSocketPosition.X;
		Translation.Z = PivotSpaceSocketPosition.Y;
		Socket.LocalTransform.SetTranslation(Translation);
	}
}

bool UPaperSprite::NeedRescaleSpriteData()
{
	if (UTexture2D* Texture = GetSourceTexture())
	{
		FIntPoint TextureSize = Texture->GetImportedSize();
		bool bTextureSizeIsZero = TextureSize.X == 0 || TextureSize.Y == 0;
		return !SourceTextureDimension.IsZero() && !bTextureSizeIsZero && (TextureSize.X != SourceTextureDimension.X || TextureSize.Y != SourceTextureDimension.Y);
	}

	return false;
}

void UPaperSprite::RebuildCollisionData()
{
	UBodySetup* OldBodySetup = BodySetup;

	// Ensure we have the data structure for the desired collision method
	switch (SpriteCollisionDomain)
	{
	case ESpriteCollisionMode::Use3DPhysics:
		BodySetup = nullptr;
		if (BodySetup == nullptr)
		{
			BodySetup = NewObject<UBodySetup>(this);
		}
		break;
	case ESpriteCollisionMode::Use2DPhysics:
		BodySetup = nullptr;
		if (BodySetup == nullptr)
		{
			BodySetup = NewObject<UBodySetup2D>(this);
		}
		break;
	case ESpriteCollisionMode::None:
		BodySetup = nullptr;
		CollisionGeometry.Reset();
		break;
	}

	if (SpriteCollisionDomain != ESpriteCollisionMode::None)
	{
		BodySetup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
		switch (CollisionGeometry.GeometryType)
		{
		case ESpritePolygonMode::Diced:
		case ESpritePolygonMode::SourceBoundingBox:
			BuildBoundingBoxCollisionData(/*bUseTightBounds=*/ false);
			break;

		case ESpritePolygonMode::TightBoundingBox:
			BuildBoundingBoxCollisionData(/*bUseTightBounds=*/ true);
			break;

		case ESpritePolygonMode::ShrinkWrapped:
			BuildGeometryFromContours(CollisionGeometry);
			BuildCustomCollisionData();
			break;

		case ESpritePolygonMode::FullyCustom:
			BuildCustomCollisionData();
			break;
		default:
			check(false); // unknown mode
		};

		// Copy across or initialize the only editable property we expose on the body setup
		if (OldBodySetup != nullptr)
		{
			BodySetup->DefaultInstance.CopyBodyInstancePropertiesFrom(&(OldBodySetup->DefaultInstance));
		}
		else
		{
			BodySetup->DefaultInstance.SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
		}
	}
}

void UPaperSprite::BuildCustomCollisionData()
{
	// Rebuild the runtime geometry
	TArray<FVector2D> CollisionData;
	Triangulate(CollisionGeometry, CollisionData);


	// Adjust the collision data to be relative to the pivot and scaled from pixels to uu
	const float UnitsPerPixel = GetUnrealUnitsPerPixel();
	for (FVector2D& Point : CollisionData)
	{
		Point = ConvertTextureSpaceToPivotSpace(Point) * UnitsPerPixel;
	}

	//@TODO: Observe if the custom data is really a hand-edited bounding box, and generate box geom instead of convex geom!
	//@TODO: Use this guy instead: DecomposeMeshToHulls
	//@TODO: Merge triangles that are convex together!

	// Bake it to the runtime structure
	switch (SpriteCollisionDomain)
	{
	case ESpriteCollisionMode::Use3DPhysics:
		{
			checkSlow(BodySetup);
			UBodySetup* BodySetup3D = BodySetup;
			BodySetup3D->AggGeom.EmptyElements();

			const FVector HalfThicknessVector = PaperAxisZ * 0.5f * CollisionThickness;

			int32 RunningIndex = 0;
			for (int32 TriIndex = 0; TriIndex < CollisionData.Num() / 3; ++TriIndex)
			{
				FKConvexElem& ConvexTri = *new (BodySetup3D->AggGeom.ConvexElems) FKConvexElem();
				ConvexTri.VertexData.Empty(6);
				for (int32 Index = 0; Index < 3; ++Index)
				{
					const FVector2D& Pos2D = CollisionData[RunningIndex++];
					
					const FVector Pos3D = (PaperAxisX * Pos2D.X) + (PaperAxisY * Pos2D.Y);

					new (ConvexTri.VertexData) FVector(Pos3D - HalfThicknessVector);
					new (ConvexTri.VertexData) FVector(Pos3D + HalfThicknessVector);
				}
				ConvexTri.UpdateElemBox();
			}

			BodySetup3D->InvalidatePhysicsData();
			BodySetup3D->CreatePhysicsMeshes();
		}
		break;
	case ESpriteCollisionMode::Use2DPhysics:
		{
			UBodySetup2D* BodySetup2D = CastChecked<UBodySetup2D>(BodySetup);
			BodySetup2D->AggGeom2D.EmptyElements();

			int32 RunningIndex = 0;
			for (int32 TriIndex = 0; TriIndex < CollisionData.Num() / 3; ++TriIndex)
			{
				FConvexElement2D& ConvexTri = *new (BodySetup2D->AggGeom2D.ConvexElements) FConvexElement2D();
				ConvexTri.VertexData.Empty(3);
				for (int32 Index = 0; Index < 3; ++Index)
				{
					const FVector2D& Pos2D = CollisionData[RunningIndex++];
					new (ConvexTri.VertexData) FVector2D(Pos2D);
				}
			}

			BodySetup2D->InvalidatePhysicsData();
			BodySetup2D->CreatePhysicsMeshes();
		}
		break;
	default:
		check(false);
		break;
	}
}

void UPaperSprite::BuildBoundingBoxCollisionData(bool bUseTightBounds)
{
	// Update the polygon data
	CreatePolygonFromBoundingBox(CollisionGeometry, bUseTightBounds);

	// Bake it to the runtime structure
	const float UnitsPerPixel = GetUnrealUnitsPerPixel();

	switch (SpriteCollisionDomain)
	{
	case ESpriteCollisionMode::Use3DPhysics:
		{
			// Store the bounding box as an actual box for 3D physics
			checkSlow(BodySetup);
			UBodySetup* BodySetup3D = BodySetup;
			BodySetup3D->AggGeom.EmptyElements();

			// Determine the box size and center in pivot space
			const FVector2D& BoxSize2DInPixels = CollisionGeometry.Polygons[0].BoxSize;
			const FVector2D& BoxPos = CollisionGeometry.Polygons[0].BoxPosition;
			const FVector2D CenterInTextureSpace = BoxPos + (BoxSize2DInPixels * 0.5f);
			const FVector2D CenterInPivotSpace = ConvertTextureSpaceToPivotSpace(CenterInTextureSpace);

			// Convert from pixels to uu
			const FVector2D BoxSize2D = BoxSize2DInPixels * UnitsPerPixel;
			const FVector2D CenterInScaledSpace = CenterInPivotSpace * UnitsPerPixel;

			// Create a new box primitive
			const FVector BoxSize3D = (PaperAxisX * BoxSize2D.X) + (PaperAxisY * BoxSize2D.Y) + (PaperAxisZ * CollisionThickness);

			FKBoxElem& Box = *new (BodySetup3D->AggGeom.BoxElems) FKBoxElem(FMath::Abs(BoxSize3D.X), FMath::Abs(BoxSize3D.Y), FMath::Abs(BoxSize3D.Z));
			Box.Center = (PaperAxisX * CenterInScaledSpace.X) + (PaperAxisY * CenterInScaledSpace.Y);

			BodySetup3D->InvalidatePhysicsData();
			BodySetup3D->CreatePhysicsMeshes();
		}
		break;
	case ESpriteCollisionMode::Use2DPhysics:
		{
			UBodySetup2D* BodySetup2D = CastChecked<UBodySetup2D>(BodySetup);
			BodySetup2D->AggGeom2D.EmptyElements();

			// Determine the box center in pivot space
			const FVector2D& BoxSize2DInPixels = CollisionGeometry.Polygons[0].BoxSize;
			const FVector2D& BoxPos = CollisionGeometry.Polygons[0].BoxPosition;
			const FVector2D CenterInTextureSpace = BoxPos + (BoxSize2DInPixels * 0.5f);
			const FVector2D CenterInPivotSpace = ConvertTextureSpaceToPivotSpace(CenterInTextureSpace);

			// Convert from pixels to uu
			const FVector2D BoxSize2D = BoxSize2DInPixels * UnitsPerPixel;
			const FVector2D CenterInScaledSpace = CenterInPivotSpace * UnitsPerPixel;

			// Create a new box primitive
			FBoxElement2D& Box = *new (BodySetup2D->AggGeom2D.BoxElements) FBoxElement2D();
			Box.Width = FMath::Abs(BoxSize2D.X);
			Box.Height = FMath::Abs(BoxSize2D.Y);
			Box.Center.X = CenterInScaledSpace.X;
			Box.Center.Y = CenterInScaledSpace.Y;

			BodySetup2D->InvalidatePhysicsData();
			BodySetup2D->CreatePhysicsMeshes();
		}
		break;
	default:
		check(false);
		break;
	}
}

void UPaperSprite::RebuildRenderData()
{
	FSpritePolygonCollection AlternateGeometry;

	switch (RenderGeometry.GeometryType)
	{
	case ESpritePolygonMode::Diced:
	case ESpritePolygonMode::SourceBoundingBox:
		CreatePolygonFromBoundingBox(RenderGeometry, /*bUseTightBounds=*/ false);
		break;

	case ESpritePolygonMode::TightBoundingBox:
		CreatePolygonFromBoundingBox(RenderGeometry, /*bUseTightBounds=*/ true);
		break;

	case ESpritePolygonMode::ShrinkWrapped:
		BuildGeometryFromContours(RenderGeometry);
		break;

	case ESpritePolygonMode::FullyCustom:
		// Do nothing special, the data is already in the polygon
		break;
	default:
		check(false); // unknown mode
	};

	// Determine the texture size
	UTexture2D* EffectiveTexture = GetBakedTexture();

	FVector2D TextureSize(1.0f, 1.0f);
	if (EffectiveTexture)
	{
		EffectiveTexture->ConditionalPostLoad();
		const int32 TextureWidth = EffectiveTexture->GetSizeX();
		const int32 TextureHeight = EffectiveTexture->GetSizeY();
		if (ensure((TextureWidth > 0) && (TextureHeight > 0)))
		{
			TextureSize = FVector2D(TextureWidth, TextureHeight);
		}
	}
	const float InverseWidth = 1.0f / TextureSize.X;
	const float InverseHeight = 1.0f / TextureSize.Y;

	// Adjust for the pivot and store in the baked geometry buffer
	const FVector2D DeltaUV((BakedSourceTexture != nullptr) ? (BakedSourceUV - SourceUV) : FVector2D::ZeroVector);

	const float UnitsPerPixel = GetUnrealUnitsPerPixel();

	if ((RenderGeometry.GeometryType == ESpritePolygonMode::Diced) && (EffectiveTexture != nullptr))
	{
		const int32 AlphaThresholdInt = FMath::Clamp<int32>(RenderGeometry.AlphaThreshold * 255, 0, 255);
		FAlphaBitmap SourceBitmap(EffectiveTexture);
		SourceBitmap.ThresholdImageBothWays(AlphaThresholdInt, 255);

		bool bSeparateOpaqueSections = true;

		// Dice up the source geometry and sort into translucent and opaque sections
		RenderGeometry.Polygons.Empty();

		const int32 X0 = (int32)SourceUV.X;
		const int32 Y0 = (int32)SourceUV.Y;
		const int32 X1 = (int32)(SourceUV.X + SourceDimension.X);
		const int32 Y1 = (int32)(SourceUV.Y + SourceDimension.Y);

		for (int32 Y = Y0; Y < Y1; Y += RenderGeometry.PixelsPerSubdivisionY)
		{
			const int32 TileHeight = FMath::Min(RenderGeometry.PixelsPerSubdivisionY, Y1 - Y);

			for (int32 X = X0; X < X1; X += RenderGeometry.PixelsPerSubdivisionX)
			{
				const int32 TileWidth = FMath::Min(RenderGeometry.PixelsPerSubdivisionX, X1 - X);

				if (!SourceBitmap.IsRegionEmpty(X, Y, X + TileWidth - 1, Y + TileHeight - 1))
				{
					FIntPoint Origin(X, Y);
					FIntPoint Dimension(TileWidth, TileHeight);
					
					SourceBitmap.TightenBounds(Origin, Dimension);

					bool bOpaqueSection = false;
					if (bSeparateOpaqueSections)
					{
						if (SourceBitmap.IsRegionEqual(Origin.X, Origin.Y, Origin.X + Dimension.X - 1, Origin.Y + Dimension.Y - 1, 255))
						{
							bOpaqueSection = true;
						}
					}

					if (bOpaqueSection)
					{
						AlternateGeometry.AddRectanglePolygon(Origin, Dimension);
					}
					else
					{
						RenderGeometry.AddRectanglePolygon(Origin, Dimension);
					}
				}
			}
		}
	}

	// Triangulate the render geometry
	TArray<FVector2D> TriangluatedPoints;
	Triangulate(RenderGeometry, /*out*/ TriangluatedPoints);

	// Triangulate the alternate render geometry, if present
	if (AlternateGeometry.Polygons.Num() > 0)
	{
		TArray<FVector2D> AlternateTriangluatedPoints;
		Triangulate(AlternateGeometry, /*out*/ AlternateTriangluatedPoints);

		AlternateMaterialSplitIndex = TriangluatedPoints.Num();
		TriangluatedPoints.Append(AlternateTriangluatedPoints);
		RenderGeometry.Polygons.Append(AlternateGeometry.Polygons);
	}
	else
	{
		AlternateMaterialSplitIndex = INDEX_NONE;
	}

	// Bake the verts
	BakedRenderData.Empty(TriangluatedPoints.Num());
	for (int32 PointIndex = 0; PointIndex < TriangluatedPoints.Num(); ++PointIndex)
	{
		const FVector2D& SourcePos = TriangluatedPoints[PointIndex];

		const FVector2D PivotSpacePos = ConvertTextureSpaceToPivotSpace(SourcePos);
		const FVector2D UV(SourcePos + DeltaUV);

		new (BakedRenderData) FVector4(PivotSpacePos.X * UnitsPerPixel, PivotSpacePos.Y * UnitsPerPixel, UV.X * InverseWidth, UV.Y * InverseHeight);
	}
	check((BakedRenderData.Num() % 3) == 0);
}

void UPaperSprite::FindTextureBoundingBox(float AlphaThreshold, /*out*/ FVector2D& OutBoxPosition, /*out*/ FVector2D& OutBoxSize)
{
	// Create an initial guess at the bounds based on the source rectangle
	int32 LeftBound = (int32)SourceUV.X;
	int32 RightBound = (int32)(SourceUV.X + SourceDimension.X - 1);

	int32 TopBound = (int32)SourceUV.Y;
	int32 BottomBound = (int32)(SourceUV.Y + SourceDimension.Y - 1);


	int32 AlphaThresholdInt = FMath::Clamp<int32>(AlphaThreshold * 255, 0, 255);
	FBitmap SourceBitmap(SourceTexture, AlphaThresholdInt);
	if (SourceBitmap.IsValid())
	{
		// Make sure the initial bounds starts in the texture
		LeftBound = FMath::Clamp(LeftBound, 0, SourceBitmap.Width-1);
		RightBound = FMath::Clamp(RightBound, 0, SourceBitmap.Width-1);
		TopBound = FMath::Clamp(TopBound, 0, SourceBitmap.Height-1);
		BottomBound = FMath::Clamp(BottomBound, 0, SourceBitmap.Height-1);

		// Pull it in from the top
		while ((TopBound < BottomBound) && SourceBitmap.IsRowEmpty(LeftBound, RightBound, TopBound))
		{
			++TopBound;
		}

		// Pull it in from the bottom
		while ((BottomBound > TopBound) && SourceBitmap.IsRowEmpty(LeftBound, RightBound, BottomBound))
		{
			--BottomBound;
		}

		// Pull it in from the left
		while ((LeftBound < RightBound) && SourceBitmap.IsColumnEmpty(LeftBound, TopBound, BottomBound))
		{
			++LeftBound;
		}

		// Pull it in from the right
		while ((RightBound > LeftBound) && SourceBitmap.IsColumnEmpty(RightBound, TopBound, BottomBound))
		{
			--RightBound;
		}
	}

	OutBoxSize.X = RightBound - LeftBound + 1;
	OutBoxSize.Y = BottomBound - TopBound + 1;
	OutBoxPosition.X = LeftBound;
	OutBoxPosition.Y = TopBound;
}

void UPaperSprite::BuildGeometryFromContours(FSpritePolygonCollection& GeomOwner)
{
	// First trim the image to the tight fitting bounding box (the other pixels can't matter)
	FVector2D InitialBoxSizeFloat;
	FVector2D InitialBoxPosFloat;
	FindTextureBoundingBox(GeomOwner.AlphaThreshold, /*out*/ InitialBoxPosFloat, /*out*/ InitialBoxSizeFloat);

	const FIntPoint InitialPos((int32)InitialBoxPosFloat.X, (int32)InitialBoxPosFloat.Y);
	const FIntPoint InitialSize((int32)InitialBoxSizeFloat.X, (int32)InitialBoxSizeFloat.Y);

	// Find the contours
	TArray< TArray<FIntPoint> > Contours;
	FindContours(InitialPos, InitialSize, GeomOwner.AlphaThreshold, SourceTexture, /*out*/ Contours);

	//@TODO: Remove fully enclosed contours - may not be needed if we're tracing only outsides

	// Convert the contours into geometry
	GeomOwner.Polygons.Empty();
	for (int32 ContourIndex = 0; ContourIndex < Contours.Num(); ++ContourIndex)
	{
		//@TODO: Optimize contours
		TArray<FIntPoint>& Contour = Contours[ContourIndex];

		SimplifyPoints(Contour, GeomOwner.SimplifyEpsilon);

		if (Contour.Num() > 0)
		{
			FSpritePolygon& Polygon = *new (GeomOwner.Polygons) FSpritePolygon();
			Polygon.Vertices.Empty(Contour.Num());

			for (int32 PointIndex = 0; PointIndex < Contour.Num(); ++PointIndex)
			{
				new (Polygon.Vertices) FVector2D(Contour[PointIndex]);
			}

			// Get intended winding
			Polygon.bNegativeWinding = !PaperGeomTools::IsPolygonWindingCCW(Polygon.Vertices);
		}
	}
}


// findme

void UPaperSprite::FindContours(const FIntPoint& ScanPos, const FIntPoint& ScanSize, float AlphaThreshold, UTexture2D* Texture, TArray< TArray<FIntPoint> >& OutPoints)
{
	OutPoints.Empty();

	if ((ScanSize.X <= 0) || (ScanSize.Y <= 0))
	{
		return;
	}

	const int32 LeftBound = ScanPos.X;
	const int32 RightBound = ScanPos.X + ScanSize.X - 1;
	const int32 TopBound = ScanPos.Y;
	const int32 BottomBound = ScanPos.Y + ScanSize.Y - 1;

	// Neighborhood array (clockwise starting at -X,-Y; assuming prev is at -X)
	const int32 NeighborX[] = {-1, 0,+1,+1,+1, 0,-1,-1};
	const int32 NeighborY[] = {-1,-1,-1, 0,+1,+1,+1, 0};
	//                       0  1  2  3  4  5  6  7
	// 0 1 2
	// 7   3
	// 6 5 4
	const int32 StateMutation[] = {
		5, //from0
		6, //from1
		7, //from2
		0, //from3
		1, //from4
		2, //from5
		3, //from6
		4, //from7
	};

	int32 AlphaThresholdInt = FMath::Clamp<int32>(AlphaThreshold * 255, 0, 255);
	FBitmap SourceBitmap(Texture, AlphaThresholdInt);
	if (SourceBitmap.IsValid())
	{
		checkSlow((LeftBound >= 0) && (TopBound >= 0) && (RightBound < SourceBitmap.Width) && (BottomBound < SourceBitmap.Height));

		// Create the 'output' boundary image
		FBoundaryImage BoundaryImage(ScanPos, ScanSize);

		bool bInsideBoundary = false;

		for (int32 Y = TopBound - 1; Y < BottomBound + 2; ++Y)
		{
			for (int32 X = LeftBound - 1; X < RightBound + 2; ++X)
			{
				const bool bAlreadyTaggedAsBoundary = BoundaryImage.GetPixel(X, Y) > 0;
				const bool bPixelInsideBounds = (X >= LeftBound && X <= RightBound && Y >= TopBound && Y <= BottomBound);
				const bool bIsFilledPixel = bPixelInsideBounds && SourceBitmap.GetPixel(X, Y) != 0;

				if (bInsideBoundary)
				{
					if (!bIsFilledPixel)
					{
						// We're leaving the known boundary
						bInsideBoundary = false;
					}
				}
				else
				{
					if (bAlreadyTaggedAsBoundary)
					{
						// We're re-entering a known boundary
						bInsideBoundary = true;
					}
					else if (bIsFilledPixel)
					{
						// Create the output chain we'll build from the boundary image
						TArray<FIntPoint>& Contour = *new (OutPoints) TArray<FIntPoint>();

						// Moving into an undiscovered boundary
						BoundaryImage.SetPixel(X, Y, 1);
						new (Contour) FIntPoint(X, Y);

						// Current pixel
						int32 NeighborPhase = 0;
						int32 PX = X;
						int32 PY = Y;

						int32 EnteredStartingSquareCount = 0;
						int32 SinglePixelCounter = 0;

						for (;;)
						{
							// Test pixel (clockwise from the current pixel)
							const int32 CX = PX + NeighborX[NeighborPhase];
							const int32 CY = PY + NeighborY[NeighborPhase];
							const bool bTestPixelInsideBounds = (CX >= LeftBound && CX <= RightBound && CY >= TopBound && CY <= BottomBound);
							const bool bTestPixelPasses = bTestPixelInsideBounds && SourceBitmap.GetPixel(CX, CY) != 0;

							UE_LOG(LogPaper2D, Log, TEXT("Outer P(%d,%d), C(%d,%d) Ph%d %s"),
								PX, PY, CX, CY, NeighborPhase, bTestPixelPasses ? TEXT("[BORDER]") : TEXT("[]"));

							if (bTestPixelPasses)
							{
								// Move to the next pixel

								// Check to see if we closed the loop
 								if ((CX == X) && (CY == Y))
 								{
// 									// If we went thru the boundary pixel more than two times, or
// 									// entered it from the same way we started, then we're done
// 									++EnteredStartingSquareCount;
// 									if ((EnteredStartingSquareCount > 2) || (NeighborPhase == 0))
// 									{
									//@TODO: Not good enough, will early out too soon some of the time!
 										bInsideBoundary = true;
 										break;
 									}
// 								}

								BoundaryImage.SetPixel(CX, CY, NeighborPhase+1);
								new (Contour) FIntPoint(CX, CY);

								PX = CX;
								PY = CY;
								NeighborPhase = StateMutation[NeighborPhase];

								SinglePixelCounter = 0;
								//NeighborPhase = (NeighborPhase + 1) % 8;
							}
							else
							{
								NeighborPhase = (NeighborPhase + 1) % 8;

								++SinglePixelCounter;
								if (SinglePixelCounter > 8)
								{
									// Went all the way around the neighborhood; it's an island of a single pixel
									break;
								}
							}
						}
					
						// Remove collinear points from the result
						RemoveCollinearPoints(/*inout*/ Contour);
					}
				}
			}
		}
	}
}

void UPaperSprite::CreatePolygonFromBoundingBox(FSpritePolygonCollection& GeomOwner, bool bUseTightBounds)
{
	FVector2D BoxSize;
	FVector2D BoxPosition;

	if (bUseTightBounds)
	{
		FindTextureBoundingBox(GeomOwner.AlphaThreshold, /*out*/ BoxPosition, /*out*/ BoxSize);
	}
	else
	{
		BoxSize = SourceDimension;
		BoxPosition = SourceUV;
	}

	// Put the bounding box into the geometry array
	GeomOwner.Polygons.Empty();
	GeomOwner.AddRectanglePolygon(BoxPosition, BoxSize);
}

void UPaperSprite::Triangulate(const FSpritePolygonCollection& Source, TArray<FVector2D>& Target)
{
	Target.Empty();
	TArray<FVector2D> AllGeneratedTriangles;
	
	// AOS -> Validate -> SOA
	TArray<bool> PolygonsNegativeWinding; // do these polygons have negative winding?
	TArray<TArray<FVector2D> > ValidPolygonTriangles;
	PolygonsNegativeWinding.Empty(Source.Polygons.Num());
	ValidPolygonTriangles.Empty(Source.Polygons.Num());
	bool bSourcePolygonHasHoles = false;

	// Correct polygon winding for additive and subtractive polygons
	// Invalid polygons (< 3 verts) removed from this list
	TArray<FSpritePolygon> ValidPolygons;
	for (int32 PolygonIndex = 0; PolygonIndex < Source.Polygons.Num(); ++PolygonIndex)
	{
		const FSpritePolygon& SourcePolygon = Source.Polygons[PolygonIndex];
		if (SourcePolygon.Vertices.Num() >= 3)
		{
			TArray<FVector2D>* FixedVertices = new (ValidPolygonTriangles)TArray<FVector2D>();
			PaperGeomTools::CorrectPolygonWinding(*FixedVertices, SourcePolygon.Vertices, SourcePolygon.bNegativeWinding);
			PolygonsNegativeWinding.Add(SourcePolygon.bNegativeWinding);
		}

		if (Source.Polygons[PolygonIndex].bNegativeWinding)
		{
			bSourcePolygonHasHoles = true;
		}
	}

	// Check if polygons overlap, or have inconsistent winding, or edges overlap
	if (!PaperGeomTools::ArePolygonsValid(ValidPolygonTriangles))
	{
		return;
	}

	// Merge each additive and associated subtractive polygons to form a list of polygons in CCW winding
	ValidPolygonTriangles = PaperGeomTools::ReducePolygons(ValidPolygonTriangles, PolygonsNegativeWinding);

	// Triangulate the polygons
	for (int32 PolygonIndex = 0; PolygonIndex < ValidPolygonTriangles.Num(); ++PolygonIndex)
	{
		TArray<FVector2D> Generated2DTriangles;
		if (PaperGeomTools::TriangulatePoly(Generated2DTriangles, ValidPolygonTriangles[PolygonIndex], Source.bAvoidVertexMerging))
		{
			AllGeneratedTriangles.Append(Generated2DTriangles);
		}
	}

	// This doesn't work when polys have holes as edges will likely form a loop around the poly
	if (!bSourcePolygonHasHoles && !Source.bAvoidVertexMerging && (Source.Polygons.Num() > 1 && AllGeneratedTriangles.Num() > 1))
	{
		TArray<FVector2D> TrianglesCopy = AllGeneratedTriangles;
		AllGeneratedTriangles.Empty();
		PaperGeomTools::RemoveRedundantTriangles(/*out*/AllGeneratedTriangles, TrianglesCopy);
	}

	Target.Append(AllGeneratedTriangles);
}

void UPaperSprite::ExtractRectsFromTexture(UTexture2D* Texture, TArray<FIntRect>& OutRects)
{
	FBitmap SpriteTextureBitmap(Texture, 0, 0);
	SpriteTextureBitmap.ExtractRects(/*out*/ OutRects);
}

void UPaperSprite::InitializeSprite(const FSpriteAssetInitParameters& InitParams)
{
	if (InitParams.bNewlyCreated)
	{
		const UPaperRuntimeSettings* DefaultSettings = GetDefault<UPaperRuntimeSettings>();
		PixelsPerUnrealUnit = DefaultSettings->DefaultPixelsPerUnrealUnit;

		if (UMaterialInterface* TranslucentMaterial = LoadObject<UMaterialInterface>(nullptr, *DefaultSettings->DefaultTranslucentSpriteMaterialName.ToString(), nullptr, LOAD_None, nullptr))
		{
			DefaultMaterial = TranslucentMaterial;
		}

		if (UMaterialInterface* OpaqueMaterial = LoadObject<UMaterialInterface>(nullptr, *DefaultSettings->DefaultOpaqueSpriteMaterialName.ToString(), nullptr, LOAD_None, nullptr))
		{
			AlternateMaterial = OpaqueMaterial;
		}
	}

	SourceTexture = InitParams.Texture;
	if (SourceTexture != nullptr)
	{
		SourceTextureDimension.Set(SourceTexture->GetImportedSize().X, SourceTexture->GetImportedSize().Y);
	}
	else
	{
		SourceTextureDimension.Set(0, 0);
	}
	SourceUV = InitParams.Offset;
	SourceDimension = InitParams.Dimension;

	RebuildCollisionData();
	RebuildRenderData();
}

void UPaperSprite::SetTrim(bool bTrimmed, const FVector2D& OriginInSourceImage, const FVector2D& SourceImageDimension)
{
	this->bTrimmedInSourceImage = bTrimmed;
	this->OriginInSourceImageBeforeTrimming = OriginInSourceImage;
	this->SourceImageDimensionBeforeTrimming = SourceImageDimension;
	RebuildRenderData();
	RebuildCollisionData();
}

void UPaperSprite::SetRotated(bool bRotated)
{
	this->bRotatedInSourceImage = bRotated;
	RebuildRenderData();
	RebuildCollisionData();
}

void UPaperSprite::SetPivotMode(ESpritePivotMode::Type PivotMode, FVector2D CustomTextureSpacePivot)
{
	this->PivotMode = PivotMode;
	this->CustomPivotPoint = CustomTextureSpacePivot;
	RebuildRenderData();
	RebuildCollisionData();
}

FVector2D UPaperSprite::ConvertTextureSpaceToPivotSpace(FVector2D Input) const
{
	const FVector2D Pivot = GetPivotPosition();

	const float X = Input.X - Pivot.X;
	const float Y = -Input.Y + Pivot.Y;

	return bRotatedInSourceImage ? FVector2D(-Y, X) : FVector2D(X, Y);
}

FVector2D UPaperSprite::ConvertPivotSpaceToTextureSpace(FVector2D Input) const
{
	const FVector2D Pivot = GetPivotPosition();

	if (bRotatedInSourceImage)
	{
		Swap(Input.X, Input.Y);
		Input.Y = -Input.Y;
	}

	const float X = Input.X + Pivot.X;
	const float Y = -Input.Y + Pivot.Y;

	return FVector2D(X, Y);
}

FVector UPaperSprite::ConvertTextureSpaceToPivotSpace(FVector Input) const
{
	const FVector2D Pivot = GetPivotPosition();

	const float X = Input.X - Pivot.X;
	const float Z = -Input.Z + Pivot.Y;

	return FVector(X, Input.Y, Z);
}

FVector UPaperSprite::ConvertPivotSpaceToTextureSpace(FVector Input) const
{
	const FVector2D Pivot = GetPivotPosition();

	const float X = Input.X + Pivot.X;
	const float Z = -Input.Z + Pivot.Y;

	return FVector(X, Input.Y, Z);
}

FVector UPaperSprite::ConvertTextureSpaceToWorldSpace(const FVector2D& SourcePoint) const
{
	const float UnitsPerPixel = GetUnrealUnitsPerPixel();

	const FVector2D SourcePointInUU = ConvertTextureSpaceToPivotSpace(SourcePoint) * UnitsPerPixel;
	return (PaperAxisX * SourcePointInUU.X) + (PaperAxisY * SourcePointInUU.Y);
}

FVector2D UPaperSprite::ConvertWorldSpaceToTextureSpace(const FVector& WorldPoint) const
{
	const FVector ProjectionX = WorldPoint.ProjectOnTo(PaperAxisX);
	const FVector ProjectionY = WorldPoint.ProjectOnTo(PaperAxisY);

	const float XValue = FMath::Sign(ProjectionX | PaperAxisX) * ProjectionX.Size() * PixelsPerUnrealUnit;
	const float YValue = FMath::Sign(ProjectionY | PaperAxisY) * ProjectionY.Size() * PixelsPerUnrealUnit;

	return ConvertPivotSpaceToTextureSpace(FVector2D(XValue, YValue));
}

FVector2D UPaperSprite::ConvertWorldSpaceDeltaToTextureSpace(const FVector& WorldSpaceDelta) const
{
	const FVector ProjectionX = WorldSpaceDelta.ProjectOnTo(PaperAxisX);
	const FVector ProjectionY = WorldSpaceDelta.ProjectOnTo(PaperAxisY);

	float XValue = FMath::Sign(ProjectionX | PaperAxisX) * ProjectionX.Size() * PixelsPerUnrealUnit;
	float YValue = FMath::Sign(ProjectionY | PaperAxisY) * ProjectionY.Size() * PixelsPerUnrealUnit;

	// Undo pivot space rotation, ignoring pivot position
	if (bRotatedInSourceImage)
	{
		Swap(XValue, YValue);
		XValue = -XValue;
	}

	return FVector2D(XValue, YValue);
}

FTransform UPaperSprite::GetPivotToWorld() const
{
	const FVector Translation(0, 0, 0);
	const FVector Scale3D(GetUnrealUnitsPerPixel());
	return FTransform(FRotator::ZeroRotator, Translation, Scale3D);
}

FVector2D UPaperSprite::GetRawPivotPosition() const
{
	FVector2D TopLeftUV = SourceUV;
	FVector2D Dimension = SourceDimension;
	
	if (bTrimmedInSourceImage)
	{
		TopLeftUV = SourceUV - OriginInSourceImageBeforeTrimming;
		Dimension = SourceImageDimensionBeforeTrimming;
	}

	if (bRotatedInSourceImage)
	{
		switch (PivotMode)
		{
		case ESpritePivotMode::Top_Left:
			return FVector2D(TopLeftUV.X + Dimension.X, TopLeftUV.Y);
		case ESpritePivotMode::Top_Center:
			return FVector2D(TopLeftUV.X + Dimension.X, TopLeftUV.Y + Dimension.Y * 0.5f);
		case ESpritePivotMode::Top_Right:
			return FVector2D(TopLeftUV.X + Dimension.X, TopLeftUV.Y + Dimension.Y);
		case ESpritePivotMode::Center_Left:
			return FVector2D(TopLeftUV.X + Dimension.X * 0.5f, TopLeftUV.Y);
		case ESpritePivotMode::Center_Center:
			return FVector2D(TopLeftUV.X + Dimension.X * 0.5f, TopLeftUV.Y + Dimension.Y * 0.5f);
		case ESpritePivotMode::Center_Right:
			return FVector2D(TopLeftUV.X + Dimension.X * 0.5f, TopLeftUV.Y + Dimension.Y);
		case ESpritePivotMode::Bottom_Left:
			return TopLeftUV;
		case ESpritePivotMode::Bottom_Center:
			return FVector2D(TopLeftUV.X, TopLeftUV.Y + Dimension.Y * 0.5f);
		case ESpritePivotMode::Bottom_Right:
			return FVector2D(TopLeftUV.X, TopLeftUV.Y + Dimension.Y);
		default:
		case ESpritePivotMode::Custom:
			return CustomPivotPoint;
			break;
		};
	}
	else
	{
		switch (PivotMode)
		{
		case ESpritePivotMode::Top_Left:
			return TopLeftUV;
		case ESpritePivotMode::Top_Center:
			return FVector2D(TopLeftUV.X + Dimension.X * 0.5f, TopLeftUV.Y);
		case ESpritePivotMode::Top_Right:
			return FVector2D(TopLeftUV.X + Dimension.X, TopLeftUV.Y);
		case ESpritePivotMode::Center_Left:
			return FVector2D(TopLeftUV.X, TopLeftUV.Y + Dimension.Y * 0.5f);
		case ESpritePivotMode::Center_Center:
			return FVector2D(TopLeftUV.X + Dimension.X * 0.5f, TopLeftUV.Y + Dimension.Y * 0.5f);
		case ESpritePivotMode::Center_Right:
			return FVector2D(TopLeftUV.X + Dimension.X, TopLeftUV.Y + Dimension.Y * 0.5f);
		case ESpritePivotMode::Bottom_Left:
			return FVector2D(TopLeftUV.X, TopLeftUV.Y + Dimension.Y);
		case ESpritePivotMode::Bottom_Center:
			return FVector2D(TopLeftUV.X + Dimension.X * 0.5f, TopLeftUV.Y + Dimension.Y);
		case ESpritePivotMode::Bottom_Right:
			return TopLeftUV + Dimension;

		default:
		case ESpritePivotMode::Custom:
			return CustomPivotPoint;
			break;
		};
	}
}

FVector2D UPaperSprite::GetPivotPosition() const
{
	FVector2D RawPivot = GetRawPivotPosition();

	if (bSnapPivotToPixelGrid)
	{
		RawPivot.X = FMath::RoundToFloat(RawPivot.X);
		RawPivot.Y = FMath::RoundToFloat(RawPivot.Y);
	}

	return RawPivot;
}

void UPaperSprite::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	Super::GetAssetRegistryTags(OutTags);

	if (AtlasGroup != nullptr)
	{
		OutTags.Add(FAssetRegistryTag(TEXT("AtlasGroupGUID"), AtlasGroup->AtlasGUID.ToString(EGuidFormats::Digits), FAssetRegistryTag::TT_Hidden));
	}
}

#endif

bool UPaperSprite::GetPhysicsTriMeshData(FTriMeshCollisionData* OutCollisionData, bool InUseAllTriData)
{
	//@TODO: Probably want to support this
	return false;
}

bool UPaperSprite::ContainsPhysicsTriMeshData(bool InUseAllTriData) const
{
	//@TODO: Probably want to support this
	return false;
}

FBoxSphereBounds UPaperSprite::GetRenderBounds() const
{
	FBox BoundingBox(ForceInit);
	
	for (int32 VertexIndex = 0; VertexIndex < BakedRenderData.Num(); ++VertexIndex)
	{
		const FVector4& VertXYUV = BakedRenderData[VertexIndex];
		const FVector Vert((PaperAxisX * VertXYUV.X) + (PaperAxisY * VertXYUV.Y));
		BoundingBox += Vert;
	}
	
	// Make the whole thing a single unit 'deep'
	const FVector HalfThicknessVector = 0.5f * PaperAxisZ;
	BoundingBox += -HalfThicknessVector;
	BoundingBox += HalfThicknessVector;

	return FBoxSphereBounds(BoundingBox);
}

FPaperSpriteSocket* UPaperSprite::FindSocket(FName SocketName)
{
	for (int32 SocketIndex = 0; SocketIndex < Sockets.Num(); ++SocketIndex)
	{
		FPaperSpriteSocket& Socket = Sockets[SocketIndex];
		if (Socket.SocketName == SocketName)
		{
			return &Socket; 
		}
	}

	return nullptr;
}

void UPaperSprite::QuerySupportedSockets(TArray<FComponentSocketDescription>& OutSockets) const
{
	for (int32 SocketIndex = 0; SocketIndex < Sockets.Num(); ++SocketIndex)
	{
		const FPaperSpriteSocket& Socket = Sockets[SocketIndex];
		new (OutSockets) FComponentSocketDescription(Socket.SocketName, EComponentSocketType::Socket);
	}
}

#if WITH_EDITOR
void UPaperSprite::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FPaperCustomVersion::GUID);
}

void UPaperSprite::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITORONLY_DATA
	const int32 PaperVer = GetLinkerCustomVersion(FPaperCustomVersion::GUID);

	bool bRebuildCollision = false;
	bool bRebuildRenderData = false;

	if (PaperVer < FPaperCustomVersion::AddTransactionalToClasses)
	{
		SetFlags(RF_Transactional);
	}

	if (PaperVer < FPaperCustomVersion::AddPivotSnapToPixelGrid)
	{
		bSnapPivotToPixelGrid = false;
	}

	if (PaperVer < FPaperCustomVersion::AddPixelsPerUnrealUnit)
	{
		PixelsPerUnrealUnit = 1.0f;
		bRebuildCollision = true;
		bRebuildRenderData = true;
	}
	else if (PaperVer < FPaperCustomVersion::FixTypoIn3DConvexHullCollisionGeneration)
	{
		bRebuildCollision = true;
	}

	if ((PaperVer < FPaperCustomVersion::AddDefaultCollisionProfileInSpriteAsset) && (BodySetup != nullptr))
	{
		BodySetup->DefaultInstance.SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	}

	if (PaperVer >= FPaperCustomVersion::AddSourceTextureSize && NeedRescaleSpriteData())
	{
		RescaleSpriteData(GetSourceTexture());
		bRebuildCollision = true;
		bRebuildRenderData = true;
	}

	if (bRebuildCollision)
	{
		RebuildCollisionData();
	}

	if (bRebuildRenderData)
	{
		RebuildRenderData();
	}
#endif
}
#endif

UTexture2D* UPaperSprite::GetBakedTexture() const
{
	return (BakedSourceTexture != nullptr) ? BakedSourceTexture : SourceTexture;
}

UMaterialInterface* UPaperSprite::GetMaterial(int32 MaterialIndex) const
{
	if (MaterialIndex == 0)
	{
		return GetDefaultMaterial();
	}
	else if (MaterialIndex == 1)
	{
		return GetAlternateMaterial();
	}
	else
	{
		return nullptr;
	}
}

int32 UPaperSprite::GetNumMaterials() const
{
	return (AlternateMaterialSplitIndex != INDEX_NONE) ? 2 : 1;
}
