// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PhysXSupport.h: PhysX support
=============================================================================*/

#pragma once

#if WITH_PHYSX

#include "PhysXIncludes.h"
#include "EngineLogs.h"

// Whether or not to use the PhysX scene lock
#ifndef USE_SCENE_LOCK
#define USE_SCENE_LOCK			1
#endif

// Whether to track PhysX memory allocations
#ifndef PHYSX_MEMORY_VALIDATION
#define PHYSX_MEMORY_VALIDATION		0
#endif

// Whether to track PhysX memory allocations
#ifndef PHYSX_MEMORY_STATS
#define PHYSX_MEMORY_STATS		0 || PHYSX_MEMORY_VALIDATION
#endif

// binary serialization requires 128 byte alignment
#ifndef PHYSX_SERIALIZATION_ALIGNMENT
#define PHYSX_SERIALIZATION_ALIGNMENT 128
#endif

#define PHYSX_MEMORY_STAT_ONLY (0)

#if USE_SCENE_LOCK

/** Scoped scene read lock - we use this instead of PxSceneReadLock because it handles NULL scene */
class FPhysXSceneReadLock
{
public:
	
	FPhysXSceneReadLock(PxScene* PInScene)
		: PScene(PInScene)
	{
		SCOPE_CYCLE_COUNTER(STAT_PhysSceneReadLock);
		if(PScene)
		{
			PScene->lockRead();
		}
	}

	~FPhysXSceneReadLock()
	{
		if(PScene)
		{
			PScene->unlockRead();
		}
	}

private:
	PxScene* PScene;
};

/** Scoped scene write lock - we use this instead of PxSceneReadLock because it handles NULL scene */
class FPhysXSceneWriteLock
{
public:
	FPhysXSceneWriteLock(PxScene* PInScene)
		: PScene(PInScene)
	{
		SCOPE_CYCLE_COUNTER(STAT_PhysSceneWriteLock);
		if(PScene)
		{
			PScene->lockWrite();
		}
	}

	~FPhysXSceneWriteLock()
	{
		if(PScene)
		{
			PScene->unlockWrite();
		}
	}

private:
	PxScene* PScene;
};

#define SCOPED_SCENE_READ_LOCK( _scene ) FPhysXSceneReadLock PREPROCESSOR_JOIN(_rlock,__LINE__)(_scene)
#define SCOPED_SCENE_WRITE_LOCK( _scene ) FPhysXSceneWriteLock PREPROCESSOR_JOIN(_wlock,__LINE__)(_scene)

#define SCENE_LOCK_READ( _scene )		{ SCOPE_CYCLE_COUNTER(STAT_PhysSceneReadLock); if((_scene) != NULL) { (_scene)->lockRead(); } }
#define SCENE_UNLOCK_READ( _scene )		{ if((_scene) != NULL) { (_scene)->unlockRead(); } }
#define SCENE_LOCK_WRITE( _scene )		{ SCOPE_CYCLE_COUNTER(STAT_PhysSceneWriteLock); if((_scene) != NULL) { (_scene)->lockWrite(); } }
#define SCENE_UNLOCK_WRITE( _scene )	{ if((_scene) != NULL) { (_scene)->unlockWrite(); } }
#else
#define SCOPED_SCENE_READ_LOCK_INDEXED( _scene, _index )
#define SCOPED_SCENE_READ_LOCK( _scene )
#define SCOPED_SCENE_WRITE_LOCK_INDEXED( _scene, _index )
#define SCOPED_SCENE_WRITE_LOCK( _scene )
#define SCENE_LOCK_READ( _scene )
#define SCENE_UNLOCK_READ( _scene )
#define SCENE_LOCK_WRITE( _scene )
#define SCENE_UNLOCK_WRITE( _scene )
#endif

/** Get a pointer to the PxScene from an SceneIndex (will be NULL if scene already shut down) */
PxScene* GetPhysXSceneFromIndex(int32 InSceneIndex);

#if WITH_APEX
/** Get a pointer to the NxApexScene from an SceneIndex (will be NULL if scene already shut down) */
NxApexScene* GetApexSceneFromIndex(int32 InSceneIndex);
#endif

