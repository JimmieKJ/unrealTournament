// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SAnimationBlendSpaceBase.h"
#include "Sorting.h"
#include "Animation/BlendSpace.h"

//////////////////////////////////////////////////////////////////////////
// SAnimationBlendSpace

struct FTriangle;

/**
 * Utility class for FDelaunayTriangleGenerator 
 * This represents Points in FDelaunayTriangleGenerator
 */
struct FPoint
{
	// position of Point
	FVector Position;
	// Triangles this point belongs to
	TArray<FTriangle *> Triangles;
	
	FPoint(FVector Pos) : Position(Pos) {}

	bool operator==( const FPoint& Other ) const 
	{
		// if same position, it's same point
		return (Other.Position == Position);
	}

	void AddTriangle(FTriangle * NewTriangle)
	{
		Triangles.AddUnique(NewTriangle);
	}

	void RemoveTriangle(FTriangle * TriangleToRemove)
	{
		Triangles.Remove(TriangleToRemove);
	}

	float GetDistance(const FPoint& Other)
	{
		return (Other.Position-Position).Size();
	}
};

struct FHalfEdge
{
	// 3 vertices in CCW order
	FPoint* Vertices[2]; 

	FHalfEdge(){};
	FHalfEdge(FPoint * A, FPoint * B)
	{
		Vertices[0] = A;
		Vertices[1] = B;
	}

	bool DoesShare(const FHalfEdge& A) const
	{
		return (Vertices[0] == A.Vertices[1] && Vertices[1] == A.Vertices[0]);
	}

	bool operator==( const FHalfEdge& Other ) const 
	{
		// if same position, it's same point
		return FMemory::Memcmp(Other.Vertices, Vertices, sizeof(Vertices)) == 0;
	}
};
/**
 * Utility class for FDelaunayTriangleGenerator 
 * This represents Triangle in FDelaunayTriangleGenerator
 */
struct FTriangle
{
	// 3 vertices in CCW order
	FPoint* Vertices[3]; 
	// average points for Vertices
	FVector	Center;
	// FEdges
	FHalfEdge Edges[3];

	bool operator==( const FTriangle& Other ) const 
	{
		// if same position, it's same point
		return FMemory::Memcmp(Other.Vertices, Vertices, sizeof(Vertices)) == 0;
	}

	FTriangle(FTriangle & Copy)
	{
		FMemory::Memcpy(Vertices, Copy.Vertices);
		FMemory::Memcpy(Edges, Copy.Edges);
		Center = Copy.Center;

		Vertices[0]->AddTriangle(this);
		Vertices[1]->AddTriangle(this);
		Vertices[2]->AddTriangle(this);
	}

	FTriangle(FPoint * A, FPoint * B, FPoint * C)
	{
		Vertices[0] = A;
		Vertices[1] = B;
		Vertices[2] = C;
		Center = (A->Position + B->Position + C->Position)/3.0f;

		Vertices[0]->AddTriangle(this);
		Vertices[1]->AddTriangle(this);
		Vertices[2]->AddTriangle(this);
		// when you make triangle first time, make sure it stays in CCW
		MakeCCW();

		// now create edges, this should be in the CCW order
		Edges[0] = FHalfEdge(Vertices[0], Vertices[1]);
		Edges[1] = FHalfEdge(Vertices[1], Vertices[2]);
		Edges[2] = FHalfEdge(Vertices[2], Vertices[0]);
	}

	FTriangle(FPoint * A)
	{
		Vertices[0] = A;
		Vertices[1] = A;
		Vertices[2] = A;
		Center = A->Position;

		Vertices[0]->AddTriangle(this);
		Vertices[1]->AddTriangle(this);
		Vertices[2]->AddTriangle(this);

		// now create edges, this should be in the CCW order
		Edges[0] = FHalfEdge(Vertices[0], Vertices[1]);
		Edges[1] = FHalfEdge(Vertices[1], Vertices[2]);
		Edges[2] = FHalfEdge(Vertices[2], Vertices[0]);
	}

