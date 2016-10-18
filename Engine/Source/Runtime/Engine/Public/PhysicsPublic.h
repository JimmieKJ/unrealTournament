// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PhysicsPublic.h
	Rigid Body Physics Public Types
=============================================================================*/

#pragma once

#include "PhysxUserData.h"
#include "DynamicMeshBuilder.h"
#include "LocalVertexFactory.h"
#include "PhysicsEngine/RigidBodyIndexPair.h"
/**
 * Physics stats
 */
DECLARE_CYCLE_STAT_EXTERN(TEXT("FetchAndStart Time (all)"), STAT_TotalPhysicsTime, STATGROUP_Physics, );

#if WITH_PHYSX

namespace physx
{
	class PxScene;
	class PxConvexMesh;
	class PxTriangleMesh;
	class PxCooking;
	class PxPhysics;
	class PxVec3;
	class PxJoint;
	class PxMat44;
	class PxCpuDispatcher;
	class PxSimulationEventCallback;
	struct PxActiveTransform;
	class PxActor;
	class PxRigidActor;
	
#if WITH_APEX
	namespace apex
	{
		class NxDestructibleAsset;
		class NxApexScene;
		struct NxApexDamageEventReportData;
		class NxApexSDK;
		class NxModuleDestructible;
		class NxDestructibleActor;
		class NxModuleClothing;
		class NxModule;
		class NxClothingActor;
		class NxClothingAsset;
		class NxApexInterface;
	}
#endif // WITH_APEX
}

using namespace physx;
#if WITH_APEX
	using namespace physx::apex;
#endif	//WITH_APEX

/** Pointer to PhysX SDK object */
extern ENGINE_API PxPhysics*			GPhysXSDK;
/** Pointer to PhysX cooking object */
extern ENGINE_API PxCooking*			GPhysXCooking;
/** Pointer to PhysX allocator */
extern ENGINE_API class FPhysXAllocator* GPhysXAllocator;

/** Pointer to PhysX Command Handler */
extern ENGINE_API class FPhysCommandHandler* GPhysCommandHandler;

#if WITH_APEX

namespace NxParameterized
{
	class Interface;
}

/** Pointer to APEX SDK object */
extern ENGINE_API NxApexSDK*			GApexSDK;
/** Pointer to APEX Destructible module object */
extern ENGINE_API NxModuleDestructible*	GApexModuleDestructible;
/** Pointer to APEX legacy module object */
extern ENGINE_API NxModule* 			GApexModuleLegacy;
#if WITH_APEX_CLOTHING
/** Pointer to APEX Clothing module object */
extern ENGINE_API NxModuleClothing*		GApexModuleClothing;
#endif //WITH_APEX_CLOTHING

#else

namespace NxParameterized
{
	typedef void Interface;
};

#endif // #if WITH_APEX

#endif // WITH_PHYSX

/** Information about a specific object involved in a rigid body collision */
struct ENGINE_API FRigidBodyCollisionInfo
{
	/** Actor involved in the collision */
	TWeakObjectPtr<AActor>					Actor;

	/** Component of Actor involved in the collision. */
	TWeakObjectPtr<UPrimitiveComponent>		Component;

	/** Index of body if this is in a PhysicsAsset. INDEX_NONE otherwise. */
	int32									BodyIndex;

	/** Name of bone if a PhysicsAsset */
	FName									BoneName;

	FRigidBodyCollisionInfo() :
		BodyIndex(INDEX_NONE),
		BoneName(NAME_None)
	{}

	/** Utility to set up the body collision info from an FBodyInstance */
	void SetFrom(const FBodyInstance* BodyInst);
	/** Get body instance */
	FBodyInstance* GetBodyInstance() const;
};

/** One entry in the array of collision notifications pending execution at the end of the physics engine run. */
struct ENGINE_API FCollisionNotifyInfo
{
	/** If this notification should be called for the Actor in Info0. */
	bool							bCallEvent0;

