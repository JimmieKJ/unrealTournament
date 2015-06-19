// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DistanceFieldAtlas.cpp
=============================================================================*/

#include "EnginePrivate.h"
#include "StaticMeshResources.h"
#include "DistanceFieldAtlas.h"

#if WITH_EDITOR
#include "DerivedDataCacheInterface.h"
#include "MeshUtilities.h"
#endif

TGlobalResource<FDistanceFieldVolumeTextureAtlas> GDistanceFieldVolumeTextureAtlas = TGlobalResource<FDistanceFieldVolumeTextureAtlas>(PF_R16F);

const int32 MaxAtlasDimension = 512;

FDistanceFieldVolumeTextureAtlas::FDistanceFieldVolumeTextureAtlas(EPixelFormat InFormat) :
	BlockAllocator(0, 0, 0, MaxAtlasDimension, MaxAtlasDimension, MaxAtlasDimension, false, false)
{
	Generation = 0;
	Format = InFormat;
}

FString FDistanceFieldVolumeTextureAtlas::GetSizeString() const
{
	if (VolumeTextureRHI)
	{
		const int32 FormatSize = GPixelFormats[Format].BlockBytes;
		float MemorySize = VolumeTextureRHI->GetSizeX() * VolumeTextureRHI->GetSizeY() * VolumeTextureRHI->GetSizeZ() * FormatSize / 1024.0f / 1024.0f;
		return FString::Printf(TEXT("Allocated %ux%ux%u distance field atlas = %.1fMb"), VolumeTextureRHI->GetSizeX(), VolumeTextureRHI->GetSizeY(), VolumeTextureRHI->GetSizeZ(), MemorySize);
	}
	else
	{
		return TEXT("");
	}
}

void FDistanceFieldVolumeTextureAtlas::AddAllocation(FDistanceFieldVolumeTexture* Texture)
{
	PendingAllocations.AddUnique(Texture);
}

void FDistanceFieldVolumeTextureAtlas::RemoveAllocation(FDistanceFieldVolumeTexture* Texture)
{
	PendingAllocations.Remove(Texture);

	if (CurrentAllocations.Contains(Texture))
	{
		const FIntVector Min = Texture->GetAllocationMin();
		const FIntVector Size = Texture->VolumeData.Size;
		verify(BlockAllocator.RemoveElement(Min.X, Min.Y, Min.Z, Size.X, Size.Y, Size.Z));
		CurrentAllocations.Remove(Texture);
	}
}

struct FCompareVolumeAllocation
{
	FORCEINLINE bool operator()(const FDistanceFieldVolumeTexture& A, const FDistanceFieldVolumeTexture& B) const
	{
		return A.GetAllocationVolume() > B.GetAllocationVolume();
	}
};