template <bool NeedsLock>
struct FPhysXSupport
{
	/** Obtains the appropriate PhysX scene lock for READING and executes the passed in lambda.
	 *  If SceneType < 0, the Sync actor is used, otherwise the async.
	 *  Note: The lambda is only executed if the physx actor requested is non-null.
	 *  returns true if the requested actor is non-null
	 */
	template <typename LambdaType>
	static bool ExecuteOnPxRigidActorReadOnly(const FBodyInstance* BI, const LambdaType& Func, int32 SceneType = -1)
	{
		if (const PxRigidActor* PRigidActor = BI->GetPxRigidActor_AssumesLocked(SceneType))
		{
			const int32 SceneIndex = (PRigidActor == BI->RigidActorSync ? BI->SceneIndexSync : BI->SceneIndexAsync);
			PxScene* PScene = GetPhysXSceneFromIndex(SceneIndex);
			if (NeedsLock)
			{
				SCENE_LOCK_READ(PScene);
			}

			Func(PRigidActor);

			if (NeedsLock)
			{
				SCENE_UNLOCK_READ(PScene);
			}

			return true;
		}

		return false;
	}

	/** Obtains the appropriate PhysX scene lock for READING and executes the passed in lambda.
	 *  Note: The lambda is only executed if the physx actor is a non-null RigidBody
	 *  returns true if found a non-null RigidBody
	 */
	template <typename LambdaType>
	static bool ExecuteOnPxRigidBodyReadOnly(const FBodyInstance* BI, const LambdaType& Func)
	{
		bool bSuccess = false;
		if (const physx::PxRigidActor* RigidActor = BI->GetPxRigidActor_AssumesLocked())
		{
			const int32 SceneIndex = (RigidActor == BI->RigidActorSync ? BI->SceneIndexSync : BI->SceneIndexAsync);
			PxScene* PScene = GetPhysXSceneFromIndex(SceneIndex);
			if (NeedsLock)
			{
				SCENE_LOCK_READ(PScene);
			}

			if (const physx::PxRigidBody* PRigidBody = RigidActor->isRigidBody())
			{
				Func(PRigidBody);
				bSuccess = true;
			}

			if (NeedsLock)
			{
				SCENE_UNLOCK_READ(PScene);
			}
		}

		return bSuccess;
	}

	/** Obtains the appropriate PhysX scene lock for WRITING and executes the passed in lambda.
	 *  Note: The lambda is only executed if the physx actor is a non-null RigidBody
	 *  returns true if found a non-null RigidBody.
	 */
	template <typename LambdaType>
	static bool ExecuteOnPxRigidBodyReadWrite(const FBodyInstance* BI, const LambdaType& Func)
	{
		bool bSuccess = false;
		if (physx::PxRigidActor* RigidActor = BI->GetPxRigidActor_AssumesLocked())
		{
			const int32 SceneIndex = (RigidActor == BI->RigidActorSync ? BI->SceneIndexSync : BI->SceneIndexAsync);
			PxScene* PScene = GetPhysXSceneFromIndex(SceneIndex);
			if(NeedsLock)
			{
				SCENE_LOCK_WRITE(PScene);
			}
			
			if (physx::PxRigidBody* PRigidBody = RigidActor->isRigidBody())
			{
				Func(PRigidBody);
				bSuccess = true;
			}

			if(NeedsLock)
			{
				SCENE_UNLOCK_WRITE(PScene);
			}
		}

		return bSuccess;
	}

	/** Obtains the appropriate PhysX scene lock for READING and executes the passed in lambda.
	 *  Note: The lambda is only executed if the physx actor is a non-null RigidDynamic
	 *  returns true if found a non-null RigidDynamic.
	 */
	template <typename LambdaType>
	static bool ExecuteOnPxRigidDynamicReadOnly(const FBodyInstance* BI, const LambdaType& Func)
	{
		bool bSuccess = false;
		if (physx::PxRigidActor* RigidActor = BI->GetPxRigidActor_AssumesLocked())
		{
			const int32 SceneIndex = (RigidActor == BI->RigidActorSync ? BI->SceneIndexSync : BI->SceneIndexAsync);
			PxScene* PScene = GetPhysXSceneFromIndex(SceneIndex);
			if (NeedsLock)
			{
				SCENE_LOCK_READ(PScene);
			}

			if (physx::PxRigidDynamic* PRigidDynamic = RigidActor->isRigidDynamic())
			{
				Func(PRigidDynamic);
				bSuccess = true;
			}

			if (NeedsLock)
			{
				SCENE_UNLOCK_READ(PScene);
			}
		}

		return bSuccess;
	}

	/** Obtains the appropriate PhysX scene lock for WRITING and executes the passed in lambda.
	 *  Note: The lambda is only executed if the physx actor is a non-null RigidDynamic
	 *  returns true if found a non-null RigidDynamic.
	 */
	template <typename LambdaType>
	static bool ExecuteOnPxRigidDynamicReadWrite(const FBodyInstance* BI, const LambdaType& Func)
	{
		bool bSuccess = false;
		if (physx::PxRigidActor* RigidActor = BI->GetPxRigidActor_AssumesLocked())
		{
			const int32 SceneIndex = (RigidActor == BI->RigidActorSync ? BI->SceneIndexSync : BI->SceneIndexAsync);
			PxScene* PScene = GetPhysXSceneFromIndex(SceneIndex);
			if (NeedsLock)
			{
				SCENE_LOCK_WRITE(PScene);
			}

			if (physx::PxRigidDynamic* PRigidDynamic = RigidActor->isRigidDynamic())
			{
				Func(PRigidDynamic);
				bSuccess = true;
			}

			if (NeedsLock)
			{
				SCENE_UNLOCK_WRITE(PScene);
			}
		}

		return bSuccess;
	}
};

