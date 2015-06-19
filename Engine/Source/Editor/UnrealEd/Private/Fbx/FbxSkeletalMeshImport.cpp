// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*
* Copyright 2009 - 2010 Autodesk, Inc.  All Rights Reserved.
*
* Permission to use, copy, modify, and distribute this software in object
* code form for any purpose and without fee is hereby granted, provided
* that the above copyright notice appears in all copies and that both
* that copyright notice and the limited warranty and restricted rights
* notice below appear in all supporting documentation.
*
* AUTODESK PROVIDES THIS PROGRAM "AS IS" AND WITH ALL FAULTS.
* AUTODESK SPECIFICALLY DISCLAIMS ANY AND ALL WARRANTIES, WHETHER EXPRESS
* OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTY
* OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR USE OR NON-INFRINGEMENT
* OF THIRD PARTY RIGHTS.  AUTODESK DOES NOT WARRANT THAT THE OPERATION
* OF THE PROGRAM WILL BE UNINTERRUPTED OR ERROR FREE.
*
* In no event shall Autodesk, Inc. be liable for any direct, indirect,
* incidental, special, exemplary, or consequential damages (including,
* but not limited to, procurement of substitute goods or services;
* loss of use, data, or profits; or business interruption) however caused
* and on any theory of liability, whether in contract, strict liability,
* or tort (including negligence or otherwise) arising in any way out
* of such code.
*
* This software is provided to the U.S. Government with the same rights
* and restrictions as described herein.
*/

/*=============================================================================
	Skeletal mesh creation from FBX data.
	Largely based on SkeletalMeshImport.cpp
=============================================================================*/

#include "UnrealEd.h"

#include "Engine.h"
#include "TextureLayout.h"
#include "SkelImport.h"
#include "FbxImporter.h"
#include "AnimEncoding.h"
#include "SSkeletonWidget.h"

#include "AssetRegistryModule.h"
#include "AssetNotifications.h"

#include "ObjectTools.h"

#include "ApexClothingUtils.h"
#include "Developer/MeshUtilities/Public/MeshUtilities.h"

#include "MessageLogModule.h"
#include "ComponentReregisterContext.h"

#include "FbxErrors.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Engine/SkeletalMeshSocket.h"

#define LOCTEXT_NAMESPACE "FBXImpoter"

using namespace UnFbx;

struct ExistingSkelMeshData;
extern ExistingSkelMeshData* SaveExistingSkelMeshData(USkeletalMesh* ExistingSkelMesh);
extern void RestoreExistingSkelMeshData(ExistingSkelMeshData* MeshData, USkeletalMesh* SkeletalMesh);

// Get the geometry deformation local to a node. It is never inherited by the
// children.
FbxAMatrix GetGeometry(FbxNode* pNode) 
{
	FbxVector4 lT, lR, lS;
	FbxAMatrix lGeometry;

	lT = pNode->GetGeometricTranslation(FbxNode::eSourcePivot);
	lR = pNode->GetGeometricRotation(FbxNode::eSourcePivot);
	lS = pNode->GetGeometricScaling(FbxNode::eSourcePivot);

	lGeometry.SetT(lT);
	lGeometry.SetR(lR);
	lGeometry.SetS(lS);

	return lGeometry;
}

// Scale all the elements of a matrix.
void MatrixScale(FbxAMatrix& pMatrix, double pValue)
{
	int32 i,j;

	for (i = 0; i < 4; i++)
	{
		for (j = 0; j < 4; j++)
		{
			pMatrix[i][j] *= pValue;
		}
	}
}

// Add a value to all the elements in the diagonal of the matrix.
void MatrixAddToDiagonal(FbxAMatrix& pMatrix, double pValue)
{
	pMatrix[0][0] += pValue;
	pMatrix[1][1] += pValue;
	pMatrix[2][2] += pValue;
	pMatrix[3][3] += pValue;
}


// Sum two matrices element by element.
void MatrixAdd(FbxAMatrix& pDstMatrix, FbxAMatrix& pSrcMatrix)
{
	int32 i,j;

	for (i = 0; i < 4; i++)
	{
		for (j = 0; j < 4; j++)
		{
			pDstMatrix[i][j] += pSrcMatrix[i][j];
		}
	}
}

void FFbxImporter::SkinControlPointsToPose(FSkeletalMeshImportData& ImportData, FbxMesh* FbxMesh, FbxShape* FbxShape, bool bUseT0 )
{
	FbxTime poseTime = FBXSDK_TIME_INFINITE;
	if(bUseT0)
	{
		poseTime = 0;
	}

	int32 VertexCount = FbxMesh->GetControlPointsCount();

	// Create a copy of the vertex array to receive vertex deformations.
	FbxVector4* VertexArray = new FbxVector4[VertexCount];

	// If a shape is provided, then it is the morphed version of the mesh.
	// So we want to deform that instead of the base mesh vertices
	if (FbxShape)
	{
		check(FbxShape->GetControlPointsCount() == VertexCount);
		FMemory::Memcpy(VertexArray, FbxShape->GetControlPoints(), VertexCount * sizeof(FbxVector4));
	}
	else
	{
		FMemory::Memcpy(VertexArray, FbxMesh->GetControlPoints(), VertexCount * sizeof(FbxVector4));
	}																	 


	int32 ClusterCount = 0;
	int32 SkinCount = FbxMesh->GetDeformerCount(FbxDeformer::eSkin);
	for( int32 i=0; i< SkinCount; i++)
	{
		ClusterCount += ((FbxSkin *)(FbxMesh->GetDeformer(i, FbxDeformer::eSkin)))->GetClusterCount();
	}
	
	// Deform the vertex array with the links contained in the mesh.
	if (ClusterCount)
	{
		FbxAMatrix MeshMatrix = ComputeTotalMatrix(FbxMesh->GetNode());
		// All the links must have the same link mode.
		FbxCluster::ELinkMode lClusterMode = ((FbxSkin*)FbxMesh->GetDeformer(0, FbxDeformer::eSkin))->GetCluster(0)->GetLinkMode();

		int32 i, j;
		int32 lClusterCount=0;

		int32 lSkinCount = FbxMesh->GetDeformerCount(FbxDeformer::eSkin);

		TArray<FbxAMatrix> ClusterDeformations;
		ClusterDeformations.AddZeroed( VertexCount );

		TArray<double> ClusterWeights;
		ClusterWeights.AddZeroed( VertexCount );
	

		if (lClusterMode == FbxCluster::eAdditive)
		{
			for (i = 0; i < VertexCount; i++)
			{
				ClusterDeformations[i].SetIdentity();
			}
		}

		for ( i=0; i<lSkinCount; ++i)
		{
			lClusterCount =( (FbxSkin *)FbxMesh->GetDeformer(i, FbxDeformer::eSkin))->GetClusterCount();
			for (j=0; j<lClusterCount; ++j)
			{
				FbxCluster* Cluster =((FbxSkin *) FbxMesh->GetDeformer(i, FbxDeformer::eSkin))->GetCluster(j);
				if (!Cluster->GetLink())
					continue;
					
				FbxNode* Link = Cluster->GetLink();
				
				FbxAMatrix lReferenceGlobalInitPosition;
				FbxAMatrix lReferenceGlobalCurrentPosition;
				FbxAMatrix lClusterGlobalInitPosition;
				FbxAMatrix lClusterGlobalCurrentPosition;
				FbxAMatrix lReferenceGeometry;
				FbxAMatrix lClusterGeometry;

				FbxAMatrix lClusterRelativeInitPosition;
				FbxAMatrix lClusterRelativeCurrentPositionInverse;
				FbxAMatrix lVertexTransformMatrix;

				if (lClusterMode == FbxCluster::eAdditive && Cluster->GetAssociateModel())
				{
					Cluster->GetTransformAssociateModelMatrix(lReferenceGlobalInitPosition);
					lReferenceGlobalCurrentPosition = Scene->GetAnimationEvaluator()->GetNodeGlobalTransform(Cluster->GetAssociateModel(), poseTime);
					// Geometric transform of the model
					lReferenceGeometry = GetGeometry(Cluster->GetAssociateModel());
					lReferenceGlobalCurrentPosition *= lReferenceGeometry;
				}
				else
				{
					Cluster->GetTransformMatrix(lReferenceGlobalInitPosition);
					lReferenceGlobalCurrentPosition = MeshMatrix; //pGlobalPosition;
					// Multiply lReferenceGlobalInitPosition by Geometric Transformation
					lReferenceGeometry = GetGeometry(FbxMesh->GetNode());
					lReferenceGlobalInitPosition *= lReferenceGeometry;
				}
				// Get the link initial global position and the link current global position.
				Cluster->GetTransformLinkMatrix(lClusterGlobalInitPosition);
				lClusterGlobalCurrentPosition = Link->GetScene()->GetAnimationEvaluator()->GetNodeGlobalTransform(Link, poseTime);

				// Compute the initial position of the link relative to the reference.
				lClusterRelativeInitPosition = lClusterGlobalInitPosition.Inverse() * lReferenceGlobalInitPosition;

				// Compute the current position of the link relative to the reference.
				lClusterRelativeCurrentPositionInverse = lReferenceGlobalCurrentPosition.Inverse() * lClusterGlobalCurrentPosition;

				// Compute the shift of the link relative to the reference.
				lVertexTransformMatrix = lClusterRelativeCurrentPositionInverse * lClusterRelativeInitPosition;

				int32 k;
				int32 lVertexIndexCount = Cluster->GetControlPointIndicesCount();

				for (k = 0; k < lVertexIndexCount; ++k) 
				{
					int32 lIndex = Cluster->GetControlPointIndices()[k];

					// Sometimes, the mesh can have less points than at the time of the skinning
					// because a smooth operator was active when skinning but has been deactivated during export.
					if (lIndex >= VertexCount)
					{
						continue;
					}

					double lWeight = Cluster->GetControlPointWeights()[k];

					if (lWeight == 0.0)
					{
						continue;
					}

					// Compute the influence of the link on the vertex.
					FbxAMatrix lInfluence = lVertexTransformMatrix;
					MatrixScale(lInfluence, lWeight);

					if (lClusterMode == FbxCluster::eAdditive)
					{
						// Multiply with to the product of the deformations on the vertex.
						MatrixAddToDiagonal(lInfluence, 1.0 - lWeight);
						ClusterDeformations[lIndex] = lInfluence * ClusterDeformations[lIndex];

						// Set the link to 1.0 just to know this vertex is influenced by a link.
						ClusterWeights[lIndex] = 1.0;
					}
					else // lLinkMode == KFbxLink::eNORMALIZE || lLinkMode == KFbxLink::eTOTAL1
					{
						// Add to the sum of the deformations on the vertex.
						MatrixAdd(ClusterDeformations[lIndex], lInfluence);

						// Add to the sum of weights to either normalize or complete the vertex.
						ClusterWeights[lIndex] += lWeight;
					}

				}
			}
		}
		
		for (i = 0; i < VertexCount; i++) 
		{
			FbxVector4 lSrcVertex = VertexArray[i];
			FbxVector4& lDstVertex = VertexArray[i];
			double lWeight = ClusterWeights[i];

			// Deform the vertex if there was at least a link with an influence on the vertex,
			if (lWeight != 0.0) 
			{
				lDstVertex = ClusterDeformations[i].MultT(lSrcVertex);

				if (lClusterMode == FbxCluster::eNormalize)
				{
					// In the normalized link mode, a vertex is always totally influenced by the links. 
					lDstVertex /= lWeight;
				}
				else if (lClusterMode == FbxCluster::eTotalOne)
				{
					// In the total 1 link mode, a vertex can be partially influenced by the links. 
					lSrcVertex *= (1.0 - lWeight);
					lDstVertex += lSrcVertex;
				}
			} 
		}
		
		// change the vertex position
		int32 ExistPointNum = ImportData.Points.Num();
		int32 StartPointIndex = ExistPointNum - VertexCount;
		for(int32 ControlPointsIndex = 0 ; ControlPointsIndex < VertexCount ;ControlPointsIndex++ )
		{
			ImportData.Points[ControlPointsIndex+StartPointIndex] = Converter.ConvertPos(MeshMatrix.MultT(VertexArray[ControlPointsIndex]));
		}
		
	}
	
	delete[] VertexArray;
}

// 3 "ProcessImportMesh..." functions outputing Unreal data from a filled FSkeletalMeshBinaryImport
// and a handfull of other minor stuff needed by these 
// Fully taken from SkeletalMeshImport.cpp

extern void ProcessImportMeshInfluences(FSkeletalMeshImportData& ImportData);
extern void ProcessImportMeshMaterials( TArray<FSkeletalMaterial>& Materials, FSkeletalMeshImportData& ImportData );
extern bool ProcessImportMeshSkeleton(FReferenceSkeleton& RefSkeleton, int32& SkeletalDepth, FSkeletalMeshImportData& ImportData);

struct tFaceRecord
{
	int32 FaceIndex;
	int32 HoekIndex;
	int32 WedgeIndex;
	uint32 SmoothFlags;
	uint32 FanFlags;
};

struct VertsFans
{	
	TArray<tFaceRecord> FaceRecord;
	int32 FanGroupCount;
};

struct tInfluences
{
	TArray<int32> RawInfIndices;
};

struct tWedgeList
{
	TArray<int32> WedgeList;
};

struct tFaceSet
{
	TArray<int32> Faces;
};

// Check whether faces have at least two vertices in common. These must be POINTS - don't care about wedges.
bool UnFbx::FFbxImporter::FacesAreSmoothlyConnected( FSkeletalMeshImportData &ImportData, int32 Face1, int32 Face2 )
{

	//if( ( Face1 >= Thing->SkinData.Faces.Num()) || ( Face2 >= Thing->SkinData.Faces.Num()) ) return false;

	if( Face1 == Face2 )
	{
		return true;
	}

	// Smoothing groups match at least one bit in binary AND ?
	if( ( ImportData.Faces[Face1].SmoothingGroups & ImportData.Faces[Face2].SmoothingGroups ) == 0  ) 
	{
		return false;
	}

	int32 VertMatches = 0;
	for( int32 i=0; i<3; i++)
	{
		int32 Point1 = ImportData.Wedges[ ImportData.Faces[Face1].WedgeIndex[i] ].VertexIndex;

		for( int32 j=0; j<3; j++)
		{
			int32 Point2 = ImportData.Wedges[ ImportData.Faces[Face2].WedgeIndex[j] ].VertexIndex;
			if( Point2 == Point1 )
			{
				VertMatches ++;
			}
		}
	}

	return ( VertMatches >= 2 );
}