	/** If this notification should be called for the Actor in Info1. */
	bool							bCallEvent1;

	/** Information about the first object involved in the collision. */
	FRigidBodyCollisionInfo			Info0;

	/** Information about the second object involved in the collision. */
	FRigidBodyCollisionInfo			Info1;

	/** Information about the collision itself */
	FCollisionImpactData			RigidCollisionData;

	FCollisionNotifyInfo() :
		bCallEvent0(false),
		bCallEvent1(false)
	{}

	/** Check that is is valid to call a notification for this entry. Looks at the IsPendingKill() flags on both Actors. */
	bool IsValidForNotify() const;
};

namespace PhysCommand
{
	enum Type
	{
		Release,
		ReleasePScene,
		DeleteCPUDispatcher,
		DeleteSimEventCallback,
		Max
	};
}


/** Container used for physics tasks that need to be deferred from GameThread. This is not safe for general purpose multi-therading*/
class FPhysCommandHandler
{
public:

	~FPhysCommandHandler();
	
	/** Executes pending commands and clears buffer **/
	void ENGINE_API Flush();
	bool ENGINE_API HasPendingCommands();

#if WITH_APEX
	/** enqueues a command to release destructible actor once apex has finished simulating */
	void ENGINE_API DeferredRelease(physx::apex::NxApexInterface* ApexInterface);
#endif

#if WITH_PHYSX
	void ENGINE_API DeferredRelease(physx::PxScene * PScene);
	void ENGINE_API DeferredDeleteSimEventCallback(physx::PxSimulationEventCallback * SimEventCallback);
	void ENGINE_API DeferredDeleteCPUDispathcer(physx::PxCpuDispatcher * CPUDispatcher);
#endif

private:

	/** Command to execute when physics simulation is done */
	struct FPhysPendingCommand
	{
		union
		{
#if WITH_APEX
			physx::apex::NxApexInterface * ApexInterface;
			physx::apex::NxDestructibleActor * DestructibleActor;
#endif
#if WITH_PHYSX
			physx::PxScene * PScene;
			physx::PxCpuDispatcher * CPUDispatcher;
			physx::PxSimulationEventCallback * SimEventCallback;
#endif
		} Pointer;

		PhysCommand::Type CommandType;
	};

	/** Execute all enqueued commands */
	void ExecuteCommands();

	/** Enqueue a command to the double buffer */
	void EnqueueCommand(const FPhysPendingCommand& Command);

	/** Array of commands waiting to execute once simulation is done */
	TArray<FPhysPendingCommand> PendingCommands;
};

namespace SleepEvent
{
	enum Type
	{
		SET_Wakeup,
		SET_Sleep
	};

}

/** Buffers used as scratch space for PhysX to avoid allocations during simulation */
struct FSimulationScratchBuffer
{
	FSimulationScratchBuffer()
		: Buffer(nullptr)
		, BufferSize(0)
	{}

	// The scratch buffer
	uint8* Buffer;

	// Allocated size of the buffer
	int32 BufferSize;
};

#if WITH_PHYSX
/** Interface for the creation of customized simulation event callbacks. */
class ISimEventCallbackFactory
{
public:
	virtual physx::PxSimulationEventCallback* Create(class FPhysScene* PhysScene, int32 SceneType) = 0;
	virtual void Destroy(physx::PxSimulationEventCallback* Callback) = 0;
};
#endif // WITH PHYSX


/** Container object for a physics engine 'scene'. */

class FPhysScene
{
public:
	/** Indicates whether the async scene is enabled or not. */
	bool							bAsyncSceneEnabled;

	/** Indicates whether the scene is using substepping */
	bool							bSubstepping;

	/** Indicates whether the scene is using substepping */
	bool							bSubsteppingAsync;

	/** Stores the number of valid scenes we are working with. This will be PST_MAX or PST_Async, 
		depending on whether the async scene is enabled or not*/
	uint32							NumPhysScenes;
	