// Utility functions for obtaining locks and executing lambda. This indirection is needed for vs2012 but should be inlined
template <typename LambdaType> bool ExecuteOnPxRigidActorReadOnly(const FBodyInstance* BI, const LambdaType& Func, int32 SceneType = -1){ return FPhysXSupport<true>::ExecuteOnPxRigidActorReadOnly(BI, Func, SceneType); }
template <typename LambdaType> bool ExecuteOnPxRigidBodyReadOnly(const FBodyInstance* BI, const LambdaType& Func) { return FPhysXSupport<true>::ExecuteOnPxRigidBodyReadOnly(BI, Func); }
template <typename LambdaType> bool ExecuteOnPxRigidBodyReadWrite(const FBodyInstance* BI, const LambdaType& Func){ return FPhysXSupport<true>::ExecuteOnPxRigidBodyReadWrite(BI, Func); }
template <typename LambdaType> bool ExecuteOnPxRigidDynamicReadOnly(const FBodyInstance* BI, const LambdaType& Func){ return FPhysXSupport<true>::ExecuteOnPxRigidDynamicReadOnly(BI, Func); }
template <typename LambdaType> bool ExecuteOnPxRigidDynamicReadWrite(const FBodyInstance* BI, const LambdaType& Func){ return FPhysXSupport<true>::ExecuteOnPxRigidDynamicReadWrite(BI, Func); }

//////// BASIC TYPE CONVERSION

/** Convert Unreal FMatrix to PhysX PxTransform */
ENGINE_API PxTransform UMatrix2PTransform(const FMatrix& UTM);
/** Convert Unreal FTransform to PhysX PxTransform */
ENGINE_API PxTransform U2PTransform(const FTransform& UTransform);
/** Convert Unreal FVector to PhysX PxVec3 */
ENGINE_API PxVec3 U2PVector(const FVector& UVec);
/** Convert Unreal FQuat to PhysX PxTransform */
ENGINE_API PxQuat U2PQuat(const FQuat& UQuat);
/** Convert Unreal FMatrix to PhysX PxMat44 */
ENGINE_API PxMat44 U2PMatrix(const FMatrix& UTM);
/** Convert Unreal FPlane to PhysX plane def */
ENGINE_API PxPlane U2PPlane(FPlane& Plane);
/** Convert PhysX PxTransform to Unreal PxTransform */
ENGINE_API FTransform P2UTransform(const PxTransform& PTM);
/** Convert PhysX PxQuat to Unreal FQuat */
ENGINE_API FQuat P2UQuat(const PxQuat& PQuat);
/** Convert PhysX plane def to Unreal FPlane */
ENGINE_API FPlane P2UPlane(PxReal P[4]);
ENGINE_API FPlane P2UPlane(PxPlane& Plane);
/** Convert PhysX PxMat44 to Unreal FMatrix */
ENGINE_API FMatrix P2UMatrix(const PxMat44& PMat);
/** Convert PhysX PxTransform to Unreal FMatrix */
ENGINE_API FMatrix PTransform2UMatrix(const PxTransform& PTM);

//////// GEOM CONVERSION
// we need this helper struct since PhysX needs geoms to be on the stack
struct UCollision2PGeom
{
	UCollision2PGeom(const FCollisionShape& CollisionShape);
	const PxGeometry * GetGeometry() { return (PxGeometry*)Storage; }
private:
	
	union StorageUnion
	{
		char box[sizeof(PxBoxGeometry)];
		char sphere[sizeof(PxSphereGeometry)];
		char capsule[sizeof(PxCapsuleGeometry)];
	};	//we need this because we can't use real union since these structs have non trivial constructors

	char Storage[sizeof(StorageUnion)];
};

/** Thresholds for aggregates  */
const uint32 AggregateMaxSize	   = 128;
const uint32 AggregateBodyShapesThreshold	   = 999999999;

/** Global CCD Switch*/
const bool bGlobalCCD = true;

/////// UTILS


/** Perform any deferred cleanup of resources (GPhysXPendingKillConvex etc) */
void DeferredPhysResourceCleanup();