int32	UnFbx::FFbxImporter::DoUnSmoothVerts(FSkeletalMeshImportData &ImportData)
{
	//
	// Connectivity: triangles with non-matching smoothing groups will be physically split.
	//
	// -> Splitting involves: the UV+material-contaning vertex AND the 3d point.
	//
	// -> Tally smoothing groups for each and every (textured) vertex.
	//
	// -> Collapse: 
	// -> start from a vertex and all its adjacent triangles - go over
	// each triangle - if any connecting one (sharing more than one vertex) gives a smoothing match,
	// accumulate it. Then IF more than one resulting section, 
	// ensure each boundary 'vert' is split _if not already_ to give each smoothing group
	// independence from all others.
	//

	int32 DuplicatedVertCount = 0;
	int32 RemappedHoeks = 0;

	int32 TotalSmoothMatches = 0;
	int32 TotalConnexChex = 0;

	// Link _all_ faces to vertices.	
	TArray<VertsFans>  Fans;
	TArray<tInfluences> PointInfluences;	
	TArray<tWedgeList>  PointWedges;

	Fans.AddZeroed( ImportData.Points.Num() );//Fans.AddExactZeroed(			Thing->SkinData.Points.Num() );
	PointInfluences.AddZeroed( ImportData.Points.Num() );//PointInfluences.AddExactZeroed( Thing->SkinData.Points.Num() );
	PointWedges.AddZeroed( ImportData.Points.Num() );//PointWedges.AddExactZeroed(	 Thing->SkinData.Points.Num() );

	// Existing points map 1:1
	ImportData.PointToRawMap.AddUninitialized( ImportData.Points.Num() );
	for(int32 i=0; i<ImportData.Points.Num(); i++)
	{
		ImportData.PointToRawMap[i] = i;
	}

	for(int32 i=0; i< ImportData.Influences.Num(); i++)
	{
		PointInfluences[ImportData.Influences[i].VertexIndex ].RawInfIndices.Add( i );
	}

	for(int32 i=0; i< ImportData.Wedges.Num(); i++)
	{
		PointWedges[ImportData.Wedges[i].VertexIndex ].WedgeList.Add( i );
	}

	for(int32 f=0; f< ImportData.Faces.Num(); f++ )
	{
		// For each face, add a pointer to that face into the Fans[vertex].
		for( int32 i=0; i<3; i++)
		{
			int32 WedgeIndex = ImportData.Faces[f].WedgeIndex[i];			
			int32 PointIndex = ImportData.Wedges[ WedgeIndex ].VertexIndex;
			tFaceRecord NewFR;

			NewFR.FaceIndex = f;
			NewFR.HoekIndex = i;			
			NewFR.WedgeIndex = WedgeIndex; // This face touches the point courtesy of Wedges[Wedgeindex].
			NewFR.SmoothFlags = ImportData.Faces[f].SmoothingGroups;
			NewFR.FanFlags = 0;
			Fans[ PointIndex ].FaceRecord.Add( NewFR );
			Fans[ PointIndex ].FanGroupCount = 0;
		}		
	}

	// Investigate connectivity and assign common group numbers (1..+) to the fans' individual FanFlags.
	for( int32 p=0; p< Fans.Num(); p++) // The fan of faces for each 3d point 'p'.
	{
		// All faces connecting.
		if( Fans[p].FaceRecord.Num() > 0 ) 
		{		
			int32 FacesProcessed = 0;
			TArray<tFaceSet> FaceSets; // Sets with indices INTO FANS, not into face array.			

			// Digest all faces connected to this vertex (p) into one or more smooth sets. only need to check 
			// all faces MINUS one..
			while( FacesProcessed < Fans[p].FaceRecord.Num()  )
			{
				// One loop per group. For the current ThisFaceIndex, tally all truly connected ones
				// and put them in a new TArray. Once no more can be connected, stop.

				int32 NewSetIndex = FaceSets.Num(); // 0 to start
				FaceSets.AddZeroed(1);						// first one will be just ThisFaceIndex.

				// Find the first non-processed face. There will be at least one.
				int32 ThisFaceFanIndex = 0;
				{
					int32 SearchIndex = 0;
					while( Fans[p].FaceRecord[SearchIndex].FanFlags == -1 ) // -1 indicates already  processed. 
					{
						SearchIndex++;
					}
					ThisFaceFanIndex = SearchIndex; //Fans[p].FaceRecord[SearchIndex].FaceIndex; 
				}

				// Initial face.
				FaceSets[ NewSetIndex ].Faces.Add( ThisFaceFanIndex );   // Add the unprocessed Face index to the "local smoothing group" [NewSetIndex].
				Fans[p].FaceRecord[ThisFaceFanIndex].FanFlags = -1;			  // Mark as processed.
				FacesProcessed++; 

				// Find all faces connected to this face, and if there's any
				// smoothing group matches, put it in current face set and mark it as processed;
				// until no more match. 
				int32 NewMatches = 0;
				do
				{
					NewMatches = 0;
					// Go over all current faces in this faceset and set if the FaceRecord (local smoothing groups) has any matches.
					// there will be at least one face already in this faceset - the first face in the fan.
					for( int32 n=0; n< FaceSets[NewSetIndex].Faces.Num(); n++)
					{				
						int32 HookFaceIdx = Fans[p].FaceRecord[ FaceSets[NewSetIndex].Faces[n] ].FaceIndex;

						//Go over the fan looking for matches.
						for( int32 s=0; s< Fans[p].FaceRecord.Num(); s++)
						{
							// Skip if same face, skip if face already processed.
							if( ( HookFaceIdx != Fans[p].FaceRecord[s].FaceIndex )  && ( Fans[p].FaceRecord[s].FanFlags != -1  ))
							{
								TotalConnexChex++;
								// Process if connected with more than one vertex, AND smooth..
								if( FacesAreSmoothlyConnected( ImportData, HookFaceIdx, Fans[p].FaceRecord[s].FaceIndex ) )
								{									
									TotalSmoothMatches++;
									Fans[p].FaceRecord[s].FanFlags = -1; // Mark as processed.
									FacesProcessed++;
									// Add 
									FaceSets[NewSetIndex].Faces.Add( s ); // Store FAN index of this face index into smoothing group's faces. 
									// Tally
									NewMatches++;
								}
							} // not the same...
						}// all faces in fan
					} // all faces in FaceSet
				}while( NewMatches );	

			}// Repeat until all faces processed.

			// For the new non-initialized  face sets, 
			// Create a new point, influences, and uv-vertex(-ices) for all individual FanFlag groups with an index of 2+ and also remap
			// the face's vertex into those new ones.
			if( FaceSets.Num() > 1 )
			{
				for( int32 f=1; f<FaceSets.Num(); f++ )
				{				
					check(ImportData.Points.Num() == ImportData.PointToRawMap.Num());

					// We duplicate the current vertex. (3d point)
					int32 NewPointIndex = ImportData.Points.Num();
					ImportData.Points.AddUninitialized();
					ImportData.Points[NewPointIndex] = ImportData.Points[p] ;

					ImportData.PointToRawMap.AddUninitialized();
					ImportData.PointToRawMap[NewPointIndex] = p;
					
					DuplicatedVertCount++;

					// Duplicate all related weights.
					for( int32 t=0; t< PointInfluences[p].RawInfIndices.Num(); t++ )
					{
						// Add new weight
						int32 NewWeightIndex = ImportData.Influences.Num();
						ImportData.Influences.AddUninitialized();
						ImportData.Influences[NewWeightIndex] = ImportData.Influences[ PointInfluences[p].RawInfIndices[t] ];
						ImportData.Influences[NewWeightIndex].VertexIndex = NewPointIndex;
					}

					// Duplicate any and all Wedges associated with it; and all Faces' wedges involved.					
					for( int32 w=0; w< PointWedges[p].WedgeList.Num(); w++)
					{
						int32 OldWedgeIndex = PointWedges[p].WedgeList[w];
						int32 NewWedgeIndex = ImportData.Wedges.Num();
						ImportData.Wedges.AddUninitialized();
						ImportData.Wedges[NewWedgeIndex] = ImportData.Wedges[ OldWedgeIndex ];
						ImportData.Wedges[ NewWedgeIndex ].VertexIndex = NewPointIndex; 

						//  Update relevant face's Wedges. Inelegant: just check all associated faces for every new wedge.
						for( int32 s=0; s< FaceSets[f].Faces.Num(); s++)
						{
							int32 FanIndex = FaceSets[f].Faces[s];
							if( Fans[p].FaceRecord[ FanIndex ].WedgeIndex == OldWedgeIndex )
							{
								// Update just the right one for this face (HoekIndex!) 
								ImportData.Faces[ Fans[p].FaceRecord[ FanIndex].FaceIndex ].WedgeIndex[ Fans[p].FaceRecord[ FanIndex ].HoekIndex ] = NewWedgeIndex;
								RemappedHoeks++;
							}
						}
					}
				}
			} //  if FaceSets.Num(). -> duplicate stuff
		}//	while( FacesProcessed < Fans[p].FaceRecord.Num() )
	} // Fans for each 3d point

	check(ImportData.Points.Num() == ImportData.PointToRawMap.Num());

	return DuplicatedVertCount; 
}

bool IsUnrealBone(FbxNode* Link)
{
	FbxNodeAttribute* Attr = Link->GetNodeAttribute();
	if (Attr)
	{
		FbxNodeAttribute::EType AttrType = Attr->GetAttributeType();
		if ( AttrType == FbxNodeAttribute::eSkeleton ||
			AttrType == FbxNodeAttribute::eMesh ||
			AttrType == FbxNodeAttribute::eNull )
		{
			return true;
		}
	}
	
	return false;
}


void UnFbx::FFbxImporter::RecursiveBuildSkeleton(FbxNode* Link, TArray<FbxNode*>& OutSortedLinks)
{
	if (IsUnrealBone(Link))
	{
		OutSortedLinks.Add(Link);
		int32 ChildIndex;
		for (ChildIndex=0; ChildIndex<Link->GetChildCount(); ChildIndex++)
		{
			RecursiveBuildSkeleton(Link->GetChild(ChildIndex),OutSortedLinks);
		}
	}
}

void UnFbx::FFbxImporter::BuildSkeletonSystem(TArray<FbxCluster*>& ClusterArray, TArray<FbxNode*>& OutSortedLinks)
{
	FbxNode* Link;
	TArray<FbxNode*> RootLinks;
	int32 ClusterIndex;
	for (ClusterIndex = 0; ClusterIndex < ClusterArray.Num(); ClusterIndex++)
	{
		Link = ClusterArray[ClusterIndex]->GetLink();
		Link = GetRootSkeleton(Link);
		int32 LinkIndex;
		for (LinkIndex=0; LinkIndex<RootLinks.Num(); LinkIndex++)
		{
			if (Link == RootLinks[LinkIndex])
			{
				break;
			}
		}
		
		// this link is a new root, add it
		if (LinkIndex == RootLinks.Num())
		{
			RootLinks.Add(Link);
		}
	}

	for (int32 LinkIndex=0; LinkIndex<RootLinks.Num(); LinkIndex++)
	{
		RecursiveBuildSkeleton(RootLinks[LinkIndex], OutSortedLinks);
	}
}

bool UnFbx::FFbxImporter::RetrievePoseFromBindPose(const TArray<FbxNode*>& NodeArray, FbxArray<FbxPose*> & PoseArray) const
{
	const int32 PoseCount = Scene->GetPoseCount();
	for(int32 PoseIndex = 0; PoseIndex < PoseCount; PoseIndex++)
	{
		FbxPose* CurrentPose = Scene->GetPose(PoseIndex);

		// current pose is bind pose, 
		if(CurrentPose && CurrentPose->IsBindPose())
		{
			// IsValidBindPose doesn't work reliably
			// It checks all the parent chain(regardless root given), and if the parent doesn't have correct bind pose, it fails
			// It causes more false positive issues than the real issue we have to worry about
			// If you'd like to try this, set CHECK_VALID_BIND_POSE to 1, and try the error message
			// when Autodesk fixes this bug, then we might be able to re-open this
			FString PoseName = CurrentPose->GetName();
			// all error report status
			FbxStatus Status;

			// it does not make any difference of checking with different node
			// it is possible pose 0 -> node array 2, but isValidBindPose function returns true even with node array 0
			for(auto Current : NodeArray)
			{
				FString CurrentName = Current->GetName();
				NodeList pMissingAncestors, pMissingDeformers, pMissingDeformersAncestors, pWrongMatrices;

				if(CurrentPose->IsValidBindPoseVerbose(Current, pMissingAncestors, pMissingDeformers, pMissingDeformersAncestors, pWrongMatrices, 0.0001, &Status))
				{
					PoseArray.Add(CurrentPose);
					UE_LOG(LogFbx, Log, TEXT("Valid bind pose for Pose (%s) - %s"), *PoseName, *FString(Current->GetName()));
					break;
				}
				else
				{
					// first try to fix up
					// add missing ancestors
					for(int i = 0; i < pMissingAncestors.GetCount(); i++)
					{
						FbxAMatrix mat = pMissingAncestors.GetAt(i)->EvaluateGlobalTransform(FBXSDK_TIME_ZERO);
						CurrentPose->Add(pMissingAncestors.GetAt(i), mat);
					}

					pMissingAncestors.Clear();
					pMissingDeformers.Clear();
					pMissingDeformersAncestors.Clear();
					pWrongMatrices.Clear();

					// check it again
					if(CurrentPose->IsValidBindPose(Current))
					{
						PoseArray.Add(CurrentPose);
						UE_LOG(LogFbx, Log, TEXT("Valid bind pose for Pose (%s) - %s"), *PoseName, *FString(Current->GetName()));
						break;
					}
					else
					{
						// first try to find parent who is null group and see if you can try test it again
						FbxNode * ParentNode = Current->GetParent();
						while(ParentNode)
						{
							FbxNodeAttribute* Attr = ParentNode->GetNodeAttribute();
							if(Attr && Attr->GetAttributeType() == FbxNodeAttribute::eNull)
							{
								// found it 
								break;
							}

							// find next parent
							ParentNode = ParentNode->GetParent();
						}

						if(ParentNode && CurrentPose->IsValidBindPose(ParentNode))
						{
							PoseArray.Add(CurrentPose);
							UE_LOG(LogFbx, Log, TEXT("Valid bind pose for Pose (%s) - %s"), *PoseName, *FString(Current->GetName()));
							break;
						}
						else
						{
							FString ErrorString = Status.GetErrorString();
							UE_LOG(LogFbx, Warning, TEXT("Not valid bind pose for Pose (%s) - Node %s : %s"), *PoseName, *FString(Current->GetName()), *ErrorString);
						}
					}
				}
			}
		}
	}

	return (PoseArray.Size() > 0);
}