	FTriangle(FPoint * A, FPoint * B)
	{
		Vertices[0] = A;
		Vertices[1] = B;
		Vertices[2] = B;
		Center = (A->Position + B->Position) / 2.0f;

		Vertices[0]->AddTriangle(this);
		Vertices[1]->AddTriangle(this);
		Vertices[2]->AddTriangle(this);

		// now create edges, this should be in the CCW order
		Edges[0] = FHalfEdge(Vertices[0], Vertices[1]);
		Edges[1] = FHalfEdge(Vertices[1], Vertices[2]);
		Edges[2] = FHalfEdge(Vertices[2], Vertices[0]);
	}

	~FTriangle()
	{
		Vertices[0]->RemoveTriangle(this);
		Vertices[1]->RemoveTriangle(this);
		Vertices[2]->RemoveTriangle(this);
	}

	bool Contains (const FPoint& Other) const
	{
		return (Other == *Vertices[0] || Other == *Vertices[1] || Other == *Vertices[2]);
	}

	float GetDistance(const FPoint& Other) const
	{
		return (Other.Position-Center).Size();
	}

	float GetDistance(const FVector& Other) const
	{
		return (Other-Center).Size();
	}

	bool HasSameHalfEdge(const FTriangle* Other) const
	{
		for (int32 I=0; I<3; ++I)
		{
			for (int32 J=0; J<3; ++J)
			{
				if (Other->Edges[I] == Edges[J])
				{
					return true;
				}
			}
		}

		return false;
	}

	bool DoesShareSameEdge(const FTriangle* Other) const
	{
		for (int32 I=0; I<3; ++I)
		{
			for (int32 J=0; J<3; ++J)
			{
				if (Other->Edges[I].DoesShare(Edges[J]))
				{
					return true;
				}
			}
		}

		return false;
	}

	// find point that doesn't share with this
	// this should only get called if it shares same edge
	FPoint * FindNonSharingPoint(const FTriangle* Other)
	{
		if (!Contains(*Other->Vertices[0]))
		{
			return Other->Vertices[0];
		}

		if (!Contains(*Other->Vertices[1]))
		{
			return Other->Vertices[1];
		}

		if (!Contains(*Other->Vertices[2]))
		{
			return Other->Vertices[2];
		}

		return NULL;
	}

private:
	void MakeCCW()
	{
		// this eventually has to happen on the plane that contains this 3 points
		// for now we ignore Z
		FVector Diff1 = Vertices[1]->Position-Vertices[0]->Position;
		FVector Diff2 = Vertices[2]->Position-Vertices[0]->Position;

		float Result = Diff1.X*Diff2.Y - Diff1.Y*Diff2.X;

		check (Result != 0.f);

		// it's in left side, we need this to be right side
		if (Result < 0.f)
		{
			// swap 1&2 
			FPoint * TempPt = Vertices[2];
			Vertices[2] = Vertices[1];
			Vertices[1] = TempPt;
		}
	}
};

/**
 * Generates triangles from sample point using Delaunay Triangulation, 
 * TriangleList contains the list of triangle after generated.
 */
class FDelaunayTriangleGenerator
{
public:
	enum ECircumCircleState
	{
		ECCS_Outside = -1,
		ECCS_On = 0,
		ECCS_Inside = 1,
	};
	/** 
	 * Reset all data
	 */
	void Reset();
	void EmptyTriangles();
	void EmptySamplePoints();

	/**
	 * Generate triangles from SamplePointList
	 */
	void Triangulate();

	/** 
	 * Add new sample point to SamplePointList
	 * It won't be added if already exists
	 */
	void AddSamplePoint(FVector NewPoint);

	/** 
	 * This is for debug purpose to step only one to triangulate
	 * StartIndex should increase manually
	 */
	void Step(int32 StartIndex);