	/** Gets the array of collision notifications, pending execution at the end of the physics engine run. */
	TArray<FCollisionNotifyInfo>& GetPendingCollisionNotifies(int32 SceneType){ return PendingCollisionData[SceneType].PendingCollisionNotifies; }

private:
	/** World that owns this physics scene */
	UWorld*							OwningWorld;

public:
	//Owning world is made private so that any code which depends on setting an owning world can update
	void SetOwningWorld(UWorld* InOwningWorld);
	UWorld* GetOwningWorld(){ return OwningWorld; }
	const UWorld* GetOwningWorld() const { return OwningWorld; }

	/** These indices are used to get the actual PxScene or NxApexScene from the GPhysXSceneMap. */
	int16								PhysXSceneIndex[PST_MAX];

	/** Whether or not the given scene is between its execute and sync point. */
	bool							bPhysXSceneExecuting[PST_MAX];

	/** Frame time, weighted with current frame time. */
	float							AveragedFrameTime[PST_MAX];

	/**
	 * Weight for averaged frame time.  Value should be in the range [0.0f, 1.0f].
	 * Weight = 0.0f => no averaging; current frame time always used.
	 * Weight = 1.0f => current frame time ignored; initial value of AveragedFrameTime[i] is always used.
	 */
	float							FrameTimeSmoothingFactor[PST_MAX];

#if WITH_PHYSX
	/** Flush the deferred actor and instance arrays, either adding or removing from the scene */
	void FlushDeferredActors();

	/** Defer the addition of an actor to a scene, this will actually be performed before the *next*
	 *  Physics tick
	 *	@param OwningInstance - The FBodyInstance that owns the actor
	 *	@param Actor - The actual PhysX actor to add
	 *	@param SceneType - The scene type to add the actor to
	 */
	void DeferAddActor(FBodyInstance* OwningInstance, PxActor* Actor, EPhysicsSceneType SceneType);

	/** Defer the addition of a group of actors to a scene, this will actually be performed before the *next*
	 *  Physics tick. 
	 *
	 *	@param OwningInstance - The FBodyInstance that owns the actor
	 *	@param Actor - The actual PhysX actor to add
	 *	@param SceneType - The scene type to add the actor to
	 */
	void DeferAddActors(TArray<FBodyInstance*>& OwningInstances, TArray<PxActor*>& Actors, EPhysicsSceneType SceneType);

	/** Defer the removal of an actor to a scene, this will actually be performed before the *next*
	 *  Physics tick
	 *	@param OwningInstance - The FBodyInstance that owns the actor
	 *	@param Actor - The actual PhysX actor to add
	 *	@param SceneType - The scene type to add the actor to
	 */
	void DeferRemoveActor(FBodyInstance* OwningInstance, PxActor* Actor, EPhysicsSceneType SceneType);

	void AddPendingSleepingEvent(PxActor* Actor, SleepEvent::Type SleepEventType, int32 SceneType);
#endif

private:
	/** DeltaSeconds from UWorld. */
	float										DeltaSeconds;
	/** DeltaSeconds from the WorldSettings. */
	float										MaxPhysicsDeltaTime;
	/** DeltaSeconds used by the last synchronous scene tick.  This may be used for the async scene tick. */
	float										SyncDeltaSeconds;
	/** LineBatcher from UWorld. */
	ULineBatchComponent*						LineBatcher;

	/** Completion event (not tasks) for the physics scenes these are fired by the physics system when it is done; prerequisites for the below */
	FGraphEventRef PhysicsSubsceneCompletion[PST_MAX];
	/** Completion events (not tasks) for the frame lagged physics scenes these are fired by the physics system when it is done; prerequisites for the below */
	FGraphEventRef FrameLaggedPhysicsSubsceneCompletion[PST_MAX];
	/** Completion events (task) for the physics scenes	(both apex and non-apex). This is a "join" of the above. */
	FGraphEventRef PhysicsSceneCompletion;

