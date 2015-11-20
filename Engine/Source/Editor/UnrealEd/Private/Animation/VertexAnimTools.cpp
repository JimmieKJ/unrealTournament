// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "FbxImporter.h"
#include "FbxExporter.h"
#include "VertexAnimTools.h"
#include "Animation/VertexAnim/VertexAnimation.h"

/** Util to find all meshes with a vertex cache deformer */
void RecursiveFindVertexAnimMeshes(FbxNode* Node, TArray<FbxMesh*>& OutVertexCacheMeshes)
{
	if (Node->GetMesh() && Node->GetMesh()->GetDeformerCount(FbxDeformer::eVertexCache) > 0 )
	{
		OutVertexCacheMeshes.Add(Node->GetMesh());
	}

	for(int32 ChildIdx=0; ChildIdx<Node->GetChildCount(); ++ChildIdx)
	{
		RecursiveFindVertexAnimMeshes(Node->GetChild(ChildIdx), OutVertexCacheMeshes);
	}
}

/** Invert mapping - find final verts that  */
void CreateImportToFinalMap(const TArray<int32>& FinalToImportMap, int32 NumImportVerts, TArray< TArray<int32> >& OutImportToFinal)
{
	OutImportToFinal.Empty();
	OutImportToFinal.AddZeroed(NumImportVerts);

	for(int32 FinalVertIdx=0; FinalVertIdx<FinalToImportMap.Num(); FinalVertIdx++)
	{
		int32 ImportVert = FinalToImportMap[FinalVertIdx];
		OutImportToFinal[ImportVert].Add(FinalVertIdx);
	}
}

/** Util to find mesh position for this vertex, so we can calculate a delta */
FVector GetVertexPosition(const FStaticLODModel& Model, int32 VertexIndex)
{
	int32 ChunkIndex;
	int32 VertIndex;
	bool bSoftVertex;
	bool bHasExtraInfluences;
	Model.GetChunkAndSkinType(VertexIndex, ChunkIndex, VertIndex, bSoftVertex, bHasExtraInfluences);

	check(ChunkIndex < Model.Chunks.Num());
	const FSkelMeshChunk& Chunk = Model.Chunks[ChunkIndex];

	// Do soft skinning for this vertex.
	FVector VertexPos;
	if(bSoftVertex)
	{
		if (bHasExtraInfluences)
		{
			const auto* SrcSoftVertex = Model.VertexBufferGPUSkin.GetVertexPtr<true>(Chunk.GetSoftVertexBufferIndex()+VertIndex);
			VertexPos = Model.VertexBufferGPUSkin.GetVertexPositionFast(SrcSoftVertex);
		}
		else
		{
			const auto* SrcSoftVertex = Model.VertexBufferGPUSkin.GetVertexPtr<false>(Chunk.GetSoftVertexBufferIndex()+VertIndex);
			VertexPos = Model.VertexBufferGPUSkin.GetVertexPositionFast(SrcSoftVertex);
		}
	}
	// Do rigid (one-influence) skinning for this vertex.
	else
	{
		const auto* SrcRigidVertex = Model.VertexBufferGPUSkin.GetVertexPtr<false>(Chunk.GetRigidVertexBufferIndex()+VertIndex);
		VertexPos = Model.VertexBufferGPUSkin.GetVertexPositionFast(SrcRigidVertex);
	}

//@todo-rco
check(VertexPos == Model.VertexBufferGPUSkin.GetVertexPositionSlow(Chunk.GetRigidVertexBufferIndex()+VertIndex));
	return VertexPos;
}