void FDistanceFieldVolumeTextureAtlas::UpdateAllocations()
{
	if (PendingAllocations.Num() > 0)
	{
		// Sort largest to smallest for best packing
		PendingAllocations.Sort(FCompareVolumeAllocation());

		for (int32 AllocationIndex = 0; AllocationIndex < PendingAllocations.Num(); AllocationIndex++)
		{
			FDistanceFieldVolumeTexture* Texture = PendingAllocations[AllocationIndex];
			const FIntVector Size = Texture->VolumeData.Size;

			if (!BlockAllocator.AddElement((uint32&)Texture->AtlasAllocationMin.X, (uint32&)Texture->AtlasAllocationMin.Y, (uint32&)Texture->AtlasAllocationMin.Z, Size.X, Size.Y, Size.Z))
			{
				UE_LOG(LogStaticMesh,Error,TEXT("Failed to allocate %ux%ux%u in distance field atlas"), Size.X, Size.Y, Size.Z);
				PendingAllocations.RemoveAt(AllocationIndex);
				AllocationIndex--;
			}
		}

		if (!VolumeTextureRHI
			|| BlockAllocator.GetSizeX() > VolumeTextureRHI->GetSizeX()
			|| BlockAllocator.GetSizeY() > VolumeTextureRHI->GetSizeY()
			|| BlockAllocator.GetSizeZ() > VolumeTextureRHI->GetSizeZ())
		{
			if (CurrentAllocations.Num() > 0)
			{
				// Remove all allocations from the layout so we have a clean slate
				BlockAllocator = FTextureLayout3d(0, 0, 0, MaxAtlasDimension, MaxAtlasDimension, MaxAtlasDimension, false, false);
				
				Generation++;

				// Re-upload all textures since we had to reallocate
				PendingAllocations.Append(CurrentAllocations);
				CurrentAllocations.Empty();

				// Sort largest to smallest for best packing
				PendingAllocations.Sort(FCompareVolumeAllocation());

				// Add all allocations back to the layout
				for (int32 AllocationIndex = 0; AllocationIndex < PendingAllocations.Num(); AllocationIndex++)
				{
					FDistanceFieldVolumeTexture* Texture = PendingAllocations[AllocationIndex];
					const FIntVector Size = Texture->VolumeData.Size;

					if (!BlockAllocator.AddElement((uint32&)Texture->AtlasAllocationMin.X, (uint32&)Texture->AtlasAllocationMin.Y, (uint32&)Texture->AtlasAllocationMin.Z, Size.X, Size.Y, Size.Z))
					{
						UE_LOG(LogStaticMesh,Error,TEXT("Failed to allocate %ux%ux%u in distance field atlas"), Size.X, Size.Y, Size.Z);
						PendingAllocations.RemoveAt(AllocationIndex);
						AllocationIndex--;
					}
				}
			}

			FRHIResourceCreateInfo CreateInfo;

			VolumeTextureRHI = RHICreateTexture3D(
				BlockAllocator.GetSizeX(), 
				BlockAllocator.GetSizeY(), 
				BlockAllocator.GetSizeZ(), 
				Format,
				1,
				TexCreate_ShaderResource,
				CreateInfo);

			UE_LOG(LogStaticMesh,Log,TEXT("Allocated %s"), *GetSizeString());
		}

		for (int32 AllocationIndex = 0; AllocationIndex < PendingAllocations.Num(); AllocationIndex++)
		{
			FDistanceFieldVolumeTexture* Texture = PendingAllocations[AllocationIndex];
			const FIntVector Size = Texture->VolumeData.Size;

			const FUpdateTextureRegion3D UpdateRegion(
				Texture->AtlasAllocationMin.X,
				Texture->AtlasAllocationMin.Y,
				Texture->AtlasAllocationMin.Z,
				0,
				0,
				0,
				Size.X,
				Size.Y,
				Size.Z);

			const int32 FormatSize = GPixelFormats[Format].BlockBytes;

			// Update the volume texture atlas
			RHIUpdateTexture3D(VolumeTextureRHI, 0, UpdateRegion, Size.X * FormatSize, Size.X * Size.Y * FormatSize, (const uint8*)Texture->VolumeData.DistanceFieldVolume.GetData());
		}

		CurrentAllocations.Append(PendingAllocations);
		PendingAllocations.Empty();
	}	
}
	
void FDistanceFieldVolumeTexture::Initialize()
{
	if (IsValidDistanceFieldVolume())
	{
		bReferencedByAtlas = true;

		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			AddAllocation,
			FDistanceFieldVolumeTexture*, DistanceFieldVolumeTexture, this,
			{
				GDistanceFieldVolumeTextureAtlas.AddAllocation(DistanceFieldVolumeTexture);
			}
		);
	}
}

void FDistanceFieldVolumeTexture::Release()
{
	if (bReferencedByAtlas)
	{
		bReferencedByAtlas = false;

		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			ReleaseAllocation,
			FDistanceFieldVolumeTexture*, DistanceFieldVolumeTexture, this,
			{
				GDistanceFieldVolumeTextureAtlas.RemoveAllocation(DistanceFieldVolumeTexture);
			}
		);
	}
}

