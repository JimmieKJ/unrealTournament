// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AnimationBlendSpaceHelpers.h"

#define BLENDSPACE_MINSAMPLE	3
#define BLENDSPACE_MINAXES		1
#define BLENDSPACE_MAXAXES		3

#define LOCTEXT_NAMESPACE "AnimationBlendSpaceHelpers"

struct FPointWithIndex
{
	FPoint Point;
	int32 OriginalIndex;

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

void FDelaunayTriangleGenerator::EmptyTriangles()
{
	for (int32 TriangleIndex = 0; TriangleIndex < TriangleList.Num(); ++TriangleIndex)
	{
		delete TriangleList[TriangleIndex];
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

void FDelaunayTriangleGenerator::AddSamplePoint(const FVector& NewPoint)
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
	SortedPoints.Reserve(SamplePointList.Num());
	for (int32 SampleIndex = 0; SampleIndex < SamplePointList.Num(); ++SampleIndex)
	{
		SortedPoints.Add(FPointWithIndex(SamplePointList[SampleIndex], SampleIndex));
	}

	// return A-B
	struct FComparePoints
	{
		FORCEINLINE bool operator()( const FPointWithIndex &A, const FPointWithIndex&B ) const
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
			
			return A.Point.Position.X < B.Point.Position.X;			
		}
	};
	// sort all points
	SortedPoints.Sort( FComparePoints() );

	// now copy back to SamplePointList
	IndiceMappingTable.Empty(SamplePointList.Num());
	IndiceMappingTable.AddUninitialized(SamplePointList.Num());
	for (int32 SampleIndex = 0; SampleIndex < SamplePointList.Num(); ++SampleIndex)
	{
		SamplePointList[SampleIndex] = SortedPoints[SampleIndex].Point;
		IndiceMappingTable[SampleIndex] = SortedPoints[SampleIndex].OriginalIndex;
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
	
	// Unrolled loop
	NormalizedPositions[0] = (T->Vertices[0]->Position - GridMin) * RecipGridSize;
	NormalizedPositions[1] = (T->Vertices[1]->Position - GridMin) * RecipGridSize;
	NormalizedPositions[2] = (T->Vertices[2]->Position - GridMin) * RecipGridSize;

	const FVector NormalizedTestPoint = ( TestPoint.Position - GridMin ) * RecipGridSize;

	// ignore Z, eventually this has to be on plane
	// http://en.wikipedia.org/wiki/Delaunay_triangulation - determinant
	const float M00 = NormalizedPositions[0].X - NormalizedTestPoint.X;
	const float M01 = NormalizedPositions[0].Y - NormalizedTestPoint.Y;
	const float M02 = NormalizedPositions[0].X * NormalizedPositions[0].X - NormalizedTestPoint.X * NormalizedTestPoint.X
		+ NormalizedPositions[0].Y*NormalizedPositions[0].Y - NormalizedTestPoint.Y * NormalizedTestPoint.Y;

	const float M10 = NormalizedPositions[1].X - NormalizedTestPoint.X;
	const float M11 = NormalizedPositions[1].Y - NormalizedTestPoint.Y;
	const float M12 = NormalizedPositions[1].X * NormalizedPositions[1].X - NormalizedTestPoint.X * NormalizedTestPoint.X
		+ NormalizedPositions[1].Y * NormalizedPositions[1].Y - NormalizedTestPoint.Y * NormalizedTestPoint.Y;

	const float M20 = NormalizedPositions[2].X - NormalizedTestPoint.X;
	const float M21 = NormalizedPositions[2].Y - NormalizedTestPoint.Y;
	const float M22 = NormalizedPositions[2].X * NormalizedPositions[2].X - NormalizedTestPoint.X * NormalizedTestPoint.X
		+ NormalizedPositions[2].Y * NormalizedPositions[2].Y - NormalizedTestPoint.Y * NormalizedTestPoint.Y;

	const float Det = M00*M11*M22+M01*M12*M20+M02*M10*M21 - (M02*M11*M20+M01*M10*M22+M00*M12*M21);
	
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
	const FVector Diff1 = B->Position - A->Position;
	const FVector Diff2 = C->Position - A->Position;

	const float Result = Diff1.X*Diff2.Y - Diff1.Y*Diff2.X;

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

bool FDelaunayTriangleGenerator::FlipTriangles(const int32 TriangleIndexOne, const int32 TriangleIndexTwo)
{
	bool bFlipped = false;
	const FTriangle* A = TriangleList[TriangleIndexOne];
	const FTriangle* B = TriangleList[TriangleIndexTwo];

	// if already optimized, don't have to do any
	FPoint* TestPt = A->FindNonSharingPoint(B);

	// If it's not inside, we don't have to do any
	if (GetCircumcircleState(A, *TestPt) != ECCS_Inside)
	{
		return false;
	}

	int32 TrianglesMade = 0;

	// otherwise, try make new triangle, and flip it
	if (IsEligibleForTriangulation(A->Vertices[0],A->Vertices[1], TestPt))
	{
		FTriangle NewTriangle(A->Vertices[0],A->Vertices[1], TestPt);
		// only flip if outside
		if (GetCircumcircleState(&NewTriangle, *A->Vertices[2])==ECCS_Outside)
		{
			// good triangle
			AddTriangle(NewTriangle, false);
			bFlipped=true;
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
			bFlipped=true;
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
			bFlipped=true;
			++TrianglesMade;
		}
	}

	// should be 2, if not we miss triangles
	check (	TrianglesMade==2 );

	return bFlipped;
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

void FDelaunayTriangleGenerator::InitializeIndiceMapping()
{
	IndiceMappingTable.Empty(SamplePointList.Num());
	IndiceMappingTable.AddUninitialized(SamplePointList.Num());
	for (int32 SampleIndex = 0; SampleIndex < SamplePointList.Num(); ++SampleIndex)
	{
		IndiceMappingTable[SampleIndex] = SampleIndex;
	}
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
	// Calculate distance from point to triangle and sort the triangle list accordingly
	TArray<FSortByDistance> SortedTriangles;
	SortedTriangles.AddUninitialized(TriangleList.Num());
	for (int32 TriangleIndex=0; TriangleIndex<TriangleList.Num(); ++TriangleIndex)
	{
		SortedTriangles[TriangleIndex].Index = TriangleIndex;
		SortedTriangles[TriangleIndex].Distance = TriangleList[TriangleIndex]->GetDistance(TestPoint);
	}
	SortedTriangles.Sort([](const FSortByDistance &A, const FSortByDistance &B) { return A.Distance < B.Distance; });

	// Now loop over the sorted triangles and test the barycentric coordinates with the point
	for (const FSortByDistance& SortedTriangle : SortedTriangles)
	{
		FTriangle* Triangle = TriangleList[SortedTriangle.Index];

		FVector Coords = FMath::GetBaryCentric2D(TestPoint, Triangle->Vertices[0]->Position, Triangle->Vertices[1]->Position, Triangle->Vertices[2]->Position);

		// Z coords often has precision error because it's derived from 1-A-B, so do more precise check
		if (FMath::Abs(Coords.Z) < KINDA_SMALL_NUMBER)
		{
			Coords.Z = 0.f;
		}

		// Is the point inside of the triangle, or on it's edge (Z coordinate should always match since the blend samples are set in 2D)
		if ( 0.f <= Coords.X && Coords.X <= 1.0 && 0.f <= Coords.Y && Coords.Y <= 1.0 && 0.f <= Coords.Z && Coords.Z <= 1.0 )
		{
			OutBaryCentricCoords = Coords;
			OutTriangle = Triangle;
			return true;
		}
	}

	return false;
}

/** 
* Fill up Grid Elements using TriangleList input
* 
* @param	SamplePoints		: Sample Point List
* @param	TriangleList		: List of triangles
*/
void FBlendSpaceGrid::GenerateGridElements(const TArray<FPoint>& SamplePoints, const TArray<FTriangle*>& TriangleList)
{
	check (NumGridDivisions.X > 0 && NumGridDivisions.Y > 0 );
	check ( GridDimensions.IsValid );

	const int32 ElementsSize = NumGridPoints.X * NumGridPoints.Y;

	Elements.Empty(ElementsSize);

	if (SamplePoints.Num() == 0 || TriangleList.Num() == 0)
	{
		return;
	}

	Elements.AddUninitialized(ElementsSize);

	FVector Weights;
	FVector PointPos;
	// when it fails to find, do distance resolution
	const int32 TotalTestPoints = (SamplePoints.Num() >= 3)? 3 : SamplePoints.Num();

	for (int32 GridPositionX = 0; GridPositionX < NumGridPoints.X; ++GridPositionX)
	{
		for (int32 GridPositionY = 0; GridPositionY < NumGridPoints.Y; ++GridPositionY)
		{
			FTriangle * SelectedTriangle = NULL;
			FEditorElement& Ele = Elements[ GridPositionX*NumGridPoints.Y + GridPositionY ];

			PointPos = GetPosFromIndex(GridPositionX, GridPositionY);
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
					const float Distance = (PointPos-SamplePoints[SamplePointIndex].Position).Size();
					SortedPoints.Add(FSortByDistance(SamplePointIndex, Distance));
				}

				SortedPoints.Sort([](const FSortByDistance &A, const FSortByDistance &B) { return A.Distance < B.Distance; });

				// do based on distance resolution
				float TotalDistance = 0.f;

				for (int32 TestPointIndex=0; TestPointIndex<TotalTestPoints; ++TestPointIndex)
				{
					TotalDistance += SortedPoints[TestPointIndex].Distance;
				}

				const bool bNonZeroDistance = !FMath::IsNearlyZero(TotalDistance);

				// now normalize
				for (int32 TestPointIndex=0; TestPointIndex<TotalTestPoints; ++TestPointIndex)
				{
					// If total distance is non-zero do the division, otherwise just set to 1.0f (this was causing NANs and dissapearing meshes)
					Ele.Weights[TestPointIndex] = bNonZeroDistance ? SortedPoints[TestPointIndex].Distance / TotalDistance : 1.0f;
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
const FVector FBlendSpaceGrid::GetPosFromIndex(const int32 GridX, const int32 GridY) const
{
	// grid X starts from 0 -> N when N == GridSizeX
	// grid Y starts from 0 -> N when N == GridSizeY
	// LeftBottom will map to Grid 0, 0
	// RightTop will map to Grid N, N

	FVector2D CoordDim (GridDimensions.GetSize());
	FVector2D EachGridSize = CoordDim / NumGridDivisions;

	// for now only 2D
	return FVector(GridX*EachGridSize.X+GridDimensions.Min.X, GridY*EachGridSize.Y+GridDimensions.Min.Y, 0.f);
}

const FEditorElement& FBlendSpaceGrid::GetElement(const int32 GridX, const int32 GridY) const
{
	check (NumGridPoints.X >= GridX);
	check (NumGridPoints.Y >= GridY);

	check ( Elements.Num() > 0 );
	return Elements[GridX * NumGridPoints.Y + GridY];
}

#undef LOCTEXT_NAMESPACE