/** Actually extract data from vertex animation into UVertexAnimation object */
void ConvertCacheToAnim(FbxCache* Cache, int ChannelIndex, UVertexAnimation* NewVertexAnim, const FbxTime& StartTime, const FbxTime& DeltaTime, int32 NumFrames)
{
	check(NewVertexAnim->BaseSkelMesh != NULL);
	check(NewVertexAnim->NumAnimatedVerts > 0);

	// Allocate buffer to read in to
	TArray<double> ReadBuffer;
	ReadBuffer.AddUninitialized(NewVertexAnim->NumAnimatedVerts*3);

	// Pre-size frame array to avoid re-allocations (costly!)
	NewVertexAnim->VertexAnimData.AddZeroed(NumFrames);

	// Build map from animated vert to final skel mesh verts
	FStaticLODModel& Model = NewVertexAnim->BaseSkelMesh->GetImportedResource()->LODModels[0];
	TArray< TArray<int32> > ImportToFinal;
	CreateImportToFinalMap(Model.MeshToImportVertexMap, Model.MaxImportVertex+1, ImportToFinal);

	FbxTime CurrentTime = StartTime;
	for(int32 FrameNum=0; FrameNum<NumFrames; FrameNum++)
	{
		bool bReadOk = Cache->Read(ChannelIndex, CurrentTime, ReadBuffer.GetData(), NewVertexAnim->NumAnimatedVerts);
		check(bReadOk);

		FVertexAnimFrame& Frame = NewVertexAnim->VertexAnimData[FrameNum];
		Frame.Time = (float)((CurrentTime - StartTime).GetSecondDouble());
		Frame.Deltas.AddUninitialized(NewVertexAnim->NumAnimatedVerts);

		for(int32 PointIdx=0; PointIdx<NewVertexAnim->NumAnimatedVerts; PointIdx++)
		{
			// Find mesh position for this vertex so we can convert to a delta
			TArray<int32>& FinalVerts = ImportToFinal[PointIdx];
			check(FinalVerts.Num() > 0); // Should always have one final vert for every import vert...
			int32 FinalVertIndex = FinalVerts[0];
			const FVector MeshVertexPos = GetVertexPosition(Model, FinalVertIndex);

			FVector AnimVertexPos;
			AnimVertexPos.X = ReadBuffer[(PointIdx*3)+0];
			AnimVertexPos.Y = ReadBuffer[(PointIdx*3)+1] * -1.f; // flip Y, the importer does this to the mesh, so we do it here as well
			AnimVertexPos.Z = ReadBuffer[(PointIdx*3)+2];

			// Create delta struct
			Frame.Deltas[PointIdx] = (AnimVertexPos - MeshVertexPos);
		}

		CurrentTime += DeltaTime;
	}
}

/** Util to get info about vertex anim - start and delta time, number of frames and number of points */
bool GetVertexAnimInfo(FbxCache* Cache, int ChannelIndex, FbxTime& StartTime, FbxTime& DeltaTime, int32& NumFrames, int32& NumAnimatedPoints)
{
	// Get the timing for the animation
	FbxTime EndTime;
	Cache->GetAnimationRange(ChannelIndex, StartTime, EndTime);
	DeltaTime = Cache->GetCacheTimePerFrame();

	// Have an initial look through the frames...
	// First iterate to count the frames
	NumAnimatedPoints = -1;
	bool bNumPointsOk = true;
	NumFrames = 0;
	FbxTime CurrentTime = StartTime;
	while(CurrentTime < EndTime)
	{
		unsigned int NumPoints;
		bool bGetPointCountOk = Cache->GetChannelPointCount(ChannelIndex, CurrentTime, NumPoints);
		check(bGetPointCountOk);

		if(NumAnimatedPoints == -1)
		{
			NumAnimatedPoints = NumPoints;
		}
		else if(NumAnimatedPoints != NumPoints)
		{
			bNumPointsOk = false;
			break;
		}

		NumFrames++;
		CurrentTime += DeltaTime;
	}

	return bNumPointsOk;
}