FIntVector FDistanceFieldVolumeTexture::GetAllocationSize() const
{
	return VolumeData.Size;
}

bool FDistanceFieldVolumeTexture::IsValidDistanceFieldVolume() const
{
	return VolumeData.Size.GetMax() > 0;
}

FDistanceFieldAsyncQueue* GDistanceFieldAsyncQueue = NULL;

#if WITH_EDITORONLY_DATA

void FDistanceFieldVolumeData::CacheDerivedData(const FString& InDDCKey, UStaticMesh* Mesh, UStaticMesh* GenerateSource, float DistanceFieldResolutionScale, bool bGenerateDistanceFieldAsIfTwoSided)
{
	TArray<uint8> DerivedData;

	if (GetDerivedDataCacheRef().GetSynchronous(*InDDCKey, DerivedData))
	{
		FMemoryReader Ar(DerivedData, /*bIsPersistent=*/ true);
		Ar << *this;
	}
	else
	{
		FAsyncDistanceFieldTask* NewTask = new FAsyncDistanceFieldTask;
		NewTask->DDCKey = InDDCKey;
		NewTask->StaticMesh = Mesh;
		NewTask->GenerateSource = GenerateSource;
		NewTask->DistanceFieldResolutionScale = DistanceFieldResolutionScale;
		NewTask->bGenerateDistanceFieldAsIfTwoSided = bGenerateDistanceFieldAsIfTwoSided;
		NewTask->GeneratedVolumeData = new FDistanceFieldVolumeData();

		for (int32 MaterialIndex = 0; MaterialIndex < Mesh->Materials.Num(); MaterialIndex++)
		{
			// Default material blend mode
			EBlendMode BlendMode = BLEND_Opaque;

			if (Mesh->Materials[MaterialIndex])
			{
				BlendMode = Mesh->Materials[MaterialIndex]->GetBlendMode();
			}

			NewTask->MaterialBlendModes.Add(BlendMode);
		}

		GDistanceFieldAsyncQueue->AddTask(NewTask);
	}
}

#endif

int32 GUseAsyncDistanceFieldBuildQueue = 1;
static FAutoConsoleVariableRef CVarAOAsyncBuildQueue(
	TEXT("r.AOAsyncBuildQueue"),
	GUseAsyncDistanceFieldBuildQueue,
	TEXT("Whether to asynchronously build distance field volume data from meshes."),
	ECVF_Default | ECVF_ReadOnly
	);

class FBuildDistanceFieldThreadRunnable : public FRunnable
{
public:
	/** Initialization constructor. */
	FBuildDistanceFieldThreadRunnable(
		FDistanceFieldAsyncQueue* InAsyncQueue
		)
		: NextThreadIndex(0)
		, AsyncQueue(*InAsyncQueue)
		, Thread(NULL)
		, bIsRunning(false)
		, bForceFinish(false)
	{}

	virtual ~FBuildDistanceFieldThreadRunnable()
	{
		check(!bIsRunning);
	}

	// FRunnable interface.
	virtual bool Init() { bIsRunning = true; return true; }
	virtual void Exit() { bIsRunning = false; }
	virtual void Stop() { bForceFinish = true; }
	virtual uint32 Run();

	void Launch()
	{
		check(!bIsRunning);

		bForceFinish = false;
		Thread = FRunnableThread::Create(this, *FString::Printf(TEXT("BuildDistanceFieldThread%u"), NextThreadIndex), 0, TPri_Normal, FPlatformAffinity::GetPoolThreadMask());
		NextThreadIndex++;
	}

	inline bool IsRunning() { return bIsRunning; }

private:

	int32 NextThreadIndex;

	FDistanceFieldAsyncQueue& AsyncQueue;