bool UnFbx::FFbxImporter::ImportBone(TArray<FbxNode*>& NodeArray, FSkeletalMeshImportData &ImportData, UFbxSkeletalMeshImportData* TemplateData ,TArray<FbxNode*> &SortedLinks, bool& bOutDiffPose, bool bDisableMissingBindPoseWarning, bool & bUseTime0AsRefPose)
{
	bOutDiffPose = false;
	int32 SkelType = 0; // 0 for skeletal mesh, 1 for rigid mesh
	FbxNode* Link = NULL;
	FbxArray<FbxPose*> PoseArray;
	TArray<FbxCluster*> ClusterArray;
	
	if (NodeArray[0]->GetMesh()->GetDeformerCount(FbxDeformer::eSkin) == 0)
	{
		SkelType = 1;
		Link = NodeArray[0];
		RecursiveBuildSkeleton(GetRootSkeleton(Link),SortedLinks);
	}
	else
	{
		// get bindpose and clusters from FBX skeleton
		
		// let's put the elements to their bind pose! (and we restore them after
		// we have built the ClusterInformation.
		int32 Default_NbPoses = SdkManager->GetBindPoseCount(Scene);
		// If there are no BindPoses, the following will generate them.
		//SdkManager->CreateMissingBindPoses(Scene);

		//if we created missing bind poses, update the number of bind poses
		int32 NbPoses = SdkManager->GetBindPoseCount(Scene);

		if ( NbPoses != Default_NbPoses)
		{
			AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, LOCTEXT("FbxSkeletaLMeshimport_SceneMissingBinding", "The imported scene has no initial binding position (Bind Pose) for the skin. The plug-in will compute one automatically. However, it may create unexpected results.")), FFbxErrors::SkeletalMesh_NoBindPoseInScene);
		}

		//
		// create the bones / skinning
		//

		for ( int32 i = 0; i < NodeArray.Num(); i++)
		{
			FbxMesh* FbxMesh = NodeArray[i]->GetMesh();
			const int32 SkinDeformerCount = FbxMesh->GetDeformerCount(FbxDeformer::eSkin);
			for (int32 DeformerIndex=0; DeformerIndex < SkinDeformerCount; DeformerIndex++)
			{
				FbxSkin* Skin = (FbxSkin*)FbxMesh->GetDeformer(DeformerIndex, FbxDeformer::eSkin);
				for ( int32 ClusterIndex = 0; ClusterIndex < Skin->GetClusterCount(); ClusterIndex++)
				{
					ClusterArray.Add(Skin->GetCluster(ClusterIndex));
				}
			}
		}

		if (ClusterArray.Num() == 0)
		{
			AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, LOCTEXT("FbxSkeletaLMeshimport_NoAssociatedCluster", "No associated clusters")), FFbxErrors::SkeletalMesh_NoAssociatedCluster);
			return false;
		}

		// get bind pose
		if(RetrievePoseFromBindPose(NodeArray, PoseArray) == false)
		{
			UE_LOG(LogFbx, Warning, TEXT("Getting valid bind pose failed. Try to recreate bind pose"));
			// if failed, delete bind pose, and retry.
			const int32 PoseCount = Scene->GetPoseCount();
			for(int32 PoseIndex = PoseCount-1; PoseIndex >= 0; --PoseIndex)
			{
				FbxPose* CurrentPose = Scene->GetPose(PoseIndex);

				// current pose is bind pose, 
				if(CurrentPose && CurrentPose->IsBindPose())
				{
					Scene->RemovePose(PoseIndex);
					CurrentPose->Destroy();
				}
			}

			SdkManager->CreateMissingBindPoses(Scene);
			if ( RetrievePoseFromBindPose(NodeArray, PoseArray) == false)
			{
				UE_LOG(LogFbx, Warning, TEXT("Recreating bind pose failed."));
			}
			else
			{
				UE_LOG(LogFbx, Warning, TEXT("Recreating bind pose succeeded."));
			}
		}

		// recurse through skeleton and build ordered table
		BuildSkeletonSystem(ClusterArray, SortedLinks);
	}

	// error check
	// if no bond is found
	if(SortedLinks.Num() == 0)
	{
		AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("FbxSkeletaLMeshimport_NoBone", "'{0}' has no bones"), FText::FromString(NodeArray[0]->GetName()))), FFbxErrors::SkeletalMesh_NoBoneFound);
		return false;
	}

	// if no bind pose is found
	if (!bUseTime0AsRefPose && PoseArray.GetCount() == 0)
	{
		// add to tokenized error message
		AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, LOCTEXT("FbxSkeletaLMeshimport_InvalidBindPose", "Could not find the bind pose.  It will use time 0 as bind pose.")), FFbxErrors::SkeletalMesh_InvalidBindPose);
		bUseTime0AsRefPose = true;
	}

	int32 LinkIndex;

	// Check for duplicate bone names and issue a warning if found
	for(LinkIndex = 0; LinkIndex < SortedLinks.Num(); ++LinkIndex)
	{
		Link = SortedLinks[LinkIndex];

		for(int32 AltLinkIndex = LinkIndex+1; AltLinkIndex < SortedLinks.Num(); ++AltLinkIndex)
		{
			FbxNode* AltLink = SortedLinks[AltLinkIndex];

			if(FCStringAnsi::Strcmp(Link->GetName(), AltLink->GetName()) == 0)
			{
				FString RawBoneName = UTF8_TO_TCHAR(Link->GetName());
				AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, FText::Format(LOCTEXT("Error_DuplicateBoneName", "Error, Could not import {0}.\nDuplicate bone name found ('{1}'). Each bone must have a unique name."),
					FText::FromString(NodeArray[0]->GetName()), FText::FromString(RawBoneName))), FFbxErrors::SkeletalMesh_DuplicateBones);
				return false;
			}
		}
	}

	FbxArray<FbxAMatrix> GlobalsPerLink;
	GlobalsPerLink.Grow(SortedLinks.Num());
	GlobalsPerLink[0].SetIdentity();

	bool GlobalLinkFoundFlag;
	FbxVector4 LocalLinkT;
	FbxQuaternion LocalLinkQ;
	FbxVector4	LocalLinkS;

	bool bAnyLinksNotInBindPose = false;
	FString LinksWithoutBindPoses;
	int32 NumberOfRoot = 0;

	int32 RootIdx = -1;

	for (LinkIndex=0; LinkIndex<SortedLinks.Num(); LinkIndex++)
	{
		// Add a bone for each FBX Link
		ImportData.RefBonesBinary.Add( VBone() );

		Link = SortedLinks[LinkIndex];

		// get the link parent and children
		int32 ParentIndex = INDEX_NONE; // base value for root if no parent found
		FbxNode* LinkParent = Link->GetParent();
		if (LinkIndex)
		{
			for (int32 ll=0; ll<LinkIndex; ++ll) // <LinkIndex because parent is guaranteed to be before child in sortedLink
			{
				FbxNode* Otherlink	= SortedLinks[ll];
				if (Otherlink == LinkParent)
				{
					ParentIndex = ll;
					break;
				}
			}
		}
		
		// see how many root this has
		// if more than 
		if (ParentIndex == INDEX_NONE)
		{
			++NumberOfRoot;
			RootIdx = LinkIndex;
			if (NumberOfRoot > 1)
			{
				AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, LOCTEXT("MultipleRootsFound", "Multiple roots are found in the bone hierarchy. We only support single root bone.")), FFbxErrors::SkeletalMesh_MultipleRoots);
				return false;
			}
		}

		GlobalLinkFoundFlag = false;
		if (!SkelType) //skeletal mesh
		{
			// there are some links, they have no cluster, but in bindpose
			if (PoseArray.GetCount())
			{
				for (int32 PoseIndex = 0; PoseIndex < PoseArray.GetCount(); PoseIndex++)
				{
					int32 PoseLinkIndex = PoseArray[PoseIndex]->Find(Link);
					if (PoseLinkIndex>=0)
					{
						FbxMatrix NoneAffineMatrix = PoseArray[PoseIndex]->GetMatrix(PoseLinkIndex);
						FbxAMatrix Matrix = *(FbxAMatrix*)(double*)&NoneAffineMatrix;
						GlobalsPerLink[LinkIndex] = Matrix;
						GlobalLinkFoundFlag = true;
						break;
					}
				}
			}

			if (!GlobalLinkFoundFlag)
			{
				// since now we set use time 0 as ref pose this won't unlikely happen
				// but leaving it just in case it still has case where it's missing partial bind pose
				if(!bUseTime0AsRefPose && !bDisableMissingBindPoseWarning)
				{
					bAnyLinksNotInBindPose = true;
					LinksWithoutBindPoses +=  UTF8_TO_TCHAR(Link->GetName());
					LinksWithoutBindPoses +=  TEXT("  \n");
				}

				for (int32 ClusterIndex=0; ClusterIndex<ClusterArray.Num(); ClusterIndex++)
				{
					FbxCluster* Cluster = ClusterArray[ClusterIndex];
					if (Link == Cluster->GetLink())
					{
						Cluster->GetTransformLinkMatrix(GlobalsPerLink[LinkIndex]);
						GlobalLinkFoundFlag = true;
						break;
					}
				}
			}
		}

		if (!GlobalLinkFoundFlag)
		{
			GlobalsPerLink[LinkIndex] = Link->EvaluateGlobalTransform();
		}
		
		if (bUseTime0AsRefPose)
		{
			FbxAMatrix& T0Matrix = Scene->GetAnimationEvaluator()->GetNodeGlobalTransform(Link, 0);
			if (GlobalsPerLink[LinkIndex] != T0Matrix)
			{
				bOutDiffPose = true;
			}
			
			GlobalsPerLink[LinkIndex] = T0Matrix;
		}
	
		if (LinkIndex)
		{
			FbxAMatrix	Matrix;
			Matrix = GlobalsPerLink[ParentIndex].Inverse() * GlobalsPerLink[LinkIndex];
			LocalLinkT = Matrix.GetT();
			LocalLinkQ = Matrix.GetQ();
			LocalLinkS = Matrix.GetS();
		}
		else	// skeleton root
		{
			// for root, this is global coordinate
			LocalLinkT = GlobalsPerLink[LinkIndex].GetT();
			LocalLinkQ = GlobalsPerLink[LinkIndex].GetQ();
			LocalLinkS = GlobalsPerLink[LinkIndex].GetS();
		}
		
		// set bone
		VBone& Bone = ImportData.RefBonesBinary[LinkIndex];
		FString BoneName;

		const char* LinkName = Link->GetName();
		BoneName = UTF8_TO_TCHAR( MakeName( LinkName ) );
		Bone.Name = BoneName;

		VJointPos& JointMatrix = Bone.BonePos;
		FbxSkeleton* Skeleton = Link->GetSkeleton();
		if (Skeleton)
		{
			JointMatrix.Length = Converter.ConvertDist(Skeleton->LimbLength.Get());
			JointMatrix.XSize = Converter.ConvertDist(Skeleton->Size.Get());
			JointMatrix.YSize = Converter.ConvertDist(Skeleton->Size.Get());
			JointMatrix.ZSize = Converter.ConvertDist(Skeleton->Size.Get());
		}
		else
		{
			JointMatrix.Length = 1. ;
			JointMatrix.XSize = 100. ;
			JointMatrix.YSize = 100. ;
			JointMatrix.ZSize = 100. ;
		}

		// get the link parent and children
		Bone.ParentIndex = ParentIndex;
		Bone.NumChildren = 0;
		for (int32 ChildIndex=0; ChildIndex<Link->GetChildCount(); ChildIndex++)
		{
			FbxNode* Child = Link->GetChild(ChildIndex);
			if (IsUnrealBone(Child))
			{
				Bone.NumChildren++;
			}
		}

		JointMatrix.Transform.SetTranslation(Converter.ConvertPos(LocalLinkT));
		JointMatrix.Transform.SetRotation(Converter.ConvertRotToQuat(LocalLinkQ));
		JointMatrix.Transform.SetScale3D(Converter.ConvertScale(LocalLinkS));
	}
	
	if(TemplateData)
	{
		FbxAMatrix FbxAddedMatrix;
		BuildFbxMatrixForImportTransform(FbxAddedMatrix, TemplateData);
		FMatrix AddedMatrix = Converter.ConvertMatrix(FbxAddedMatrix);

		VBone& RootBone = ImportData.RefBonesBinary[RootIdx];
		FTransform& RootTransform = RootBone.BonePos.Transform;
		RootTransform.SetFromMatrix(RootTransform.ToMatrixWithScale() * AddedMatrix);
	}
	
	if(bAnyLinksNotInBindPose)
	{
		AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("FbxSkeletaLMeshimport_BonesAreMissingFromBindPose", "The following bones are missing from the bind pose:\n{0}\nThis can happen for bones that are not vert weighted. If they are not in the correct orientation after importing,\nplease set the \"Use T0 as ref pose\" option or add them to the bind pose and reimport the skeletal mesh."), FText::FromString(LinksWithoutBindPoses))), FFbxErrors::SkeletalMesh_BonesAreMissingFromBindPose);
	}
	
	return true;
}