/** Calculates correct impulse at the body's center of mass and adds the impulse to the body. */
ENGINE_API void AddRadialImpulseToPxRigidBody_AssumesLocked(PxRigidBody& PRigidBody, const FVector& Origin, float Radius, float Strength, uint8 Falloff, bool bVelChange);


/** Calculates correct impulse at the body's center of mass and adds the impulse to the body. */
DEPRECATED(4.8, "Please call AddRadialImpulseToPxRigidBody_AssumesLocked and make sure you obtain the appropriate PhysX scene locks")
inline void AddRadialImpulseToPxRigidBody(PxRigidBody& PRigidBody, const FVector& Origin, float Radius, float Strength, uint8 Falloff, bool bVelChange)
{
	AddRadialImpulseToPxRigidBody_AssumesLocked(PRigidBody, Origin, Radius, Strength, Falloff, bVelChange);
}

ENGINE_API void AddRadialForceToPxRigidBody_AssumesLocked(PxRigidBody& PRigidBody, const FVector& Origin, float Radius, float Strength, uint8 Falloff, bool bAccelChange);

/** Calculates correct force at the body's center of mass and adds force to the body. */
DEPRECATED(4.8, "Please call AddRadialImpulseToPxRigidBody_AssumesLocked and make sure you obtain the appropriate PhysX scene locks")
inline void AddRadialForceToPxRigidBody(PxRigidBody& PRigidBody, const FVector& Origin, float Radius, float Strength, uint8 Falloff, bool bAccelChange)
{
	AddRadialForceToPxRigidBody_AssumesLocked(PRigidBody, Origin, Radius, Strength, Falloff, bAccelChange);
}

bool IsRigidBodyNonKinematic_AssumesLocked(const PxRigidBody* PRigidBody);

/** Util to see if a PxRigidActor is non-kinematic */
DEPRECATED(4.8, "Please call IsRigidBodyNonKinematic_AssumesLocked and make sure you obtain the appropriate PhysX scene locks")
inline bool IsRigidBodyNonKinematic(PxRigidBody* PRigidBody)
{
	return IsRigidBodyNonKinematic_AssumesLocked(PRigidBody);
}


/////// GLOBAL POINTERS

/** Pointer to PhysX Foundation singleton */
extern PxFoundation*			GPhysXFoundation;
/** Pointer to PhysX profile zone manager */
extern PxProfileZoneManager*	GPhysXProfileZoneManager;

#if WITH_APEX
/** 
 *	Map from SceneIndex to actual NxApexScene. This indirection allows us to set it to null when we kill the scene, 
 *	and therefore abort trying to destroy PhysX objects after the scene has been destroyed (eg. on game exit). 
 */
extern TMap<int32, NxApexScene*>	GPhysXSceneMap;
#else // #if WITH_APEX
/** 
 *	Map from SceneIndex to actual PxScene. This indirection allows us to set it to null when we kill the scene, 
 *	and therefore abort trying to destroy PhysX objects after the scene has been destroyed (eg. on game exit). 
 */
extern TMap<int32, PxScene*>	GPhysXSceneMap;
#endif // #if WITH_APEX

/** Total number of PhysX convex meshes around currently. */
extern int32					GNumPhysXConvexMeshes;

// The following are used for deferred cleanup - object that cannot be destroyed until all uses have been destroyed, but GC guarantees no order.

/** Array of PxConvexMesh objects which are awaiting cleaning up. */
extern TArray<PxConvexMesh*>	GPhysXPendingKillConvex;

/** Array of PxTriangleMesh objects which are awaiting cleaning up. */
extern ENGINE_API TArray<PxTriangleMesh*>	GPhysXPendingKillTriMesh;

/** Array of PxHeightField objects which are awaiting cleaning up. */
extern ENGINE_API TArray<PxHeightField*>	GPhysXPendingKillHeightfield;

/** Array of PxMaterial objects which are awaiting cleaning up. */
extern TArray<PxMaterial*>		GPhysXPendingKillMaterial;

/** Utility class to keep track of shared physics data */
class FPhysxSharedData
{
public:
	static FPhysxSharedData& Get();

	FPhysxSharedData()
	{
		SharedObjects = PxCreateCollection();
	}

	~FPhysxSharedData()
	{
		SharedObjects->release();
	}

	void Add(PxBase* Obj);
	void Remove(PxBase* Obj)	{ if(Obj) { SharedObjects->remove(*Obj); } }

	const PxCollection* GetCollection()	{ return SharedObjects; }

	void DumpSharedMemoryUsage(FOutputDevice* Ar);
private:
	/** Collection of shared physx objects */
	PxCollection* SharedObjects;

};

ENGINE_API PxCollection* MakePhysXCollection(const TArray<UPhysicalMaterial*>& PhysicalMaterials, const TArray<UBodySetup*>& BodySetups, uint64 BaseId);