	/** 
	 * Sort SamplePointList in the order of 1) -X->+X 2) -Y->+Y 3) -Z->+Z
	 * Sort Point of the input point in the order of
	 * 1) -> +x. 2) -> +y, 3) -> +z
	 * by that, it will sort from -X to +X, if same is found, next will be Y, and if same, next will be Z
	 */
	void SortSamples();

	~FDelaunayTriangleGenerator()
	{
		Reset();
	}

	/** 
	 * Get TriangleList
	 */
	const TArray<FTriangle*> & GetTriangleList() const { return TriangleList; };

	/** 
	 * Get SamplePointList
	 */
	const TArray<FPoint> & GetSamplePointList() const { return SamplePointList; };

	void EditPointValue(int32 SamplePointIndex, FVector NewValue)
	{
		SamplePointList[SamplePointIndex] = NewValue;
	}

	/** Original index - before sorted to match original data **/
	int32 GetOriginalIndex(int32 NewSortedSamplePointList) const
	{
		return IndiceMappingTable[NewSortedSamplePointList];
	}

	void InitializeIndiceMapping();

	/* Set the grid box, so we can normalize the sample points */
	void SetGridBox(const FBox& Box)
	{
		FVector Size = Box.GetSize();

		Size.X = FMath::Max( Size.X, DELTA );
		Size.Y = FMath::Max( Size.Y, DELTA );
		Size.Z = FMath::Max( Size.Z, DELTA );

		GridMin = Box.Min;
		RecipGridSize = FVector(1.0f, 1.0f, 1.0f) / Size;
	}

private:
	/** 
	 * The key function in Delaunay Triangulation
	 * return true if the TestPoint is WITHIN the triangle circumcircle
	 *	http://en.wikipedia.org/wiki/Delaunay_triangulation 
	 */
	ECircumCircleState GetCircumcircleState(const FTriangle* T, const FPoint& TestPoint);

	/**
	 * return true if they can make triangle
	 */
	bool IsEligibleForTriangulation(const FPoint* A, const FPoint* B, const FPoint* C)
	{
		return (IsCollinear(A, B, C)==false);
	}

	/** 
	 * return true if 3 points are collinear
	 * by that if those 3 points create straight line
	 */
	bool IsCollinear(const FPoint* A, const FPoint* B, const FPoint* C);

	/** 
	 * return true if all points are coincident
	 * (i.e. if all points are the same)
	 */
	bool AllCoincident(const TArray<FPoint>& InPoints);

	/**
	 * Flip TriangleList(I) with TriangleList(J). 
	 */
	bool FlipTriangles(int32 I, int32 J);

	/** 
	 * Add new Triangle
	 */
	void AddTriangle(FTriangle & newTriangle, bool bCheckHalfEdge=true);

	/** 
	 * Used as incremental step to triangulate all points
	 * Create triangles TotalNum of PointList
	 */
	int32 GenerateTriangles(TArray<FPoint> & PointList, const int32 TotalNum);

private:
	/**
	 * Data Structure for triangulation
	 * SamplePointList is the input data
	 */
	TArray<FPoint>		SamplePointList;
	/**
	 * This is map back to original indices after sorted
	 */
	TArray<int32>			IndiceMappingTable;

	/**
	 * Output data for this generator
	 */
	TArray<FTriangle*>	TriangleList;

	/**
	 * We need to normalize the points before the circumcircle test, so we need the extents of the grid.
	 * Store 1 / GridSize to avoid recalculating it every time through GetCirculcircleState
	*/
	FVector				GridMin;
	FVector				RecipGridSize;
};

// @todo fixmelh : this code is mess between fvector2D and fvector
// ideally FBlendSpaceGrid will be handled in 3D, and SBlendSpaceGridWidget only knows about 2D
// in the future this should all change to 3D