USkeletalMesh* UnFbx::FFbxImporter::ImportSkeletalMesh(UObject* InParent, TArray<FbxNode*>& NodeArray, const FName& Name, EObjectFlags Flags, UFbxSkeletalMeshImportData* TemplateImportData, FString Filename, TArray<FbxShape*> *FbxShapeArray, FSkeletalMeshImportData* OutData, bool bCreateRenderData )
{
	if (NodeArray.Num() == 0)
	{
		return NULL;
	}

	int32 SkelType = 0; // 0 for skeletal mesh, 1 for rigid mesh
	
	FbxNode* Node = NodeArray[0];
	// find the mesh by its name
	FbxMesh* FbxMesh = Node->GetMesh();

	if( !FbxMesh )
	{
		AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("FbxSkeletaLMeshimport_NodeInvalidSkeletalMesh", "Fbx node: '{0}' is not a valid skeletal mesh"), FText::FromString(Node->GetName()))), FFbxErrors::Generic_Mesh_MeshNotFound);
		return NULL;
	}
	if (FbxMesh->GetDeformerCount(FbxDeformer::eSkin) == 0)
	{
		SkelType = 1;
	}

	// warning for missing smoothing group info
	CheckSmoothingInfo(FbxMesh);

	Parent = InParent;
	

	struct ExistingSkelMeshData* ExistSkelMeshDataPtr = NULL;

	if ( !FbxShapeArray  )
	{
		UObject* ExistingObject = StaticFindObjectFast(UObject::StaticClass(), InParent, *Name.ToString(), false, false, RF_PendingKill);
		USkeletalMesh* ExistingSkelMesh = Cast<USkeletalMesh>(ExistingObject);

		if (ExistingSkelMesh)
		{
#if WITH_APEX_CLOTHING
			//for supporting re-import 
			ApexClothingUtils::BackupClothingDataFromSkeletalMesh(ExistingSkelMesh);
#endif// #if WITH_APEX_CLOTHING

			ExistingSkelMesh->PreEditChange(NULL);
			ExistSkelMeshDataPtr = SaveExistingSkelMeshData(ExistingSkelMesh);
		}
		// if any other object exists, we can't import with this name
		else if (ExistingObject)
		{
			AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("FbxSkeletaLMeshimport_OverlappingName", "Same name but different class: '{0}' already exists"), FText::FromString(ExistingObject->GetName()))), FFbxErrors::Generic_SameNameAssetExists);
			return NULL;
		}
	}

	// [from USkeletalMeshFactory::FactoryCreateBinary]
	USkeletalMesh* SkeletalMesh = NewObject<USkeletalMesh>(InParent, Name, Flags);
	
	SkeletalMesh->PreEditChange(NULL);
	
	FSkeletalMeshImportData TempData;
	// Fill with data from buffer - contains the full .FBX file. 	
	FSkeletalMeshImportData* SkelMeshImportDataPtr = &TempData;
	if( OutData )
	{
		SkelMeshImportDataPtr = OutData;
	}

	bool bDiffPose;
	TArray<FbxNode*> SortedLinkArray;
	FbxArray<FbxAMatrix> GlobalsPerLink;

	bool bUseTime0AsRefPose = ImportOptions->bUseT0AsRefPose;
	// Note: importing morph data causes additional passes through this function, so disable the warning dialogs
	// from popping up again on each additional pass.  
	if ( !ImportBone(NodeArray, *SkelMeshImportDataPtr, TemplateImportData, SortedLinkArray,  bDiffPose, (FbxShapeArray != NULL), bUseTime0AsRefPose))
	{
		AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, LOCTEXT("FbxSkeletaLMeshimport_MultipleRootFound", "Multiple roots found")), FFbxErrors::SkeletalMesh_MultipleRoots);
		// I can't delete object here since this is middle of import
		// but I can move to transient package, and GC will automatically collect it
		SkeletalMesh->ClearFlags( RF_Standalone );
		SkeletalMesh->Rename(NULL, GetTransientPackage());
		return NULL;
	}

 	FbxNode* RootNode = Scene->GetRootNode();
 	if(RootNode && TemplateImportData)
 	{
 		ApplyTransformSettingsToFbxNode(RootNode, TemplateImportData);
 	}

	// Create a list of all unique fbx materials.  This needs to be done as a separate pass before reading geometry
	// so that we know about all possible materials before assigning material indices to each triangle
	TArray<FbxSurfaceMaterial*> FbxMaterials;
	for( int32 NodeIndex = 0; NodeIndex < NodeArray.Num(); ++NodeIndex )
	{
		Node = NodeArray[NodeIndex];

		int32 MaterialCount = Node->GetMaterialCount();

		for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
		{
			FbxSurfaceMaterial* FbxMaterial = Node->GetMaterial( MaterialIndex );
			if( !FbxMaterials.Contains( FbxMaterial ) )
			{
				FbxMaterials.Add( FbxMaterial );
				// Add an entry for each unique material
				SkelMeshImportDataPtr->Materials.Add( VMaterial() );
			}
		}
	}

	for ( int32 NodeIndex = 0; NodeIndex < NodeArray.Num(); ++NodeIndex)
	{
		Node = NodeArray[NodeIndex];
		FbxMesh = Node->GetMesh();
		FbxSkin* Skin = (FbxSkin*)FbxMesh->GetDeformer(0, FbxDeformer::eSkin);
		FbxShape* FbxShape = NULL;
		if (FbxShapeArray)
		{
			FbxShape = (*FbxShapeArray)[NodeIndex];
		}

		// NOTE: This function may invalidate FbxMesh and set it to point to a an updated version
		if (!FillSkelMeshImporterFromFbx( *SkelMeshImportDataPtr, FbxMesh, Skin, FbxShape, SortedLinkArray, FbxMaterials ) )
		{
			// I can't delete object here since this is middle of import
			// but I can move to transient package, and GC will automatically collect it
			SkeletalMesh->ClearFlags( RF_Standalone );
			SkeletalMesh->Rename(NULL, GetTransientPackage());
			return NULL;
		}
		
		if (bUseTime0AsRefPose && bDiffPose)
		{
			// deform skin vertex to the frame 0 from bind pose
			SkinControlPointsToPose(*SkelMeshImportDataPtr, FbxMesh, FbxShape, true);
		}
	}
	
	// reorder material according to "SKinXX" in material name
	SetMaterialSkinXXOrder(*SkelMeshImportDataPtr );
	
	if( ImportOptions->bPreserveSmoothingGroups )
	{
		DoUnSmoothVerts(*SkelMeshImportDataPtr);
	}
	else
	{
		SkelMeshImportDataPtr->PointToRawMap.AddUninitialized( SkelMeshImportDataPtr->Points.Num() );
		for(int32 PointIdx=0; PointIdx<SkelMeshImportDataPtr->Points.Num(); PointIdx++)
		{
			SkelMeshImportDataPtr->PointToRawMap[PointIdx] = PointIdx;
		}
	}

	// process materials from import data
	ProcessImportMeshMaterials(SkeletalMesh->Materials,*SkelMeshImportDataPtr);
	
	// process reference skeleton from import data
	int32 SkeletalDepth = 0;
	if(!ProcessImportMeshSkeleton(SkeletalMesh->RefSkeleton,SkeletalDepth,*SkelMeshImportDataPtr))
	{
		SkeletalMesh->ClearFlags( RF_Standalone );
		SkeletalMesh->Rename(NULL, GetTransientPackage());
		return NULL;
	}
	UE_LOG(LogFbx, Warning, TEXT("Bones digested - %i  Depth of hierarchy - %i"), SkeletalMesh->RefSkeleton.GetNum(), SkeletalDepth );

	// process bone influences from import data
	ProcessImportMeshInfluences(*SkelMeshImportDataPtr);

	FSkeletalMeshResource* ImportedResource = SkeletalMesh->GetImportedResource();
	check(ImportedResource->LODModels.Num() == 0);
	ImportedResource->LODModels.Empty();
	new(ImportedResource->LODModels)FStaticLODModel();

	SkeletalMesh->LODInfo.Empty();
	SkeletalMesh->LODInfo.AddZeroed();
	SkeletalMesh->LODInfo[0].LODHysteresis = 0.02f;
	FSkeletalMeshOptimizationSettings Settings;
	// set default reduction settings values
	SkeletalMesh->LODInfo[0].ReductionSettings = Settings;

	// Create initial bounding box based on expanded version of reference pose for meshes without physics assets. Can be overridden by artist.
	FBox BoundingBox(SkelMeshImportDataPtr->Points.GetData(), SkelMeshImportDataPtr->Points.Num());
	FBox Temp = BoundingBox;
	FVector MidMesh		= 0.5f*(Temp.Min + Temp.Max);
	BoundingBox.Min		= Temp.Min + 1.0f*(Temp.Min - MidMesh);
	BoundingBox.Max		= Temp.Max + 1.0f*(Temp.Max - MidMesh);
	// Tuck up the bottom as this rarely extends lower than a reference pose's (e.g. having its feet on the floor).
	// Maya has Y in the vertical, other packages have Z.
	//BEN const int32 CoordToTuck = bAssumeMayaCoordinates ? 1 : 2;
	//BEN BoundingBox.Min[CoordToTuck]	= Temp.Min[CoordToTuck] + 0.1f*(Temp.Min[CoordToTuck] - MidMesh[CoordToTuck]);
	BoundingBox.Min[2]	= Temp.Min[2] + 0.1f*(Temp.Min[2] - MidMesh[2]);
	SkeletalMesh->Bounds= FBoxSphereBounds(BoundingBox);

	// Store whether or not this mesh has vertex colors
	SkeletalMesh->bHasVertexColors = SkelMeshImportDataPtr->bHasVertexColors;

	FStaticLODModel& LODModel = ImportedResource->LODModels[0];
	
	// Pass the number of texture coordinate sets to the LODModel.  Ensure there is at least one UV coord
	LODModel.NumTexCoords = FMath::Max<uint32>(1,SkelMeshImportDataPtr->NumTexCoords);

	if( bCreateRenderData )
	{
		TArray<FVector> LODPoints;
		TArray<FMeshWedge> LODWedges;
		TArray<FMeshFace> LODFaces;
		TArray<FVertInfluence> LODInfluences;
		TArray<int32> LODPointToRawMap;
		SkelMeshImportDataPtr->CopyLODImportData(LODPoints,LODWedges,LODFaces,LODInfluences,LODPointToRawMap);

		const bool bShouldComputeNormals = !ImportOptions->ShouldImportNormals() || !SkelMeshImportDataPtr->bHasNormals;
		const bool bShouldComputeTangents = !ImportOptions->ShouldImportTangents() || !SkelMeshImportDataPtr->bHasTangents;

		IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");
		
		TArray<FText> WarningMessages;
		TArray<FName> WarningNames;
		// Create actual rendering data.
		if ( !MeshUtilities.BuildSkeletalMesh(ImportedResource->LODModels[0],SkeletalMesh->RefSkeleton,LODInfluences,LODWedges,LODFaces,LODPoints,LODPointToRawMap, ImportOptions->bKeepOverlappingVertices, bShouldComputeNormals, bShouldComputeTangents, &WarningMessages, &WarningNames ) )
		{
			if (WarningNames.Num() == WarningMessages.Num())
			{
				// temporary hack of message/names, should be one token or a struct
				for(int32 MessageIdx = 0; MessageIdx<WarningMessages.Num(); ++MessageIdx)
				{
					AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, WarningMessages[MessageIdx]), WarningNames[MessageIdx]);
				}
			}

			SkeletalMesh->MarkPendingKill();
			return NULL;
		}
		else if (WarningMessages.Num() > 0)
		{
			// temporary hack of message/names, should be one token or a struct
			if(WarningNames.Num() == WarningMessages.Num())
			{
				// temporary hack of message/names, should be one token or a struct
				for(int32 MessageIdx = 0; MessageIdx<WarningMessages.Num(); ++MessageIdx)
				{
					AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, WarningMessages[MessageIdx]), WarningNames[MessageIdx]);
				}
			}
		}

		// Presize the per-section shadow casting array with the number of sections in the imported LOD.
		const int32 NumSections = LODModel.Sections.Num();
		for ( int32 SectionIndex = 0 ; SectionIndex < NumSections ; ++SectionIndex )
		{
			SkeletalMesh->LODInfo[0].TriangleSortSettings.AddZeroed();
		}

		if (ExistSkelMeshDataPtr)
		{
			RestoreExistingSkelMeshData(ExistSkelMeshDataPtr, SkeletalMesh);
		}

		// Store the current file path and timestamp for re-import purposes
		UFbxSkeletalMeshImportData* ImportData = UFbxSkeletalMeshImportData::GetImportDataForSkeletalMesh(SkeletalMesh, TemplateImportData);
		SkeletalMesh->AssetImportData->SourceFilePath = FReimportManager::SanitizeImportFilename(UFactory::CurrentFilename, SkeletalMesh);
		SkeletalMesh->AssetImportData->SourceFileTimestamp = IFileManager::Get().GetTimeStamp(*UFactory::CurrentFilename).ToString();
		SkeletalMesh->AssetImportData->bDirty = false;

		SkeletalMesh->CalculateInvRefMatrices();
		SkeletalMesh->PostEditChange();
		SkeletalMesh->MarkPackageDirty();

		// Now iterate over all skeletal mesh components re-initialising them.
		for(TObjectIterator<USkeletalMeshComponent> It; It; ++It)
		{
			USkeletalMeshComponent* SkelComp = *It;
			if(SkelComp->SkeletalMesh == SkeletalMesh)
			{
				FComponentReregisterContext ReregisterContext(SkelComp);
			}
		}
	}

	if(InParent != GetTransientPackage())
	{
		// Create PhysicsAsset if requested and if physics asset is null
		if (ImportOptions->bCreatePhysicsAsset )
		{
			if (SkeletalMesh->PhysicsAsset == NULL)
			{
				FString ObjectName = FString::Printf(TEXT("%s_PhysicsAsset"), *SkeletalMesh->GetName());
				UPhysicsAsset * NewPhysicsAsset = CreateAsset<UPhysicsAsset>(InParent->GetName(), ObjectName, true); 
				if (!NewPhysicsAsset)
				{
					AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("FbxSkeletaLMeshimport_CouldNotCreatePhysicsAsset", "Could not create Physics Asset ('{0}') for '{1}'"), FText::FromString(ObjectName), FText::FromString(SkeletalMesh->GetName()))), FFbxErrors::SkeletalMesh_FailedToCreatePhyscisAsset);
				}
				else
				{
					FPhysAssetCreateParams NewBodyData;
					NewBodyData.Initialize();
					FText CreationErrorMessage;
					bool bSuccess = FPhysicsAssetUtils::CreateFromSkeletalMesh(NewPhysicsAsset, SkeletalMesh, NewBodyData, CreationErrorMessage);
					if (!bSuccess)
					{
						AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, CreationErrorMessage), FFbxErrors::SkeletalMesh_FailedToCreatePhyscisAsset);
						// delete the asset since we could not have create physics asset
						TArray<UObject*> ObjectsToDelete;
						ObjectsToDelete.Add(NewPhysicsAsset);
						ObjectTools::DeleteObjects(ObjectsToDelete, false);
					}
				}
			}
		}
		// if physics asset is selected
		else if (ImportOptions->PhysicsAsset)
		{
			SkeletalMesh->PhysicsAsset = ImportOptions->PhysicsAsset;
		}

		// see if we have skeleton set up
		// if creating skeleton, create skeleeton
		USkeleton* Skeleton = ImportOptions->SkeletonForAnimation;
		if (Skeleton == NULL)
		{
			FString ObjectName = FString::Printf(TEXT("%s_Skeleton"), *SkeletalMesh->GetName());
			Skeleton = CreateAsset<USkeleton>(InParent->GetName(), ObjectName, true); 
			if (!Skeleton)
			{
				// same object exists, try to see if it's skeleton, if so, load
				Skeleton = LoadObject<USkeleton>(InParent, *ObjectName);

				// if not skeleton, we're done, we can't create skeleton with same name
				// @todo in the future, we'll allow them to rename
				if (!Skeleton)
				{
					AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, FText::Format(LOCTEXT("FbxSkeletaLMeshimport_SkeletonRecreateError", "'{0}' already exists. It fails to recreate it."), FText::FromString(ObjectName))), FFbxErrors::SkeletalMesh_SkeletonRecreateError);
					return SkeletalMesh;
				}
			}
		}

		// merge bones to the selected skeleton
		if ( !Skeleton->MergeAllBonesToBoneTree( SkeletalMesh ) )
		{
			if ( EAppReturnType::Yes == FMessageDialog::Open( EAppMsgType::YesNo, 
										LOCTEXT("SkeletonFailed_BoneMerge", "FAILED TO MERGE BONES:\n\n This could happen if significant hierarchical change has been made\n - i.e. inserting bone between nodes\n Would you like to regenerate Skeleton from this mesh? \n\n ***WARNING: THIS WILL REQUIRE RECOMPRESS ALL ANIMATION DATA AND POTENTIALLY INVALIDATE***\n") ) ) 
				{
					if ( Skeleton->RecreateBoneTree( SkeletalMesh ) )
					{
						FAssetNotifications::SkeletonNeedsToBeSaved(Skeleton);
					}
				}
		}
		else
		{
			// ask if they'd like to update their position form this mesh
			if ( ImportOptions->SkeletonForAnimation && ImportOptions->bUpdateSkeletonReferencePose ) 
			{
				Skeleton->UpdateReferencePoseFromMesh(SkeletalMesh);
				FAssetNotifications::SkeletonNeedsToBeSaved(Skeleton);
			}
		}
	}
#if WITH_APEX_CLOTHING
	//for supporting re-import 
	ApexClothingUtils::ReapplyClothingDataToSkeletalMesh(SkeletalMesh);
#endif// #if WITH_APEX_CLOTHING

	return SkeletalMesh;
}

UObject* UnFbx::FFbxImporter::CreateAssetOfClass(UClass* AssetClass, FString ParentPackageName, FString ObjectName, bool bAllowReplace)
{
	// See if this sequence already exists.
	UObject* 	ParentPkg = CreatePackage(NULL, *ParentPackageName);
	FString 	ParentPath = FString::Printf(TEXT("%s/%s"), *FPackageName::GetLongPackagePath(*ParentPackageName), *ObjectName);
	UObject* 	Parent = CreatePackage(NULL, *ParentPath);
	// See if an object with this name exists
	UObject* Object = LoadObject<UObject>(Parent, *ObjectName, NULL, LOAD_None, NULL);

	// if object with same name but different class exists, warn user
	if ((Object != NULL) && (Object->GetClass() != AssetClass))
	{
		UnFbx::FFbxImporter* FFbxImporter = UnFbx::FFbxImporter::GetInstance();
		FFbxImporter->AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, LOCTEXT("Error_AssetExist", "Asset with same name exists. Can't overwrite another asset")), FFbxErrors::Generic_SameNameAssetExists);
		return NULL;
	}

	// if object with same name exists, warn user
	if ( Object != NULL && !bAllowReplace )
	{
		// until we have proper error message handling so we don't ask every time, but once, I'm disabling it. 
		// 		if ( EAppReturnType::Yes != FMessageDialog::Open( EAppMsgType::YesNo, LocalizeSecure(NSLOCTEXT("UnrealEd", "Error_AssetExistAsk", "Asset with the name (`~) exists. Would you like to overwrite?").ToString(), *ParentPath) ) ) 
		// 		{
		// 			return NULL;
		// 		}
		UnFbx::FFbxImporter* FFbxImporter = UnFbx::FFbxImporter::GetInstance();
		FFbxImporter->AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("FbxSkeletaLMeshimport_SameNameExist", "Asset with the name ('{0}') exists. Overwriting..."), FText::FromString(ParentPath))), FFbxErrors::Generic_SameNameAssetOverriding);
	}

	if (Object == NULL)
	{
		// add it to the set
		// do not add to the set, now create independent asset
		Object = NewObject<UObject>(Parent, AssetClass, *ObjectName, RF_Public | RF_Standalone);
		Object->MarkPackageDirty();
		// Notify the asset registry
		FAssetRegistryModule::AssetCreated(Object);
	}

	return Object;
}

void UnFbx::FFbxImporter::SetupAnimationDataFromMesh(USkeletalMesh* SkeletalMesh, UObject* InParent, TArray<FbxNode*>& NodeArray, UFbxAnimSequenceImportData* TemplateImportData, const FString& Name)
{
	USkeleton* Skeleton = SkeletalMesh->Skeleton;

	if (Scene->GetSrcObjectCount<FbxAnimStack>() > 0)
	{
		if ( ensure ( Skeleton != NULL ) )
		{
			TArray<FbxNode*> FBXMeshNodeArray;
			FbxNode* SkeletonRoot = FindFBXMeshesByBone(SkeletalMesh->Skeleton->GetReferenceSkeleton().GetBoneName(0), true, FBXMeshNodeArray);

			TArray<FbxNode*> SortedLinks;
			RecursiveBuildSkeleton(SkeletonRoot, SortedLinks);

			// when importing animation from SkeletalMesh, add new Group Anim, a lot of times they're same name
			UPackage * OuterPackage = InParent->GetOutermost();
			FString AnimName = (ImportOptions->AnimationName!="")? ImportOptions->AnimationName : Name+TEXT("_Anim");
			// give animouter as outer
			ImportAnimations(Skeleton, OuterPackage, SortedLinks, AnimName, TemplateImportData, NodeArray);
		}
	}
}