/** Utility wrapper for a uint8 TArray for loading into PhysX. */
class FPhysXInputStream : public PxInputStream
{
public:
	/** Raw byte data */
	const uint8	*Data;
	/** Number of bytes */
	int32				DataSize;
	/** Current read position withing Data buffer */
	mutable uint32			ReadPos;

	FPhysXInputStream()
		: Data(NULL)
		, DataSize(0)
		, ReadPos(0)
	{}

	FPhysXInputStream(const uint8 *InData, const int32 InSize)
		: Data(InData)
		, DataSize( InSize )
		, ReadPos(0)
	{}

	virtual PxU32 read(void* Dest, PxU32 Count) override
	{
		check(Data);
		check(Dest);
		check(Count);
		const uint32 EndPos = ReadPos + Count;
		if( EndPos <= (uint32)DataSize )
		{
			FMemory::Memcpy( Dest, Data + ReadPos, Count );
			ReadPos = EndPos;
			return Count;
		}
		else
		{
			return 0;
		}
	}
};

/** Utility class for reading cooked physics data. */
class FPhysXFormatDataReader
{
public:
	TArray< PxConvexMesh* > ConvexMeshes;
	TArray< PxConvexMesh* > ConvexMeshesNegX;
	PxTriangleMesh* TriMesh;
	PxTriangleMesh* TriMeshNegX;

	FPhysXFormatDataReader( FByteBulkData& InBulkData );

private:

	PxConvexMesh* ReadConvexMesh( FBufferReader& Ar, uint8* InBulkDataPtr, int32 InBulkDataSize );
	PxTriangleMesh* ReadTriMesh( FBufferReader& Ar, uint8* InBulkDataPtr, int32 InBulkDataSize );
};

/** PhysX memory allocator wrapper */
class FPhysXAllocator : public PxAllocatorCallback
{
#if PHYSX_MEMORY_STATS
	TMap<FName, size_t> AllocationsByType;

	struct FPhysXAllocationHeader
	{
		FPhysXAllocationHeader(	FName InAllocationTypeName, size_t InAllocationSize )
		:	AllocationTypeName(InAllocationTypeName)
		,	AllocationSize(InAllocationSize)
		{
			static_assert(sizeof(FPhysXAllocationHeader) == 32, "FPhysXAllocationHeader size must be 32 bytes.");
			MagicPadding();
		}

		void MagicPadding()
		{
			for (uint8 ByteCount = 0; ByteCount < sizeof(Padding); ++ByteCount)
			{
				Padding[ByteCount] = 'A' + ByteCount % 4;
			}
		}

		bool operator==(const FPhysXAllocationHeader& OtherHeader) const
		{
			bool bHeaderSame = AllocationTypeName == OtherHeader.AllocationTypeName && AllocationSize == OtherHeader.AllocationSize;
			for (uint8 ByteCount = 0; ByteCount < sizeof(Padding); ++ByteCount)
			{
				bHeaderSame &= Padding[ByteCount] == OtherHeader.Padding[ByteCount];
			}

			return bHeaderSame;
		}

		FName AllocationTypeName;
		size_t	AllocationSize;
		uint8 Padding[8];	//physx needs 16 byte alignment. Additionally we fill padding with a pattern to see if there's any memory stomps

		void Validate() const
		{
			bool bValid = true;
			for (uint8 ByteCount = 0; ByteCount < sizeof(Padding); ++ByteCount)
			{
				bValid &= Padding[ByteCount] == 'A' + ByteCount % 4;
			}

			check(bValid);

			FPhysXAllocationHeader* AllocationFooter = (FPhysXAllocationHeader*) (((uint8*)this) + sizeof(FPhysXAllocationHeader)+AllocationSize);
			check(*AllocationFooter == *this);
		}
	};
	
#endif

public:

#if PHYSX_MEMORY_VALIDATION

	/** Iterates over all allocations and checks that they the headers and footers are valid */
	void ValidateHeaders()
	{
		check(IsInGameThread());
		FPhysXAllocationHeader* TmpHeader = nullptr;
		while(NewHeaders.Dequeue(TmpHeader))
		{
			AllocatedHeaders.Add(TmpHeader);
		}

		while (OldHeaders.Dequeue(TmpHeader))
		{
			AllocatedHeaders.Remove(TmpHeader);
		}

		FScopeLock Lock(&ValidationCS);	//this is needed in case another thread is freeing the header
		for (FPhysXAllocationHeader* Header : AllocatedHeaders)
		{
			Header->Validate();
		}
	}
#endif

	FPhysXAllocator()
	{}

	virtual ~FPhysXAllocator() 
	{}

