// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "PersonaPrivatePCH.h"
#include "SAnimationBlendSpace.h"
#include "SAnimationSequenceBrowser.h"
#include "Persona.h"
#include "AssetData.h"
#include "Editor/ContentBrowser/Public/ContentBrowserModule.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "ScopedTransaction.h"
#include "SNotificationList.h"
#include "Animation/BlendSpaceBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogBlendSpace, Log, All);

#define BLENDSPACE_MINSAMPLE	3
#define BLENDSPACE_MINAXES		1
#define BLENDSPACE_MAXAXES		3

#define LOCTEXT_NAMESPACE "BlendSpaceDialog"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct FPointWithIndex
{
	FPoint	Point;
	int32		OriginalIndex;

	FPointWithIndex(const FPoint& P, int32 InOriginalIndex)
		:	Point(P),
			OriginalIndex(InOriginalIndex)
	{
	}
};

struct FSortByDistance
{
	int32 Index;
	float Distance;
	FSortByDistance(int32 InIndex, float InDistance) 
		: Index(InIndex), Distance(InDistance) {}
};

struct FCompareDistance
{
	FORCEINLINE bool operator()( const FSortByDistance &A, const FSortByDistance &B ) const
	{
		return A.Distance < B.Distance;
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/** Utility struct for converting between grid space and local/absolute screen space */
struct FGridSpaceConverter
{
	FVector MinInput;
	FVector MaxInput;
	FVector InputRange;
	FVector PixelsPerInput;
	FVector2D	ScreenSize;
	FVector2D	ScreenMin;

	FGridSpaceConverter(const FVector& InMinInput, const FVector& InMaxInput, const FSlateRect& InWindowRect)
	{
		ScreenSize= InWindowRect.GetSize();
		ScreenMin = FVector2D (InWindowRect.Left, InWindowRect.Top);

		MinInput = InMinInput;
		MaxInput = InMaxInput;
		InputRange = MaxInput- MinInput;

		PixelsPerInput.X = InputRange.X != 0 ? ( ScreenSize.X / InputRange.X ) : 0;
		PixelsPerInput.Y = InputRange.Y != 0 ? ( ScreenSize.Y / InputRange.Y ) : 0;
	}

	/** Local Widget Space -> Curve Input domain. */
	FVector ScreenToInput(const FVector2D& Screen) const
	{
		FVector Output = MinInput;
		FVector2D LocalScreen = Screen - ScreenMin;

		if (PixelsPerInput.X != 0.f)
		{
			Output.X = LocalScreen.X/PixelsPerInput.X + MinInput.X;
		}
		if (PixelsPerInput.Y != 0.f)
		{
			Output.Y = MaxInput.Y - LocalScreen.Y/PixelsPerInput.Y;
		}

		return Output;
	}

	/** Curve Input domain -> local Widget Space */
	FVector2D InputToScreen(const FVector& Input) const
	{
		FVector2D Screen;
		
		Screen.X = (Input.X - MinInput.X) * PixelsPerInput.X + ScreenMin.X;
		Screen.Y = (MaxInput.Y - Input.Y) * PixelsPerInput.Y + ScreenMin.Y;
		return Screen;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FDelaunayTriangleGenerator::EmptyTriangles()
{
	for (int32 I=0; I<TriangleList.Num(); ++I)
	{
		delete TriangleList[I];
	}

	TriangleList.Empty();
}
void FDelaunayTriangleGenerator::EmptySamplePoints()
{
	SamplePointList.Empty();
}

void FDelaunayTriangleGenerator::Reset()
{
	EmptyTriangles();
	EmptySamplePoints();
}

void FDelaunayTriangleGenerator::AddSamplePoint(FVector NewPoint)
{
	SamplePointList.AddUnique(FPoint(NewPoint));
}

void FDelaunayTriangleGenerator::Triangulate()
{
	if (SamplePointList.Num() == 0)
	{
		return;
	}
	else if (SamplePointList.Num() == 1)
	{
		// degenerate case 1
		FTriangle Triangle(&SamplePointList[0]);
		AddTriangle(Triangle);
	}
	else if (SamplePointList.Num() == 2)
	{
		// degenerate case 2
		FTriangle Triangle(&SamplePointList[0], &SamplePointList[1]);
		AddTriangle(Triangle);
	}
	else
	{
		SortSamples();

		// first choose first 3 points
		for (int32 I = 2; I<SamplePointList.Num(); ++I)
		{
			GenerateTriangles(SamplePointList, I + 1);
		}

		// degenerate case 3: many points all collinear or coincident
		if (TriangleList.Num() == 0)
		{
			if (AllCoincident(SamplePointList))
			{
				// coincident case - just create one triangle
				FTriangle Triangle(&SamplePointList[0]);
				AddTriangle(Triangle);
			}
			else
			{
				// collinear case: create degenerate triangles between pairs of points
				for (int32 PointIndex = 0; PointIndex < SamplePointList.Num() - 1; ++PointIndex)
				{
					FTriangle Triangle(&SamplePointList[PointIndex], &SamplePointList[PointIndex + 1]);
					AddTriangle(Triangle);
				}
			}
		}
	}
}

void FDelaunayTriangleGenerator::SortSamples()
{
	TArray<FPointWithIndex> SortedPoints;
	for (int32 I=0; I<SamplePointList.Num(); ++I)
	{
		SortedPoints.Add(FPointWithIndex(SamplePointList[I], I));
	}

	// return A-B
	struct FComparePoints
	{
		bool operator()( const FPointWithIndex &A, const FPointWithIndex&B ) const
		{
			// the sorting happens from -> +X, -> +Y,  -> for now ignore Z ->+Z
			if( A.Point.Position.X == B.Point.Position.X ) // same, then compare Y
			{
				if( A.Point.Position.Y == B.Point.Position.Y )
				{
					return A.Point.Position.Z < B.Point.Position.Z;
				}
				else
				{
					return A.Point.Position.Y < B.Point.Position.Y;
				}
			}
			else
			{
				return A.Point.Position.X < B.Point.Position.X;
			}
		}
	};
	// sort all points
	SortedPoints.Sort( FComparePoints() );

	// now copy back to SamplePointList
	IndiceMappingTable.Empty(SamplePointList.Num());
	IndiceMappingTable.AddUninitialized(SamplePointList.Num());
	for (int32 I=0; I<SamplePointList.Num(); ++I)
	{
		SamplePointList[I] = SortedPoints[I].Point;
		IndiceMappingTable[I] = SortedPoints[I].OriginalIndex;
	}
}

/** 
* The key function in Delaunay Triangulation
* return true if the TestPoint is WITHIN the triangle circumcircle
*	http://en.wikipedia.org/wiki/Delaunay_triangulation 
*/
FDelaunayTriangleGenerator::ECircumCircleState FDelaunayTriangleGenerator::GetCircumcircleState(const FTriangle* T, const FPoint& TestPoint)
{
	const int32 NumPointsPerTriangle = 3;

	// First off, normalize all the points
	FVector NormalizedPositions[NumPointsPerTriangle];
	
	for ( int32 I = 0; I < NumPointsPerTriangle; ++I )
	{
		NormalizedPositions[I] = ( T->Vertices[I]->Position - GridMin ) * RecipGridSize;
	}

	FVector NormalizedTestPoint = ( TestPoint.Position - GridMin ) * RecipGridSize;

	// ignore Z, eventually this has to be on plane
	// http://en.wikipedia.org/wiki/Delaunay_triangulation - determinant
	float M00 = NormalizedPositions[0].X - NormalizedTestPoint.X;
	float M01 = NormalizedPositions[0].Y - NormalizedTestPoint.Y;
	float M02 = NormalizedPositions[0].X * NormalizedPositions[0].X - NormalizedTestPoint.X * NormalizedTestPoint.X
		+ NormalizedPositions[0].Y*NormalizedPositions[0].Y - NormalizedTestPoint.Y * NormalizedTestPoint.Y;

	float M10 = NormalizedPositions[1].X - NormalizedTestPoint.X;
	float M11 = NormalizedPositions[1].Y - NormalizedTestPoint.Y;
	float M12 = NormalizedPositions[1].X * NormalizedPositions[1].X - NormalizedTestPoint.X * NormalizedTestPoint.X
		+ NormalizedPositions[1].Y * NormalizedPositions[1].Y - NormalizedTestPoint.Y * NormalizedTestPoint.Y;

	float M20 = NormalizedPositions[2].X - NormalizedTestPoint.X;
	float M21 = NormalizedPositions[2].Y - NormalizedTestPoint.Y;
	float M22 = NormalizedPositions[2].X * NormalizedPositions[2].X - NormalizedTestPoint.X * NormalizedTestPoint.X
		+ NormalizedPositions[2].Y * NormalizedPositions[2].Y - NormalizedTestPoint.Y * NormalizedTestPoint.Y;

	float Det = M00*M11*M22+M01*M12*M20+M02*M10*M21 - (M02*M11*M20+M01*M10*M22+M00*M12*M21);
	
	if( FMath::Abs(Det) <= KINDA_SMALL_NUMBER )
	{
		return ECCS_On;
	}
	else if( Det > 0.f )
	{
		return ECCS_Inside;
	}

	return ECCS_Outside;
}

/** 
* return true if 3 points are collinear
* by that if those 3 points create straight line
*/
bool FDelaunayTriangleGenerator::IsCollinear(const FPoint* A, const FPoint* B, const FPoint* C)
{
	// 		FVector A2B = (B.Position-A.Position).SafeNormal();
	// 		FVector A2C = (C.Position-A.Position).SafeNormal();
	// 		
	// 		float fDot = fabs(A2B | A2C);
	// 		return (fDot > SMALL_NUMBER);

	// this eventually has to happen on the plane that contains this 3 pages
	// for now we ignore Z
	FVector Diff1 = B->Position-A->Position;
	FVector Diff2 = C->Position-A->Position;

	float Result = Diff1.X*Diff2.Y - Diff1.Y*Diff2.X;

	return (Result == 0.f);
}

bool FDelaunayTriangleGenerator::AllCoincident(const TArray<FPoint>& InPoints)
{
	if (InPoints.Num() > 0)
	{
		const FPoint& FirstPoint = InPoints[0];
		for (int32 PointIndex = 0; PointIndex < InPoints.Num(); ++PointIndex)
		{
			const FPoint& Point = InPoints[PointIndex];
			if (Point.Position != FirstPoint.Position)
			{
				return false;
			}
		}

		return true;
	}

	return false;
}

bool FDelaunayTriangleGenerator::FlipTriangles(int32 I, int32 J)
{
	bool Flipped = false;
	FTriangle *A = TriangleList[I];
	FTriangle *B = TriangleList[J];

	// if already optimized, don't have to do any
	FPoint * TestPt = A->FindNonSharingPoint(B);

	// If it's not inside, we don't have to do any
	if (GetCircumcircleState(A, *TestPt)!=ECCS_Inside)
	{
		return false;
	}

	int32 TrianglesMade=0;

	// otherwise, try make new triangle, and flip it
	if (IsEligibleForTriangulation(A->Vertices[0],A->Vertices[1], TestPt))
	{
		FTriangle NewTriangle(A->Vertices[0],A->Vertices[1], TestPt);
		// only flip if outside
		if (GetCircumcircleState(&NewTriangle, *A->Vertices[2])==ECCS_Outside)
		{
			// good triangle
			AddTriangle(NewTriangle, false);
			Flipped=true;
			++TrianglesMade;
		}

	}

	if (IsEligibleForTriangulation(A->Vertices[0],A->Vertices[2], TestPt))
	{
		FTriangle NewTriangle(A->Vertices[0],A->Vertices[2], TestPt);
		if (GetCircumcircleState(&NewTriangle, *A->Vertices[1])==ECCS_Outside)
		{
			// good triangle
			AddTriangle(NewTriangle, false);
			Flipped=true;
			++TrianglesMade;
		}
	}

	if (IsEligibleForTriangulation(A->Vertices[1],A->Vertices[2], TestPt))
	{
		FTriangle NewTriangle(A->Vertices[1],A->Vertices[2], TestPt);
		if (GetCircumcircleState(&NewTriangle, *A->Vertices[0])==ECCS_Outside)
		{
			// good triangle
			AddTriangle(NewTriangle, false);
			Flipped=true;
			++TrianglesMade;
		}
	}

	// should be 2, if not we miss triangles
	check (	TrianglesMade==2 );

	return Flipped;
}

void FDelaunayTriangleGenerator::AddTriangle(FTriangle& newTriangle, bool bCheckHalfEdge/*=true*/)
{
	// see if it's same vertices
	for (int32 I=0;I<TriangleList.Num(); ++I)
	{
		if (newTriangle == *TriangleList[I])
		{
			return;
		}

		if (bCheckHalfEdge && newTriangle.HasSameHalfEdge(TriangleList[I]))
		{
			return;
		}
	}


	TriangleList.Add(new FTriangle(newTriangle));
}

int32 FDelaunayTriangleGenerator::GenerateTriangles(TArray<FPoint>& PointList, const int32 TotalNum)
{
	if (TotalNum == BLENDSPACE_MINSAMPLE)
	{
		if (IsEligibleForTriangulation(&PointList[0], &PointList[1], &PointList[2]))
		{
			FTriangle Triangle(&PointList[0], &PointList[1], &PointList[2]);
			AddTriangle(Triangle);
		}
	}
	else if (TriangleList.Num() == 0)
	{
		FPoint * TestPoint = &PointList[TotalNum-1];

		// so far no triangle is made, try to make it with new points that are just entered
		for (int32 I=0; I<TotalNum-2; ++I)
		{
			if (IsEligibleForTriangulation(&PointList[I], &PointList[I+1], TestPoint))
			{
				FTriangle NewTriangle (&PointList[I], &PointList[I+1], TestPoint);
				AddTriangle(NewTriangle);
			}
		}
	}
	else
	{
		// get the last addition
		FPoint * TestPoint = &PointList[TotalNum-1];
		int32 TriangleNum = TriangleList.Num();
	
		for (int32 I=0; I<TriangleList.Num(); ++I)
		{
			FTriangle * Triangle = TriangleList[I];
			if (IsEligibleForTriangulation(Triangle->Vertices[0], Triangle->Vertices[1], TestPoint))
			{
				FTriangle NewTriangle (Triangle->Vertices[0], Triangle->Vertices[1], TestPoint);
				AddTriangle(NewTriangle);
			}

			if (IsEligibleForTriangulation(Triangle->Vertices[0], Triangle->Vertices[2], TestPoint))
			{
				FTriangle NewTriangle (Triangle->Vertices[0], Triangle->Vertices[2], TestPoint);
				AddTriangle(NewTriangle);
			}

			if (IsEligibleForTriangulation(Triangle->Vertices[1], Triangle->Vertices[2], TestPoint))
			{
				FTriangle NewTriangle (Triangle->Vertices[1], Triangle->Vertices[2], TestPoint);
				AddTriangle(NewTriangle);
			}
		}

		// this is locally optimization part
		// we need to make sure all triangles are locally optimized. If not optimize it. 
		for (int32 I=0; I<TriangleList.Num(); ++I)
		{
			FTriangle * A = TriangleList[I];
			for (int32 J=I+1; J<TriangleList.Num(); ++J)
			{
				FTriangle * B = TriangleList[J];

				// does share same edge
				if (A->DoesShareSameEdge(B))
				{
					// then test to see if locally optimized
					if (FlipTriangles(I, J))
					{
						// if this flips, remove current triangle
						delete TriangleList[I];
						delete TriangleList[J];
						//I need to remove J first because other wise, 
						//  index J isn't valid anymore
						TriangleList.RemoveAt(J);
						TriangleList.RemoveAt(I);
						// start over since we modified triangle
						// once we don't have any more to flip, we're good to go!
						I=-1;
						break;
					}
				}
			}
		}
	}

	return TriangleList.Num();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
bool FBlendSpaceGrid::FindTriangleThisPointBelongsTo(const FVector& TestPoint, FVector& OutBaryCentricCoords, FTriangle*& OutTriangle, const TArray<FTriangle*>& TriangleList) const
{
	// sort triangle by distance to TestPoint
	TArray<FSortByDistance> SortedTriangles;

	// remove myself, which is the last point
	for (int32 I=0; I<TriangleList.Num(); ++I)
	{
		float Distance = TriangleList[I]->GetDistance(TestPoint);
		SortedTriangles.Add(FSortByDistance(I, Distance));
	}

	SortedTriangles.Sort( FCompareDistance() );

	// now go through triangle, and get barycentric coords
	for (int32 I=0; I<SortedTriangles.Num(); ++I)
	{
		int32 TriIndex = SortedTriangles[I].Index;
		FTriangle * Triangle = TriangleList[TriIndex];

		FVector Coords = FMath::GetBaryCentric2D(TestPoint, Triangle->Vertices[0]->Position, Triangle->Vertices[1]->Position, Triangle->Vertices[2]->Position);

		// Z coords often has precision error because it's derived from 1-A-B, so do more precise check
		if (fabs(Coords.Z) < KINDA_SMALL_NUMBER)
		{
			Coords.Z = 0.f;
		}

		// inside of triangle? or lies on the triangle? Z should lie all the time 
		if ( 0.f <= Coords.X && Coords.X <= 1.0 && 0.f <= Coords.Y && Coords.Y <= 1.0 && 0.f <= Coords.Z && Coords.Z <= 1.0 )
		{
			OutBaryCentricCoords = Coords;
			OutTriangle = Triangle;
			return true;
		}
	}

	return false;
/*
	// @todo, maybe remove the distance based, but find better way to do this
	OutTriangle = NULL;

	TArray<FTriangle*>	Candidates;
	TArray<FVector>		BaryCentricCoords;

	// now go through triangle, and get barycentric coords
	for (auto Iter = TriangleList.CreateIterator(); Iter; ++Iter)
	{
		FTriangle * Triangle = (*Iter);
		FVector Coords = FMath::GetBaryCentric2D(TestPoint, Triangle->Vertices[0]->Position, Triangle->Vertices[1]->Position, Triangle->Vertices[2]->Position);
		
		// Z coords often has precision error because it's derived from 1-A-B, so do more precise check
		if (fabs(Coords.Z) < KINDA_SMALL_NUMBER)
		{
			Coords.Z = 0.f;
		}

		// inside of triangle? or lies on the triangle? Z should lie all the time 
		if ( 0.f <= Coords.X && Coords.X <= 1.0 && 0.f <= Coords.Y && Coords.Y <= 1.0 && 0.f <= Coords.Z && Coords.Z <= 1.0 )
		{
			// collect all candidates
			Candidates.Add(Triangle);
			BaryCentricCoords.Add(Coords);
		}
	}

	// now I have list of triangles, now if there is TestPoint2, use that to get that value
	for (int32 I=0; I<Candidates.Num(); ++I)
	{

	}
	return (OutTriangle!=NULL);*/
}

/** 
* Fill up Grid Elements using TriangleList input
* 
* @param	SamplePoints		: Sample Point List
* @param	TriangleList		: List of triangles
*/
void FBlendSpaceGrid::GenerateGridElements(const TArray<FPoint>& SamplePoints, const TArray<FTriangle*>& TriangleList)
{
	check ( GridNum.X > 0 && GridNum.Y > 0 );
	check ( GridDim.IsValid );

	// grid 5 means, indexing from 0 - 5 to have 5 grids. 
	int32 GridSizeX = GridNum.X+1;
	int32 GridSizeY = GridNum.Y+1;
	int32 ElementsSize = GridSizeX*GridSizeY;

	Elements.Empty(ElementsSize);

	if (SamplePoints.Num() == 0 || TriangleList.Num() == 0)
	{
		return;
	}

	Elements.AddUninitialized(ElementsSize);

	FVector Weights;
	FVector PointPos;
	// when it fails to find, do distance resolution
	int32 TotalTestPoints = (SamplePoints.Num() >= 3)? 3 : SamplePoints.Num();

	for (int32 I=0; I<GridSizeX; ++I)
	{
		for (int32 J=0; J<GridSizeY; ++J)
		{
			FTriangle * SelectedTriangle = NULL;
			FEditorElement& Ele = Elements[ I*GridSizeY + J ];

			PointPos = GetPosFromIndex(I, J);
			if ( FindTriangleThisPointBelongsTo(PointPos, Weights, SelectedTriangle, TriangleList) )
			{
				// found it
				Ele.Weights[0] = Weights.X;
				Ele.Weights[1] = Weights.Y;
				Ele.Weights[2] = Weights.Z;
				// need to find sample point index
				// @todo fix this with better solution
				// lazy me
				Ele.Indices[0] = SamplePoints.Find(*SelectedTriangle->Vertices[0]);
				Ele.Indices[1] = SamplePoints.Find(*SelectedTriangle->Vertices[1]);
				Ele.Indices[2] = SamplePoints.Find(*SelectedTriangle->Vertices[2]);

				check (Ele.Indices[0]!=INDEX_NONE);
				check (Ele.Indices[1]!=INDEX_NONE);
				check (Ele.Indices[2]!=INDEX_NONE);
			}
			else
			{
				// error back up solution
				// find closest point and do based on distance to the 3 points
				TArray<FSortByDistance> SortedPoints;

				// remove myself, which is the last point
				for (int32 SamplePointIndex=0; SamplePointIndex<SamplePoints.Num(); ++SamplePointIndex)
				{
					float Distance = (PointPos-SamplePoints[SamplePointIndex].Position).Size();
					SortedPoints.Add(FSortByDistance(SamplePointIndex, Distance));
				}

				SortedPoints.Sort( FCompareDistance() );

				// do based on distance resolution
				float TotalDistance = 0.f;

				for (int32 TestPointIndex=0; TestPointIndex<TotalTestPoints; ++TestPointIndex)
				{
					TotalDistance += SortedPoints[TestPointIndex].Distance;
				}

				// now normalize
				for (int32 TestPointIndex=0; TestPointIndex<TotalTestPoints; ++TestPointIndex)
				{
					Ele.Weights[TestPointIndex] = SortedPoints[TestPointIndex].Distance/TotalDistance;
					Ele.Indices[TestPointIndex] = SortedPoints[TestPointIndex].Index;
				}

				// if less than 3, add extra
				for (int32 TestPointIndex=TotalTestPoints; TestPointIndex<3; ++TestPointIndex)
				{
					Ele.Weights[TestPointIndex] = 0.f;
					Ele.Indices[TestPointIndex] = INDEX_NONE;
				}
			}
		}
	}
}

/** 
* Convert grid index (GridX, GridY) to triangle coords and returns FVector
*/
FVector FBlendSpaceGrid::GetPosFromIndex(int32 GridX, int32 GridY) const
{
	// grid X starts from 0 -> N when N == GridSizeX
	// grid Y starts from 0 -> N when N == GridSizeY
	// LeftBottom will map to Grid 0, 0
	// RightTop will map to Grid N, N

	FVector2D CoordDim (GridDim.GetSize());
	FVector2D EachGridSize = CoordDim/GridNum;

	// for now only 2D
	return FVector(GridX*EachGridSize.X+GridDim.Min.X, GridY*EachGridSize.Y+GridDim.Min.Y, 0.f);
}

/** 
* Get GridX, GridY indices from give Pos
*/
FIntPoint FBlendSpaceGrid::GetIndexFromPos(FVector GridPos, bool bSnap) const
{
	// grid 5 means, indexing from 0 - 5 to have 5 grids. 
	int32 GridSizeX = GridNum.X+1;
	int32 GridSizeY = GridNum.Y+1;

	FVector2D CoordDim (GridDim.GetSize());

	// 5 % of grid, allow it to be snapped
	FVector2D Threshold = CoordDim/GridNum*0.05f;
	FIntPoint BestPoint = FIntPoint::NoneValue;
	float BestDiffLenSq = (CoordDim * 2.0f).SizeSquared();

	for (int32 I=0; I<GridSizeX; ++I)
	{
		for (int32 J=0; J<GridSizeY; ++J)
		{
			FVector PointPos = GetPosFromIndex(I, J);
			FVector Diff = (PointPos-GridPos).GetAbs();
			float DiffLenSq = Diff.SizeSquared();

			// if within, success
			if (Diff.X < Threshold.X && Diff.Y < Threshold.Y)
			{
				return FIntPoint(I, J);
			}

			if (bSnap && BestDiffLenSq > DiffLenSq)
			{
				BestDiffLenSq = DiffLenSq;
				BestPoint = FIntPoint(I, J);
			}
		}
	}

	return BestPoint;
}

bool	FBlendSpaceGrid::IsInside(FVector Pos) const
{
	return ( Pos.X >= GridDim.Min.X && Pos.X <= GridDim.Max.X
		&& Pos.Y >= GridDim.Min.Y && Pos.Y <= GridDim.Max.Y 
		&& Pos.Z >= GridDim.Min.Z && Pos.Z <= GridDim.Max.Z );
}

const FEditorElement& FBlendSpaceGrid::GetElement(int32 GX, int32 GY) const
{
	// grid 5 means, indexing from 0 - 5 to have 5 grids. 
	int32 GridSizeX = GridNum.X+1;
	int32 GridSizeY = GridNum.Y+1;

	check (GridSizeX >= GX);
	check (GridSizeY >= GY);

	check ( Elements.Num() > 0 );
	return Elements[ GX*GridSizeY + GY ];
}

// mapping functions
TOptional<FVector2D> SBlendSpaceGridWidget::GetWidgetPosFromEditorPos(const FVector& EditorPos, const FSlateRect& WindowRect) const
{
	TOptional<FVector2D> OutWidgetPos;

	// widget pos is from left top to right bottom
	// where grid Pos is from (Min) to (Max) in vector space
	// You need to inverse Y
	if (BlendSpaceGrid.IsInside(EditorPos))
	{
		const FBox& GridDim = BlendSpaceGrid.GetGridDim();
		FGridSpaceConverter GridSpaceConverter(GridDim.Min, GridDim.Max, WindowRect);
		FVector2D WidgetPos = GridSpaceConverter.InputToScreen(EditorPos);
		OutWidgetPos = TOptional<FVector2D>(WidgetPos);
	}

	return OutWidgetPos;
}

TOptional<FVector> SBlendSpaceGridWidget::GetEditorPosFromWidgetPos(const FVector2D& WidgetPos, const FSlateRect& WindowRect) const
{
	TOptional<FVector> OutGridPos;

	if (WidgetPos.X >= WindowRect.Left && WidgetPos.X <= WindowRect.Right && 
		WidgetPos.Y >= WindowRect.Top && WidgetPos.Y <= WindowRect.Bottom)
	{
		FVector GridPos;
		const FBox& GridDim = BlendSpaceGrid.GetGridDim();
		FGridSpaceConverter GridSpaceConverter(GridDim.Min, GridDim.Max, WindowRect);
		GridPos = GridSpaceConverter.ScreenToInput(WidgetPos);
		OutGridPos = TOptional<FVector>(GridPos);
	}

	return OutGridPos;
}

FVector SBlendSpaceGridWidget::SnapEditorPosToGrid(const FVector& InPos) const
{
	FIntPoint GridIndices = BlendSpaceGrid.GetIndexFromPos(InPos, true);
	return BlendSpaceGrid.GetPosFromIndex(GridIndices.X, GridIndices.Y);
}

void FDelaunayTriangleGenerator::InitializeIndiceMapping()
{
	IndiceMappingTable.Empty(SamplePointList.Num());
	IndiceMappingTable.AddUninitialized(SamplePointList.Num());
	for (int32 I=0; I<SamplePointList.Num(); ++I)
	{
		IndiceMappingTable[I] = I;
	}
}

void SBlendSpaceGridWidget::ResampleData()
{
	SBlendSpaceWidget::ResampleData();

	// clear first
	ClearPoints();

	// you don't like to overwrite the link here (between visible points vs sample points, 
	// so allow this if no triangle is generated
	const FBlendParameter& BlendParamX = BlendSpace->GetBlendParameter(0);
	const FBlendParameter& BlendParamY = BlendSpace->GetBlendParameter(1);
	FIntPoint GridSize(BlendParamX.GridNum, BlendParamY.GridNum);
	FVector GridMin(BlendParamX.Min, BlendParamY.Min, 0.f);
	FVector GridMax(BlendParamX.Max, BlendParamY.Max, 0.f);
	FBox GridDim(GridMin, GridMax);

	SetGridInfo(GridSize, GridDim);

	if (CachedSamples.Num())
	{
		for (int32 I=0; I<CachedSamples.Num(); ++I)
		{
			Generator.AddSamplePoint(CachedSamples[I].SampleValue);
		}

		// add initial samples, now initialize the mapping tabel
		Generator.InitializeIndiceMapping();

		// triangulate
		Generator.Triangulate();

		// once triangulated, generated grid
		const TArray<FPoint>& Points = Generator.GetSamplePointList();
		const TArray<FTriangle*>& Triangles = Generator.GetTriangleList();
		BlendSpaceGrid.GenerateGridElements(Points, Triangles );

		// now fill up grid elements in BlendSpace using this Element information
		// @todo fixmelh: maybe use the cached indice mapping for Fillup Grid Elements
		TArray<FVector> SamplePoints;

		SamplePoints.AddUninitialized(Points.Num());
		for (int32 I=0; I<Points.Num(); ++I)
		{
			SamplePoints[I] = Points[I].Position;
		}

		if (Triangles.Num() > 0 )
		{
			const TArray<FEditorElement>& GridElements = BlendSpaceGrid.GetElements();
			BlendSpace->FillupGridElements(SamplePoints, GridElements);
		}
	}
}

/**
* Construct this widget
*
* @param	InArgs	The declaration data for this widget
*/
void SBlendSpaceGridWidget::Construct(const FArguments& InArgs)
{
	SBlendSpaceWidget::Construct(SBlendSpaceWidget::FArguments()
									.BlendSpace(InArgs._BlendSpace)
									.OnRefreshSamples(InArgs._OnRefreshSamples)
									.OnNotifyUser(InArgs._OnNotifyUser));
	// initialize variables
	PreviewInput = InArgs._PreviewInput;
	check (BlendSpace);

	CachedGridIndices = FIntPoint::NoneValue;
}

void SBlendSpaceGridWidget::DrawHighlightGrid(const FVector2D& LeftTopPos, const FVector2D& RightBottomPos, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId ) const
{
	FVector2D DrawSize = RightBottomPos - LeftTopPos;
	FVector2D Pos = LeftTopPos;

	// if end point is outside
	FVector2D OutMargin = AllottedGeometry.Size-(Pos+DrawSize);
	FPaintGeometry MyGeometry =	AllottedGeometry.ToPaintGeometry( Pos, DrawSize );
	const FSlateBrush* StyleInfo = FEditorStyle::GetBrush( TEXT( "EditableTextBox.Background.Hovered" ) );
	FSlateDrawElement::MakeBox( 
		OutDrawElements,
		LayerId, 
		MyGeometry, 
		StyleInfo, 
		MyClippingRect, 
		ESlateDrawEffect::None,
		FLinearColor(0.6f, 0.3f, 0.15f)
		);
}

FText SBlendSpaceGridWidget::GetInputText(const FVector& GridPos) const
{
	FFormatNamedArguments Args;
	Args.Add( TEXT("BlendParam01DisplayName"), FText::FromString( BlendSpace->GetBlendParameter(0).DisplayName ) );
	Args.Add( TEXT("XGridPos"), GridPos.X );

	Args.Add( TEXT("BlendParam02DisplayName"), FText::FromString( BlendSpace->GetBlendParameter(1).DisplayName ) );
	Args.Add( TEXT("YGridPos"), GridPos.Y );

	return FText::Format( LOCTEXT("BlendSpaceParamGridMessage", "{BlendParam01DisplayName} [{XGridPos}]\n{BlendParam02DisplayName} [{YGridPos}]\n"), Args );
}

int32 SBlendSpaceGridWidget::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	SCompoundWidget::OnPaint( Args, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled && IsEnabled() );

	FDrawLines LineBatcher;

	int32 DefaultLayer = LayerId;
	int32 GridLayer = LayerId+1;
	int32 HighlightLayer = LayerId+2;
	int32 SampleLayer = LayerId+3;
	int32 TooltipLayer = LayerId+4;

	FColor HighlightColor = FColor(241,146,88);
	FColor ImportantColor = FColor(241,146,88);

	FSlateRect WindowRect = GetWindowRectFromGeometry(AllottedGeometry);

	// draw the borderline for grid
	{
		TArray<FVector2D> LinePoints = {
			FVector2D(WindowRect.Left, WindowRect.Top),
			FVector2D(WindowRect.Left, WindowRect.Bottom),
			FVector2D(WindowRect.Right, WindowRect.Bottom),
			FVector2D(WindowRect.Right, WindowRect.Top),
			FVector2D(WindowRect.Left, WindowRect.Top)
		};

		FSlateDrawElement::MakeLines( 
			OutDrawElements,
			DefaultLayer,
			AllottedGeometry.ToPaintGeometry(),
			LinePoints,
			MyClippingRect,
			ESlateDrawEffect::None,
			FColor(90,90,90),
			false
			);

	}

	int32 HighlightedSampleIndex = GetHighlightedSample(WindowRect);
	TOptional<FVector2D> MousePos = GetWidgetPosFromEditorPos(LastValidMouseEditorPoint, WindowRect);

	// we should snap to grid when dragging, snap mouse pos
	if (MousePos.IsSet() &&
		bBeingDragged &&
		CachedGridIndices.X != INDEX_NONE && CachedGridIndices.Y != INDEX_NONE)
	{
		MousePos = GetWidgetPosFromEditorPos(BlendSpaceGrid.GetPosFromIndex(CachedGridIndices.X, CachedGridIndices.Y), WindowRect);
	}

	// draw sample points
	DrawSamplePoints(WindowRect, AllottedGeometry, MyClippingRect, OutDrawElements, SampleLayer, HighlightedSampleIndex);

	// first draw grid if exists
	int32 GridX = BlendSpaceGrid.GetGridNum().X;
	int32 GridY = BlendSpaceGrid.GetGridNum().Y;

	// draw grid lines
	{
		for (int32 I=0; I<GridX; ++I)
		{
			FVector GridPos1 = BlendSpaceGrid.GetPosFromIndex(I, 0);
			TOptional<FVector2D> Pos1 = GetWidgetPosFromEditorPos(GridPos1, WindowRect);
			FVector GridPos2 = BlendSpaceGrid.GetPosFromIndex(I, GridY);
			TOptional<FVector2D> Pos2 = GetWidgetPosFromEditorPos(GridPos2, WindowRect);

			LineBatcher.AddLine(Pos1.GetValue(), Pos2.GetValue(), FColor(90, 90, 90), DefaultLayer);
		}

		// draw grid lines
		for (int32 I=0; I<GridY; ++I)
		{
			FVector GridPos1 = BlendSpaceGrid.GetPosFromIndex(0, I);
			TOptional<FVector2D> Pos1 = GetWidgetPosFromEditorPos(GridPos1, WindowRect);
			FVector GridPos2 = BlendSpaceGrid.GetPosFromIndex(GridX, I);
			TOptional<FVector2D> Pos2 = GetWidgetPosFromEditorPos(GridPos2, WindowRect);

			LineBatcher.AddLine(Pos1.GetValue(), Pos2.GetValue(), FColor(90, 90, 90), DefaultLayer);
		}
	}

	GridX = CachedGridIndices.X;
	GridY = CachedGridIndices.Y;

	// if preview is on, print what is the value
	// tool tip drawing, if mouse is outside, we don't like to draw and if it's not being dragged
	if ( MousePos.IsSet() && !bBeingDragged )
	{
		const TArray<FEditorElement>& Elements = BlendSpaceGrid.GetElements();
		bool bIsOnValidSample = (HighlightedSampleIndex != INDEX_NONE) && CachedSamples.IsValidIndex(HighlightedSampleIndex);

		if(bIsOnValidSample && CachedSamples[HighlightedSampleIndex].bIsValid == false)
		{
			FText InvalidSampleMessage = LOCTEXT("BlendSpaceInvalidSample", "This sample is in an invalid location.\n\nPlease drag it to a grid point.");
			DrawToolTip( MousePos.GetValue(), InvalidSampleMessage, AllottedGeometry, MyClippingRect, FColor(255, 20, 20), OutDrawElements, TooltipLayer);
		}
		else if ( Elements.Num() > 0 && GridX != INDEX_NONE &&  GridY != INDEX_NONE )
		{
			// if we have elements, see if we're at any grid point, if so, display what is the value for it

			const FEditorElement& Ele = BlendSpaceGrid.GetElement(GridX,GridY);
			FVector GridPos(BlendSpaceGrid.GetPosFromIndex(GridX, GridY));
			TArray<UAnimSequence*>	Seqs;
			TArray<float>			Weights;
			for (int32 I=0; I<3; ++I)
			{
				if ( Ele.Weights[I] > ZERO_ANIMWEIGHT_THRESH )
				{
					int32 OriginalIndex = Generator.GetOriginalIndex(Ele.Indices[I]);
					if (CachedSamples.IsValidIndex(OriginalIndex))
					{
						UAnimSequence * Seq = CachedSamples[OriginalIndex].Animation;
						Seqs.Add(Seq);
						Weights.Add(Ele.Weights[I]);
					}
				}
			}
			FText ToolTipMessage = GetToolTipText(GridPos, Seqs, Weights);
			DrawToolTip( MousePos.GetValue(), ToolTipMessage, AllottedGeometry, MyClippingRect, FColor::White, OutDrawElements, TooltipLayer);
		}
		else if ( Elements.Num() > 0 )
		{
			// consolidate all samples and sort them, so that we can handle from biggest weight to smallest
			TArray<FBlendSampleData> SampleDataList;
			const TArray<struct FBlendSample>& SampleData = BlendSpace->GetBlendSamples();

			// if no sample data is found, return 
			if (BlendSpace->GetSamplesFromBlendInput(LastValidMouseEditorPoint, SampleDataList))
			{
				TArray<UAnimSequence*>	Seqs;
				TArray<float>			Weights;

				for (int32 I=0; I<SampleDataList.Num(); ++I)
				{
					const FBlendSample& Sample = SampleData[SampleDataList[I].SampleDataIndex];
					if (Sample.Animation)
					{
						UAnimSequence * Seq = Sample.Animation;
						Seqs.Add(Seq);
						Weights.Add(SampleDataList[I].GetWeight());
					}
				}		

				FText ToolTipMessage;
				if (bPreviewOn)
				{
					ToolTipMessage = FText::Format( LOCTEXT("Previewing", "Previewing\n\n{0}"), GetToolTipText(LastValidMouseEditorPoint, Seqs, Weights) );
				}
				else
				{
					ToolTipMessage = GetToolTipText(LastValidMouseEditorPoint, Seqs, Weights);
				}

				DrawToolTip(MousePos.GetValue(), ToolTipMessage, AllottedGeometry, MyClippingRect, FColor::White, OutDrawElements, TooltipLayer);
			}

			if (bPreviewOn)
			{
				TOptional<FVector2D> PreviewBSInput = GetWidgetPosFromEditorPos(PreviewInput.Get(), WindowRect);
				if (PreviewBSInput.IsSet())
				{
					DrawSamplePoint(PreviewBSInput.GetValue(), EBlendSpaceSampleState::Highlighted, AllottedGeometry, MyClippingRect, OutDrawElements, HighlightLayer);
				}
			}
		}
		else if (bIsOnValidSample)
		{
			// if i'm on top of sample, draw tooltip as well
			UAnimSequence * Seq = CachedSamples[HighlightedSampleIndex].Animation;
			const FText Sequence = (Seq)? FText::FromString( Seq->GetName() ) : LOCTEXT("None", "None");
			const FText ToolTipMessage = FText::Format( LOCTEXT("Sequence", "{0}\nSequence ({1})"), GetInputText(LastValidMouseEditorPoint), Sequence );
			DrawToolTip(MousePos.GetValue(), ToolTipMessage, AllottedGeometry, MyClippingRect, FColor::White, OutDrawElements, TooltipLayer);
		}
	}

	// draw triangles
	const TArray<FTriangle*>& TriangleList = Generator.GetTriangleList();
	// now I'd like to get all triangles all 4 points are available
	TArray<FTriangle*> NewTriangles;
	if ( MousePos.IsSet() && TriangleList.Num() > 0 )
	{
		FVector BaryCentricCoords;
		FTriangle * HighlightTriangle = NULL;

		// if your mouse is on grid point, show the triangles
		if ( GridX != INDEX_NONE &&  GridY != INDEX_NONE )
		{
			TOptional<FVector> MouseGridPos = GetEditorPosFromWidgetPos(MousePos.GetValue(), WindowRect);
			// do not search it if grid is selected
			if (MouseGridPos.IsSet())
			{
				BlendSpaceGrid.FindTriangleThisPointBelongsTo(MouseGridPos.GetValue(), BaryCentricCoords, HighlightTriangle, TriangleList);
			}
		}
		else
		{
			// otherwise highlight the grid it's in right now
			FVector2D LeftTop, RightBottom;
			if (GetMinMaxFromGridPos(LastValidMouseEditorPoint, LeftTop, RightBottom, WindowRect))
			{
				DrawHighlightGrid(LeftTop, RightBottom, AllottedGeometry, MyClippingRect, OutDrawElements, DefaultLayer);

				// Draw Lines from each point and draw texts
				FGridBlendSample LBSample, RBSample, LTSample, RTSample;
				GetBlendSpace()->GetGridSamplesFromBlendInput(LastValidMouseEditorPoint, LBSample, RBSample, LTSample, RTSample);

				FVector2D WindowPos = MousePos.GetValue();
				
				FVector2D StartPos = LeftTop;
				LineBatcher.AddLine(StartPos, WindowPos, ImportantColor, GridLayer);
				DrawText(StartPos, FText::AsNumber( LTSample.BlendWeight), AllottedGeometry, MyClippingRect, ImportantColor, OutDrawElements, GridLayer);

				StartPos = FVector2D(LeftTop.X, RightBottom.Y);
				LineBatcher.AddLine(StartPos, WindowPos, ImportantColor, GridLayer);
				DrawText(StartPos, FText::AsNumber( LBSample.BlendWeight), AllottedGeometry, MyClippingRect, ImportantColor, OutDrawElements, GridLayer);

				StartPos = FVector2D(RightBottom.X, LeftTop.Y);
				LineBatcher.AddLine(StartPos, WindowPos, ImportantColor, GridLayer);
				DrawText(StartPos, FText::AsNumber( RTSample.BlendWeight), AllottedGeometry, MyClippingRect, ImportantColor, OutDrawElements, GridLayer);

				StartPos = RightBottom;
				LineBatcher.AddLine(StartPos, WindowPos, ImportantColor, GridLayer);
				DrawText(StartPos, FText::AsNumber( RBSample.BlendWeight), AllottedGeometry, MyClippingRect, ImportantColor, OutDrawElements, GridLayer);

				UE_LOG(LogBlendSpace, Verbose, TEXT("BlendSpace weight: LT(%0.2f), LB(%0.2f), RT(%0.2f), RB(%0.2f)"), LTSample.BlendWeight, LBSample.BlendWeight, RTSample.BlendWeight, RBSample.BlendWeight);
				if (LBSample.BlendWeight > 0.f)
				{
					TOptional<FVector> GridValue = GetEditorPosFromWidgetPos(FVector2D(LeftTop.X, RightBottom.Y), WindowRect);
					if (GridValue.IsSet())
					{
						FVector DummyBaryCentricCoords;
						FTriangle * Triangle = NULL;
						if (BlendSpaceGrid.FindTriangleThisPointBelongsTo(GridValue.GetValue(), DummyBaryCentricCoords, Triangle, TriangleList))
						{
							UE_LOG(LogBlendSpace, Verbose, TEXT("LB: %s"), *DummyBaryCentricCoords.ToString());
							NewTriangles.Add(Triangle);
						}
					}
				}				

				if (RBSample.BlendWeight > 0.f)
				{
					TOptional<FVector> GridValue = GetEditorPosFromWidgetPos(RightBottom, WindowRect);
					if (GridValue.IsSet())
					{
						FVector DummyBaryCentricCoords;
						FTriangle * Triangle = NULL;
						if (BlendSpaceGrid.FindTriangleThisPointBelongsTo(GridValue.GetValue(), DummyBaryCentricCoords, Triangle, TriangleList))
						{
							UE_LOG(LogBlendSpace, Verbose, TEXT("RB: %s"), *DummyBaryCentricCoords.ToString());
							NewTriangles.Add(Triangle);
						}
					}
				}				

				if (LTSample.BlendWeight > 0.f)
				{
					TOptional<FVector> GridValue = GetEditorPosFromWidgetPos(LeftTop, WindowRect);
					if (GridValue.IsSet())
					{
						FVector DummyBaryCentricCoords;
						FTriangle * Triangle = NULL;
						if (BlendSpaceGrid.FindTriangleThisPointBelongsTo(GridValue.GetValue(), DummyBaryCentricCoords, Triangle, TriangleList))
						{
							UE_LOG(LogBlendSpace, Verbose, TEXT("LT: %s"), *DummyBaryCentricCoords.ToString());
							NewTriangles.Add(Triangle);
						}
					}
				}				

				if (RTSample.BlendWeight > 0.f)
				{
					TOptional<FVector> GridValue = GetEditorPosFromWidgetPos(FVector2D(RightBottom.X, LeftTop.Y), WindowRect);
					if (GridValue.IsSet())
					{
						FVector DummyBaryCentricCoords;
						FTriangle * Triangle = NULL;
						if (BlendSpaceGrid.FindTriangleThisPointBelongsTo(GridValue.GetValue(), DummyBaryCentricCoords, Triangle, TriangleList))
						{
							UE_LOG(LogBlendSpace, Verbose, TEXT("RT: %s"), *DummyBaryCentricCoords.ToString());
							NewTriangles.Add(Triangle);
						}
					}
				}				
			}
		}
	
		// draw all triangles
		for (int32 I=0; I<TriangleList.Num(); ++I)
		{
			FTriangle * Triangle = TriangleList[I];
			bool bHighlighted = (HighlightTriangle == Triangle) ||  NewTriangles.Find(Triangle)!=INDEX_NONE;
			// if currently highlighted, use highlight color
			FColor ColorToDraw = (bHighlighted)?((bPreviewOn)? ImportantColor: HighlightColor): FColor(200,200,200);

			int32 MyLayerId = (bHighlighted)? HighlightLayer: GridLayer;

			const TOptional<FVector2D> A = GetWidgetPosFromEditorPos(Triangle->Vertices[0]->Position, WindowRect);
			const TOptional<FVector2D> B = GetWidgetPosFromEditorPos(Triangle->Vertices[1]->Position, WindowRect);
			const TOptional<FVector2D> C = GetWidgetPosFromEditorPos(Triangle->Vertices[2]->Position, WindowRect);

			LineBatcher.AddTriangle(A.GetValue(), B.GetValue(), C.GetValue(), ColorToDraw, MyLayerId);
		}

		if (HighlightTriangle!=NULL)
		{
			FVector2D TextPos = GetWidgetPosFromEditorPos(HighlightTriangle->Vertices[0]->Position, WindowRect).GetValue();
			FColor Color = (bPreviewOn)? ImportantColor: HighlightColor;

			FNumberFormattingOptions Options;
			Options.MinimumFractionalDigits = 5;

			DrawText( FVector2D(TextPos.X, TextPos.Y + 14), FText::AsNumber( BaryCentricCoords.X, &Options ), AllottedGeometry, MyClippingRect, Color, OutDrawElements, HighlightLayer);
			LineBatcher.AddLine(MousePos.GetValue(), TextPos, Color, HighlightLayer);

			TextPos = GetWidgetPosFromEditorPos(HighlightTriangle->Vertices[1]->Position, WindowRect).GetValue();
			DrawText( FVector2D(TextPos.X, TextPos.Y + 14), FText::AsNumber( BaryCentricCoords.Y, &Options), AllottedGeometry, MyClippingRect, Color, OutDrawElements, HighlightLayer);
			LineBatcher.AddLine(MousePos.GetValue(), TextPos, Color, HighlightLayer);

			TextPos = GetWidgetPosFromEditorPos(HighlightTriangle->Vertices[2]->Position, WindowRect).GetValue();
			DrawText( FVector2D(TextPos.X, TextPos.Y + 14), FText::AsNumber( BaryCentricCoords.Z, &Options), AllottedGeometry, MyClippingRect, Color, OutDrawElements, HighlightLayer);
			LineBatcher.AddLine(MousePos.GetValue(), TextPos, Color, HighlightLayer);
		}
	}

	LineBatcher.Draw(AllottedGeometry, MyClippingRect, OutDrawElements);

	// very hard coded, will need to fix this, need to increase 1 more since ToolTip uses one more
	LayerId = TooltipLayer + 1;

	return LayerId;
}

FReply SBlendSpaceGridWidget::UpdateLastMousePosition( const FGeometry& MyGeometry, const FVector2D& ScreenSpacePosition, bool bClampToWindowRect, bool bSnapToGrid )
{
	FSlateRect WindowRect = GetWindowRectFromGeometry(MyGeometry);
	FVector2D NewPoint = MyGeometry.AbsoluteToLocal(ScreenSpacePosition);

	if(bClampToWindowRect)
	{
		NewPoint.X = FMath::Clamp<float>(NewPoint.X, WindowRect.Left, WindowRect.Right);
		NewPoint.Y = FMath::Clamp<float>(NewPoint.Y, WindowRect.Top, WindowRect.Bottom);
	}

	TOptional<FVector> GridPos = GetEditorPosFromWidgetPos(NewPoint, WindowRect);
	if (GridPos.IsSet())
	{
		CachedGridIndices = BlendSpaceGrid.GetIndexFromPos(GridPos.GetValue(), bBeingDragged);
		LastValidMouseEditorPoint = GridPos.GetValue();
		if(bSnapToGrid)
		{
			LastValidMouseEditorPoint = SnapEditorPosToGrid(LastValidMouseEditorPoint);
		}
	}

	return FReply::Handled();
}

// debug/test code
void SBlendSpaceGridWidget::AddSample()
{
	// add boundary -  hard coded for now
	AddGridPoint(FVector(0, 0, 0));
	AddGridPoint(FVector(0, 100, 0));
	AddGridPoint(FVector(100, 100, 0));
	AddGridPoint(FVector(100, 0, 0));

	Generator.EmptyTriangles();
	Generator.SortSamples();
}

bool SBlendSpaceGridWidget::IsOnTheBorder(const FIntPoint& GridIndices) const
{
	return (GridIndices.X == 0 || GridIndices.X == BlendSpace->GetBlendParameter(0).GridNum ||
		GridIndices.Y == 0 || GridIndices.Y == BlendSpace->GetBlendParameter(1).GridNum );
}

void SBlendSpaceEditor::OnBlendSpaceParamtersChanged()
{
	GetBlendSpaceGridWidget()->CachedGridIndices = FIntPoint::NoneValue;
	BlendSpaceWidget->LastValidMouseEditorPoint = FVector::ZeroVector;

	UpdateBlendParameters();
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SBlendSpaceEditor::Construct(const FArguments& InArgs)
{
	SBlendSpaceEditorBase::Construct(SBlendSpaceEditorBase::FArguments()
									 .Persona(InArgs._Persona)
									 .BlendSpace(InArgs._BlendSpace)
									);
	this->ChildSlot
	[
		SNew(SVerticalBox)

		+SVerticalBox::Slot()
		.AutoHeight()
		[
			MakeEditorHeader()
		]

		+SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SOverlay)
			+SOverlay::Slot()
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				.FillWidth(1)
				.Padding(4.0f)
				[
					SNew(SHorizontalBox)
					
					// Vertical axis
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2)
					.HAlign(HAlign_Left)
					[
						SNew(SVerticalBox) 

						+SVerticalBox::Slot()
						.FillHeight(1)
						.VAlign(VAlign_Top)
						[
							SAssignNew(ParameterY_Max, STextBlock)
						]

						+SVerticalBox::Slot()
						.AutoHeight()
						.VAlign(VAlign_Top)
						[
							SAssignNew(ParameterY_Min, STextBlock)
						]
					]

					// Grid and horizontal axis
					+SHorizontalBox::Slot()
					.FillWidth(1)
					.Padding(2) 
					[
						SNew(SVerticalBox)

						// Grid area
						+SVerticalBox::Slot()
						.FillHeight(1)
						[
							SAssignNew(BlendSpaceWidget, SBlendSpaceGridWidget)
							.Cursor(EMouseCursor::Crosshairs)
							.BlendSpace(GetBlendSpace())
							.PreviewInput(this, &SBlendSpaceEditor::GetPreviewBlendInput)
							.OnRefreshSamples(this, &SBlendSpaceEditor::RefreshSampleDataPanel)
							.OnNotifyUser(this, &SBlendSpaceEditor::NotifyUser)
						]

						// Horizontal axis
						+SVerticalBox::Slot()
						.AutoHeight()
						.VAlign(VAlign_Top)
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot()
							.AutoWidth()
							.HAlign(HAlign_Left)
							[
								SAssignNew(ParameterX_Min, STextBlock)
							]

							+SHorizontalBox::Slot()
							.FillWidth(1)
							.HAlign(HAlign_Right)
							[
								SAssignNew(ParameterX_Max, STextBlock)
							]
						]
					]
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2)
				.HAlign(HAlign_Fill)
				[
					SNew(SVerticalBox)

					+SVerticalBox::Slot() .FillHeight(1) 
					[
						SNew(SScrollBox)

						+SScrollBox::Slot()
						[
							// add parameter values
							SNew(SVerticalBox)

							+SVerticalBox::Slot()
							.AutoHeight()
							.Padding(5)
							[
								SAssignNew(ParameterWidget, SBlendSpaceParameterWidget)
								.BlendSpace(BlendSpace)
								.OnParametersUpdated(this, &SBlendSpaceEditor::OnBlendSpaceParamtersChanged)
							]

							+SVerticalBox::Slot()
							.AutoHeight()
							.Padding(5)
							[
								// miscellaneous information
								SAssignNew(SamplesWidget, SBlendSpaceSamplesWidget)
								.BlendSpace(BlendSpace)
								.OnSamplesUpdated(this, &SBlendSpaceEditor::OnBlendSpaceSamplesChanged)
								.OnNotifyUser(this, &SBlendSpaceEditor::NotifyUser)
							]
						]
					]

					+SVerticalBox::Slot()
					.AutoHeight()
					.Padding(5)
					[
						MakeDisplayOptionsBox()
					]
				]
			]

			+SOverlay::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Bottom)
			.Padding(15)
			[
				SAssignNew(NotificationListPtr, SNotificationList)
				.Visibility(EVisibility::HitTestInvisible)
			]
		]
	];

	ParameterWidget->ConstructParameterPanel();
	SamplesWidget->ConstructSamplesPanel();
	UpdateBlendParameters();
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SBlendSpaceEditor::UpdateBlendParameters()
{
	// update grid info
	const FBlendParameter& BlendParamX = BlendSpace->GetBlendParameter(0);
	const FBlendParameter& BlendParamY = BlendSpace->GetBlendParameter(1);
	FIntPoint GridSize(BlendParamX.GridNum, BlendParamY.GridNum);
	FVector GridMin(BlendParamX.Min, BlendParamY.Min, 0.f);
	FVector GridMax(BlendParamX.Max, BlendParamY.Max, 0.f);
	FBox GridDim(GridMin, GridMax);
	GetBlendSpaceGridWidget()->SetGridInfo(GridSize, GridDim);

	FString ParameterXName = BlendParamX.DisplayName;
	FString ParameterYName = BlendParamY.DisplayName;
	// update UI for parameters
	if (ParameterXName==TEXT("None"))
	{
		ParameterXName = TEXT("X");
	}

	if (ParameterYName==TEXT("None"))
	{
		ParameterYName = TEXT("Y");
	}

	static const FNumberFormattingOptions FormatOptions = FNumberFormattingOptions()
		.SetMinimumFractionalDigits(2)
		.SetMaximumFractionalDigits(2);
	static const FText XMinMaxFmt = FText::FromString(TEXT("{0}[{1}]"));
	static const FText YMinMaxFmt = FText::FromString(TEXT("{0}\n[{1}]"));

	ParameterX_Min->SetText(FText::Format(XMinMaxFmt, FText::FromString(ParameterXName), FText::AsNumber(BlendParamX.Min, &FormatOptions)));
	ParameterX_Max->SetText(FText::Format(XMinMaxFmt, FText::FromString(ParameterXName), FText::AsNumber(BlendParamX.Max, &FormatOptions)));
	ParameterY_Min->SetText(FText::Format(YMinMaxFmt, FText::FromString(ParameterYName), FText::AsNumber(BlendParamY.Min, &FormatOptions)));
	ParameterY_Max->SetText(FText::Format(YMinMaxFmt, FText::FromString(ParameterYName), FText::AsNumber(BlendParamY.Max, &FormatOptions)));

	BlendSpaceWidget->ResampleData();
}