	/** The runnable thread */
	TScopedPointer<FRunnableThread> Thread;

	TScopedPointer<FQueuedThreadPool> WorkerThreadPool;

	volatile bool bIsRunning;
	volatile bool bForceFinish;
};

FQueuedThreadPool* CreateWorkerThreadPool()
{
	const int32 NumThreads = FMath::Max<int32>(FPlatformMisc::NumberOfCoresIncludingHyperthreads() - 2, 1);
	FQueuedThreadPool* WorkerThreadPool = FQueuedThreadPool::Allocate();
	WorkerThreadPool->Create(NumThreads, 32 * 1024, TPri_BelowNormal);
	return WorkerThreadPool;
}

uint32 FBuildDistanceFieldThreadRunnable::Run()
{
	bool bHasWork = true;

	while (!bForceFinish && bHasWork)
	{
		// LIFO build order, since meshes actually visible in a map are typically loaded last
		FAsyncDistanceFieldTask* Task = AsyncQueue.TaskQueue.Pop();

		if (Task)
		{
			if (!WorkerThreadPool)
			{
				WorkerThreadPool = CreateWorkerThreadPool();
			}

			AsyncQueue.Build(Task, *WorkerThreadPool);
			bHasWork = true;
		}
		else
		{
			bHasWork = false;
		}
	}

	WorkerThreadPool = NULL;

	return 0;
}

FDistanceFieldAsyncQueue::FDistanceFieldAsyncQueue() 
{
#if WITH_EDITOR
	MeshUtilities = NULL;
#endif

	ThreadRunnable = new FBuildDistanceFieldThreadRunnable(this);
}

FDistanceFieldAsyncQueue::~FDistanceFieldAsyncQueue()
{
}

void FDistanceFieldAsyncQueue::AddTask(FAsyncDistanceFieldTask* Task)
{
#if WITH_EDITOR
	if (!MeshUtilities)
	{
		MeshUtilities = &FModuleManager::Get().LoadModuleChecked<IMeshUtilities>(TEXT("MeshUtilities"));
	}
	
	ReferencedTasks.Add(Task);

	if (GUseAsyncDistanceFieldBuildQueue)
	{
		TaskQueue.Push(Task);

		if (!ThreadRunnable->IsRunning())
		{
			ThreadRunnable->Launch();
		}
	}
	else
	{
		TScopedPointer<FQueuedThreadPool> WorkerThreadPool(CreateWorkerThreadPool());
		Build(Task, *WorkerThreadPool);
	}
#else
	UE_LOG(LogStaticMesh,Fatal,TEXT("Tried to build a distance field without editor support (this should have been done during cooking)"));
#endif
}

void FDistanceFieldAsyncQueue::BlockUntilBuildComplete(UStaticMesh* StaticMesh, bool bWarnIfBlocked)
{
	bool bReferenced = false;
	bool bHadToBlock = false;
	double StartTime = 0;

	do 
	{
		ProcessAsyncTasks();

		bReferenced = false;

		for (int TaskIndex = 0; TaskIndex < ReferencedTasks.Num(); TaskIndex++)
		{
			bReferenced = bReferenced || ReferencedTasks[TaskIndex]->StaticMesh == StaticMesh;
			bReferenced = bReferenced || ReferencedTasks[TaskIndex]->GenerateSource == StaticMesh;
		}

		if (bReferenced)
		{
			if (!bHadToBlock)
			{
				StartTime = FPlatformTime::Seconds();
			}

			bHadToBlock = true;
			FPlatformProcess::Sleep(.01f);
		}
	} 
	while (bReferenced);

	if (bHadToBlock && bWarnIfBlocked)
	{
		UE_LOG(LogStaticMesh, Warning, TEXT("Main thread blocked for %.3fs for async distance field build of %s to complete!  This can happen if the mesh is rebuilt excessively."),
			(float)(FPlatformTime::Seconds() - StartTime), 
			*StaticMesh->GetName());
	}
}