	virtual void* allocate(size_t size, const char* typeName, const char* filename, int line) override
	{
#if PHYSX_MEMORY_STATS
		INC_DWORD_STAT_BY(STAT_MemoryPhysXTotalAllocationSize, size);


		FString AllocationString = FString::Printf(TEXT("%s %s:%d"), ANSI_TO_TCHAR(typeName), ANSI_TO_TCHAR(filename), line);
		FName AllocationName(*AllocationString);

		// Assign header
		FPhysXAllocationHeader* AllocationHeader = (FPhysXAllocationHeader*)FMemory::Malloc(size + sizeof(FPhysXAllocationHeader) * 2, 16);
		AllocationHeader->AllocationTypeName = AllocationName;
		AllocationHeader->AllocationSize = size;
		AllocationHeader->MagicPadding();
		FPhysXAllocationHeader* AllocationFooter = (FPhysXAllocationHeader*) (((uint8*)AllocationHeader) + size + sizeof(FPhysXAllocationHeader));
		AllocationFooter->AllocationTypeName = AllocationName;
		AllocationFooter->AllocationSize = size;
		AllocationFooter->MagicPadding();

		size_t* TotalByType = AllocationsByType.Find(AllocationName);	//TODO: this is not thread safe!
		if( TotalByType )
		{
			*TotalByType += size;
		}
		else
		{
			AllocationsByType.Add(AllocationName, size);
		}

#if PHYSX_MEMORY_VALIDATION
		NewHeaders.Enqueue(AllocationHeader);
#endif

		return (uint8*)AllocationHeader + sizeof(FPhysXAllocationHeader);
#else
		void* ptr = FMemory::Malloc(size, 16);
		#if PHYSX_MEMORY_STAT_ONLY
			INC_DWORD_STAT_BY(STAT_MemoryPhysXTotalAllocationSize, FMemory::GetAllocSize(ptr));
		#endif
		return ptr;
#endif
	}
	 
	virtual void deallocate(void* ptr) override
	{
#if PHYSX_MEMORY_STATS
		if( ptr )
		{
			FPhysXAllocationHeader* AllocationHeader = (FPhysXAllocationHeader*)((uint8*)ptr - sizeof(FPhysXAllocationHeader));
#if PHYSX_MEMORY_VALIDATION
			AllocationHeader->Validate();
			OldHeaders.Enqueue(AllocationHeader);
			FScopeLock Lock(&ValidationCS);	//this is needed in case we are in the middle of validating the headers
#endif

			DEC_DWORD_STAT_BY(STAT_MemoryPhysXTotalAllocationSize, AllocationHeader->AllocationSize);
			size_t* TotalByType = AllocationsByType.Find(AllocationHeader->AllocationTypeName);
			*TotalByType -= AllocationHeader->AllocationSize;
			FMemory::Free(AllocationHeader);
		}
#else
		#if PHYSX_MEMORY_STAT_ONLY
			DEC_DWORD_STAT_BY(STAT_MemoryPhysXTotalAllocationSize, FMemory::GetAllocSize(ptr));
		#endif
		FMemory::Free(ptr);
#endif
	}

#if PHYSX_MEMORY_STATS
	void DumpAllocations(FOutputDevice* Ar)
	{
		struct FSortBySize
		{
			FORCEINLINE bool operator()( const size_t& A, const size_t& B ) const 
			{ 
				// Sort descending
				return B < A;
			}
		};
				
		AllocationsByType.ValueSort(FSortBySize());
		for( auto It=AllocationsByType.CreateConstIterator(); It; ++It )
		{
			Ar->Logf(TEXT("%-10d %s"), It.Value(), *It.Key().ToString());
		}
	}
#endif

#if PHYSX_MEMORY_VALIDATION
private:
	FCriticalSection ValidationCS;
	TSet<FPhysXAllocationHeader*> AllocatedHeaders;

	//Since this needs to be thread safe we can't add to the allocated headers set until we're on the game thread
	TQueue<FPhysXAllocationHeader*, EQueueMode::Mpsc> NewHeaders;
	TQueue<FPhysXAllocationHeader*, EQueueMode::Mpsc> OldHeaders;

#endif
};

class FPhysXBroadcastingAllocator : public PxBroadcastingAllocator
{

};