	// Data for scene scratch buffers, these will be allocated once on FPhysScene construction and used
	// for the calls to PxScene::simulate to save it calling into the OS to allocate during simulation
	FSimulationScratchBuffer SimScratchBuffers[PST_MAX];

#if WITH_PHYSX

	struct FDeferredSceneData
	{
		/** The PhysX scene index used*/
		int32 SceneIndex;

		/** Body instances awaiting scene add */
		TArray<FBodyInstance*> AddInstances;
		/** PhysX Actors awaiting scene add */
		TArray<PxActor*> AddActors;

		/** Body instances awaiting scene remove */
		TArray<FBodyInstance*> RemoveInstances;
		/** PhysX Actors awaiting scene remove */
		TArray<PxActor*> RemoveActors;

		void FlushDeferredActors();
		void DeferAddActor(FBodyInstance* OwningInstance, PxActor* Actor);
		void DeferAddActors(TArray<FBodyInstance*>& OwningInstances, TArray<PxActor*>& Actors);
		void DeferRemoveActor(FBodyInstance* OwningInstance, PxActor* Actor);
	};

	FDeferredSceneData DeferredSceneData[PST_MAX];


	/** Dispatcher for CPU tasks */
	class PxCpuDispatcher*			CPUDispatcher[PST_MAX];
	/** Simulation event callback object */
	physx::PxSimulationEventCallback*			SimEventCallback[PST_MAX];
#if WITH_VEHICLE
	/** Vehicle scene */
	class FPhysXVehicleManager*			VehicleManager;
#endif
#endif	//

	struct FPendingCollisionData
	{
		/** Array of collision notifications, pending execution at the end of the physics engine run. */
		TArray<FCollisionNotifyInfo>	PendingCollisionNotifies;
	};

	FPendingCollisionData PendingCollisionData[PST_MAX];

public:

#if WITH_PHYSX
	/** Static factory used to override the simulation event callback from other modules.
	If not set it defaults to using FPhysXSimEventCallback. */
	ENGINE_API static TSharedPtr<ISimEventCallbackFactory> SimEventCallbackFactory;


	/** Utility for looking up the PxScene of the given EPhysicsSceneType associated with this FPhysScene.  SceneType must be in the range [0,PST_MAX). */
	ENGINE_API physx::PxScene*					GetPhysXScene(uint32 SceneType);

#if WITH_VEHICLE
	/** Get the vehicle manager */
	FPhysXVehicleManager*						GetVehicleManager();
#endif
#endif

#if WITH_APEX
	/** Utility for looking up the NxApexScene of the given EPhysicsSceneType associated with this FPhysScene.  SceneType must be in the range [0,PST_MAX). */
	physx::apex::NxApexScene*				GetApexScene(uint32 SceneType);
#endif
	ENGINE_API FPhysScene();
	ENGINE_API ~FPhysScene();

	/** Start simulation on the physics scene of the given type */
	ENGINE_API void TickPhysScene(uint32 SceneType, FGraphEventRef& InOutCompletionEvent);

	/** Set the gravity and timing of all physics scenes */
	ENGINE_API void SetUpForFrame(const FVector* NewGrav, float InDeltaSeconds = 0.0f, float InMaxPhysicsDeltaTime = 0.0f);

	/** Starts a frame */
	ENGINE_API void StartFrame();

	/** Ends a frame */
	ENGINE_API void EndFrame(ULineBatchComponent* InLineBatcher);

	/** Starts cloth Simulation*/
	ENGINE_API void StartAsync();

	/** returns the completion event for a frame */
	FGraphEventRef GetCompletionEvent()
	{
		return PhysicsSceneCompletion;
	}

	/** Ensures that the collision tree is built. */
	ENGINE_API void EnsureCollisionTreeIsBuilt(UWorld* World);

	/** Waits for all physics scenes to complete */
	ENGINE_API void WaitPhysScenes();