USkeletalMesh* UnFbx::FFbxImporter::ReimportSkeletalMesh(USkeletalMesh* Mesh, UFbxSkeletalMeshImportData* TemplateImportData)
{
	if ( !ensure(Mesh) )
	{
		// You need a mesh in order to reimport
		return NULL;
	}

	if ( !ensure(TemplateImportData) )
	{
		// You need import data in order to reimport
		return NULL;
	}

	char MeshName[1024];
	FCStringAnsi::Strcpy(MeshName, TCHAR_TO_UTF8(*Mesh->GetName()));
	TArray<FbxNode*>* FbxNodes = NULL;
	USkeletalMesh* NewMesh = NULL;

	// support to update rigid animation mesh
	ImportOptions->bImportRigidMesh = true;

	// get meshes in Fbx file
	//the function also fill the collision models, so we can update collision models correctly
	TArray< TArray<FbxNode*>* > FbxSkelMeshArray;
	FillFbxSkelMeshArrayInScene(Scene->GetRootNode(), FbxSkelMeshArray, false);

	// if there is only one mesh, use it without name checking 
	// (because the "Used As Full Name" option enables users name the Unreal mesh by themselves
	if (FbxSkelMeshArray.Num() > 0 )
	{
		FbxNodes = FbxSkelMeshArray[0];
	}
	else
	{
		// @todo - FBX Importing - We need proper support for reimport if the file contains more than one skeletal mesh. 

	}

	if (FbxNodes)
	{
		// set import options, how about others?
		ImportOptions->bImportMaterials = false;
		ImportOptions->bImportTextures = false;
		ImportOptions->bImportAnimations = false;
		// check if there is LODGroup for this skeletal mesh
		int32 MaxLODLevel = 1;
		int32 NumPrevLODs = Mesh->LODInfo.Num();

		for (int32 j = 0; j < (*FbxNodes).Num(); j++)
		{
			FbxNode* Node = (*FbxNodes)[j];
			if (Node->GetNodeAttribute() && Node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGroup)
			{
				// get max LODgroup level
				if (MaxLODLevel < Node->GetChildCount())
				{
					MaxLODLevel = Node->GetChildCount();
				}
			}
		}

		int32 LODIndex;
		for (LODIndex = 0; LODIndex < MaxLODLevel; LODIndex++)
		{
			TArray<FbxNode*> SkelMeshNodeArray;
			for (int32 j = 0; j < (*FbxNodes).Num(); j++)
			{
				FbxNode* Node = (*FbxNodes)[j];
				if (Node->GetNodeAttribute() && Node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGroup)
				{
					if (Node->GetChildCount() > LODIndex)
					{
						SkelMeshNodeArray.Add(Node->GetChild(LODIndex));
					}
					else // in less some LODGroups have less level, use the last level
					{
						SkelMeshNodeArray.Add(Node->GetChild(Node->GetChildCount() - 1));
					}
				}
				else
				{
					SkelMeshNodeArray.Add(Node);
				}
			}

			if (LODIndex == 0)
			{
				NewMesh = ImportSkeletalMesh( Mesh->GetOuter(), SkelMeshNodeArray, *Mesh->GetName(), RF_Public|RF_Standalone, TemplateImportData, FReimportManager::ResolveImportFilename(TemplateImportData->SourceFilePath, Mesh));
			}
			else if (NewMesh) // the base skeletal mesh is imported successfully
			{
				USkeletalMesh* BaseSkeletalMesh = Cast<USkeletalMesh>(NewMesh);
				UObject *LODObject = ImportSkeletalMesh( GetTransientPackage(), SkelMeshNodeArray, NAME_None, RF_NoFlags, TemplateImportData, FReimportManager::ResolveImportFilename(TemplateImportData->SourceFilePath, BaseSkeletalMesh) );
				ImportSkeletalMeshLOD( Cast<USkeletalMesh>(LODObject), BaseSkeletalMesh, LODIndex, false);

				// Set LOD Model's DisplayFactor
				// if this LOD is newly added, then set DisplayFactor
				// Don't override DispalyFactor of existing LODs
				if(LODIndex >= NumPrevLODs)
				{
					BaseSkeletalMesh->LODInfo[LODIndex].ScreenSize = 1.0f / (MaxLODLevel * LODIndex);
				}
			}

			// import morph target
			if ( NewMesh)
			{
				// @fixme: @question : where do they import this morph? where to? What morph target sets?
				ImportFbxMorphTarget(SkelMeshNodeArray, NewMesh, NewMesh->GetOutermost(), FReimportManager::ResolveImportFilename(TemplateImportData->SourceFilePath, NewMesh), LODIndex);
			}
		}
	}
	else
	{
		// no mesh found in the FBX file
		AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("FbxSkeletaLMeshimport_NoFBXMeshMatch", "No FBX mesh matches the Unreal mesh '{0}'."), FText::FromString(Mesh->GetName()))), FFbxErrors::Generic_Mesh_MeshNotFound);
	}

	return NewMesh;
}

void UnFbx::FFbxImporter::SetMaterialSkinXXOrder(FSkeletalMeshImportData& ImportData )
{
	TArray<int32> MaterialIndexToSkinIndex;
	{
		int32 MaterialCount = ImportData.Materials.Num();
		MaterialIndexToSkinIndex.AddZeroed(MaterialCount);

		bool bNeedsReorder = false;
		for(int32 MaterialIndex=0; MaterialIndex < MaterialCount; ++MaterialIndex)
		{
			// get skin index
			FString MatName = ImportData.Materials[MaterialIndex].MaterialImportName;

			if (MatName.Len() > 6)
			{
				int32 Offset =  MatName.Find( TEXT("_SKIN"),ESearchCase::IgnoreCase, ESearchDir::FromEnd);
				if( Offset != INDEX_NONE )
				{
					// Chop off the material name so we are left with the number in _SKINXX
					FString SkinXXNumber = MatName.Right( MatName.Len() - (Offset+1) ).RightChop( 4 );

					if ( SkinXXNumber.IsNumeric()  )
					{
						bNeedsReorder = true;

						int32 TmpIndex = FPlatformString::Atoi( *SkinXXNumber );
						MaterialIndexToSkinIndex[MaterialIndex] = TmpIndex;

						// remove the 'skinXX' suffix from the material name					
						ImportData.Materials[MaterialIndex].MaterialImportName.LeftChop( Offset );
					}
				}
			}
		}

		if (bNeedsReorder)
		{
			// re-order the materials
			TArray< VMaterial > ExistingMatList = ImportData.Materials;

			for(int32 MaterialIndex=0; MaterialIndex < MaterialCount; ++MaterialIndex)
			{
				int32 NewIndex = MaterialIndexToSkinIndex[MaterialIndex];
				if( ExistingMatList.IsValidIndex( NewIndex ) )
				{
					ImportData.Materials[MaterialIndex] = ExistingMatList[ NewIndex ];
				}
			}

			// remapping the material index for each triangle
			int32 FaceNum = ImportData.Faces.Num();
			for( int32 TriangleIndex = 0; TriangleIndex < FaceNum ; TriangleIndex++)
			{
				VTriangle& Triangle = ImportData.Faces[TriangleIndex];
				Triangle.MatIndex = MaterialIndexToSkinIndex[ Triangle.MatIndex ];
			}
		}
	}
}