/**
 * BlendSpace Grid
 * Using triangulated space, create FEditorElement of each grid point using SamplePoint
 */
class FBlendSpaceGrid
{
public:
	/**
	 * Find Triangle this TestPoint is within
	 * 
	 * @param	TestPoint				Point to test
	 * @param	OutBaryCentricCoords	Output BaryCentricCoords2D of the point in the triangle // for now it's only 2D
	 * @param	OutTriangle				The triangle that this point is within or lie
	 * @param	TriangleList			TriangleList to test
	 * 
	 * @return	true if successfully found the triangle this point is within
	 */
	bool FindTriangleThisPointBelongsTo(const FVector& TestPoint, FVector& OutBaryCentricCoords, FTriangle * & OutTriangle, const TArray<FTriangle*> & TriangleList) const;

	/**  
	 * Fill up Grid Elements using TriangleList input - Grid information should have been set by SetGridInfo
	 * 
	 * @param	SamplePoints		: Sample Point List
	 * @param	TriangleList		: List of triangles
	 */
	void GenerateGridElements(const TArray<FPoint>& SamplePoints, const TArray<FTriangle*> & TriangleList);

	/** 
	 * default value 
	 */
	FBlendSpaceGrid()
		:	GridDim(FVector(0, 0, 0), FVector(100, 100, 0))
		,	GridNum(5, 5)
	{
	}

	void Reset()
	{
		Elements.Empty();
	}

	void SetGridInfo( const FIntPoint& InGridNum, const FBox& InGridDim )
	{
		GridNum = InGridNum;
		GridDim = InGridDim;
	}

	const FEditorElement &			GetElement(int32 GX, int32 GY) const;
	const TArray<FEditorElement>&	GetElements() const { return Elements;}
	const FIntPoint &				GetGridNum() const { return GridNum; }
	const FBox&						GetGridDim() const { return GridDim;	}
	/** 
	 * Get FVector2D(GridX, GridY) indices from give Pos
	 */
	FIntPoint	GetIndexFromPos(FVector GridPos, bool bSnap) const;

	/** 
	 * Convert grid index (GridX, GridY) to triangle coords and returns FVector
	*/
	FVector GetPosFromIndex(int32 GridX, int32 GridY) const;

	/**
	 * return true if Pos is within this GridDim
	 */
	bool	IsInside(FVector Pos) const;

private:
	// Grid Dimension
	FBox GridDim;

	// how many rows/cols for each axis
	FIntPoint GridNum; 

	// Each point data -output data
	TArray<FEditorElement>	Elements; // 2D array saved in 1D array, to search (x, y), x*GridSizeX+y;
};

/** Widget with a handler for OnPaint; convenient for testing various DrawPrimitives. */
class SBlendSpaceGridWidget : public SBlendSpaceWidget
{
public:
	SLATE_BEGIN_ARGS(SBlendSpaceGridWidget)
		: _PreviewInput()
		, _BlendSpace(NULL)
		{}

		/** allow adding point using mouse **/
		SLATE_ATTRIBUTE(FVector, PreviewInput)
		SLATE_EVENT(FOnRefreshSamples, OnRefreshSamples)
		SLATE_ARGUMENT(UBlendSpace*, BlendSpace)
		SLATE_EVENT(FOnNotifyUser, OnNotifyUser)
	SLATE_END_ARGS()

	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;

	/**
	 * Clear all points
	 */
	void ClearPoints()
	{
		Generator.Reset();
		BlendSpaceGrid.Reset();
	}

	virtual void ResampleData() override;

	/** 
	 * Manually AddPoint
	 */
	void AddWidgetPoint(const FVector2D & Point, const FSlateRect& WindowRect)
	{
		TOptional<FVector> GridPos = GetEditorPosFromWidgetPos(Point, WindowRect);
		if (GridPos.IsSet())
		{
			Generator.AddSamplePoint(GridPos.GetValue());
		}
	}