void SBlendSpaceEditor::SetBlendSpace(UBlendSpace* NewBlendSpace)
{
	BlendSpace = NewBlendSpace;

	ParameterWidget->SetBlendSpace(BlendSpace);
	UpdateBlendParameters();
}

FVector NormalizeGridPos(const FBox& GridDim, const FVector GridPos)
{
	FVector Size= GridDim.GetSize();
	// this should not happen, if so we'll get exception
	ensure (Size.X !=0 && Size.Y !=0);
	Size.Z = 1.f;

	return (GridPos - GridDim.Min) / Size;
}

FVector UnnormalizeGridPos(const FBox& GridDim, const FVector& NormalizedGridPos)
{
	// this should not happen
	return (NormalizedGridPos * GridDim.GetSize()) + GridDim.Min;
}
/** 
* Get Min/Max windows position from Grid Pos
*/
bool SBlendSpaceGridWidget::GetMinMaxFromGridPos(FVector GridPos, FVector2D& WindowLeftTop, FVector2D& WindowRightBottom, const FSlateRect& WindowRect) const
{
	// grid 5 means, indexing from 0 - 5 to have 5 grids. 
	const FBox& GridDim = BlendSpaceGrid.GetGridDim();
	FVector InvGridSize = FVector(BlendSpaceGrid.GetGridNum(), 1.f).Reciprocal();

	// find normalized pos from 0 - 1
	FVector NormalizedGridPos = NormalizeGridPos(GridDim, GridPos);
	FVector Multiplier = NormalizedGridPos/InvGridSize;
	FVector GridMin, GridMax;
	GridMin.X = InvGridSize.X * FGenericPlatformMath::TruncToFloat(Multiplier.X);
	GridMin.Y = InvGridSize.Y * FGenericPlatformMath::TruncToFloat(Multiplier.Y);
	GridMin.Z = 0.f;

	GridMax = GridMin + InvGridSize;
	GridMax.Z = 0.f;

	GridMin = UnnormalizeGridPos(GridDim, GridMin);
	GridMax = UnnormalizeGridPos(GridDim, GridMax);

	// Grid Min/Max for Y is from bottom to top
	// but in window it's top to bottom
	// so to get WindowLeftTop, you'll need GridMin.X, GridMax.Y
	TOptional<FVector2D> LT, RB;
	LT = GetWidgetPosFromEditorPos(FVector(GridMin.X, GridMax.Y, 0.f), WindowRect);
	RB = GetWidgetPosFromEditorPos(FVector(GridMax.X, GridMin.Y, 0.f), WindowRect);

	if (LT.IsSet() && RB.IsSet())
	{
		WindowLeftTop = LT.GetValue();
		WindowRightBottom = RB.GetValue();

		TOptional<FVector> Min = GetEditorPosFromWidgetPos(WindowLeftTop, WindowRect);
		TOptional<FVector> Max = GetEditorPosFromWidgetPos(WindowRightBottom, WindowRect);
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