	/** Kill the visual debugger */
	ENGINE_API void KillVisualDebugger();

	/** Fetches results, fires events, and adds debug lines */
	void ProcessPhysScene(uint32 SceneType);

	/** Sync components in the scene to physics bodies that changed */
	void SyncComponentsToBodies_AssumesLocked(uint32 SceneType);

	/** Call after WaitPhysScene on the synchronous scene to make deferred OnRigidBodyCollision calls.  */
	void DispatchPhysNotifications_AssumesLocked();

	/** Add any debug lines from the physics scene of the given type to the supplied line batcher. */
	ENGINE_API void AddDebugLines(uint32 SceneType, class ULineBatchComponent* LineBatcherToUse);

	/** @return Whether physics scene supports scene origin shifting */
	static bool SupportsOriginShifting() { return true; }

	/** @return Whether physics scene is using substepping */
	bool IsSubstepping(uint32 SceneType) const
	{
		if (SceneType == PST_Sync) return bSubstepping;
		if (SceneType == PST_Async) return bSubsteppingAsync;
		return false;
	}
	
	/** Shifts physics scene origin by specified offset */
	void ApplyWorldOffset(FVector InOffset);

	/** Returns whether an async scene is setup and can be used. This depends on the console variable "p.EnableAsyncScene". */
	ENGINE_API bool HasAsyncScene() const { return bAsyncSceneEnabled; }

	/** Lets the scene update anything related to this BodyInstance as it's now being terminated */
	DEPRECATED(4.8, "Please call AddCustomPhysics_AssumesLocked and make sure you obtain the appropriate PhysX scene locks")
	void TermBody(FBodyInstance* BodyInstance)
	{
		TermBody_AssumesLocked(BodyInstance);
	}

	/** Lets the scene update anything related to this BodyInstance as it's now being terminated */
	void TermBody_AssumesLocked(FBodyInstance* BodyInstance);

	/** Add a custom callback for next step that will be called on every substep */
	DEPRECATED(4.8, "Please call AddCustomPhysics_AssumesLocked and make sure you obtain the appropriate PhysX scene locks")
	void AddCustomPhysics(FBodyInstance* BodyInstance, FCalculateCustomPhysics& CalculateCustomPhysics)
	{
		AddCustomPhysics_AssumesLocked(BodyInstance, CalculateCustomPhysics);
	}

	/** Add a custom callback for next step that will be called on every substep */
	void AddCustomPhysics_AssumesLocked(FBodyInstance* BodyInstance, FCalculateCustomPhysics& CalculateCustomPhysics);

	/** Adds a force to a body - We need to go through scene to support substepping */
	DEPRECATED(4.8, "Please call AddForce_AssumesLocked and make sure you obtain the appropriate PhysX scene locks")
	void AddForce(FBodyInstance* BodyInstance, const FVector& Force, bool bAllowSubstepping, bool bAccelChange)
	{
		AddForce_AssumesLocked(BodyInstance, Force, bAllowSubstepping, bAccelChange);
	}