bool UnFbx::FFbxImporter::FillSkelMeshImporterFromFbx( FSkeletalMeshImportData& ImportData, FbxMesh*& Mesh, FbxSkin* Skin, FbxShape* FbxShape, TArray<FbxNode*> &SortedLinks, const TArray<FbxSurfaceMaterial*>& FbxMaterials )
{
	FbxNode* Node = Mesh->GetNode();

	//remove the bad polygons before getting any data from mesh
	Mesh->RemoveBadPolygons();

	//Get the base layer of the mesh
	FbxLayer* BaseLayer = Mesh->GetLayer(0);
	if (BaseLayer == NULL)
	{
		AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, LOCTEXT("FbxSkeletaLMeshimport_NoGeometry", "There is no geometry information in mesh")), FFbxErrors::Generic_Mesh_NoGeometry);
		return false;
	}

	// Do some checks before proceeding, check to make sure the number of bones does not exceed the maximum supported
	if(SortedLinks.Num() > MAX_BONES)
	{
		AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, FText::Format(LOCTEXT("FbxSkeletaLMeshimport_ExceedsMaxBoneCount", "'{0}' mesh has '{1}' bones which exceeds the maximum allowed bone count of {2}."), FText::FromString(Node->GetName()), FText::AsNumber(SortedLinks.Num()), FText::AsNumber(MAX_BONES))), FFbxErrors::SkeletalMesh_ExceedsMaxBoneCount);
		return false;
	}

	//
	//	store the UVs in arrays for fast access in the later looping of triangles 
	//
	// mapping from UVSets to Fbx LayerElementUV
	// Fbx UVSets may be duplicated, remove the duplicated UVSets in the mapping 
	int32 LayerCount = Mesh->GetLayerCount();
	TArray<FString> UVSets;

	UVSets.Empty();
	if (LayerCount > 0)
	{
		int32 UVLayerIndex;
		for (UVLayerIndex = 0; UVLayerIndex<LayerCount; UVLayerIndex++)
		{
			FbxLayer* lLayer = Mesh->GetLayer(UVLayerIndex);
			int32 UVSetCount = lLayer->GetUVSetCount();
			if(UVSetCount)
			{
				FbxArray<FbxLayerElementUV const*> EleUVs = lLayer->GetUVSets();
				for (int32 UVIndex = 0; UVIndex<UVSetCount; UVIndex++)
				{
					FbxLayerElementUV const* ElementUV = EleUVs[UVIndex];
					if (ElementUV)
					{
						const char* UVSetName = ElementUV->GetName();
						FString LocalUVSetName = UTF8_TO_TCHAR(UVSetName);

						UVSets.AddUnique(LocalUVSetName);
					}
				}
			}
		}
	}

	// If the the UV sets are named using the following format (UVChannel_X; where X ranges from 1 to 4)
	// we will re-order them based on these names.  Any UV sets that do not follow this naming convention
	// will be slotted into available spaces.
	if( UVSets.Num() > 0 )
	{
		for(int32 ChannelNumIdx = 0; ChannelNumIdx < 4; ChannelNumIdx++)
		{
			FString ChannelName = FString::Printf( TEXT("UVChannel_%d"), ChannelNumIdx+1 );
			int32 SetIdx = UVSets.Find( ChannelName );

			// If the specially formatted UVSet name appears in the list and it is in the wrong spot,
			// we will swap it into the correct spot.
			if( SetIdx != INDEX_NONE && SetIdx != ChannelNumIdx )
			{
				// If we are going to swap to a position that is outside the bounds of the
				// array, then we pad out to that spot with empty data.
				for(int32 ArrSize = UVSets.Num(); ArrSize < ChannelNumIdx+1; ArrSize++)
				{
					UVSets.Add( FString(TEXT("")) );
				}
				//Swap the entry into the appropriate spot.
				UVSets.Swap( SetIdx, ChannelNumIdx );
			}
		}
	}


	TArray<UMaterialInterface*> Materials;
	if (ImportOptions->bImportMaterials)
	{
		CreateNodeMaterials(Node, Materials, UVSets);
	}
	else if (ImportOptions->bImportTextures)
	{
		ImportTexturesFromNode(Node);
	}

	// Maps local mesh material index to global material index
	TArray<int32> MaterialMapping;

	int32 MaterialCount = Node->GetMaterialCount();

	MaterialMapping.AddUninitialized(MaterialCount);

	for( int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex )
	{
		FbxSurfaceMaterial* FbxMaterial = Node->GetMaterial( MaterialIndex );

		int32 ExistingMatIndex = INDEX_NONE;
		FbxMaterials.Find( FbxMaterial, ExistingMatIndex ); 
		if( ExistingMatIndex != INDEX_NONE )
		{
			// Reuse existing material
			MaterialMapping[MaterialIndex] = ExistingMatIndex;

			if (ImportOptions->bImportMaterials && Materials.IsValidIndex(MaterialIndex) )
			{
				ImportData.Materials[ExistingMatIndex].MaterialImportName = UTF8_TO_TCHAR(MakeName(FbxMaterial->GetName()));
				ImportData.Materials[ExistingMatIndex].Material = Materials[MaterialIndex];
			}
		}
		else
		{
			MaterialMapping[MaterialIndex] = 0;
		}
	}

	if( LayerCount > 0 && ImportOptions->bPreserveSmoothingGroups )
	{
		// Check and see if the smooothing data is valid.  If not generate it from the normals
		BaseLayer = Mesh->GetLayer(0);
		if( BaseLayer )
		{
			const FbxLayerElementSmoothing* SmoothingLayer = BaseLayer->GetSmoothing();

			if( SmoothingLayer )
			{
				bool bValidSmoothingData = false;
				FbxLayerElementArrayTemplate<int32>& Array = SmoothingLayer->GetDirectArray();
				for( int32 SmoothingIndex = 0; SmoothingIndex < Array.GetCount(); ++SmoothingIndex )
				{
					if( Array[SmoothingIndex] != 0 )
					{
						bValidSmoothingData = true;
						break;
					}
				}

				if( !bValidSmoothingData && Mesh->GetPolygonVertexCount() > 0 )
				{
					GeometryConverter->ComputeEdgeSmoothingFromNormals(Mesh);
				}
			}
		}
	}

	// Must do this before triangulating the mesh due to an FBX bug in TriangulateMeshAdvance
	int32 LayerSmoothingCount = Mesh->GetLayerCount(FbxLayerElement::eSmoothing);
	for(int32 i = 0; i < LayerSmoothingCount; i++)
	{
		GeometryConverter->ComputePolygonSmoothingFromEdgeSmoothing (Mesh, i);
	}

	//
	// Convert data format to unreal-compatible
	//

	if (!Mesh->IsTriangleMesh())
	{
		UE_LOG(LogFbx, Log, TEXT("Triangulating skeletal mesh %s"), UTF8_TO_TCHAR(Node->GetName()));
		
		const bool bReplace = true;
		FbxNodeAttribute* ConvertedNode = GeometryConverter->Triangulate(Mesh, bReplace);
		if( ConvertedNode != NULL && ConvertedNode->GetAttributeType() == FbxNodeAttribute::eMesh )
		{
			Mesh = ConvertedNode->GetNode()->GetMesh();
		}
		else
		{
			AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("FbxSkeletaLMeshimport_TriangulatingFailed", "Unable to triangulate mesh '{0}'. Check detail for Ouput Log."), FText::FromString(Node->GetName()))), FFbxErrors::Generic_Mesh_TriangulationFailed);
			return false;
		}
	}
	
	// renew the base layer
	BaseLayer = Mesh->GetLayer(0);
	Skin = (FbxSkin*)static_cast<FbxGeometry*>(Mesh)->GetDeformer(0, FbxDeformer::eSkin);

	//
	//	store the UVs in arrays for fast access in the later looping of triangles 
	//
	uint32 UniqueUVCount = UVSets.Num();
	FbxLayerElementUV** LayerElementUV = NULL;
	FbxLayerElement::EReferenceMode* UVReferenceMode = NULL;
	FbxLayerElement::EMappingMode* UVMappingMode = NULL;
	if (UniqueUVCount > 0)
	{
		LayerElementUV = new FbxLayerElementUV*[UniqueUVCount];
		UVReferenceMode = new FbxLayerElement::EReferenceMode[UniqueUVCount];
		UVMappingMode = new FbxLayerElement::EMappingMode[UniqueUVCount];
	}
	else
	{
		AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("FbxSkeletaLMeshimport_NoUVSet", "Mesh '{0}' has no UV set. Creating a default set."), FText::FromString(Node->GetName()))), FFbxErrors::SkeletalMesh_NoUVSet);
	}
	
	LayerCount = Mesh->GetLayerCount();
	for (uint32 UVIndex = 0; UVIndex < UniqueUVCount; UVIndex++)
	{
		LayerElementUV[UVIndex] = NULL;
		for (int32 UVLayerIndex = 0; UVLayerIndex<LayerCount; UVLayerIndex++)
		{
			FbxLayer* lLayer = Mesh->GetLayer(UVLayerIndex);
			int32 UVSetCount = lLayer->GetUVSetCount();
			if(UVSetCount)
			{
				FbxArray<FbxLayerElementUV const*> EleUVs = lLayer->GetUVSets();
				for (int32 FbxUVIndex = 0; FbxUVIndex<UVSetCount; FbxUVIndex++)
				{
					FbxLayerElementUV const* ElementUV = EleUVs[FbxUVIndex];
					if (ElementUV)
					{
						const char* UVSetName = ElementUV->GetName();
						FString LocalUVSetName = UTF8_TO_TCHAR(UVSetName);
						if (LocalUVSetName == UVSets[UVIndex])
						{
							LayerElementUV[UVIndex] = const_cast<FbxLayerElementUV*>(ElementUV);
							UVReferenceMode[UVIndex] = LayerElementUV[FbxUVIndex]->GetReferenceMode();
							UVMappingMode[UVIndex] = LayerElementUV[FbxUVIndex]->GetMappingMode();
							break;
						}
					}
				}
			}
		}
	}

	//
	// get the smoothing group layer
	//
	bool bSmoothingAvailable = false;

	FbxLayerElementSmoothing const* SmoothingInfo = BaseLayer->GetSmoothing();
	FbxLayerElement::EReferenceMode SmoothingReferenceMode(FbxLayerElement::eDirect);
	FbxLayerElement::EMappingMode SmoothingMappingMode(FbxLayerElement::eByEdge);
	if (SmoothingInfo)
	{
 		if( SmoothingInfo->GetMappingMode() == FbxLayerElement::eByEdge )
		{
			if (!GeometryConverter->ComputePolygonSmoothingFromEdgeSmoothing(Mesh))
			{
				AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("FbxSkeletaLMeshimport_ConvertSmoothingGroupFailed", "Unable to fully convert the smoothing groups for mesh '{0}'"), FText::FromString(Mesh->GetName()))), FFbxErrors::Generic_Mesh_ConvertSmoothingGroupFailed);
				bSmoothingAvailable = false;
			}
		}

		if( SmoothingInfo->GetMappingMode() == FbxLayerElement::eByPolygon )
		{
			bSmoothingAvailable = true;
		}


		SmoothingReferenceMode = SmoothingInfo->GetReferenceMode();
		SmoothingMappingMode = SmoothingInfo->GetMappingMode();
	}


	//
	//	get the "material index" layer
	//
	FbxLayerElementMaterial* LayerElementMaterial = BaseLayer->GetMaterials();
	FbxLayerElement::EMappingMode MaterialMappingMode = LayerElementMaterial ? 
		LayerElementMaterial->GetMappingMode() : FbxLayerElement::eByPolygon;

	UniqueUVCount = FMath::Min<uint32>( UniqueUVCount, MAX_TEXCOORDS );

	// One UV set is required but only import up to MAX_TEXCOORDS number of uv layers
	ImportData.NumTexCoords = FMath::Max<uint32>( ImportData.NumTexCoords, UniqueUVCount );

	//
	// get the first vertex color layer
	//
	FbxLayerElementVertexColor* LayerElementVertexColor = BaseLayer->GetVertexColors();
	FbxLayerElement::EReferenceMode VertexColorReferenceMode(FbxLayerElement::eDirect);
	FbxLayerElement::EMappingMode VertexColorMappingMode(FbxLayerElement::eByControlPoint);
	if (LayerElementVertexColor)
	{
		VertexColorReferenceMode = LayerElementVertexColor->GetReferenceMode();
		VertexColorMappingMode = LayerElementVertexColor->GetMappingMode();
		ImportData.bHasVertexColors = true;
	}

	//
	// get the first normal layer
	//
	FbxLayerElementNormal* LayerElementNormal = BaseLayer->GetNormals();
	FbxLayerElementTangent* LayerElementTangent = BaseLayer->GetTangents();
	FbxLayerElementBinormal* LayerElementBinormal = BaseLayer->GetBinormals();

	//whether there is normal, tangent and binormal data in this mesh
	bool bHasNormalInformation = LayerElementNormal != NULL;
	bool bHasTangentInformation = LayerElementTangent != NULL && LayerElementBinormal != NULL;

	ImportData.bHasNormals = bHasNormalInformation;
	ImportData.bHasTangents = bHasTangentInformation;

	FbxLayerElement::EReferenceMode NormalReferenceMode(FbxLayerElement::eDirect);
	FbxLayerElement::EMappingMode NormalMappingMode(FbxLayerElement::eByControlPoint);
	if (LayerElementNormal)
	{
		NormalReferenceMode = LayerElementNormal->GetReferenceMode();
		NormalMappingMode = LayerElementNormal->GetMappingMode();
	}

	FbxLayerElement::EReferenceMode TangentReferenceMode(FbxLayerElement::eDirect);
	FbxLayerElement::EMappingMode TangentMappingMode(FbxLayerElement::eByControlPoint);
	if (LayerElementTangent)
	{
		TangentReferenceMode = LayerElementTangent->GetReferenceMode();
		TangentMappingMode = LayerElementTangent->GetMappingMode();
	}

	//
	// create the points / wedges / faces
	//
	int32 ControlPointsCount = Mesh->GetControlPointsCount();
	int32 ExistPointNum = ImportData.Points.Num();
	ImportData.Points.AddUninitialized(ControlPointsCount);

	// Construct the matrices for the conversion from right handed to left handed system
	FbxAMatrix TotalMatrix;
	FbxAMatrix TotalMatrixForNormal;
	TotalMatrix = ComputeTotalMatrix(Node);
	TotalMatrixForNormal = TotalMatrix.Inverse();
	TotalMatrixForNormal = TotalMatrixForNormal.Transpose();

	int32 ControlPointsIndex;
	for( ControlPointsIndex = 0 ; ControlPointsIndex < ControlPointsCount ;ControlPointsIndex++ )
	{
		FbxVector4 Position;
		if (FbxShape)
		{
			Position = FbxShape->GetControlPoints()[ControlPointsIndex];
		}
		else
		{
			Position = Mesh->GetControlPoints()[ControlPointsIndex];
		}																	 
		FbxVector4 FinalPosition;
		FinalPosition = TotalMatrix.MultT(Position);
		ImportData.Points[ControlPointsIndex+ExistPointNum] = Converter.ConvertPos(FinalPosition);
	}
	
	bool OddNegativeScale = IsOddNegativeScale(TotalMatrix);
	
	int32 TriangleCount = Mesh->GetPolygonCount();
	int32 ExistFaceNum = ImportData.Faces.Num();
	ImportData.Faces.AddUninitialized( TriangleCount );
	int32 ExistWedgesNum = ImportData.Wedges.Num();
	VVertex TmpWedges[3];

	for( int32 TriangleIndex = ExistFaceNum, LocalIndex = 0 ; TriangleIndex < ExistFaceNum+TriangleCount ; TriangleIndex++, LocalIndex++ )
	{

		VTriangle& Triangle = ImportData.Faces[TriangleIndex];

		//
		// smoothing mask
		//
		// set the face smoothing by default. It could be any number, but not zero
		Triangle.SmoothingGroups = 255; 
		if ( bSmoothingAvailable)
		{
			if (SmoothingInfo)
			{
				if (SmoothingMappingMode == FbxLayerElement::eByPolygon)
				{
					int32 lSmoothingIndex = (SmoothingReferenceMode == FbxLayerElement::eDirect) ? LocalIndex : SmoothingInfo->GetIndexArray().GetAt(LocalIndex);
					Triangle.SmoothingGroups = SmoothingInfo->GetDirectArray().GetAt(lSmoothingIndex);
				}
				else
				{
					AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("FbxSkeletaLMeshimport_Unsupportingsmoothinggroup", "Unsupported Smoothing group mapping mode on mesh '{0}'"), FText::FromString(Mesh->GetName()))), FFbxErrors::Generic_Mesh_UnsupportingSmoothingGroup);
				}
			}
		}

		for (int32 VertexIndex=0; VertexIndex<3; VertexIndex++)
		{
			// If there are odd number negative scale, invert the vertex order for triangles
			int32 UnrealVertexIndex = OddNegativeScale ? 2 - VertexIndex : VertexIndex;

			int32 ControlPointIndex = Mesh->GetPolygonVertex(LocalIndex, VertexIndex);
			//
			// normals, tangents and binormals
			//
			if( ImportOptions->ShouldImportNormals() && bHasNormalInformation )
			{
				int32 TmpIndex = LocalIndex*3 + VertexIndex;
				//normals may have different reference and mapping mode than tangents and binormals
				int32 NormalMapIndex = (NormalMappingMode == FbxLayerElement::eByControlPoint) ? 
										ControlPointIndex : TmpIndex;
				int32 NormalValueIndex = (NormalReferenceMode == FbxLayerElement::eDirect) ? 
										NormalMapIndex : LayerElementNormal->GetIndexArray().GetAt(NormalMapIndex);

				//tangents and binormals share the same reference, mapping mode and index array
				int32 TangentMapIndex = TmpIndex;

				FbxVector4 TempValue;

				if( ImportOptions->ShouldImportTangents() && bHasTangentInformation )
				{
					TempValue = LayerElementTangent->GetDirectArray().GetAt(TangentMapIndex);
					TempValue = TotalMatrixForNormal.MultT(TempValue);
					Triangle.TangentX[ UnrealVertexIndex ] = Converter.ConvertDir(TempValue);
					Triangle.TangentX[ UnrealVertexIndex ].Normalize();

					TempValue = LayerElementBinormal->GetDirectArray().GetAt(TangentMapIndex);
					TempValue = TotalMatrixForNormal.MultT(TempValue);
					Triangle.TangentY[ UnrealVertexIndex ] = -Converter.ConvertDir(TempValue);
					Triangle.TangentY[ UnrealVertexIndex ].Normalize();
				}

				TempValue = LayerElementNormal->GetDirectArray().GetAt(NormalValueIndex);
				TempValue = TotalMatrixForNormal.MultT(TempValue);
				Triangle.TangentZ[ UnrealVertexIndex ] = Converter.ConvertDir(TempValue);
				Triangle.TangentZ[ UnrealVertexIndex ].Normalize();
			
			}
			else
			{
				int32 NormalIndex;
				for( NormalIndex = 0; NormalIndex < 3; ++NormalIndex )
				{
					Triangle.TangentX[ NormalIndex ] = FVector::ZeroVector;
					Triangle.TangentY[ NormalIndex ] = FVector::ZeroVector;
					Triangle.TangentZ[ NormalIndex ] = FVector::ZeroVector;
				}
			}
		}
		
		//
		// material index
		//
		Triangle.MatIndex = 0; // default value
		if (MaterialCount>0)
		{
			if (LayerElementMaterial)
			{
				switch(MaterialMappingMode)
				{
				// material index is stored in the IndexArray, not the DirectArray (which is irrelevant with 2009.1)
				case FbxLayerElement::eAllSame:
					{	
						Triangle.MatIndex = MaterialMapping[ LayerElementMaterial->GetIndexArray().GetAt(0) ];
					}
					break;
				case FbxLayerElement::eByPolygon:
					{	
						int32 Index = LayerElementMaterial->GetIndexArray().GetAt(LocalIndex);							
						if (!MaterialMapping.IsValidIndex(Index))
						{
							AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, LOCTEXT("FbxSkeletaLMeshimport_MaterialIndexInconsistency", "Face material index inconsistency - forcing to 0")), FFbxErrors::Generic_Mesh_MaterialIndexInconsistency);
						}
						else
						{
							Triangle.MatIndex = MaterialMapping[Index];
						}
					}
					break;
				}
			}

			// When import morph, we don't check the material index 
			// because we don't import material for morph, so the ImportData.Materials contains zero material
			if ( !FbxShape && (Triangle.MatIndex < 0 ||  Triangle.MatIndex >= FbxMaterials.Num() ) )
			{
				AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, LOCTEXT("FbxSkeletaLMeshimport_MaterialIndexInconsistency", "Face material index inconsistency - forcing to 0")), FFbxErrors::Generic_Mesh_MaterialIndexInconsistency);
				Triangle.MatIndex = 0;
			}
		}
		ImportData.MaxMaterialIndex = FMath::Max<uint32>( ImportData.MaxMaterialIndex, Triangle.MatIndex );

		Triangle.AuxMatIndex = 0;
		for (int32 VertexIndex=0; VertexIndex<3; VertexIndex++)
		{
			// If there are odd number negative scale, invert the vertex order for triangles
			int32 UnrealVertexIndex = OddNegativeScale ? 2 - VertexIndex : VertexIndex;

			TmpWedges[UnrealVertexIndex].MatIndex = Triangle.MatIndex;
			TmpWedges[UnrealVertexIndex].VertexIndex = ExistPointNum + Mesh->GetPolygonVertex(LocalIndex,VertexIndex);
			// Initialize all colors to white.
			TmpWedges[UnrealVertexIndex].Color = FColor::White;
		}

		//
		// uvs
		//
		uint32 UVLayerIndex;
		// Some FBX meshes can have no UV sets, so also check the UniqueUVCount
		for ( UVLayerIndex = 0; UVLayerIndex< UniqueUVCount; UVLayerIndex++ )
		{
			// ensure the layer has data
			if (LayerElementUV[UVLayerIndex] != NULL) 
			{
				// Get each UV from the layer
				for (int32 VertexIndex=0;VertexIndex<3;VertexIndex++)
				{
					// If there are odd number negative scale, invert the vertex order for triangles
					int32 UnrealVertexIndex = OddNegativeScale ? 2 - VertexIndex : VertexIndex;

					int32 lControlPointIndex = Mesh->GetPolygonVertex(LocalIndex, VertexIndex);
					int32 UVMapIndex = (UVMappingMode[UVLayerIndex] == FbxLayerElement::eByControlPoint) ? 
							lControlPointIndex : LocalIndex*3+VertexIndex;
					int32 UVIndex = (UVReferenceMode[UVLayerIndex] == FbxLayerElement::eDirect) ? 
							UVMapIndex : LayerElementUV[UVLayerIndex]->GetIndexArray().GetAt(UVMapIndex);
					FbxVector2	UVVector = LayerElementUV[UVLayerIndex]->GetDirectArray().GetAt(UVIndex);

					TmpWedges[UnrealVertexIndex].UVs[ UVLayerIndex ].X = static_cast<float>(UVVector[0]);
					TmpWedges[UnrealVertexIndex].UVs[ UVLayerIndex ].Y = 1.f - static_cast<float>(UVVector[1]);
				}
			}
			else if( UVLayerIndex == 0 )
			{
				// Set all UV's to zero.  If we are here the mesh had no UV sets so we only need to do this for the
				// first UV set which always exists.

				for (int32 VertexIndex=0; VertexIndex<3; VertexIndex++)
				{
					TmpWedges[VertexIndex].UVs[UVLayerIndex].X = 0.0f;
					TmpWedges[VertexIndex].UVs[UVLayerIndex].Y = 0.0f;
				}
			}
		}

		// Read vertex colors if they exist.
		if( LayerElementVertexColor )
		{
			switch(VertexColorMappingMode)
			{
			case FbxLayerElement::eByControlPoint:
				{
					for (int32 VertexIndex=0;VertexIndex<3;VertexIndex++)
					{
						int32 UnrealVertexIndex = OddNegativeScale ? 2 - VertexIndex : VertexIndex;

						FbxColor VertexColor = (VertexColorReferenceMode == FbxLayerElement::eDirect)
							?	LayerElementVertexColor->GetDirectArray().GetAt(Mesh->GetPolygonVertex(LocalIndex,VertexIndex))
							:	LayerElementVertexColor->GetDirectArray().GetAt(LayerElementVertexColor->GetIndexArray().GetAt(Mesh->GetPolygonVertex(LocalIndex,VertexIndex)));

						TmpWedges[UnrealVertexIndex].Color =	FColor(	uint8(255.f*VertexColor.mRed),
																uint8(255.f*VertexColor.mGreen),
																uint8(255.f*VertexColor.mBlue),
																uint8(255.f*VertexColor.mAlpha));
					}
				}
				break;
			case FbxLayerElement::eByPolygonVertex:
				{	
					for (int32 VertexIndex=0;VertexIndex<3;VertexIndex++)
					{
						int32 UnrealVertexIndex = OddNegativeScale ? 2 - VertexIndex : VertexIndex;

						FbxColor VertexColor = (VertexColorReferenceMode == FbxLayerElement::eDirect)
							?	LayerElementVertexColor->GetDirectArray().GetAt(LocalIndex*3+VertexIndex)
							:	LayerElementVertexColor->GetDirectArray().GetAt(LayerElementVertexColor->GetIndexArray().GetAt(LocalIndex*3+VertexIndex));

						TmpWedges[UnrealVertexIndex].Color =	FColor(	uint8(255.f*VertexColor.mRed),
																uint8(255.f*VertexColor.mGreen),
																uint8(255.f*VertexColor.mBlue),
																uint8(255.f*VertexColor.mAlpha));
					}
				}
				break;
			}
		}
		
		//
		// basic wedges matching : 3 unique per face. TODO Can we do better ?
		//
		for (int32 VertexIndex=0; VertexIndex<3; VertexIndex++)
		{
			int32 w;
			
			w = ImportData.Wedges.AddUninitialized();
			ImportData.Wedges[w].VertexIndex = TmpWedges[VertexIndex].VertexIndex;
			ImportData.Wedges[w].MatIndex = TmpWedges[VertexIndex].MatIndex;
			ImportData.Wedges[w].Color = TmpWedges[VertexIndex].Color;
			ImportData.Wedges[w].Reserved = 0;
			FMemory::Memcpy( ImportData.Wedges[w].UVs, TmpWedges[VertexIndex].UVs, sizeof(FVector2D)*MAX_TEXCOORDS );
			
			Triangle.WedgeIndex[VertexIndex] = w;
		}
		
	}
	
	// now we can work on a per-cluster basis with good ordering
	if (Skin) // skeletal mesh
	{
		// create influences for each cluster
		int32 ClusterIndex;
		for (ClusterIndex=0; ClusterIndex<Skin->GetClusterCount(); ClusterIndex++)
		{
			FbxCluster* Cluster = Skin->GetCluster(ClusterIndex);
			// When Maya plug-in exports rigid binding, it will generate "CompensationCluster" for each ancestor links.
			// FBX writes these "CompensationCluster" out. The CompensationCluster also has weight 1 for vertices.
			// Unreal importer should skip these clusters.
			if(Cluster && FCStringAnsi::Strcmp(Cluster->GetUserDataID(), "Maya_ClusterHint") == 0 && FCStringAnsi::Strcmp(Cluster->GetUserData(), "CompensationCluster")  == 0)
			{
				continue;
			}
			
			FbxNode* Link = Cluster->GetLink();
			// find the bone index
			int32 BoneIndex = -1;
			for (int32 LinkIndex = 0; LinkIndex < SortedLinks.Num(); LinkIndex++)
			{
				if (Link == SortedLinks[LinkIndex])
				{
					BoneIndex = LinkIndex;
					break;
				}
			}

			//	get the vertex indices
			int32 ControlPointIndicesCount = Cluster->GetControlPointIndicesCount();
			int32* ControlPointIndices = Cluster->GetControlPointIndices();
			double* Weights = Cluster->GetControlPointWeights();

			//	for each vertex index in the cluster
			for (int32 ControlPointIndex = 0; ControlPointIndex < ControlPointIndicesCount; ++ControlPointIndex) 
			{
				ImportData.Influences.AddUninitialized();
				ImportData.Influences.Last().BoneIndex = BoneIndex;
				ImportData.Influences.Last().Weight = static_cast<float>(Weights[ControlPointIndex]);
				ImportData.Influences.Last().VertexIndex = ExistPointNum + ControlPointIndices[ControlPointIndex];
			}
		}
	}
	else // for rigid mesh
	{
		// find the bone index
		int32 BoneIndex = -1;
		for (int32 LinkIndex = 0; LinkIndex < SortedLinks.Num(); LinkIndex++)
		{
			// the bone is the node itself
			if (Node == SortedLinks[LinkIndex])
			{
				BoneIndex = LinkIndex;
				break;
			}
		}
		
		//	for each vertex in the mesh
		for (int32 ControlPointIndex = 0; ControlPointIndex < ControlPointsCount; ++ControlPointIndex) 
		{
			ImportData.Influences.AddUninitialized();
			ImportData.Influences.Last().BoneIndex = BoneIndex;
			ImportData.Influences.Last().Weight = 1.0;
			ImportData.Influences.Last().VertexIndex = ExistPointNum + ControlPointIndex;
		}
	}

	//
	// clean up
	//
	if (UniqueUVCount > 0)
	{
		delete[] LayerElementUV;
		delete[] UVReferenceMode;
		delete[] UVMappingMode;
	}
	
	return true;
}

