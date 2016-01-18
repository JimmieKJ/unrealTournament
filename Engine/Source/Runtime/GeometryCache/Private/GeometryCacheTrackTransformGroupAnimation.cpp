// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "GeometryCacheModulePrivatePCH.h"
#include "GeometryCacheTrackTransformGroupAnimation.h"


GEOMETRYCACHE_API UGeometryCacheTrack_TransformGroupAnimation::UGeometryCacheTrack_TransformGroupAnimation(const FObjectInitializer& ObjectInitializer /*= FObjectInitializer::Get()*/) : UGeometryCacheTrack(ObjectInitializer)
{
}

SIZE_T UGeometryCacheTrack_TransformGroupAnimation::GetResourceSize(EResourceSizeMode::Type Mode)
{
	SIZE_T ResourceSize = 0;
	ResourceSize += UGeometryCacheTrack::GetResourceSize(Mode);
	ResourceSize += MeshData.GetResourceSize();

	return ResourceSize;
}

void UGeometryCacheTrack_TransformGroupAnimation::Serialize(FArchive& Ar)
{
	UGeometryCacheTrack::Serialize(Ar);
	Ar << MeshData;
}

const bool UGeometryCacheTrack_TransformGroupAnimation::UpdateMeshData(const float Time, const bool bLooping, int32& InOutMeshSampleIndex, FGeometryCacheMeshData*& OutMeshData)
{
	// If InOutMeshSampleIndex equals -1 (first creation) update the OutVertices and InOutMeshSampleIndex
	if (InOutMeshSampleIndex == -1)
	{
		OutMeshData = &MeshData;
		InOutMeshSampleIndex = 0;
		return true;
	}

	return false;
}

void UGeometryCacheTrack_TransformGroupAnimation::SetMesh(const FGeometryCacheMeshData& NewMeshData)
{
	MeshData = NewMeshData;
	NumMaterials = NewMeshData.BatchesInfo.Num();
}
