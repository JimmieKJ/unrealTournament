// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "GeometryCacheMeshData.h"

#include "GeometryCacheTrackTransformGroupAnimation.generated.h"

/** Derived GeometryCacheTrack class, used for Transform animation. */
UCLASS(collapsecategories, hidecategories = Object, BlueprintType, config = Engine)
class GEOMETRYCACHE_API UGeometryCacheTrack_TransformGroupAnimation : public UGeometryCacheTrack
{
	GENERATED_UCLASS_BODY()
		
	// Begin UObject interface.
	virtual SIZE_T GetResourceSize(EResourceSizeMode::Type Mode) override;
	virtual void Serialize(FArchive& Ar) override;
	// End UObject interface.
	
	// Begin UGeometryCacheTrack interface.
	virtual const bool UpdateMeshData(const float Time, const bool bLooping, int32& InOutMeshSampleIndex, FGeometryCacheMeshData*& OutMeshData) override;
	// End UGeometryCacheTrack interface.

	/**
	* Sets/updates the MeshData for this track
	*
	* @param NewMeshData - GeometryCacheMeshData instance later used as the rendered mesh	
	*/
	UFUNCTION()
	void SetMesh(const FGeometryCacheMeshData& NewMeshData);	
private:
	FGeometryCacheMeshData MeshData;
};