	void AddForce_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Force, bool bAllowSubstepping, bool bAccelChange);

	/** Adds a force to a body at a specific position - We need to go through scene to support substepping */
	DEPRECATED(4.8, "Please call AddForceAtPosition_AssumesLocked and make sure you obtain the appropriate PhysX scene locks")
	void AddForceAtPosition(FBodyInstance* BodyInstance, const FVector& Force, const FVector& Position, bool bAllowSubstepping)
	{
		AddForceAtPosition_AssumesLocked(BodyInstance, Force, Position, bAllowSubstepping);
	}

	/** Adds a force to a body at a specific position - We need to go through scene to support substepping */
	void AddForceAtPosition_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Force, const FVector& Position, bool bAllowSubstepping);

	/** Adds a radial force to a body - We need to go through scene to support substepping */
	DEPRECATED(4.8, "Please call AddRadialForceToBody_AssumesLocked and make sure you obtain the appropriate PhysX scene locks")
	void AddRadialForceToBody(FBodyInstance* BodyInstance, const FVector& Origin, const float Radius, const float Strength, const uint8 Falloff, bool bAccelChange, bool bAllowSubstepping)
	{
		AddRadialForceToBody_AssumesLocked(BodyInstance, Origin, Radius, Strength, Falloff, bAccelChange, bAllowSubstepping);
	}

	/** Adds a radial force to a body - We need to go through scene to support substepping */
	void AddRadialForceToBody_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Origin, const float Radius, const float Strength, const uint8 Falloff, bool bAccelChange, bool bAllowSubstepping);

	/** Adds torque to a body - We need to go through scene to support substepping */
	DEPRECATED(4.8, "Please call AddTorque_AssumesLocked and make sure you obtain the appropriate PhysX scene locks")
	void AddTorque(FBodyInstance* BodyInstance, const FVector& Torque, bool bAllowSubstepping, bool bAccelChange)
	{
		AddTorque_AssumesLocked(BodyInstance, Torque, bAllowSubstepping, bAccelChange);
	}

	/** Adds torque to a body - We need to go through scene to support substepping */
	void AddTorque_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Torque, bool bAllowSubstepping, bool bAccelChange);

	/** Sets a Kinematic actor's target position - We need to do this here to support substepping*/
	DEPRECATED(4.8, "Please call SetKinematicTarget_AssumesLocked and make sure you obtain the appropriate PhysX scene locks")
	void SetKinematicTarget(FBodyInstance* BodyInstance, const FTransform& TargetTM, bool bAllowSubstepping)
	{
		SetKinematicTarget_AssumesLocked(BodyInstance, TargetTM, bAllowSubstepping);
	}
	
	/** Sets a Kinematic actor's target position - We need to do this here to support substepping*/
	void SetKinematicTarget_AssumesLocked(FBodyInstance* BodyInstance, const FTransform& TargetTM, bool bAllowSubstepping);

	/** Gets a Kinematic actor's target position - We need to do this here to support substepping
	  * Returns true if kinematic target has been set. If false the OutTM is invalid */
	DEPRECATED(4.8, "Please call GetKinematicTarget_AssumesLocked and make sure you obtain the appropriate PhysX scene locks")
	bool GetKinematicTarget(const FBodyInstance* BodyInstance, FTransform& OutTM) const
	{
		return GetKinematicTarget_AssumesLocked(BodyInstance, OutTM);
	}

	/** Gets a Kinematic actor's target position - We need to do this here to support substepping
	  * Returns true if kinematic target has been set. If false the OutTM is invalid */
	bool GetKinematicTarget_AssumesLocked(const FBodyInstance* BodyInstance, FTransform& OutTM) const;

	/** Gets the collision disable table */
	const TMap<uint32, TMap<struct FRigidBodyIndexPair, bool> *> & GetCollisionDisableTableLookup()
	{
		return CollisionDisableTableLookup;
	}

	/** Adds to queue of skelmesh we want to add to collision disable table */
	ENGINE_API void DeferredAddCollisionDisableTable(uint32 SkelMeshCompID, TMap<struct FRigidBodyIndexPair, bool> * CollisionDisableTable);

	/** Adds to queue of skelmesh we want to remove from collision disable table */
	ENGINE_API void DeferredRemoveCollisionDisableTable(uint32 SkelMeshCompID);

#if WITH_APEX
	/** Adds a damage event to be fired when fetchResults is done */
	void AddPendingDamageEvent(class UDestructibleComponent* DestructibleComponent, const NxApexDamageEventReportData& DamageEvent);
#endif

	/** Add this SkeletalMeshComponent to the list needing kinematic bodies updated before simulating physics */
	void MarkForPreSimKinematicUpdate(USkeletalMeshComponent* InSkelComp, ETeleportType InTeleport, bool bNeedsSkinning);

	/** Remove this SkeletalMeshComponent from set needing kinematic update before simulating physics*/
	void ClearPreSimKinematicUpdate(USkeletalMeshComponent* InSkelComp);