void FVertexAnimTools::ImportVertexAnimtion(UnFbx::FFbxImporter* FFbxImporter, USkeletalMesh * SkelMesh, UPackage * Package, FString& Filename)
{
	check(SkelMesh);
	check(SkelMesh->GetImportedResource()->LODModels.Num() > 0);
	FStaticLODModel& Model = SkelMesh->GetImportedResource()->LODModels[0];
	if(Model.MeshToImportVertexMap.Num() == 0)
	{
		UE_LOG(LogFbx, Warning, TEXT("ImportVertexAnimtion: Mesh does not contain necessary vertex anim mapping."));
		return;
	}

	FbxNode* RootNodeToImport = FFbxImporter->Scene->GetRootNode();

	/* Exporting from Maya with skin and vertex cache seems to throw away the skin deformer, so not sure how to find the 'right' mesh...
	TArray<FbxNode*> MeshArray;
	FName RootBoneName = (SkelMesh->Skeleton)? SkelMesh->Skeleton->GetBoneName(0) : SkelMesh->RefSkeleton.RefBoneInfo(0).Name;
	FFbxImporter->FindFBXMeshesByBone(RootBoneName, false, MeshArray);
	*/

	// Find mesh with vertex deformer
	TArray<FbxMesh*> VertexCacheMeshes;
	RecursiveFindVertexAnimMeshes(RootNodeToImport, VertexCacheMeshes);
	if(VertexCacheMeshes.Num() == 0)
	{
		UE_LOG(LogFbx, Warning, TEXT("ImportVertexAnimtion: No vertex cache deformed mesh found."));
		return;
	}

	// Get deformer and open cache
	FbxMesh* Mesh = VertexCacheMeshes[0];
	FbxVertexCacheDeformer* Deformer = static_cast<FbxVertexCacheDeformer*>(Mesh->GetDeformer(0, FbxDeformer::eVertexCache));
	check(Deformer != NULL); // tested in RecursiveFindVertexAnimMeshes
	FbxCache* Cache = Deformer->GetCache();
	bool bOpenSuccess = Cache->OpenFileForRead();
	if(!bOpenSuccess)
	{
		UE_LOG(LogFbx, Warning, TEXT("ImportVertexAnimtion: Unable to open cache file."));
		return;
	}

	// Check cache format
	if(Cache->GetCacheFileFormat() != FbxCache::eMayaCache)
	{
		UE_LOG(LogFbx, Warning, TEXT("ImportVertexAnimtion: Only Maya vertex animation supported."));
		return;
	}

	// Get channel
	FbxString DeformerChannelName = Deformer->Channel.Get();
	int ChannelIndex = Cache->GetChannelIndex(DeformerChannelName);
	if(ChannelIndex == -1)
	{
		UE_LOG(LogFbx, Warning, TEXT("ImportVertexAnimtion: No Cache channel for deformer."));
		return;
	}

	// Get data type
	FbxCache::EMCDataType DataType;
	bool bGetDataTypeOk = Cache->GetChannelDataType(ChannelIndex, DataType);
	check(bGetDataTypeOk);
	if(DataType != FbxCache::EMCDataType::eDoubleVectorArray)
	{
		UE_LOG(LogFbx, Warning, TEXT("ImportVertexAnimtion: Only double array format supported at the moment."));
		return;
	}

	// Get other info about anim
	FbxTime StartTime, DeltaTime;
	int32 NumFrames, NumAnimatedPoints;
	bool bInfoOk = GetVertexAnimInfo(Cache, ChannelIndex, StartTime, DeltaTime, NumFrames, NumAnimatedPoints);
	if(!bInfoOk)
	{
		UE_LOG(LogFbx, Warning, TEXT("ImportVertexAnimtion: Invalid vertex animation."));
		return;
	}

	UVertexAnimation* NewVertexAnim = UnFbx::FFbxImporter::CreateAsset<UVertexAnimation>( Package->GetName(), *FPaths::GetBaseFilename(Filename) );
	if (NewVertexAnim != NULL)
	{
		// Save the mesh we are importing for
		NewVertexAnim->BaseSkelMesh = SkelMesh;
		NewVertexAnim->NumAnimatedVerts = NumAnimatedPoints;

		ConvertCacheToAnim(Cache, ChannelIndex, NewVertexAnim, StartTime, DeltaTime, NumFrames);

		UE_LOG(LogFbx, Log, TEXT("ImportVertexAnimtion: Success! %d frames."), NewVertexAnim->GetNumFrames());
	}
}