/** PhysX output stream wrapper */
class FPhysXErrorCallback : public PxErrorCallback
{
public:
	virtual void reportError(PxErrorCode::Enum e, const char* message, const char* file, int line) override
	{
		// if not in game, ignore Perf warnings - i.e. Moving Static actor in editor will produce this warning
		if (GIsEditor && e == PxErrorCode::ePERF_WARNING)
		{
			return;
		}

		// @MASSIVE HACK - muting 'triangle too big' warning :(
		if(line == 223)
		{
			return;
		}

		// Make string to print out, include physx file/line
		FString ErrorString = FString::Printf( TEXT("PHYSX: %s (%d) %d : %s"), ANSI_TO_TCHAR(file), line, (int32)e, ANSI_TO_TCHAR(message) );

		if(e == PxErrorCode::eOUT_OF_MEMORY ||  e == PxErrorCode::eINTERNAL_ERROR || e == PxErrorCode::eABORT)
		{
			//UE_LOG(LogPhysics, Error, *ErrorString);
			UE_LOG(LogPhysics, Warning, TEXT("%s"), *ErrorString);
		}
		else if(e == PxErrorCode::eINVALID_PARAMETER || e == PxErrorCode::eINVALID_OPERATION || e == PxErrorCode::ePERF_WARNING)
		{
			UE_LOG(LogPhysics, Warning, TEXT("%s"), *ErrorString);
		}
#if UE_BUILD_DEBUG
		else if (e == PxErrorCode::eDEBUG_WARNING)
		{
			UE_LOG(LogPhysics, Warning, TEXT("%s"), *ErrorString);
		}
#endif
		else
		{
			UE_LOG(LogPhysics, Log, TEXT("%s"), *ErrorString);
		}

	}
};

/** 'Shader' used to filter simulation collisions. Could be called on any thread. */
PxFilterFlags PhysXSimFilterShader(	PxFilterObjectAttributes attributes0, PxFilterData filterData0, 
									PxFilterObjectAttributes attributes1, PxFilterData filterData1,
									PxPairFlags& pairFlags, const void* constantBlock, PxU32 constantBlockSize );


/** Event callback used to notify engine about various collision events */
class FPhysXSimEventCallback : public PxSimulationEventCallback
{
	virtual void onConstraintBreak(PxConstraintInfo* constraints, PxU32 count) override;
	virtual void onWake(PxActor** actors, PxU32 count) override {}
	virtual void onSleep(PxActor** actors, PxU32 count) override {}
	virtual void onTrigger(PxTriggerPair* pairs, PxU32 count) override {}
	virtual void onContact(const PxContactPairHeader& PairHeader, const PxContactPair* Pairs, PxU32 NumPairs) override;
};

/** Used to dispatch physx tasks to task graph */
class FPhysXCPUDispatcher : public PxCpuDispatcher
{
	virtual void submitTask(PxBaseTask& task ) override;
	virtual PxU32 getWorkerCount() const override;
};

/** Used to dispatch physx tasks to the game thread */
class FPhysXCPUDispatcherSingleThread : public PxCpuDispatcher
{
	virtual void submitTask( PxBaseTask& task ) override;
	virtual PxU32 getWorkerCount() const override;
};

#if WITH_APEX
/**
	"Null" render resource manager callback for APEX
	This just gives a trivial implementation of the interface, since we are not using the APEX rendering API
*/
class FApexNullRenderResourceManager : public NxUserRenderResourceManager
{
public:
	// NxUserRenderResourceManager interface.

	virtual NxUserRenderVertexBuffer*	createVertexBuffer(const NxUserRenderVertexBufferDesc&) override
	{
		return NULL;
	}
	virtual NxUserRenderIndexBuffer*	createIndexBuffer(const NxUserRenderIndexBufferDesc&) override
	{
		return NULL;
	}
	virtual NxUserRenderBoneBuffer*		createBoneBuffer(const NxUserRenderBoneBufferDesc&) override
	{
		return NULL;
	}
	virtual NxUserRenderInstanceBuffer*	createInstanceBuffer(const NxUserRenderInstanceBufferDesc&) override
	{
		return NULL;
	}
	virtual NxUserRenderSpriteBuffer*   createSpriteBuffer(const NxUserRenderSpriteBufferDesc&) override
	{
		return NULL;
	}
	
	virtual NxUserRenderSurfaceBuffer*  createSurfaceBuffer(const NxUserRenderSurfaceBufferDesc& desc)   override
	{
		return NULL;
	}
	
	virtual NxUserRenderResource*		createResource(const NxUserRenderResourceDesc&) override
	{
		return NULL;
	}
	virtual void						releaseVertexBuffer(NxUserRenderVertexBuffer&) override {}
	virtual void						releaseIndexBuffer(NxUserRenderIndexBuffer&) override {}
	virtual void						releaseBoneBuffer(NxUserRenderBoneBuffer&) override {}
	virtual void						releaseInstanceBuffer(NxUserRenderInstanceBuffer&) override {}
	virtual void						releaseSpriteBuffer(NxUserRenderSpriteBuffer&) override {}
	virtual void                        releaseSurfaceBuffer(NxUserRenderSurfaceBuffer& buffer) override{}
	virtual void						releaseResource(NxUserRenderResource&) override {}

