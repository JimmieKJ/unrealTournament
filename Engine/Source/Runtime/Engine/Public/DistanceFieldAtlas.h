// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DistanceFieldAtlas.h
=============================================================================*/

#pragma once

#include "TextureLayout3d.h"
#include "GCObject.h"

// DDC key for distance field data, must be changed when modifying the generation code or data format
#define DISTANCEFIELD_DERIVEDDATA_VER TEXT("7768798764B445A9543C94442EA899D")

/** Represents a distance field volume texture for a single UStaticMesh. */
class FDistanceFieldVolumeTexture
{
public:
	FDistanceFieldVolumeTexture(const class FDistanceFieldVolumeData& InVolumeData) :
		VolumeData(InVolumeData),
		AtlasAllocationMin(FIntVector(-1, -1, -1)),
		bReferencedByAtlas(false)
	{}

	~FDistanceFieldVolumeTexture()
	{
		// Make sure we have been properly removed from the atlas before deleting
		check(!bReferencedByAtlas);
	}

	/** Called at load time on game thread */
	void Initialize();

	/** Called before unload on game thread */
	void Release();

	FIntVector GetAllocationMin() const
	{
		return AtlasAllocationMin;
	}

	FIntVector GetAllocationSize() const;

	int32 GetAllocationVolume() const
	{
		return GetAllocationSize().X * GetAllocationSize().Y * GetAllocationSize().Z;
	}

	bool IsValidDistanceFieldVolume() const;

private:
	const FDistanceFieldVolumeData& VolumeData;
	FIntVector AtlasAllocationMin;
	bool bReferencedByAtlas;

	friend class FDistanceFieldVolumeTextureAtlas;
};

/** Global volume texture atlas that collects all static mesh resource distance fields. */
class FDistanceFieldVolumeTextureAtlas : public FRenderResource
{
public:
	FDistanceFieldVolumeTextureAtlas(EPixelFormat InFormat);

	virtual void ReleaseRHI() override
	{
		VolumeTextureRHI.SafeRelease();
	}

	int32 GetSizeX() const { return VolumeTextureRHI->GetSizeX(); }
	int32 GetSizeY() const { return VolumeTextureRHI->GetSizeY(); }
	int32 GetSizeZ() const { return VolumeTextureRHI->GetSizeZ(); }

	ENGINE_API FString GetSizeString() const;

	/** Add an allocation to the atlas. */
	void AddAllocation(FDistanceFieldVolumeTexture* Texture);

	/** Remove an allocation from the atlas. This must be done prior to deleting the FDistanceFieldVolumeTexture object. */
	void RemoveAllocation(FDistanceFieldVolumeTexture* Texture);

	/** Reallocates the volume texture if necessary and uploads new allocations. */
	ENGINE_API void UpdateAllocations();

	int32 GetGeneration() const { return Generation; }

	EPixelFormat Format;
	FTexture3DRHIRef VolumeTextureRHI;

private:
	/** Manages the atlas layout. */
	FTextureLayout3d BlockAllocator;

	/** Allocations that are waiting to be added until the next update. */
	TArray<FDistanceFieldVolumeTexture*> PendingAllocations;

	/** Allocations that have already been added, stored in case we need to realloc. */
	TArray<FDistanceFieldVolumeTexture*> CurrentAllocations;

	/** Incremented when the atlas is reallocated, so dependencies know to update. */
	int32 Generation;
};

extern ENGINE_API TGlobalResource<FDistanceFieldVolumeTextureAtlas> GDistanceFieldVolumeTextureAtlas;

/** Distance field data payload and output of the mesh build process. */
class FDistanceFieldVolumeData : public FDeferredCleanupInterface
{
public:

	/** Signed distance field volume stored in local space. */
	TArray<FFloat16> DistanceFieldVolume;

	/** Dimensions of DistanceFieldVolume. */
	FIntVector Size;

	/** Local space bounding box of the distance field volume. */
	FBox LocalBoundingBox;

	/** Whether the mesh was closed and therefore a valid distance field was supported. */
	bool bMeshWasClosed;

	/** Whether the distance field was built assuming that every triangle is a frontface. */
	bool bBuiltAsIfTwoSided;