private:
	/** Initialize a scene of the given type.  Must only be called once for each scene type. */
	void InitPhysScene(uint32 SceneType);

	/** Terminate a scene of the given type.  Must only be called once for each scene type. */
	void TermPhysScene(uint32 SceneType);

	/** Called when all subscenes of a given scene are complete, calls  ProcessPhysScene*/
	void SceneCompletionTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent, EPhysicsSceneType SceneType);

	/** Helper function for determining which scene a dyanmic body is in*/
	EPhysicsSceneType SceneType_AssumesLocked(const FBodyInstance* BodyInstance) const;

	/** Process kinematic updates on any deferred skeletal meshes */
	void UpdateKinematicsOnDeferredSkelMeshes();

	/** Task created from TickPhysScene so we can substep without blocking */
	bool SubstepSimulation(uint32 SceneType, FGraphEventRef& InOutCompletionEvent);

	/** Set whether we're doing a static load and want to stall, or are during gameplay and want to distribute over many frames */
	void SetIsStaticLoading(bool bStaticLoading);

#if WITH_PHYSX
	/** User data wrapper passed to physx */
	struct FPhysxUserData PhysxUserData;

	/** Cache of active transforms sorted into types */ //TODO: this solution is not great
	TArray<struct FBodyInstance*> ActiveBodyInstances[PST_MAX];	//body instances that have moved
	TArray<const physx::PxRigidActor*> ActiveDestructibleActors[PST_MAX];	//destructible actors that have moved

	/** Fetch results from simulation and get the active transforms. Make sure to lock before calling this function as the fetch and data you use must be treated as an atomic operation */
	void UpdateActiveTransforms(uint32 SceneType);
	void RemoveActiveBody_AssumesLocked(FBodyInstance* BodyInstance, uint32 SceneType);

#endif

	class FPhysSubstepTask * PhysSubSteppers[PST_MAX];

#if WITH_APEX
	TUniquePtr<struct FPendingApexDamageManager> PendingApexDamageManager;
#endif

	struct FPendingCollisionDisableTable
	{
		uint32 SkelMeshCompID;
		TMap<struct FRigidBodyIndexPair, bool>* CollisionDisableTable;
	};

	/** Updates CollisionDisableTableLookup with the deferred insertion and deletion */
	void FlushDeferredCollisionDisableTableQueue();

	/** Queue of deferred collision table insertion and deletion */
	TArray<FPendingCollisionDisableTable> DeferredCollisionDisableTableQueue;

	/** Map from SkeletalMeshComponent UniqueID to a pointer to the collision disable table inside its PhysicsAsset */
	TMap< uint32, TMap<struct FRigidBodyIndexPair, bool>* >		CollisionDisableTableLookup;

#if WITH_PHYSX
	TMap<PxActor*, SleepEvent::Type> PendingSleepEvents[PST_MAX];
#endif

	/** Information about how to perform kinematic update before physics */
	struct FDeferredKinematicUpdateInfo
	{
		/** Whether to teleport physics bodies or not */
		ETeleportType	TeleportType;
		/** Whether to update skinning info */
		bool			bNeedsSkinning;
	};

	/** Map of SkeletalMeshComponents that need their bone transforms sent to the physics engine before simulation. */
	TMap<USkeletalMeshComponent*, FDeferredKinematicUpdateInfo>	DeferredKinematicUpdateSkelMeshes;

	FDelegateHandle PreGarbageCollectDelegateHandle;
};

/**
* Return true if we should be running in single threaded mode, ala dedicated server
**/
FORCEINLINE bool PhysSingleThreadedMode()
{
	if (IsRunningDedicatedServer() || FPlatformMisc::NumberOfCores() < 3 || !FPlatformProcess::SupportsMultithreading())
	{
		return true;
	}
	return false;
}