	virtual physx::PxU32				getMaxBonesForMaterial(void*) override
	{
		return 0;
	}
	virtual bool						getSpriteLayoutData(physx::PxU32 , physx::PxU32 , NxUserRenderSpriteBufferDesc* ) override
	{
		return false;
	}
	virtual bool						getInstanceLayoutData(physx::PxU32 , physx::PxU32 , NxUserRenderInstanceBufferDesc* ) override
	{
		return false;
	}

};
extern FApexNullRenderResourceManager GApexNullRenderResourceManager;

/**
	APEX resource callback
	The resource callback is how APEX asks the application to find assets when it needs them
*/
class FApexResourceCallback : public NxResourceCallback
{
public:
	// NxResourceCallback interface.

	virtual void* requestResource(const char* NameSpace, const char* Name) override
	{
		// Here a pointer is looked up by name and returned
		(void)NameSpace;
		(void)Name;

		return NULL;
	}

	virtual void  releaseResource(const char* NameSpace, const char* Name, void* Resource) override
	{
		// Here we release a named resource
		(void)NameSpace;
		(void)Name;
		(void)Resource;
	}
};
extern FApexResourceCallback GApexResourceCallback;

/**
	APEX PhysX3 interface
	This interface allows us to modify the PhysX simulation filter shader data with contact pair flags 
*/
class FApexPhysX3Interface : public NxApexPhysX3Interface
{
public:
	// NxApexPhysX3Interface interface.

	virtual void				setContactReportFlags(physx::PxShape* PShape, physx::PxPairFlags PFlags, NxDestructibleActor* actor, PxU16 actorChunkIndex) override;

	virtual physx::PxPairFlags	getContactReportFlags(const physx::PxShape* PShape) const override;
};
extern FApexPhysX3Interface GApexPhysX3Interface;

/**
	APEX Destructible chunk report interface
	This interface delivers summaries (which can be detailed to the single chunk level, depending on the settings)
	of chunk fracture and destroy events.
*/
class FApexChunkReport : public NxUserChunkReport
{
public:
	// NxUserChunkReport interface.

	virtual void	onDamageNotify(const NxApexDamageEventReportData& damageEvent) override;
	virtual void	onStateChangeNotify(const NxApexChunkStateEventData& visibilityEvent) override;
	virtual bool	releaseOnNoChunksVisible(const NxDestructibleActor* destructible) override;
};
extern FApexChunkReport GApexChunkReport;
#endif // #if WITH_APEX

/** Util to determine whether to use NegX version of mesh, and what transform (rotation) to apply. */
bool CalcMeshNegScaleCompensation(const FVector& InScale3D, PxTransform& POutTransform);

/** Utility wrapper for a PhysX output stream that only counts the memory. */
class FPhysXCountMemoryStream : public PxOutputStream
{
public:
	/** Memory used by the serialized object(s) */
	uint32 UsedMemory;

	FPhysXCountMemoryStream()
		: UsedMemory(0)
	{}

	virtual PxU32 write(const void* Src, PxU32 Count) override
	{
		UsedMemory += Count;
		return Count;
	}
};

/** 
 * Returns the in-memory size of the specified object by serializing it.
 *
 * @param	Obj					Object to determine the memory footprint for
 * @param	SharedCollection	Shared collection of data to ignore
 * @returns						Size of the object in bytes determined by serialization
 **/
ENGINE_API SIZE_T GetPhysxObjectSize(PxBase* Obj, const PxCollection* SharedCollection);
#endif // WITH_PHYSX


#include "../Collision/PhysicsFiltering.h"

/** Helper struct holding physics body filter data during initialisation */
struct FShapeFilterData
{
	PxFilterData SimFilter;
	PxFilterData QuerySimpleFilter;
	PxFilterData QueryComplexFilter;
};

/** Helper object to hold initialisation data for shapes */
struct FShapeData
{
	FShapeData()
		: SyncShapeFlags(0)
		, AsyncShapeFlags(0)
		, SimpleShapeFlags(0)
		, ComplexShapeFlags(0)
		, SyncBodyFlags(0)
		, AsyncBodyFlags(0)
	{

	}

	TEnumAsByte<ECollisionEnabled::Type> CollisionEnabled;
	FShapeFilterData FilterData;
	PxShapeFlags SyncShapeFlags;
	PxShapeFlags AsyncShapeFlags;
	PxShapeFlags SimpleShapeFlags;
	PxShapeFlags ComplexShapeFlags;
	PxRigidBodyFlags SyncBodyFlags;
	PxRigidBodyFlags AsyncBodyFlags;
};