void UnFbx::FFbxImporter::InsertNewLODToBaseSkeletalMesh(USkeletalMesh* InSkeletalMesh, USkeletalMesh* BaseSkeletalMesh, int32 DesiredLOD)
{
	FSkeletalMeshResource* ImportedResource = InSkeletalMesh->GetImportedResource();
	FSkeletalMeshResource* DestImportedResource = BaseSkeletalMesh->GetImportedResource();

	FStaticLODModel& NewLODModel = ImportedResource->LODModels[0];

	// If we want to add this as a new LOD to this mesh - add to LODModels/LODInfo array.
	if (DesiredLOD == DestImportedResource->LODModels.Num())
	{
		new(DestImportedResource->LODModels)FStaticLODModel();

		// Add element to LODInfo array.
		BaseSkeletalMesh->LODInfo.AddZeroed();
		check(BaseSkeletalMesh->LODInfo.Num() == DestImportedResource->LODModels.Num());
		BaseSkeletalMesh->LODInfo[DesiredLOD] = InSkeletalMesh->LODInfo[0];
	}
	else
	{
		// if it's overwriting existing LOD, need to update section information
		// update to the right # of sections 
		// Set up LODMaterialMap to number of materials in new mesh.
		// ImportedResource->LOD 0 is the newly imported mesh
		FSkeletalMeshLODInfo& LODInfo = BaseSkeletalMesh->LODInfo[DesiredLOD];
		// if section # has been changed
		if (LODInfo.TriangleSortSettings.Num() != NewLODModel.Sections.Num())
		{
			// Save old information so that I can copy it over
			TArray<FTriangleSortSettings> OldTriangleSortSettings;

			OldTriangleSortSettings = LODInfo.TriangleSortSettings;

			// resize to the correct number
			LODInfo.TriangleSortSettings.Empty(NewLODModel.Sections.Num());
			// fill up data
			for (int32 SectionIndex = 0; SectionIndex < NewLODModel.Sections.Num(); ++SectionIndex)
			{
				// if found from previous data, copy over
				if (SectionIndex < OldTriangleSortSettings.Num())
				{
					LODInfo.TriangleSortSettings.Add(OldTriangleSortSettings[SectionIndex]);
				}
				else
				{
					// if not add default data
					LODInfo.TriangleSortSettings.AddZeroed();
				}
			}
		}
	}

	// Set up LODMaterialMap to number of materials in new mesh.
	FSkeletalMeshLODInfo& LODInfo = BaseSkeletalMesh->LODInfo[DesiredLOD];
	LODInfo.LODMaterialMap.Empty();

	// Now set up the material mapping array.
	for (int32 MatIdx = 0; MatIdx < InSkeletalMesh->Materials.Num(); MatIdx++)
	{
		// Try and find the auto-assigned material in the array.
		int32 LODMatIndex = INDEX_NONE;
		if (InSkeletalMesh->Materials[MatIdx].MaterialInterface != NULL)
		{
			LODMatIndex = BaseSkeletalMesh->Materials.Find(InSkeletalMesh->Materials[MatIdx]);
		}

		// If we didn't just use the index - but make sure its within range of the Materials array.
		if (LODMatIndex == INDEX_NONE)
		{
			LODMatIndex = FMath::Clamp(MatIdx, 0, BaseSkeletalMesh->Materials.Num() - 1);
		}

		LODInfo.LODMaterialMap.Add(LODMatIndex);
	}

	// if new LOD has more material slot, add the extra to main skeletal
	if (BaseSkeletalMesh->Materials.Num() < InSkeletalMesh->Materials.Num())
	{
		BaseSkeletalMesh->Materials.AddZeroed(InSkeletalMesh->Materials.Num() - BaseSkeletalMesh->Materials.Num());
	}

	// same from here as FbxImporter

	// Release all resources before replacing the model
	BaseSkeletalMesh->PreEditChange(NULL);

	// Index buffer will be destroyed when we copy the LOD model so we must copy the index buffer and reinitialize it after the copy
	FMultiSizeIndexContainerData Data;
	NewLODModel.MultiSizeIndexContainer.GetIndexBufferData(Data);

	// Assign new FStaticLODModel to desired slot in selected skeletal mesh.
	DestImportedResource->LODModels[DesiredLOD] = NewLODModel;

	DestImportedResource->LODModels[DesiredLOD].MultiSizeIndexContainer.RebuildIndexBuffer(Data);

	// rebuild vertex buffers and reinit RHI resources
	BaseSkeletalMesh->PostEditChange();
}

bool UnFbx::FFbxImporter::ImportSkeletalMeshLOD(USkeletalMesh* InSkeletalMesh, USkeletalMesh* BaseSkeletalMesh, int32 DesiredLOD, bool bNeedToReregister, TArray<UActorComponent*>* ReregisterAssociatedComponents)
{
	check(InSkeletalMesh);
	check(BaseSkeletalMesh);

	FSkeletalMeshResource* ImportedResource = InSkeletalMesh->GetImportedResource();
	FSkeletalMeshResource* DestImportedResource = BaseSkeletalMesh->GetImportedResource();

	// Now we copy the base FStaticLODModel from the imported skeletal mesh as the new LOD in the selected mesh.
	check(ImportedResource->LODModels.Num() == 1);

	// Names of root bones must match.
	// If the names of root bones don't match, the LOD Mesh does not share skeleton with base Mesh. 
	if (InSkeletalMesh->RefSkeleton.GetBoneName(0) != BaseSkeletalMesh->RefSkeleton.GetBoneName(0))
	{
		AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, FText::Format(LOCTEXT("LODRootNameIncorrect", "Root bone in LOD is '{0}' instead of '{1}'.\nImport failed."),
			FText::FromName(InSkeletalMesh->RefSkeleton.GetBoneName(0)), FText::FromName(BaseSkeletalMesh->RefSkeleton.GetBoneName(0)))), FFbxErrors::SkeletalMesh_LOD_RootNameIncorrect);

		return false;
	}

	// We do some checking here that for every bone in the mesh we just imported, it's in our base ref skeleton, and the parent is the same.
	for (int32 i = 0; i < InSkeletalMesh->RefSkeleton.GetNum(); i++)
	{
		int32 LODBoneIndex = i;
		FName LODBoneName = InSkeletalMesh->RefSkeleton.GetBoneName(LODBoneIndex);
		int32 BaseBoneIndex = BaseSkeletalMesh->RefSkeleton.FindBoneIndex(LODBoneName);
		if (BaseBoneIndex == INDEX_NONE)
		{
			// If we could not find the bone from this LOD in base mesh - we fail.
			AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, FText::Format(LOCTEXT("LODBoneDoesNotMatch", "Bone '{0}' not found in base SkeletalMesh '{1}'.\nImport failed."),
				FText::FromName(LODBoneName), FText::FromString(BaseSkeletalMesh->GetName()))), FFbxErrors::SkeletalMesh_LOD_BonesDoNotMatch);

			return false;
		}

		if (i > 0)
		{
			int32 LODParentIndex = InSkeletalMesh->RefSkeleton.GetParentIndex(LODBoneIndex);
			FName LODParentName = InSkeletalMesh->RefSkeleton.GetBoneName(LODParentIndex);

			int32 BaseParentIndex = BaseSkeletalMesh->RefSkeleton.GetParentIndex(BaseBoneIndex);
			FName BaseParentName = BaseSkeletalMesh->RefSkeleton.GetBoneName(BaseParentIndex);

			if (LODParentName != BaseParentName)
			{
				// If bone has different parents, display an error and don't allow import.
				AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, FText::Format(LOCTEXT("LODBoneHasIncorrectParent", "Bone '{0}' in LOD has parent '{1}' instead of '{2}'"),
					FText::FromName(LODBoneName), FText::FromName(LODParentName), FText::FromName(BaseParentName))), FFbxErrors::SkeletalMesh_LOD_IncorrectParent);

				return false;
			}
		}
	}

	FStaticLODModel& NewLODModel = ImportedResource->LODModels[0];

	// Enforce LODs having only single-influence vertices.
	bool bCheckSingleInfluence;
	GConfig->GetBool(TEXT("AnimSetViewer"), TEXT("CheckSingleInfluenceLOD"), bCheckSingleInfluence, GEditorIni);
	if (bCheckSingleInfluence &&
		DesiredLOD > 0)
	{
		for (int32 ChunkIndex = 0; ChunkIndex < NewLODModel.Chunks.Num(); ChunkIndex++)
		{
			if (NewLODModel.Chunks[ChunkIndex].SoftVertices.Num() > 0)
			{
				AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText(LOCTEXT("LODHasSoftVertices", "Warning: The mesh LOD you are importing has some vertices with more than one influence."))), FFbxErrors::SkeletalMesh_LOD_HasSoftVerts);
			}
		}
	}

	// If this LOD is going to be the lowest one, we check all bones we have sockets on are present in it.
	if (DesiredLOD == DestImportedResource->LODModels.Num() ||
		DesiredLOD == DestImportedResource->LODModels.Num() - 1)
	{
		const TArray<USkeletalMeshSocket*>& Sockets = BaseSkeletalMesh->GetMeshOnlySocketList();

		for (int32 i = 0; i < Sockets.Num(); i++)
		{
			// Find bone index the socket is attached to.
			USkeletalMeshSocket* Socket = Sockets[i];
			int32 SocketBoneIndex = InSkeletalMesh->RefSkeleton.FindBoneIndex(Socket->BoneName);

			// If this LOD does not contain the socket bone, abort import.
			if (SocketBoneIndex == INDEX_NONE)
			{
				AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, FText::Format(LOCTEXT("LODMissingSocketBone", "This LOD is missing bone '{0}' used by socket '{1}'.\nAborting import."),
					FText::FromName(Socket->BoneName), FText::FromName(Socket->SocketName))), FFbxErrors::SkeletalMesh_LOD_MissingSocketBone);

				return false;
			}
		}
	}

	// Fix up the ActiveBoneIndices array.
	for (int32 i = 0; i < NewLODModel.ActiveBoneIndices.Num(); i++)
	{
		int32 LODBoneIndex = NewLODModel.ActiveBoneIndices[i];
		FName LODBoneName = InSkeletalMesh->RefSkeleton.GetBoneName(LODBoneIndex);
		int32 BaseBoneIndex = BaseSkeletalMesh->RefSkeleton.FindBoneIndex(LODBoneName);
		NewLODModel.ActiveBoneIndices[i] = BaseBoneIndex;
	}

	// Fix up the chunk BoneMaps.
	for (int32 ChunkIndex = 0; ChunkIndex < NewLODModel.Chunks.Num(); ChunkIndex++)
	{
		FSkelMeshChunk& Chunk = NewLODModel.Chunks[ChunkIndex];
		for (int32 i = 0; i < Chunk.BoneMap.Num(); i++)
		{
			int32 LODBoneIndex = Chunk.BoneMap[i];
			FName LODBoneName = InSkeletalMesh->RefSkeleton.GetBoneName(LODBoneIndex);
			int32 BaseBoneIndex = BaseSkeletalMesh->RefSkeleton.FindBoneIndex(LODBoneName);
			Chunk.BoneMap[i] = BaseBoneIndex;
		}
	}

	// Create the RequiredBones array in the LODModel from the ref skeleton.
	for (int32 i = 0; i < NewLODModel.RequiredBones.Num(); i++)
	{
		FName LODBoneName = InSkeletalMesh->RefSkeleton.GetBoneName(NewLODModel.RequiredBones[i]);
		int32 BaseBoneIndex = BaseSkeletalMesh->RefSkeleton.FindBoneIndex(LODBoneName);
		if (BaseBoneIndex != INDEX_NONE)
		{
			NewLODModel.RequiredBones[i] = BaseBoneIndex;
		}
		else
		{
			NewLODModel.RequiredBones.RemoveAt(i--);
		}
	}

	// Also sort the RequiredBones array to be strictly increasing.
	NewLODModel.RequiredBones.Sort();
	NewLODModel.ActiveBoneIndices.Sort();

	// To be extra-nice, we apply the difference between the root transform of the meshes to the verts.
	FMatrix LODToBaseTransform = InSkeletalMesh->GetRefPoseMatrix(0).InverseFast() * BaseSkeletalMesh->GetRefPoseMatrix(0);

	for (int32 ChunkIndex = 0; ChunkIndex < NewLODModel.Chunks.Num(); ChunkIndex++)
	{
		FSkelMeshChunk& Chunk = NewLODModel.Chunks[ChunkIndex];
		// Fix up rigid verts.
		for (int32 i = 0; i < Chunk.RigidVertices.Num(); i++)
		{
			Chunk.RigidVertices[i].Position = LODToBaseTransform.TransformPosition(Chunk.RigidVertices[i].Position);
			Chunk.RigidVertices[i].TangentX = LODToBaseTransform.TransformVector(Chunk.RigidVertices[i].TangentX);
			Chunk.RigidVertices[i].TangentY = LODToBaseTransform.TransformVector(Chunk.RigidVertices[i].TangentY);
			Chunk.RigidVertices[i].TangentZ = LODToBaseTransform.TransformVector(Chunk.RigidVertices[i].TangentZ);
		}

		// Fix up soft verts.
		for (int32 i = 0; i < Chunk.SoftVertices.Num(); i++)
		{
			Chunk.SoftVertices[i].Position = LODToBaseTransform.TransformPosition(Chunk.SoftVertices[i].Position);
			Chunk.SoftVertices[i].TangentX = LODToBaseTransform.TransformVector(Chunk.SoftVertices[i].TangentX);
			Chunk.SoftVertices[i].TangentY = LODToBaseTransform.TransformVector(Chunk.SoftVertices[i].TangentY);
			Chunk.SoftVertices[i].TangentZ = LODToBaseTransform.TransformVector(Chunk.SoftVertices[i].TangentZ);
		}
	}

	if (bNeedToReregister)
	{
		// Shut down the skeletal mesh component that is previewing this mesh.
		if (ReregisterAssociatedComponents)
		{
			FMultiComponentReregisterContext ReregisterContext(*ReregisterAssociatedComponents);
			// wait until resources are released
			FlushRenderingCommands();

			InsertNewLODToBaseSkeletalMesh(InSkeletalMesh, BaseSkeletalMesh, DesiredLOD);

			// ReregisterContexts go out of scope here, reregistering associated components with the scene.
		}
		else
		{
			TComponentReregisterContext<USkinnedMeshComponent> ComponentReregisterContext;

			InsertNewLODToBaseSkeletalMesh(InSkeletalMesh, BaseSkeletalMesh, DesiredLOD);

			// ReregisterContexts go out of scope here, reregistering skel components with the scene.
		}
	}
	else
	{
		InsertNewLODToBaseSkeletalMesh(InSkeletalMesh, BaseSkeletalMesh, DesiredLOD);
	}

	return true;
}