void FDistanceFieldAsyncQueue::BlockUntilAllBuildsComplete()
{
	do 
	{
		ProcessAsyncTasks();
		FPlatformProcess::Sleep(.01f);
	} 
	while (GetNumOutstandingTasks() > 0);
}

void FDistanceFieldAsyncQueue::Build(FAsyncDistanceFieldTask* Task, FQueuedThreadPool& ThreadPool)
{
#if WITH_EDITOR
	const FStaticMeshLODResources& LODModel = Task->GenerateSource->RenderData->LODResources[0];

	MeshUtilities->GenerateSignedDistanceFieldVolumeData(
		LODModel,
		ThreadPool,
		Task->MaterialBlendModes,
		Task->GenerateSource->RenderData->Bounds,
		Task->DistanceFieldResolutionScale,
		Task->bGenerateDistanceFieldAsIfTwoSided,
		*Task->GeneratedVolumeData);

	CompletedTasks.Push(Task);
#endif
}

void FDistanceFieldAsyncQueue::AddReferencedObjects(FReferenceCollector& Collector)
{	
	for (int TaskIndex = 0; TaskIndex < ReferencedTasks.Num(); TaskIndex++)
	{
		// Make sure none of the UObjects referenced by the async tasks are GC'ed during the task
		Collector.AddReferencedObject(ReferencedTasks[TaskIndex]->StaticMesh);
		Collector.AddReferencedObject(ReferencedTasks[TaskIndex]->GenerateSource);
	}
}

void FDistanceFieldAsyncQueue::ProcessAsyncTasks()
{
#if WITH_EDITOR
	TArray<FAsyncDistanceFieldTask*> LocalCompletedTasks;
	CompletedTasks.PopAll(LocalCompletedTasks);

	for (int TaskIndex = 0; TaskIndex < LocalCompletedTasks.Num(); TaskIndex++)
	{
		FAsyncDistanceFieldTask* Task = LocalCompletedTasks[TaskIndex];

		ReferencedTasks.Remove(Task);

		Task->GeneratedVolumeData->VolumeTexture.Initialize();
		FDistanceFieldVolumeData* OldVolumeData = Task->StaticMesh->RenderData->LODResources[0].DistanceFieldData;

		{
			// Cause all components using this static mesh to get re-registered, which will recreate their proxies and primitive uniform buffers
			FStaticMeshComponentRecreateRenderStateContext RecreateRenderStateContext(Task->StaticMesh, false);

			// Assign the new volume data
			Task->StaticMesh->RenderData->LODResources[0].DistanceFieldData = Task->GeneratedVolumeData;
		}

		OldVolumeData->VolumeTexture.Release();

		// Rendering thread may still be referencing the old one, use the deferred cleanup interface to delete it next frame when it is safe
		BeginCleanup(OldVolumeData);

		{
			TArray<uint8> DerivedData;
			// Save built distance field volume to DDC
			FMemoryWriter Ar(DerivedData, /*bIsPersistent=*/ true);
			Ar << *(Task->StaticMesh->RenderData->LODResources[0].DistanceFieldData);
			GetDerivedDataCacheRef().Put(*Task->DDCKey, DerivedData);
		}

		delete Task;
	}

	if (ReferencedTasks.Num() > 0 && !ThreadRunnable->IsRunning())
	{
		ThreadRunnable->Launch();
	}
#endif
}

void FDistanceFieldAsyncQueue::Shutdown()
{
	ThreadRunnable->Stop();
	bool bLogged = false;

	while (ThreadRunnable->IsRunning())
	{
		if (!bLogged)
		{
			bLogged = true;
			UE_LOG(LogStaticMesh,Warning,TEXT("Waiting until distance field task is complete so it is safe to destruct UObjects"));
		}
		FPlatformProcess::Sleep(0.01f);
	}
}