	/** Whether the mesh was a plane with very little extent in Z. */
	bool bMeshWasPlane;

	FDistanceFieldVolumeTexture VolumeTexture;

	FDistanceFieldVolumeData() :
		Size(FIntVector(0, 0, 0)),
		LocalBoundingBox(0),
		bMeshWasClosed(true),
		bBuiltAsIfTwoSided(false),
		bMeshWasPlane(false),
		VolumeTexture(*this)
	{}

	// Use the deferred cleanup interface to safely delete even when the rendering thread stores a reference to this volume data
	virtual void FinishCleanup() override
	{
		delete this;
	}

	SIZE_T GetResourceSize() const
	{
		return sizeof(*this) + DistanceFieldVolume.GetAllocatedSize();
	}

#if WITH_EDITORONLY_DATA

	void CacheDerivedData(const FString& InDDCKey, UStaticMesh* Mesh, UStaticMesh* GenerateSource, float DistanceFieldResolutionScale, bool bGenerateDistanceFieldAsIfTwoSided);

#endif

	friend FArchive& operator<<(FArchive& Ar,FDistanceFieldVolumeData& Data)
	{
		// Note: this is derived data, no need for versioning (bump the DDC guid)
		Ar << Data.DistanceFieldVolume << Data.Size << Data.LocalBoundingBox << Data.bMeshWasClosed << Data.bBuiltAsIfTwoSided << Data.bMeshWasPlane;
		return Ar;
	}
};

/** A task to build a distance field for a single mesh */
class FAsyncDistanceFieldTask
{
public:
	TArray<EBlendMode> MaterialBlendModes;
	UStaticMesh* StaticMesh;
	UStaticMesh* GenerateSource;
	float DistanceFieldResolutionScale;
	bool bGenerateDistanceFieldAsIfTwoSided;
	FString DDCKey;
	FDistanceFieldVolumeData* GeneratedVolumeData;
};

/** Class that manages asynchronous building of mesh distance fields. */
class FDistanceFieldAsyncQueue : public FGCObject
{
public:

	ENGINE_API FDistanceFieldAsyncQueue();

	ENGINE_API virtual ~FDistanceFieldAsyncQueue();

	/** Adds a new build task. */
	ENGINE_API void AddTask(FAsyncDistanceFieldTask* Task);

	/** Blocks the main thread until the async build of the specified mesh is complete. */
	ENGINE_API void BlockUntilBuildComplete(UStaticMesh* StaticMesh, bool bWarnIfBlocked);

	/** Blocks the main thread until all async builds complete. */
	ENGINE_API void BlockUntilAllBuildsComplete();

	/** Called once per frame, fetches completed tasks and applies them to the scene. */
	ENGINE_API void ProcessAsyncTasks();

	/** Exposes UObject references used by the async build. */
	ENGINE_API void AddReferencedObjects(FReferenceCollector& Collector);

	/** Blocks until it is safe to shut down (worker threads are idle). */
	ENGINE_API void Shutdown();

	int32 GetNumOutstandingTasks() const
	{
		return ReferencedTasks.Num();
	}

private:

	/** Builds a single task with the given threadpool.  Called from the worker thread. */
	void Build(FAsyncDistanceFieldTask* Task, class FQueuedThreadPool& ThreadPool);

	/** Thread that will build any tasks in TaskQueue and exit when there are no more. */
	class TScopedPointer<class FBuildDistanceFieldThreadRunnable> ThreadRunnable;

	/** Game-thread managed list of tasks in the async system. */
	TArray<FAsyncDistanceFieldTask*> ReferencedTasks;

	/** Tasks that have not yet started processing yet. */
	TLockFreePointerList<FAsyncDistanceFieldTask> TaskQueue;

	/** Tasks that have completed processing. */
	TLockFreePointerList<FAsyncDistanceFieldTask> CompletedTasks;

	class IMeshUtilities* MeshUtilities;

	friend class FBuildDistanceFieldThreadRunnable;
};

/** Global build queue. */
extern ENGINE_API FDistanceFieldAsyncQueue* GDistanceFieldAsyncQueue;