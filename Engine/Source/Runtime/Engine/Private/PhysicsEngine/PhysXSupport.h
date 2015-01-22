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
#ifndef PHYSX_MEMORY_STATS
#define PHYSX_MEMORY_STATS		0
#endif

#if USE_SCENE_LOCK

/** Scoped scene read lock - we use this instead of PxSceneReadLock because it handles NULL scene */
class FPhysXSceneReadLock
{
public:
	
	FPhysXSceneReadLock(PxScene* PInScene)
		: PScene(PInScene)
	{
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

#define SCOPED_SCENE_TOKENPASTE_INNER(x,y) x##y
#define SCOPED_SCENE_TOKENPASTE(x,y) SCOPED_SCENE_TOKENPASTE_INNER(x,y)
#define SCOPED_SCENE_READ_LOCK( _scene ) FPhysXSceneReadLock SCOPED_SCENE_TOKENPASTE(_rlock,__LINE__)(_scene)
#define SCOPED_SCENE_WRITE_LOCK( _scene ) FPhysXSceneWriteLock SCOPED_SCENE_TOKENPASTE(_wlock,__LINE__)(_scene)

#define SCENE_LOCK_READ( _scene ) if((_scene) != NULL) { (_scene)->lockRead(); }
#define SCENE_UNLOCK_READ( _scene ) if((_scene) != NULL) { (_scene)->unlockRead(); }
#define SCENE_LOCK_WRITE( _scene ) if((_scene) != NULL) { (_scene)->lockWrite(); }
#define SCENE_UNLOCK_WRITE( _scene ) if((_scene) != NULL) { (_scene)->unlockWrite(); }
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

/** Get a pointer to the PxScene from an SceneIndex (will be NULL if scene already shut down) */
PxScene* GetPhysXSceneFromIndex(int32 InSceneIndex);

#if WITH_APEX
/** Get a pointer to the NxApexScene from an SceneIndex (will be NULL if scene already shut down) */
NxApexScene* GetApexSceneFromIndex(int32 InSceneIndex);
#endif


/** Perform any deferred cleanup of resources (GPhysXPendingKillConvex etc) */
void DeferredPhysResourceCleanup();

/** Calculates correct impulse at the body's center of mass and adds the impulse to the body. */
ENGINE_API void AddRadialImpulseToPxRigidBody(PxRigidBody& PRigidBody, const FVector& Origin, float Radius, float Strength, uint8 Falloff, bool bVelChange);

/** Calculates correct force at the body's center of mass and adds force to the body. */
ENGINE_API void AddRadialForceToPxRigidBody(PxRigidBody& PRigidBody, const FVector& Origin, float Radius, float Strength, uint8 Falloff);

/** Util to see if a PxRigidActor is non-kinematic */
bool IsRigidBodyNonKinematic(PxRigidBody* PRigidBody);


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
		{}

		FName AllocationTypeName;
		size_t	AllocationSize;
	};
	
#endif

public:
	FPhysXAllocator()
	{}

	virtual ~FPhysXAllocator() 
	{}

	virtual void* allocate(size_t size, const char* typeName, const char* filename, int line) override
	{
#if PHYSX_MEMORY_STATS
		static_assert(sizeof(FPhysXAllocationHeader) <= 16, "FPhysXAllocationHeader size must be less than 16 bytes.");

		INC_DWORD_STAT_BY(STAT_MemoryPhysXTotalAllocationSize, size);

		FString AllocationString = FString::Printf(TEXT("%s %s:%d"), ANSI_TO_TCHAR(typeName), ANSI_TO_TCHAR(filename), line);
		FName AllocationName(*AllocationString);

		// Assign header
		FPhysXAllocationHeader* AllocationHeader = (FPhysXAllocationHeader*)FMemory::Malloc(size + 16, 16);
		AllocationHeader->AllocationTypeName = AllocationName;
		AllocationHeader->AllocationSize = size;

		// Assign map to track by type
		size_t* TotalByType = AllocationsByType.Find(AllocationName);
		if( TotalByType )
		{
			*TotalByType += size;
		}
		else
		{
			AllocationsByType.Add(AllocationName, size);
		}

		return (uint8*)AllocationHeader + 16;
#else
		return FMemory::Malloc(size, 16);
#endif
	}
	 
	virtual void deallocate(void* ptr) override
	{
#if PHYSX_MEMORY_STATS
		if( ptr )
		{
			FPhysXAllocationHeader* AllocationHeader = (FPhysXAllocationHeader*)((uint8*)ptr - 16);
			DEC_DWORD_STAT_BY(STAT_MemoryPhysXTotalAllocationSize, AllocationHeader->AllocationSize);
			size_t* TotalByType = AllocationsByType.Find(AllocationHeader->AllocationTypeName);
			*TotalByType -= AllocationHeader->AllocationSize;
			FMemory::Free(AllocationHeader);
		}
#else
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