	/** 
	 * Manually AddPoint
	 */
	void AddGridPoint(const FVector& Point)
	{
		Generator.AddSamplePoint(Point);
	}

	/**
	 * Add Samples : debug functionality - might not be up-to-date - only works with test triangulation window
	 */
	void AddSample();

	/** 
	 * Mapping function between WidgetPos and EditorPos
	 */
	virtual TOptional<FVector2D>	GetWidgetPosFromEditorPos(const FVector& EditorPos, const FSlateRect& WindowRect) const override;
	virtual TOptional<FVector>		GetEditorPosFromWidgetPos(const FVector2D & WidgetPos, const FSlateRect& WindowRect) const override;

	/**
	 * Snaps a position in editor space to the editor grid
	 */
	virtual FVector					SnapEditorPosToGrid(const FVector& InPos) const override;

	/**
	 * Utility function to set the grid info for both the BlendSpaceGrid and the Generator in one go
	 */
	void SetGridInfo( const FIntPoint& InGridNum, const FBox& InGridDim )
	{
		BlendSpaceGrid.SetGridInfo( InGridNum, InGridDim );
		Generator.SetGridBox( InGridDim );
	}

private:
	/** 
	 * Triangle Generator
	 */
	FDelaunayTriangleGenerator	Generator;
	/** 
	 * Blend Space Grid to represent data
	 */
	FBlendSpaceGrid		BlendSpaceGrid;

	/** miscellanous data structure to debug and to track mouse **/
	FIntPoint						CachedGridIndices;

	friend class SBlendSpaceEditor;

	TAttribute<FVector>				PreviewInput;

	/** Get derived blend space from base class pointer */
	UBlendSpace* GetBlendSpace() { return Cast<UBlendSpace>(BlendSpace); }
	const UBlendSpace* GetBlendSpace() const { return Cast<UBlendSpace>(BlendSpace); }

	/** Utility functions **/
	virtual FText GetInputText(const FVector& GridPos) const override;
	bool IsOnTheBorder(const FIntPoint& GridIndices) const;

	/** Utility function for OnPaint **/
	void DrawHighlightGrid(const FVector2D & LeftTopPos, const FVector2D & RightBottomPos, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId ) const;

	virtual FReply UpdateLastMousePosition( const FGeometry& MyGeometry, const FVector2D& ScreenSpacePosition, bool bClampToWindowRect = false, bool bSnapToGrid = false  ) override;
	bool GetMinMaxFromGridPos(FVector GridPos, FVector2D & WindowLeftTop, FVector2D & WindowRightBottom, const FSlateRect& WindowRect) const;
};

class SBlendSpaceEditor : public SBlendSpaceEditorBase
{
public:
	SLATE_BEGIN_ARGS(SBlendSpaceEditor)
		: _BlendSpace(NULL)			
		, _Persona()
		{}

		SLATE_ARGUMENT(UBlendSpace*, BlendSpace)
		SLATE_ARGUMENT(TSharedPtr<class FPersona>, Persona)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	
	void SetBlendSpace(UBlendSpace* NewBlendSpace);
	UBlendSpace* GetBlendSpace() const { return Cast<UBlendSpace>(BlendSpace); }

private:
	TSharedPtr<STextBlock> ParameterX_Max;
	TSharedPtr<STextBlock> ParameterY_Max;
	TSharedPtr<STextBlock> ParameterX_Min;
	TSharedPtr<STextBlock> ParameterY_Min;

private:
	// Updates the UI to reflect the current blend space parameter values */
	void UpdateBlendParameters();

	/** Handler for when blend space parameters have been changed */
	void OnBlendSpaceParamtersChanged();

	/** Get 2D specific cast of base widget pointer */
	SBlendSpaceGridWidget* GetBlendSpaceGridWidget() { return static_cast<SBlendSpaceGridWidget*>(BlendSpaceWidget.Get()); }
};