/**
 * A class encapsulating morph target processing that occurs during import on a separate thread
 */
class FAsyncImportMorphTargetWork : public FNonAbandonableTask
{
public:
	FAsyncImportMorphTargetWork( USkeletalMesh* InTempSkelMesh, int32 InLODIndex, FSkeletalMeshImportData& InImportData, bool bInKeepOverlappingVertices )
		: TempSkeletalMesh(InTempSkelMesh)
		, ImportData(InImportData)
		, LODIndex(InLODIndex)
		, bKeepOverlappingVertices(bInKeepOverlappingVertices)
	{
		MeshUtilities = &FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");
	}

	void DoWork()
	{
		TArray<FVector> LODPoints;
		TArray<FMeshWedge> LODWedges;
		TArray<FMeshFace> LODFaces;
		TArray<FVertInfluence> LODInfluences;
		TArray<int32> LODPointToRawMap;
		ImportData.CopyLODImportData(LODPoints,LODWedges,LODFaces,LODInfluences,LODPointToRawMap);
	
		ImportData.Empty();
		MeshUtilities->BuildSkeletalMesh( TempSkeletalMesh->GetImportedResource()->LODModels[0], TempSkeletalMesh->RefSkeleton, LODInfluences, LODWedges, LODFaces, LODPoints, LODPointToRawMap, bKeepOverlappingVertices);
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncImportMorphTargetWork, STATGROUP_ThreadPoolAsyncTasks);
	}

private:
	USkeletalMesh* TempSkeletalMesh;
	FSkeletalMeshImportData ImportData;
	IMeshUtilities* MeshUtilities;
	int32 LODIndex;
	bool bKeepOverlappingVertices;
};

void UnFbx::FFbxImporter::ImportMorphTargetsInternal( TArray<FbxNode*>& SkelMeshNodeArray, USkeletalMesh* BaseSkelMesh, UObject* InParent, const FString& InFilename, int32 LODIndex )
{
	FbxString ShapeNodeName;
	TMap<FString, TArray<FbxShape*>> ShapeNameToShapeArray;

	// Temp arrays to keep track of data being used by threads
	TArray<USkeletalMesh*> TempMeshes;
	TArray<UMorphTarget*> MorphTargets;

	// Array of pending tasks that are not complete
	TIndirectArray<FAsyncTask<FAsyncImportMorphTargetWork> > PendingWork;

	GWarn->BeginSlowTask( NSLOCTEXT("FbxImporter", "BeginGeneratingMorphModelsTask", "Generating Morph Models"), true);

	// For each morph in FBX geometries, we create one morph target for the Unreal skeletal mesh
	for (int32 NodeIndex = 0; NodeIndex < SkelMeshNodeArray.Num(); NodeIndex++)
	{
		FbxGeometry* Geometry = (FbxGeometry*)SkelMeshNodeArray[NodeIndex]->GetNodeAttribute();
		if (Geometry)
		{
			const int32 BlendShapeDeformerCount = Geometry->GetDeformerCount(FbxDeformer::eBlendShape);

			/************************************************************************/
			/* collect all the shapes                                               */
			/************************************************************************/
			for(int32 BlendShapeIndex = 0; BlendShapeIndex<BlendShapeDeformerCount; ++BlendShapeIndex)
			{
				FbxBlendShape* BlendShape = (FbxBlendShape*)Geometry->GetDeformer(BlendShapeIndex, FbxDeformer::eBlendShape);
				const int32 BlendShapeChannelCount = BlendShape->GetBlendShapeChannelCount();

				FString BlendShapeName = UTF8_TO_TCHAR(MakeName(BlendShape->GetName()));

				// see below where this is used for explanation...
				const bool bMightBeBadMAXFile = (BlendShapeName == FString("Morpher"));

				for(int32 ChannelIndex = 0; ChannelIndex<BlendShapeChannelCount; ++ChannelIndex)
				{
					FbxBlendShapeChannel* Channel = BlendShape->GetBlendShapeChannel(ChannelIndex);
					if(Channel)
					{
						//Find which shape should we use according to the weight.
						const int32 CurrentChannelShapeCount = Channel->GetTargetShapeCount();
						
						FString ChannelName = UTF8_TO_TCHAR(MakeName(Channel->GetName()));

						// Maya adds the name of the blendshape and an underscore to the front of the channel name, so remove it
						if(ChannelName.StartsWith(BlendShapeName))
						{
							ChannelName = ChannelName.Right(ChannelName.Len() - (BlendShapeName.Len()+1));
						}

						for(int32 ShapeIndex = 0; ShapeIndex<CurrentChannelShapeCount; ++ShapeIndex)
						{
							FbxShape* Shape = Channel->GetTargetShape(ShapeIndex);
			
							FString ShapeName;
							if( CurrentChannelShapeCount > 1 )
							{
								ShapeName = UTF8_TO_TCHAR(MakeName(Shape->GetName() ) );
							}
							else
							{
								if (bMightBeBadMAXFile)
								{
									ShapeName = UTF8_TO_TCHAR(MakeName(Shape->GetName()));
								}
								else
								{
									// Maya concatenates the number of the shape to the end of its name, so instead use the name of the channel
									ShapeName = ChannelName;
								}
							}

							TArray<FbxShape*> & ShapeArray = ShapeNameToShapeArray.FindOrAdd(ShapeName);
							if (ShapeArray.Num() == 0)
							{
								ShapeArray.AddZeroed(SkelMeshNodeArray.Num());
							}

							ShapeArray[NodeIndex] = Shape;
						}
					}
				}
			}
		}
	} // for NodeIndex

	int32 ShapeIndex = 0;
	int32 TotalShapeCount = ShapeNameToShapeArray.Num();
	// iterate through shapename, and create morphtarget
	for (auto Iter = ShapeNameToShapeArray.CreateIterator(); Iter; ++Iter)
	{
		FString ShapeName = Iter.Key();
		TArray<FbxShape*> & ShapeArray = Iter.Value();

		FFormatNamedArguments Args;
		Args.Add(TEXT("ShapeName"), FText::FromString(ShapeName));
		Args.Add(TEXT("CurrentShapeIndex"), ShapeIndex + 1);
		Args.Add(TEXT("TotalShapes"), TotalShapeCount);
		const FText StatusUpate = FText::Format(NSLOCTEXT("FbxImporter", "GeneratingMorphTargetMeshStatus", "Generating morph target mesh {ShapeName} ({CurrentShapeIndex} of {TotalShapes})"), Args);

		GWarn->StatusUpdate(ShapeIndex + 1, TotalShapeCount, StatusUpate);

		FSkeletalMeshImportData ImportData;

		// See if this morph target already exists.
		UMorphTarget * Result = FindObject<UMorphTarget>(BaseSkelMesh, *ShapeName);
		// we only create new one for LOD0, otherwise don't create new one
		if (!Result)
		{
			if (LODIndex == 0)
			{
				Result = NewObject<UMorphTarget>(BaseSkelMesh, FName(*ShapeName));
			}
			else
			{
				AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, FText::Format(FText::FromString("Could not find the {0} morphtarget for LOD {1}. \
																																																																																											Make sure the name for morphtarget matches with LOD 0"), FText::FromString(ShapeName), FText::FromString(FString::FromInt(LODIndex)))),
																																																																																											FFbxErrors::SkeletalMesh_LOD_MissingMorphTarget);
			}
		}

		if (Result)
		{
			// now we get a shape for whole mesh, import to unreal as a morph target
			// @todo AssetImportData do we need import data for this temp mesh?
			UFbxSkeletalMeshImportData* TmpMeshImportData = NULL;
			USkeletalMesh* TmpSkeletalMesh = (USkeletalMesh*)ImportSkeletalMesh(GetTransientPackage(), SkelMeshNodeArray, NAME_None, (EObjectFlags)0, TmpMeshImportData, FPaths::GetBaseFilename(InFilename), &ShapeArray, &ImportData, false);
			TempMeshes.Add(TmpSkeletalMesh);
			MorphTargets.Add(Result);

			// Process the skeletal mesh on a separate thread
			FAsyncTask<FAsyncImportMorphTargetWork>* NewWork = new (PendingWork)FAsyncTask<FAsyncImportMorphTargetWork>(TmpSkeletalMesh, LODIndex, ImportData, ImportOptions->bKeepOverlappingVertices);
			NewWork->StartBackgroundTask();
			++ShapeIndex;
		}
	}

	// Wait for all importing tasks to complete
	int32 NumCompleted = 0;
	int32 NumTasks = PendingWork.Num();
	do
	{
		// Check for completed async compression tasks.
		int32 NumNowCompleted = 0;
		for ( int32 TaskIndex=0; TaskIndex < PendingWork.Num(); ++TaskIndex )
		{
			if ( PendingWork[TaskIndex].IsDone() )
			{
				NumNowCompleted++;
			}
		}
		if (NumNowCompleted > NumCompleted)
		{
			NumCompleted = NumNowCompleted;
			FFormatNamedArguments Args;
			Args.Add( TEXT("NumCompleted"), NumCompleted );
			Args.Add( TEXT("NumTasks"), NumTasks );
			GWarn->StatusUpdate( NumCompleted, NumTasks, FText::Format( LOCTEXT("ImportingMorphTargetStatus", "Importing Morph Target: {NumCompleted} of {NumTasks}"), Args ) );
		}
		FPlatformProcess::Sleep(0.1f);
	} while ( NumCompleted < NumTasks );

	// Create morph streams for each morph target we are importing.
	// This has to happen on a single thread since the skeletal meshes' bulk data is locked and cant be accessed by multiple threads simultaneously
	for (int32 Index = 0; Index < TempMeshes.Num(); Index++)
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("NumCompleted"), Index+1 );
		Args.Add( TEXT("NumTasks"), TempMeshes.Num() );
		GWarn->StatusUpdate( Index+1, TempMeshes.Num(), FText::Format( LOCTEXT("BuildingMorphTargetRenderDataStatus", "Building Morph Target Render Data: {NumCompleted} of {NumTasks}"), Args ) );

		UMorphTarget* MorphTarget = MorphTargets[Index];
		USkeletalMesh* TmpSkeletalMesh = TempMeshes[ Index ];

		FMorphMeshRawSource TargetMeshRawData( TmpSkeletalMesh );
		FMorphMeshRawSource BaseMeshRawData( BaseSkelMesh, LODIndex );

		MorphTarget->PostProcess( BaseSkelMesh, BaseMeshRawData, TargetMeshRawData, LODIndex, ImportOptions->ShouldImportNormals() == false );
	}

	GWarn->EndSlowTask();
}	

// Import Morph target
void UnFbx::FFbxImporter::ImportFbxMorphTarget(TArray<FbxNode*> &SkelMeshNodeArray, USkeletalMesh* BaseSkelMesh, UObject* InParent, const FString& Filename, int32 LODIndex)
{
	bool bHasMorph = false;
	int32 NodeIndex;
	// check if there are morph in this geometry
	for (NodeIndex = 0; NodeIndex < SkelMeshNodeArray.Num(); NodeIndex++)
	{
		FbxGeometry* Geometry = (FbxGeometry*)SkelMeshNodeArray[NodeIndex]->GetNodeAttribute();
		if (Geometry)
		{
			bHasMorph = Geometry->GetDeformerCount(FbxDeformer::eBlendShape) > 0;
			if (bHasMorph)
			{
				break;
			}
		}
	}
	
	if (bHasMorph)
	{
		ImportMorphTargetsInternal( SkelMeshNodeArray, BaseSkelMesh, InParent, Filename, LODIndex );
	}
}

void UnFbx::FFbxImporter::AddTokenizedErrorMessage(TSharedRef<FTokenizedMessage> Error, FName FbxErrorName)
{
	// check to see if Logger exists, this way, we guarantee only prints to FBX import
	// when we meant to print
	if (Logger)
	{
		Logger->TokenizedErrorMessages.Add(Error);

		if(FbxErrorName != NAME_None)
		{
			Error->AddToken(FFbxErrorToken::Create(FbxErrorName));
		}
	}
	else
	{
		// if not found, use normal log
		UE_LOG(LogFbx, Warning, TEXT("%s"), *(Error->ToText().ToString()));
	}
}

void UnFbx::FFbxImporter::ClearTokenizedErrorMessages()
{
	if(Logger)
	{
		Logger->TokenizedErrorMessages.Empty();
	}
}

void UnFbx::FFbxImporter::FlushToTokenizedErrorMessage(EMessageSeverity::Type Severity)
{
	if (!ErrorMessage.IsEmpty())
	{
		AddTokenizedErrorMessage(FTokenizedMessage::Create(Severity, FText::Format(FText::FromString("{0}"), FText::FromString(ErrorMessage))), NAME_None);
	}
}

void UnFbx::FFbxImporter::SetLogger(class FFbxLogger * InLogger)
{
	// this should be only called by top level functions
	// if you set it you can't set it again. Otherwise, you'll lose all log information
	check(Logger == NULL);
	Logger = InLogger;
}

// just in case if DeleteScene/CleanUp is getting called too late
void UnFbx::FFbxImporter::ClearLogger()
{
	Logger = NULL;
}

FFbxLogger::FFbxLogger()
{
}

FFbxLogger::~FFbxLogger()
{
	if(TokenizedErrorMessages.Num() > 0)
	{
		const TCHAR* LogTitle = TEXT("FBXImport");
		FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
		TSharedPtr<class IMessageLogListing> LogListing = MessageLogModule.GetLogListing(LogTitle);
		LogListing->SetLabel(FText::FromString("FBX Import"));
		LogListing->ClearMessages();

		LogListing->AddMessages(TokenizedErrorMessages);
		MessageLogModule.OpenMessageLog(LogTitle);
	}
}
#undef LOCTEXT_NAMESPACE