#if WITH_PHYSX
/** Struct used for passing info to the PhysX shader */

struct FPhysSceneShaderInfo
{
	FPhysScene * PhysScene;
};

#endif

/** Enum to indicate types of simple shapes */
enum EKCollisionPrimitiveType
{
	KPT_Sphere = 0,
	KPT_Box,
	KPT_Sphyl,
	KPT_Convex,
	KPT_Unknown
};

// Only used for legacy serialization (ver < VER_UE4_REMOVE_PHYS_SCALED_GEOM_CACHES)
class FKCachedConvexDataElement
{
public:
	TArray<uint8>	ConvexElementData;

	friend FArchive& operator<<( FArchive& Ar, FKCachedConvexDataElement& S )
	{
		S.ConvexElementData.BulkSerialize(Ar);
		return Ar;
	}
};

// Only used for legacy serialization (ver < VER_UE4_REMOVE_PHYS_SCALED_GEOM_CACHES)
class FKCachedConvexData
{
public:
	TArray<FKCachedConvexDataElement>	CachedConvexElements;

	friend FArchive& operator<<( FArchive& Ar, FKCachedConvexData& S )
	{
		Ar << S.CachedConvexElements;
		return Ar;
	}
};

// Only used for legacy serialization (ver < VER_UE4_ADD_BODYSETUP_GUID)
struct FKCachedPerTriData
{
	TArray<uint8> CachedPerTriData;

	friend FArchive& operator<<( FArchive& Ar, FKCachedPerTriData& S )
	{
		S.CachedPerTriData.BulkSerialize(Ar);
		return Ar;
	}
};



class FConvexCollisionVertexBuffer : public FVertexBuffer 
{
public:
	TArray<FDynamicMeshVertex> Vertices;

	virtual void InitRHI() override;
};

class FConvexCollisionIndexBuffer : public FIndexBuffer 
{
public:
	TArray<int32> Indices;

	virtual void InitRHI() override;
};

class FConvexCollisionVertexFactory : public FLocalVertexFactory
{
public:

	FConvexCollisionVertexFactory()
	{}

	/** Initialization constructor. */
	FConvexCollisionVertexFactory(const FConvexCollisionVertexBuffer* VertexBuffer)
	{
		InitConvexVertexFactory(VertexBuffer);
	}


	void InitConvexVertexFactory(const FConvexCollisionVertexBuffer* VertexBuffer);
};

class FKConvexGeomRenderInfo
{
public:
	FConvexCollisionVertexBuffer* VertexBuffer;
	FConvexCollisionIndexBuffer* IndexBuffer;
	FConvexCollisionVertexFactory* CollisionVertexFactory;

	FKConvexGeomRenderInfo()
	: VertexBuffer(NULL)
	, IndexBuffer(NULL)
	, CollisionVertexFactory(NULL)
	{}

	/** Util to see if this render info has some valid geometry to render. */
	bool HasValidGeometry()
	{
		return 
			(VertexBuffer != NULL) && 
			(VertexBuffer->Vertices.Num() > 0) && 
			(IndexBuffer != NULL) &&
			(IndexBuffer->Indices.Num() > 0);
	}
};

/**
 *	Load the required modules for PhysX
 */
ENGINE_API void LoadPhysXModules();
/** 
 *	Unload the required modules for PhysX
 */
void UnloadPhysXModules();

ENGINE_API void	InitGamePhys();
ENGINE_API void	TermGamePhys();


bool	ExecPhysCommands(const TCHAR* Cmd, FOutputDevice* Ar, UWorld* InWorld);

/** Util to list to log all currently awake rigid bodies */
void	ListAwakeRigidBodies(bool bIncludeKinematic, UWorld* world);


FTransform FindBodyTransform(AActor* Actor, FName BoneName);
FBox	FindBodyBox(AActor* Actor, FName BoneName);

