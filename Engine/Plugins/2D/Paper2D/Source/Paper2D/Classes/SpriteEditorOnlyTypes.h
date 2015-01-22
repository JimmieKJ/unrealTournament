// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SpriteEditorOnlyTypes.generated.h"

// A single polygon, may be convex or concave, etc...
// bNegativeWinding tells us if the winding should be negative (CW) regardless of the order in Vertices
USTRUCT()
struct FSpritePolygon
{
	GENERATED_USTRUCT_BODY()

	FSpritePolygon() 
	: bNegativeWinding(false)
	{
	}

	UPROPERTY(Category=Physics, EditAnywhere)
	TArray<FVector2D> Vertices;

	UPROPERTY()//Category=Physics, EditAnywhere)
	FVector2D BoxSize;
	
	UPROPERTY()//Category=Physics, EditAnywhere)
	FVector2D BoxPosition;

	UPROPERTY()
	bool bNegativeWinding;
};


// Method of specifying polygons for a sprite's render or collision data
UENUM()
namespace ESpritePolygonMode
{
	enum Type
	{
		// Use the bounding box of the source sprite (no optimization)
		SourceBoundingBox,

		// Tighten the bounding box around the sprite to exclude fully transparent areas (the default)
		TightBoundingBox,

		// Bounding box that can be edited explicitly
		//CustomBox,

		// Shrink-wrapped geometry
		ShrinkWrapped,

		// Fully custom geometry; edited by hand
		FullyCustom,

		// Diced (split up into smaller squares, including only non-empty ones in the final geometry).  This option is only supported for Render geometry and will be ignored for Collision geometry.
		Diced
	};
}

// Method of specifying polygons for a sprite's render or collision data
UENUM()
namespace ESpriteSubdivisionMode
{
	enum Type
	{
		// Don't subdivide this sprite
		NotSubdivided,

		// Subdivide the sprite, 
		SubdivideAutomatic,

		// Bounding box that can be edited explicitly
		SubdivideManually,

		// Shrink-wrapped geometry
		ShrinkWrapped,

		// Fully custom geometry; edited by hand
		FullyCustom,
	};
}

USTRUCT()
struct FSpritePolygonCollection
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(Category=PolygonData, EditAnywhere, AdvancedDisplay)
	TArray<FSpritePolygon> Polygons;

	// The geometry type
	UPROPERTY(Category=PolygonData, EditAnywhere)
	TEnumAsByte<ESpritePolygonMode::Type> GeometryType;

	// Size of a single subdivision (in pixels) in X (for Diced mode)
	UPROPERTY(Category=PolygonData, EditAnywhere)
	int32 PixelsPerSubdivisionX;

	// Size of a single subdivision (in pixels) in Y (for Diced mode)
	UPROPERTY(Category=PolygonData, EditAnywhere)
	int32 PixelsPerSubdivisionY;

	// Experimental: Hint to the triangulation routine that extra vertices should be preserved
	UPROPERTY(Category=PolygonData, EditAnywhere, AdvancedDisplay)
	bool bAvoidVertexMerging;

	// Alpha threshold for a transparent pixel (range 0..1, anything equal or below this value will be considered unimportant)
	UPROPERTY(Category=PolygonData, EditAnywhere, AdvancedDisplay)
	float AlphaThreshold;

	// This is the threshold below which multiple vertices will be merged together when doing shrink-wrapping.  Higher values result in fewer vertices.
	UPROPERTY(Category=PolygonData, EditAnywhere, AdvancedDisplay)
	float SimplifyEpsilon;

	FSpritePolygonCollection()
		: GeometryType(ESpritePolygonMode::TightBoundingBox)
		, PixelsPerSubdivisionX(32)
		, PixelsPerSubdivisionY(32)
		, bAvoidVertexMerging(false)
		, AlphaThreshold(0.0f)
		, SimplifyEpsilon(2.0f)
	{
	}

	void AddRectanglePolygon(FVector2D Position, FVector2D Size)
	{
		FSpritePolygon& Poly = *new (Polygons) FSpritePolygon();
		new (Poly.Vertices) FVector2D(Position.X, Position.Y);
		new (Poly.Vertices) FVector2D(Position.X + Size.X, Position.Y);
		new (Poly.Vertices) FVector2D(Position.X + Size.X, Position.Y + Size.Y);
		new (Poly.Vertices) FVector2D(Position.X, Position.Y + Size.Y);
		Poly.BoxSize = Size;
		Poly.BoxPosition = Position;
	}

	void Reset()
	{
		Polygons.Empty();
		GeometryType = ESpritePolygonMode::TightBoundingBox;
	}
};

USTRUCT()
struct FSpriteAssetInitParameters
{
	GENERATED_USTRUCT_BODY()

	FSpriteAssetInitParameters()
		: Texture(nullptr)
		, Offset(FVector2D::ZeroVector)
		, Dimension(FVector2D::ZeroVector)
		, bNewlyCreated(false)
	{
	}

	// Set the texture and the offset/dimension to fully match the specified texture 
	void SetTextureAndFill(UTexture2D* InTexture)
	{
		Texture = InTexture;
		if (Texture != nullptr)
		{
			Dimension = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
			Offset = FVector2D::ZeroVector;
		}
		else
		{
			Dimension = FVector2D::ZeroVector;
			Offset = FVector2D::ZeroVector;
		}
	}

public:
	// The texture to use
	UTexture2D* Texture;

	// The offset within the texture (in pixels)
	FVector2D Offset;

	// The dimension of the subregion within the texture (in pixels)
	FVector2D Dimension;

	// Is this sprite newly created (should we pull the default pixels/uu and materials from the project settings)?
	bool bNewlyCreated;
};

UENUM()
namespace ESpritePivotMode
{
	enum Type
	{
		Top_Left,
		Top_Center,
		Top_Right,
		Center_Left,
		Center_Center,
		Center_Right,
		Bottom_Left,
		Bottom_Center,
		Bottom_Right,
		Custom
	};
}
