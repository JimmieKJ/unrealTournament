// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "EnginePrivate.h"
#include "PhysicsPublic.h"
#include "Components/SkeletalMeshComponent.h"

#if WITH_PHYSX
	#include "PhysXSupport.h"
	#include "../Vehicles/PhysXVehicleManager.h"
	#include "PhysXSupport.h"
#if WITH_VEHICLE
	#include "../Vehicles/PhysXVehicleManager.h"
#include "PhysXSupport.h"
#include "../Vehicles/PhysXVehicleManager.h"
#endif
#endif

#include "PhysSubstepTasks.h"	//needed even if not substepping, contains common utility class for PhysX
#include "PhysicsEngine/PhysicsCollisionHandler.h"
#include "Components/DestructibleComponent.h"
#include "Components/LineBatchComponent.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "PhysicsEngine/BodySetup.h"

/** Physics stats **/

DEFINE_STAT(STAT_TotalPhysicsTime);
DECLARE_CYCLE_STAT(TEXT("Start Physics Time (sync)"), STAT_PhysicsKickOffDynamicsTime, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Fetch Results Time (sync)"), STAT_PhysicsFetchDynamicsTime, STATGROUP_Physics);

DECLARE_CYCLE_STAT(TEXT("Start Physics Time (cloth)"), STAT_PhysicsKickOffDynamicsTime_Cloth, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Fetch Results Time (cloth)"), STAT_PhysicsFetchDynamicsTime_Cloth, STATGROUP_Physics);

DECLARE_CYCLE_STAT(TEXT("Start Physics Time (async)"), STAT_PhysicsKickOffDynamicsTime_Async, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Fetch Results Time (async)"), STAT_PhysicsFetchDynamicsTime_Async, STATGROUP_Physics);


DECLARE_CYCLE_STAT(TEXT("Phys Events Time"), STAT_PhysicsEventTime, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("SyncComponentsToBodies (sync)"), STAT_SyncComponentsToBodies, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("SyncComponentsToBodies (cloth)"), STAT_SyncComponentsToBodies_Cloth, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("SyncComponentsToBodies (async)"), STAT_SyncComponentsToBodies_Async, STATGROUP_Physics);

DECLARE_DWORD_COUNTER_STAT(TEXT("Broadphase Adds"), STAT_NumBroadphaseAdds, STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("Broadphase Removes"), STAT_NumBroadphaseRemoves, STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("Active Constraints"), STAT_NumActiveConstraints, STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("Active Simulated Bodies"), STAT_NumActiveSimulatedBodies, STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("Active Kinematic Bodies"), STAT_NumActiveKinematicBodies, STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("Mobile Bodies"), STAT_NumMobileBodies, STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("Static Bodies"), STAT_NumStaticBodies, STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("Shapes"), STAT_NumShapes, STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("Cloths"), STAT_NumCloths, STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("ClothVerts"), STAT_NumClothVerts, STATGROUP_Physics);

DECLARE_DWORD_COUNTER_STAT(TEXT("(ASync) Broadphase Adds"), STAT_NumBroadphaseAddsAsync, STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("(ASync) Broadphase Removes"), STAT_NumBroadphaseRemovesAsync, STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("(ASync) Active Constraints"), STAT_NumActiveConstraintsAsync, STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("(ASync) Active Simulated Bodies"), STAT_NumActiveSimulatedBodiesAsync, STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("(ASync) Active Kinematic Bodies"), STAT_NumActiveKinematicBodiesAsync, STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("(ASync) Mobile Bodies"), STAT_NumMobileBodiesAsync, STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("(ASync) Static Bodies"), STAT_NumStaticBodiesAsync, STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("(ASync) Shapes"), STAT_NumShapesAsync, STATGROUP_Physics);

#define USE_ADAPTIVE_FORCES_FOR_ASYNC_SCENE			1
#define USE_SPECIAL_FRICTION_MODEL_FOR_ASYNC_SCENE	0

static int16 PhysXSceneCount = 1;
static const int PhysXSlowRebuildRate = 10;

EPhysicsSceneType FPhysScene::SceneType_AssumesLocked(const FBodyInstance* BodyInstance) const
{
#if WITH_PHYSX
	//This is a helper function for dynamic actors - static actors are in both scenes
	check(BodyInstance->GetPxRigidBody_AssumesLocked());
	return UPhysicsSettings::Get()->bEnableAsyncScene && BodyInstance->UseAsyncScene(this) ? PST_Async : PST_Sync;
#endif

	return PST_Sync;
}

/**
* Return true if we should be running in single threaded mode, ala dedicated server
**/

/**
* Return true if we should lag the async scene a frame
**/
FORCEINLINE static bool FrameLagAsync()
{
	if (IsRunningDedicatedServer())
	{
		return false;
	}
	return true;
}

#if WITH_APEX

//This level of indirection is needed because we don't want to expose NxApexDamageEventReportData in a public engine header
struct FPendingApexDamageEvent
{
	TWeakObjectPtr<class UDestructibleComponent> DestructibleComponent;
	NxApexDamageEventReportData DamageEvent;
	TArray<NxApexChunkData> ApexChunkData;

	FPendingApexDamageEvent(UDestructibleComponent* InDestructibleComponent, const NxApexDamageEventReportData& InDamageEvent)
		: DestructibleComponent(InDestructibleComponent)
		, DamageEvent(InDamageEvent)
	{
		ApexChunkData.AddUninitialized(InDamageEvent.fractureEventListSize);
		for(uint32 ChunkIdx = 0; ChunkIdx < InDamageEvent.fractureEventListSize; ++ChunkIdx)
		{
			ApexChunkData[ChunkIdx] = InDamageEvent.fractureEventList[ChunkIdx];
		}

		DamageEvent.fractureEventList = ApexChunkData.GetData();
	}

};

struct FPendingApexDamageManager
{
	TArray<FPendingApexDamageEvent> PendingDamageEvents;
};
#endif

#if WITH_PHYSX

FAutoConsoleTaskPriority CPrio_FPhysXTask(
	TEXT("TaskGraph.TaskPriorities.PhysXTask"),
	TEXT("Task and thread priority for FPhysXTask."),
	ENamedThreads::HighThreadPriority, // if we have high priority task threads, then use them...
	ENamedThreads::NormalTaskPriority, // .. at normal task priority
	ENamedThreads::HighTaskPriority // if we don't have hi pri threads, then use normal priority threads at high task priority instead
	);

FAutoConsoleTaskPriority CPrio_FPhysXTask_Cloth(
	TEXT("TaskGraph.TaskPriorities.PhysXTask.Cloth"),
	TEXT("Task and thread priority for FPhysXTask (cloth)."),
	ENamedThreads::HighThreadPriority, // if we have high priority task threads, then use them...
	ENamedThreads::NormalTaskPriority, // .. at normal task priority
	ENamedThreads::HighTaskPriority // if we don't have hi pri threads, then use normal priority threads at high task priority instead
	);


template <bool IsCloth>
class FPhysXTask
{
	PxBaseTask&	Task;

public:
	FPhysXTask(PxBaseTask* InTask)
		: Task(*InTask)
	{
	}

	~FPhysXTask()
	{
		Task.release();
	}

	static FORCEINLINE TStatId GetStatId()
	{
		if (!IsCloth)
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FPhysXTask, STATGROUP_Physics);
		}
		else
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FPhysXClothTask, STATGROUP_Physics);
		}

	}
	static FORCEINLINE ENamedThreads::Type GetDesiredThread()
	{
		if (!IsCloth)
		{
			return CPrio_FPhysXTask.Get();
		}
		else
		{
			return CPrio_FPhysXTask_Cloth.Get();
		}
	}
	static FORCEINLINE ESubsequentsMode::Type GetSubsequentsMode()
	{
		return ESubsequentsMode::TrackSubsequents;
	}

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		FPlatformMisc::BeginNamedEvent(FColor::Black, Task.getName());
		Task.run();
		FPlatformMisc::EndNamedEvent();
	}
};


/** Used to dispatch physx tasks to task graph */
template <bool IsClothScene>
class FPhysXCPUDispatcher : public PxCpuDispatcher
{
public:
	virtual void submitTask(PxBaseTask& Task) override
	{
		TGraphTask<FPhysXTask<IsClothScene>>::CreateTask(NULL).ConstructAndDispatchWhenReady(&Task);
	}

	virtual PxU32 getWorkerCount() const override
	{
		return FTaskGraphInterface::Get().GetNumWorkerThreads();
	}
};

DECLARE_CYCLE_STAT(TEXT("PhysX Single Thread Task"), STAT_PhysXSingleThread, STATGROUP_Physics);

/** Used to dispatch physx tasks to the game thread */
class FPhysXCPUDispatcherSingleThread : public PxCpuDispatcher
{
	TArray<PxBaseTask*> TaskStack;

	virtual void submitTask(PxBaseTask& Task) override
	{
		SCOPE_CYCLE_COUNTER(STAT_PhysXSingleThread);
		check(IsInGameThread());

		TaskStack.Push(&Task);
		if (TaskStack.Num() > 1)
		{
			return;
		}
		Task.run();
		Task.release();
		while (TaskStack.Num() > 1)
		{
			PxBaseTask& ChildTask = *TaskStack.Pop();
			ChildTask.run();
			ChildTask.release();
		}
		verify(&Task == TaskStack.Pop() && !TaskStack.Num());
	}

	virtual PxU32 getWorkerCount() const override
	{
		return 1;
	}
};

TSharedPtr<ISimEventCallbackFactory> FPhysScene::SimEventCallbackFactory;

#endif // WITH_PHYSX


/** Exposes creation of physics-engine scene outside Engine (for use with PhAT for example). */
FPhysScene::FPhysScene()
#if WITH_APEX
	: PendingApexDamageManager(new FPendingApexDamageManager)
#endif
{
	LineBatcher = NULL;
	OwningWorld = NULL;
#if WITH_PHYSX
#if WITH_VEHICLE
	VehicleManager = NULL;
#endif
	PhysxUserData = FPhysxUserData(this);
	
#endif	//#if WITH_PHYSX

	UPhysicsSettings * PhysSetting = UPhysicsSettings::Get();
	FMemory::Memzero(FrameTimeSmoothingFactor);
	FrameTimeSmoothingFactor[PST_Sync] = PhysSetting->SyncSceneSmoothingFactor;
	FrameTimeSmoothingFactor[PST_Async] = PhysSetting->AsyncSceneSmoothingFactor;

	bSubstepping = PhysSetting->bSubstepping;
	bSubsteppingAsync = PhysSetting->bSubsteppingAsync;
	bAsyncSceneEnabled = PhysSetting->bEnableAsyncScene;
	NumPhysScenes = bAsyncSceneEnabled ? PST_Async + 1 : PST_Cloth + 1;


	// Create scenes of all scene types
	for (uint32 SceneType = 0; SceneType < NumPhysScenes; ++SceneType)
	{
		// Create the physics scene
		InitPhysScene(SceneType);

		// Also initialize scene data
		bPhysXSceneExecuting[SceneType] = false;

		// Initialize to a value which would be acceptable if FrameTimeSmoothingFactor[i] = 1.0f, i.e. constant simulation substeps
		AveragedFrameTime[SceneType] = PhysSetting->InitialAverageFrameRate;

		// gets from console variable, and clamp to [0, 1] - 1 should be fixed time as 30 fps
		FrameTimeSmoothingFactor[SceneType] = FMath::Clamp<float>(FrameTimeSmoothingFactor[SceneType], 0.f, 1.f);
	}

	if (!bAsyncSceneEnabled)
	{
		PhysXSceneIndex[PST_Async] = 0;
	}

	// Make sure we use the sync scene for apex world support of destructibles in the async scene
#if WITH_APEX
	NxApexScene* ApexScene = GetApexScene(bAsyncSceneEnabled ? PST_Async : PST_Sync);
	check(ApexScene);
	PxScene* SyncPhysXScene = GetPhysXScene(PST_Sync);
	check(SyncPhysXScene);
	check(GApexModuleDestructible);
	GApexModuleDestructible->setWorldSupportPhysXScene(*ApexScene, SyncPhysXScene);
	GApexModuleDestructible->setDamageApplicationRaycastFlags(NxDestructibleActorRaycastFlags::AllChunks, *ApexScene);
#endif

	PreGarbageCollectDelegateHandle = FCoreUObjectDelegates::PreGarbageCollect.AddRaw(this, &FPhysScene::WaitPhysScenes);

#if WITH_PHYSX
	// Initialise PhysX scratch buffers (only if size > 0)
	int32 SceneScratchBufferSize = PhysSetting->SimulateScratchMemorySize;
	if(SceneScratchBufferSize > 0)
	{
		for(uint32 SceneType = 0; SceneType < PST_MAX; ++SceneType)
		{
			if(SceneType < NumPhysScenes)
			{
				// Only allocate a scratch buffer if we have a scene and we are not using that cloth scene.
				// Clothing actors are not simulated with this scene but simulated per-actor
				PxScene* Scene = GetPhysXScene(SceneType);
				if(SceneType != PST_Cloth && Scene)
				{
					// We have a valid scene, so allocate the buffer for it
					SimScratchBuffers[SceneType].Buffer = (uint8*)FMemory::Malloc(SceneScratchBufferSize, 16);
					SimScratchBuffers[SceneType].BufferSize = SceneScratchBufferSize;
				}
			}
		}
	}
#endif
}

void FPhysScene::SetOwningWorld(UWorld* InOwningWorld)
{
	OwningWorld = InOwningWorld;
}

/** Exposes destruction of physics-engine scene outside Engine. */
FPhysScene::~FPhysScene()
{
	FCoreUObjectDelegates::PreGarbageCollect.Remove(PreGarbageCollectDelegateHandle);
	// Make sure no scenes are left simulating (no-ops if not simulating)
	WaitPhysScenes();
	// Loop through scene types to get all scenes
	for (uint32 SceneType = 0; SceneType < NumPhysScenes; ++SceneType)
	{

		// Destroy the physics scene
		TermPhysScene(SceneType);

#if WITH_PHYSX
		GPhysCommandHandler->DeferredDeleteCPUDispathcer(CPUDispatcher[SceneType]);
#endif	//#if WITH_PHYSX
	}

#if WITH_PHYSX
	// Free the scratch buffers
 	for(uint32 SceneType = 0; SceneType < PST_MAX; ++SceneType)
	{
		if(SimScratchBuffers[SceneType].Buffer != nullptr)
		{
			FMemory::Free(SimScratchBuffers[SceneType].Buffer);
			SimScratchBuffers[SceneType].Buffer = nullptr;
			SimScratchBuffers[SceneType].BufferSize = 0;
		}
	}
#endif
}

namespace
{

bool UseSyncTime(uint32 SceneType)
{
	return (FrameLagAsync() && SceneType == PST_Async);
}

}

bool FPhysScene::GetKinematicTarget_AssumesLocked(const FBodyInstance* BodyInstance, FTransform& OutTM) const
{
#if WITH_PHYSX
	if (PxRigidDynamic * PRigidDynamic = BodyInstance->GetPxRigidDynamic_AssumesLocked())
	{
		uint32 BodySceneType = SceneType_AssumesLocked(BodyInstance);
		if (IsSubstepping(BodySceneType))
		{
			FPhysSubstepTask * PhysSubStepper = PhysSubSteppers[BodySceneType];
			return PhysSubStepper->GetKinematicTarget_AssumesLocked(BodyInstance, OutTM);
		}
		else
		{
			PxTransform POutTM;
			bool validTM = PRigidDynamic->getKinematicTarget(POutTM);
			if (validTM)
			{
				OutTM = P2UTransform(POutTM);
				return true;
			}
		}
	}
#endif

	return false;
}

void FPhysScene::SetKinematicTarget_AssumesLocked(FBodyInstance* BodyInstance, const FTransform& TargetTransform, bool bAllowSubstepping)
{
	TargetTransform.DiagnosticCheck_IsValid();

#if WITH_PHYSX
	if (PxRigidDynamic * PRigidDynamic = BodyInstance->GetPxRigidDynamic_AssumesLocked())
	{
		const bool bIsKinematicTarget = IsRigidBodyKinematicAndInSimulationScene_AssumesLocked(PRigidDynamic);
		if(bIsKinematicTarget)
		{
			uint32 BodySceneType = SceneType_AssumesLocked(BodyInstance);
			if (bAllowSubstepping && IsSubstepping(BodySceneType))
			{
				FPhysSubstepTask * PhysSubStepper = PhysSubSteppers[BodySceneType];
				PhysSubStepper->SetKinematicTarget_AssumesLocked(BodyInstance, TargetTransform);
			}
			else
			{
				const PxTransform PNewPose = U2PTransform(TargetTransform);
				PRigidDynamic->setKinematicTarget(PNewPose);
			}
		}
		else
		{
			const PxTransform PNewPose = U2PTransform(TargetTransform);
			PRigidDynamic->setGlobalPose(PNewPose);
		}

	}
#endif
}

void FPhysScene::AddCustomPhysics_AssumesLocked(FBodyInstance* BodyInstance, FCalculateCustomPhysics& CalculateCustomPhysics)
{
#if WITH_PHYSX
	uint32 BodySceneType = SceneType_AssumesLocked(BodyInstance);
	if (IsSubstepping(BodySceneType))
	{
		FPhysSubstepTask * PhysSubStepper = PhysSubSteppers[SceneType_AssumesLocked(BodyInstance)];
		PhysSubStepper->AddCustomPhysics_AssumesLocked(BodyInstance, CalculateCustomPhysics);
	}
	else
	{
		// Since physics frame is set up before "pre-physics" tick group is called, can just fetch delta time from there
		CalculateCustomPhysics.ExecuteIfBound(this->DeltaSeconds, BodyInstance);
	}
#endif
}

void FPhysScene::AddForce_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Force, bool bAllowSubstepping, bool bAccelChange)
{
#if WITH_PHYSX

	if (PxRigidBody * PRigidBody = BodyInstance->GetPxRigidBody_AssumesLocked())
	{
		uint32 BodySceneType = SceneType_AssumesLocked(BodyInstance);
		if (bAllowSubstepping && IsSubstepping(BodySceneType))
		{
			FPhysSubstepTask * PhysSubStepper = PhysSubSteppers[BodySceneType];
			PhysSubStepper->AddForce_AssumesLocked(BodyInstance, Force, bAccelChange);
		}
		else
		{
			PRigidBody->addForce(U2PVector(Force), bAccelChange ? PxForceMode::eACCELERATION : PxForceMode::eFORCE, true);
		}
	}
#endif
}

void FPhysScene::AddForceAtPosition_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Force, const FVector& Position, bool bAllowSubstepping)
{
#if WITH_PHYSX

	if (PxRigidBody * PRigidBody = BodyInstance->GetPxRigidBody_AssumesLocked())
	{
		uint32 BodySceneType = SceneType_AssumesLocked(BodyInstance);
		if (bAllowSubstepping && IsSubstepping(BodySceneType))
		{
			FPhysSubstepTask * PhysSubStepper = PhysSubSteppers[BodySceneType];
			PhysSubStepper->AddForceAtPosition_AssumesLocked(BodyInstance, Force, Position);
		}
		else
		{
			PxRigidBodyExt::addForceAtPos(*PRigidBody, U2PVector(Force), U2PVector(Position), PxForceMode::eFORCE, true);
		}
	}
#endif
}

void FPhysScene::AddRadialForceToBody_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Origin, const float Radius, const float Strength, const uint8 Falloff, bool bAccelChange, bool bAllowSubstepping)
{
#if WITH_PHYSX

	if (PxRigidBody * PRigidBody = BodyInstance->GetPxRigidBody_AssumesLocked())
	{
		uint32 BodySceneType = SceneType_AssumesLocked(BodyInstance);
		if (bAllowSubstepping && IsSubstepping(BodySceneType))
		{
			FPhysSubstepTask * PhysSubStepper = PhysSubSteppers[BodySceneType];
			PhysSubStepper->AddRadialForceToBody_AssumesLocked(BodyInstance, Origin, Radius, Strength, Falloff, bAccelChange);
		}
		else
		{
			AddRadialForceToPxRigidBody_AssumesLocked(*PRigidBody, Origin, Radius, Strength, Falloff, bAccelChange);
		}
	}
#endif
}

void FPhysScene::AddTorque_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Torque, bool bAllowSubstepping, bool bAccelChange)
{
#if WITH_PHYSX

	if (PxRigidBody * PRigidBody = BodyInstance->GetPxRigidBody_AssumesLocked())
	{
		uint32 BodySceneType = SceneType_AssumesLocked(BodyInstance);
		if (bAllowSubstepping && IsSubstepping(BodySceneType))
		{
			FPhysSubstepTask * PhysSubStepper = PhysSubSteppers[BodySceneType];
			PhysSubStepper->AddTorque_AssumesLocked(BodyInstance, Torque, bAccelChange);
		}
		else
		{
			PRigidBody->addTorque(U2PVector(Torque), bAccelChange ? PxForceMode::eACCELERATION : PxForceMode::eFORCE, true);
		}
	}
#endif
}

#if WITH_PHYSX
void FPhysScene::RemoveActiveBody_AssumesLocked(FBodyInstance* BodyInstance, uint32 SceneType)
{
	int32 BodyIndex = ActiveBodyInstances[SceneType].Find(BodyInstance);
	if (BodyIndex != INDEX_NONE)
	{
		ActiveBodyInstances[SceneType][BodyIndex] = nullptr;
	}

	PendingSleepEvents[SceneType].Remove(BodyInstance->GetPxRigidActor_AssumesLocked(SceneType));
}
#endif
void FPhysScene::TermBody_AssumesLocked(FBodyInstance* BodyInstance)
{
	if (PxRigidBody* PRigidBody = BodyInstance->GetPxRigidBody_AssumesLocked())
	{
		FPhysSubstepTask* PhysSubStepper = PhysSubSteppers[SceneType_AssumesLocked(BodyInstance)];
		PhysSubStepper->RemoveBodyInstance_AssumesLocked(BodyInstance);
	}

	// Remove body from any pending deferred addition / removal
	for(FDeferredSceneData& Deferred : DeferredSceneData)
	{
		int32 FoundIdx = INDEX_NONE;
		if(Deferred.AddInstances.Find(BodyInstance, FoundIdx))
		{
			Deferred.AddActors.RemoveAtSwap(FoundIdx);
			Deferred.AddInstances.RemoveAtSwap(FoundIdx);
		}
	}

#if WITH_PHYSX
	RemoveActiveBody_AssumesLocked(BodyInstance, PST_Sync);
	RemoveActiveBody_AssumesLocked(BodyInstance, PST_Async);
#endif
}

#if WITH_APEX
void FPhysScene::AddPendingDamageEvent(UDestructibleComponent* DestructibleComponent, const NxApexDamageEventReportData& DamageEvent)
{
	check(IsInGameThread());
	FPendingApexDamageEvent* Pending = new (PendingApexDamageManager->PendingDamageEvents) FPendingApexDamageEvent(DestructibleComponent, DamageEvent);
}
#endif

FAutoConsoleTaskPriority CPrio_PhysXStepSimulation(
	TEXT("TaskGraph.TaskPriorities.PhysXStepSimulation"),
	TEXT("Task and thread priority for FPhysSubstepTask::StepSimulation."),
	ENamedThreads::HighThreadPriority, // if we have high priority task threads, then use them...
	ENamedThreads::NormalTaskPriority, // .. at normal task priority
	ENamedThreads::HighTaskPriority // if we don't have hi pri threads, then use normal priority threads at high task priority instead
	);

bool FPhysScene::SubstepSimulation(uint32 SceneType, FGraphEventRef &InOutCompletionEvent)
{
#if WITH_PHYSX
	check(SceneType != PST_Cloth); //we don't bother sub-stepping cloth
	float UseDelta = UseSyncTime(SceneType)? SyncDeltaSeconds : DeltaSeconds;
	float SubTime = PhysSubSteppers[SceneType]->UpdateTime(UseDelta);
	PxScene* PScene = GetPhysXScene(SceneType);
	if(SubTime <= 0.f)
	{
		return false;
	}else
	{
		//we have valid scene and subtime so enqueue task
		PhysXCompletionTask* Task = new PhysXCompletionTask(InOutCompletionEvent, SceneType, PScene->getTaskManager(), &SimScratchBuffers[SceneType]);
		ENamedThreads::Type NamedThread = PhysSingleThreadedMode() ? ENamedThreads::GameThread : ENamedThreads::SetTaskPriority(ENamedThreads::GameThread, ENamedThreads::HighTaskPriority);

		DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.SubstepSimulationImp"),
			STAT_FSimpleDelegateGraphTask_SubstepSimulationImp,
			STATGROUP_TaskGraphTasks);

		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateRaw(PhysSubSteppers[SceneType], &FPhysSubstepTask::StepSimulation, Task),
			GET_STATID(STAT_FSimpleDelegateGraphTask_SubstepSimulationImp), NULL, NamedThread
		);
		return true;
	}
#endif

}

/** Adds to queue of skelmesh we want to add to collision disable table */
void FPhysScene::DeferredAddCollisionDisableTable(uint32 SkelMeshCompID, TMap<struct FRigidBodyIndexPair, bool> * CollisionDisableTable)
{
	check(IsInGameThread());

	FPendingCollisionDisableTable PendingCollisionDisableTable;
	PendingCollisionDisableTable.SkelMeshCompID = SkelMeshCompID;
	PendingCollisionDisableTable.CollisionDisableTable = CollisionDisableTable;

	DeferredCollisionDisableTableQueue.Add(PendingCollisionDisableTable);
}

/** Adds to queue of skelmesh we want to remove from collision disable table */
void FPhysScene::DeferredRemoveCollisionDisableTable(uint32 SkelMeshCompID)
{
	check(IsInGameThread());

	FPendingCollisionDisableTable PendingDisableCollisionTable;
	PendingDisableCollisionTable.SkelMeshCompID = SkelMeshCompID;
	PendingDisableCollisionTable.CollisionDisableTable = NULL;

	DeferredCollisionDisableTableQueue.Add(PendingDisableCollisionTable);
}

void FPhysScene::FlushDeferredCollisionDisableTableQueue()
{
	check(IsInGameThread());
	for (int32 i = 0; i < DeferredCollisionDisableTableQueue.Num(); ++i)
	{
		FPendingCollisionDisableTable & PendingCollisionDisableTable = DeferredCollisionDisableTableQueue[i];

		if (PendingCollisionDisableTable.CollisionDisableTable)
		{
			CollisionDisableTableLookup.Add(PendingCollisionDisableTable.SkelMeshCompID, PendingCollisionDisableTable.CollisionDisableTable);
		}
		else
		{
			CollisionDisableTableLookup.Remove(PendingCollisionDisableTable.SkelMeshCompID);
		}
	}

	DeferredCollisionDisableTableQueue.Empty();
}

void GatherPhysXStats_AssumesLocked(PxScene* PSyncScene, PxScene* PAsyncScene)
{
	/** Gather PhysX stats */
	if (PSyncScene)
	{
		PxSimulationStatistics SimStats;
		PSyncScene->getSimulationStatistics(SimStats);

		SET_DWORD_STAT(STAT_NumActiveConstraints, SimStats.nbActiveConstraints);
		SET_DWORD_STAT(STAT_NumActiveSimulatedBodies, SimStats.nbActiveDynamicBodies);
		SET_DWORD_STAT(STAT_NumActiveKinematicBodies, SimStats.nbActiveKinematicBodies);
		SET_DWORD_STAT(STAT_NumStaticBodies, SimStats.nbStaticBodies);
		SET_DWORD_STAT(STAT_NumMobileBodies, SimStats.nbDynamicBodies);
			
		//SET_DWORD_STAT(STAT_NumBroadphaseAdds, SimStats.getNbBroadPhaseAdds(PxSimulationStatistics::VolumeType::eRIGID_BODY));	//TODO: These do not seem to work
		//SET_DWORD_STAT(STAT_NumBroadphaseRemoves, SimStats.getNbBroadPhaseRemoves(PxSimulationStatistics::VolumeType::eRIGID_BODY));

		uint32 NumShapes = 0;
		for (int32 GeomType = 0; GeomType < PxGeometryType::eGEOMETRY_COUNT; ++GeomType)
		{
			NumShapes += SimStats.nbShapes[GeomType];
		}

		SET_DWORD_STAT(STAT_NumShapes, NumShapes);

	}

	if(PAsyncScene)
	{
		//Having to duplicate because of macros. In theory we can fix this but need to get this quickly
		PxSimulationStatistics SimStats;
		PAsyncScene->getSimulationStatistics(SimStats);

		SET_DWORD_STAT(STAT_NumActiveConstraintsAsync, SimStats.nbActiveConstraints);
		SET_DWORD_STAT(STAT_NumActiveSimulatedBodiesAsync, SimStats.nbActiveDynamicBodies);
		SET_DWORD_STAT(STAT_NumActiveKinematicBodiesAsync, SimStats.nbActiveKinematicBodies);
		SET_DWORD_STAT(STAT_NumStaticBodiesAsync, SimStats.nbStaticBodies);
		SET_DWORD_STAT(STAT_NumMobileBodiesAsync, SimStats.nbDynamicBodies);

		//SET_DWORD_STAT(STAT_NumBroadphaseAddsAsync, SimStats.getNbBroadPhaseAdds(PxSimulationStatistics::VolumeType::eRIGID_BODY)); //TODO: These do not seem to work
		//SET_DWORD_STAT(STAT_NumBroadphaseRemovesAsync, SimStats.getNbBroadPhaseRemoves(PxSimulationStatistics::VolumeType::eRIGID_BODY));

		uint32 NumShapes = 0;
		for (int32 GeomType = 0; GeomType < PxGeometryType::eGEOMETRY_COUNT; ++GeomType)
		{
			NumShapes += SimStats.nbShapes[GeomType];
		}

		SET_DWORD_STAT(STAT_NumShapesAsync, NumShapes);
	}
}

DECLARE_FLOAT_COUNTER_STAT(TEXT("Sync Sim Time (ms)"), STAT_PhysSyncSim, STATGROUP_Physics);
DECLARE_FLOAT_COUNTER_STAT(TEXT("Async Sim Time (ms)"), STAT_PhysAsyncSim, STATGROUP_Physics);
DECLARE_FLOAT_COUNTER_STAT(TEXT("Cloth Sim Time (ms)"), STAT_PhysClothSim, STATGROUP_Physics);

double GSimStartTime[PST_MAX] = {0.f, 0.f, 0.f};

void FinishSceneStat(uint32 Scene)
{
	if (Scene < PST_MAX)	//PST_MAX used when we don't care
	{
		float SceneTime = float(FPlatformTime::Seconds() - GSimStartTime[Scene]) * 1000.0f;
		switch(Scene)
		{
			case PST_Sync:
			INC_FLOAT_STAT_BY(STAT_PhysSyncSim, SceneTime); break;
			case PST_Async:
			INC_FLOAT_STAT_BY(STAT_PhysAsyncSim, SceneTime); break;
			case PST_Cloth:
			INC_FLOAT_STAT_BY(STAT_PhysClothSim, SceneTime); break;
		}
	}
}

#if WITH_APEX
void GatherApexStats(const UWorld* World, NxApexScene* ApexScene)
{
#if STATS
	QUICK_SCOPE_CYCLE_COUNTER(STAT_GatherApexStats);

	if ( FThreadStats::IsCollectingData(GET_STATID(STAT_NumCloths)) ||  FThreadStats::IsCollectingData(GET_STATID(STAT_NumClothVerts)) )
	{
		SCOPED_APEX_SCENE_READ_LOCK(ApexScene);
		int32 NumVerts = 0;
		int32 NumCloths = 0;
		for (TObjectIterator<USkeletalMeshComponent> Itr; Itr; ++Itr)
		{
			if (Itr->GetWorld() != World) { continue; }
			const TArray<FClothingActor>& ClothingActors = Itr->GetClothingActors();
			for (const FClothingActor& ClothingActor : ClothingActors)
			{
				if (ClothingActor.ApexClothingActor && ClothingActor.ApexClothingActor->getActivePhysicalLod() == 1)
				{
					NumCloths++;
					NumVerts += ClothingActor.ApexClothingActor->getNumSimulationVertices();
				}
			}
		}
		SET_DWORD_STAT(STAT_NumCloths, NumCloths);        // number of recently simulated apex cloths.
		SET_DWORD_STAT(STAT_NumClothVerts, NumVerts);	  // number of recently simulated apex vertices.
	}
#endif
}
#endif

void FPhysScene::MarkForPreSimKinematicUpdate(USkeletalMeshComponent* InSkelComp, ETeleportType InTeleport, bool bNeedsSkinning)
{
	// If null, or pending kill, do nothing
	if (InSkelComp != nullptr && !InSkelComp->IsPendingKill())
	{
		// If we are already flagged, just need to update info
		if(InSkelComp->bDeferredKinematicUpdate)
		{
			FDeferredKinematicUpdateInfo* Info = DeferredKinematicUpdateSkelMeshes.Find(InSkelComp);
			check(Info != nullptr); // If the bool was set, we must be in the map!
			// If we are currently not going to teleport physics, but this update wants to, we 'upgrade' it
			if (Info->TeleportType == ETeleportType::None && InTeleport == ETeleportType::TeleportPhysics)
			{
				Info->TeleportType = ETeleportType::TeleportPhysics;
			}

			// If we need skinning, remember that
			if (bNeedsSkinning)
			{
				Info->bNeedsSkinning = true;
			}
		}
		// We are not flagged yet..
		else
		{
			// Set info and add to map
			FDeferredKinematicUpdateInfo Info;
			Info.TeleportType = InTeleport;
			Info.bNeedsSkinning = bNeedsSkinning;
			DeferredKinematicUpdateSkelMeshes.Add(InSkelComp, Info);

			// Set flag on component
			InSkelComp->bDeferredKinematicUpdate = true;
		}
	}
}

void FPhysScene::ClearPreSimKinematicUpdate(USkeletalMeshComponent* InSkelComp)
{
	// If non-null, and flagged for deferred update..
	if(InSkelComp != nullptr && InSkelComp->bDeferredKinematicUpdate)
	{
		// Remove from map
		int32 NumRemoved = DeferredKinematicUpdateSkelMeshes.Remove(InSkelComp);

		check(NumRemoved == 1); // Should be in map if flag was set!

		// Clear flag
		InSkelComp->bDeferredKinematicUpdate = false;
	}
}


void FPhysScene::UpdateKinematicsOnDeferredSkelMeshes()
{
	for (TMap< USkeletalMeshComponent*, FDeferredKinematicUpdateInfo >::TIterator It(DeferredKinematicUpdateSkelMeshes); It; ++It)
	{
		USkeletalMeshComponent* SkelComp = (*It).Key;
		FDeferredKinematicUpdateInfo Info = (*It).Value;

		check(SkelComp->bDeferredKinematicUpdate); // Should be true if in map!

		// Perform kinematic updates
		SkelComp->UpdateKinematicBonesToAnim(SkelComp->GetComponentSpaceTransforms(), Info.TeleportType, Info.bNeedsSkinning, EAllowKinematicDeferral::DisallowDeferral);

		// Clear deferred flag
		SkelComp->bDeferredKinematicUpdate = false; 
	}

	// Empty map now all is done
	DeferredKinematicUpdateSkelMeshes.Reset();
}


/** Exposes ticking of physics-engine scene outside Engine. */
void FPhysScene::TickPhysScene(uint32 SceneType, FGraphEventRef& InOutCompletionEvent)
{
	SCOPE_CYCLE_COUNTER(STAT_TotalPhysicsTime);
	CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_PhysicsKickOffDynamicsTime, SceneType == PST_Sync);
	CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_PhysicsKickOffDynamicsTime_Async, SceneType == PST_Async);
	CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_PhysicsKickOffDynamicsTime_Cloth, SceneType == PST_Cloth);

	check(SceneType < NumPhysScenes);

	GSimStartTime[SceneType] = FPlatformTime::Seconds();

	if (bPhysXSceneExecuting[SceneType] != 0)
	{
		// Already executing this scene, must call WaitPhysScene before calling this function again.
		UE_LOG(LogPhysics, Log, TEXT("TickPhysScene: Already executing scene (%d) - aborting."), SceneType);
		return;
	}

	if (IsSubstepping(SceneType))	//we don't bother sub-stepping cloth
	{
		//We're about to start stepping so swap buffers. Might want to find a better place for this?
		PhysSubSteppers[SceneType]->SwapBuffers();
	}

	/**
	* clamp down... if this happens we are simming physics slower than real-time, so be careful with it.
	* it can improve framerate dramatically (really, it is the same as scaling all velocities down and
	* enlarging all timesteps) but at the same time, it will screw with networking (client and server will
	* diverge a lot more.)
	*/

	float UseDelta = FMath::Min(UseSyncTime(SceneType) ? SyncDeltaSeconds : DeltaSeconds, MaxPhysicsDeltaTime);

	// Only simulate a positive time step.
	if (UseDelta <= 0.f)
	{
		if (UseDelta < 0.f)
		{
			// only do this if negative. Otherwise, whenever we pause, this will come up
			UE_LOG(LogPhysics, Warning, TEXT("TickPhysScene: Negative timestep (%f) - aborting."), UseDelta);
		}
		return;
	}

	/**
	* Weight frame time according to PhysScene settings.
	*/
	AveragedFrameTime[SceneType] *= FrameTimeSmoothingFactor[SceneType];
	AveragedFrameTime[SceneType] += (1.0f - FrameTimeSmoothingFactor[SceneType])*UseDelta;

	// Set execution flag
	bPhysXSceneExecuting[SceneType] = true;

	check(!InOutCompletionEvent.GetReference()); // these should be gone because nothing is outstanding
	InOutCompletionEvent = FGraphEvent::CreateGraphEvent();
	bool bTaskOutstanding = false;

	// Update any skeletal meshes that need their bone transforms sent to physics sim
	UpdateKinematicsOnDeferredSkelMeshes();

#if WITH_PHYSX

#if WITH_VEHICLE
	if (VehicleManager && SceneType == PST_Sync)
	{
		float TickTime = AveragedFrameTime[SceneType];
		if (IsSubstepping(SceneType))
		{
			TickTime = UseSyncTime(SceneType) ? SyncDeltaSeconds : DeltaSeconds;
		}
		VehicleManager->PreTick(TickTime);

		if (IsSubstepping(SceneType) == false)
		{
			VehicleManager->Update(AveragedFrameTime[SceneType]);
		}
	}
#endif

#if !WITH_APEX
	PxScene* PScene = GetPhysXScene(SceneType);
	if (PScene && (UseDelta > 0.f))
#else
	NxApexScene* ApexScene = GetApexScene(SceneType);
	if(ApexScene && UseDelta > 0.f)
#endif
	{
		if(IsSubstepping(SceneType)) //we don't bother sub-stepping cloth
		{
			bTaskOutstanding = SubstepSimulation(SceneType, InOutCompletionEvent);
		}
		else
		{
#if !WITH_APEX
			PhysXCompletionTask* Task = new PhysXCompletionTask(InOutCompletionEvent, SceneType, PScene->getTaskManager());
			PScene->lockWrite();
			PScene->simulate(AveragedFrameTime[SceneType], Task, SimScratchBuffers[SceneType].Buffer, SimScratchBuffers[SceneType].BufferSize);
			PScene->unlockWrite();
			Task->removeReference();
			bTaskOutstanding = true;
#else
			PhysXCompletionTask* Task = new PhysXCompletionTask(InOutCompletionEvent, SceneType, ApexScene->getTaskManager());
			ApexScene->simulate(AveragedFrameTime[SceneType], true, Task, SimScratchBuffers[SceneType].Buffer, SimScratchBuffers[SceneType].BufferSize);
			Task->removeReference();
			bTaskOutstanding = true;

			if (SceneType == PST_Cloth)
			{
				GatherApexStats(this->OwningWorld, ApexScene);
			}

#endif
		}
	}
#endif

	if (!bTaskOutstanding)
	{
		TArray<FBaseGraphTask*> NewTasks;
		InOutCompletionEvent->DispatchSubsequents(NewTasks, ENamedThreads::AnyThread); // nothing to do, so nothing to wait for
	}
	bSubstepping = UPhysicsSettings::Get()->bSubstepping;
	bSubsteppingAsync = UPhysicsSettings::Get()->bSubsteppingAsync;
}

void FPhysScene::KillVisualDebugger()
{
	if (GPhysXSDK)
	{
		if (PxVisualDebuggerConnectionManager* PVD = GPhysXSDK->getPvdConnectionManager())
		{
			PVD->disconnect();
		}
	}
}

void FPhysScene::WaitPhysScenes()
{
	check(IsInGameThread());

	FGraphEventArray ThingsToComplete;
	if (PhysicsSceneCompletion.GetReference())
	{
		ThingsToComplete.Add(PhysicsSceneCompletion);
	}
	// Loop through scene types to get all scenes
	// we just wait on everything, though some of these are redundant
	for (uint32 SceneType = 0; SceneType < NumPhysScenes; ++SceneType)
	{
		if (PhysicsSubsceneCompletion[SceneType].GetReference())
		{
			ThingsToComplete.Add(PhysicsSubsceneCompletion[SceneType]);
		}
		if (FrameLaggedPhysicsSubsceneCompletion[SceneType].GetReference())
		{
			ThingsToComplete.Add(FrameLaggedPhysicsSubsceneCompletion[SceneType]);
		}
	}
	if (ThingsToComplete.Num())
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FPhysScene_WaitPhysScenes);
		FTaskGraphInterface::Get().WaitUntilTasksComplete(ThingsToComplete, ENamedThreads::GameThread);
	}
}

void FPhysScene::SceneCompletionTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent, EPhysicsSceneType SceneType)
{
	ProcessPhysScene(SceneType);
}

void FPhysScene::ProcessPhysScene(uint32 SceneType)
{
	checkSlow(SceneType < PST_MAX);

	SCOPE_CYCLE_COUNTER(STAT_TotalPhysicsTime);
	CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_PhysicsFetchDynamicsTime, SceneType == PST_Sync);
	CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_PhysicsFetchDynamicsTime_Cloth, SceneType == PST_Cloth);
	CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_PhysicsFetchDynamicsTime_Async, SceneType == PST_Async);

	check(SceneType < NumPhysScenes);
	if (bPhysXSceneExecuting[SceneType] == 0)
	{
		// Not executing this scene, must call TickPhysScene before calling this function again.
		UE_LOG(LogPhysics, Log, TEXT("WaitPhysScene`: Not executing this scene (%d) - aborting."), SceneType);
		return;
	}

	if (FrameLagAsync())
	{
		static_assert(PST_MAX == 3, "Physics scene static test failed."); // Here we assume the PST_Sync is the master and never fame lagged
		if (SceneType == PST_Sync)
		{
			// the one frame lagged one should be done by now.
			check(!FrameLaggedPhysicsSubsceneCompletion[PST_Async].GetReference() || FrameLaggedPhysicsSubsceneCompletion[PST_Async]->IsComplete());
		}
		else if (SceneType == PST_Async)
		{
			FrameLaggedPhysicsSubsceneCompletion[PST_Async] = NULL;
		}
	}


	// Reset execution flag

	bool bSuccess = false;

//This fetches and gets active transforms. It's important that the function that calls this locks because getting the transforms and using the data must be an atomic operation
#if WITH_PHYSX
	PxScene* PScene = GetPhysXScene(SceneType);
	check(PScene);
	PxU32 OutErrorCode = 0;

#if !WITH_APEX
	PScene->lockWrite();
	bSuccess = PScene->fetchResults(true, &OutErrorCode);
	PScene->unlockWrite();
#else	//	#if !WITH_APEX
	// The APEX scene calls the fetchResults function for the PhysX scene, so we only call ApexScene->fetchResults().
	NxApexScene* ApexScene = GetApexScene(SceneType);
	check(ApexScene);
	bSuccess = ApexScene->fetchResults(true, &OutErrorCode);
#endif	//	#if !WITH_APEX

	if(bSuccess)
	{
		UpdateActiveTransforms(SceneType);
	}
	
	if (OutErrorCode != 0)
	{
		UE_LOG(LogPhysics, Log, TEXT("PHYSX FETCHRESULTS ERROR: %d"), OutErrorCode);
	}
#endif // WITH_PHYSX

	PhysicsSubsceneCompletion[SceneType] = NULL;
	bPhysXSceneExecuting[SceneType] = false;
}
#if WITH_PHYSX
void FPhysScene::UpdateActiveTransforms(uint32 SceneType)
{
	checkSlow(SceneType < PST_MAX);

	if (SceneType == PST_Cloth)	//cloth doesn't bother with updating components to bodies so we don't need to store any transforms
	{
		return;
	}
	PxScene* PScene = GetPhysXScene(SceneType);
	check(PScene);
	SCOPED_SCENE_READ_LOCK(PScene);

	PxU32 NumTransforms = 0;
	const PxActiveTransform* PActiveTransforms = PScene->getActiveTransforms(NumTransforms);
	ActiveBodyInstances[SceneType].Empty(NumTransforms);
	ActiveDestructibleActors[SceneType].Empty(NumTransforms);

	for (PxU32 TransformIdx = 0; TransformIdx < NumTransforms; ++TransformIdx)
	{
		const PxActiveTransform& PActiveTransform = PActiveTransforms[TransformIdx];
		PxRigidActor* RigidActor = PActiveTransform.actor->isRigidActor();
		ensure(!RigidActor->userData || !FPhysxUserData::IsGarbage(RigidActor->userData));

		if (FBodyInstance* BodyInstance = FPhysxUserData::Get<FBodyInstance>(RigidActor->userData))
		{
			if (BodyInstance->InstanceBodyIndex == INDEX_NONE && BodyInstance->OwnerComponent.IsValid() && BodyInstance->IsInstanceSimulatingPhysics())
			{
				ActiveBodyInstances[SceneType].Add(BodyInstance);
			}
		}
		else if (const FDestructibleChunkInfo* DestructibleChunkInfo = FPhysxUserData::Get<FDestructibleChunkInfo>(RigidActor->userData))
		{
			ActiveDestructibleActors[SceneType].Add(RigidActor);
		}
		
	}
}
#endif

void FPhysScene::SyncComponentsToBodies_AssumesLocked(uint32 SceneType)
{
	checkSlow(SceneType < PST_MAX);

	SCOPE_CYCLE_COUNTER(STAT_TotalPhysicsTime);
	CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_SyncComponentsToBodies, SceneType == PST_Sync);
	CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_SyncComponentsToBodies_Cloth, SceneType == PST_Cloth);
	CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_SyncComponentsToBodies_Async, SceneType == PST_Async);

	for (FBodyInstance* BodyInstance : ActiveBodyInstances[SceneType])
	{
		if (BodyInstance == nullptr) { continue; }

		check(BodyInstance->OwnerComponent->IsRegistered()); // shouldn't have a physics body for a non-registered component!

		AActor* Owner = BodyInstance->OwnerComponent->GetOwner();

		// See if the transform is actually different, and if so, move the component to match physics
		const FTransform NewTransform = BodyInstance->GetUnrealWorldTransform_AssumesLocked();
		if (!NewTransform.EqualsNoScale(BodyInstance->OwnerComponent->ComponentToWorld))
		{
			const FVector MoveBy = NewTransform.GetLocation() - BodyInstance->OwnerComponent->ComponentToWorld.GetLocation();
			const FQuat NewRotation = NewTransform.GetRotation();

			//@warning: do not reference BodyInstance again after calling MoveComponent() - events from the move could have made it unusable (destroying the actor, SetPhysics(), etc)
			BodyInstance->OwnerComponent->MoveComponent(MoveBy, NewRotation, false, NULL, MOVECOMP_SkipPhysicsMove);
		}

		// Check if we didn't fall out of the world
		if (Owner != NULL && !Owner->IsPendingKill())
		{
			Owner->CheckStillInWorld();
		}
	}

#if WITH_APEX
	if (ActiveDestructibleActors[SceneType].Num())
	{
		UDestructibleComponent::UpdateDestructibleChunkTM(ActiveDestructibleActors[SceneType]);
	}
#endif
}

void FPhysScene::DispatchPhysNotifications_AssumesLocked()
{
	SCOPE_CYCLE_COUNTER(STAT_PhysicsEventTime);

	for(int32 SceneType = 0; SceneType < PST_MAX; ++SceneType)
	{
		TArray<FCollisionNotifyInfo>& PendingCollisionNotifies = GetPendingCollisionNotifies(SceneType);

		// Let the game-specific PhysicsCollisionHandler process any physics collisions that took place
		if (OwningWorld != NULL && OwningWorld->PhysicsCollisionHandler != NULL)
		{
			OwningWorld->PhysicsCollisionHandler->HandlePhysicsCollisions_AssumesLocked(PendingCollisionNotifies);
		}

		// Fire any collision notifies in the queue.
		for (int32 i = 0; i<PendingCollisionNotifies.Num(); i++)
		{
			FCollisionNotifyInfo& NotifyInfo = PendingCollisionNotifies[i];
			if (NotifyInfo.RigidCollisionData.ContactInfos.Num() > 0)
			{
				if (NotifyInfo.bCallEvent0 && NotifyInfo.IsValidForNotify() && NotifyInfo.Info0.Actor.IsValid())
				{
					NotifyInfo.Info0.Actor->DispatchPhysicsCollisionHit(NotifyInfo.Info0, NotifyInfo.Info1, NotifyInfo.RigidCollisionData);
				}

				// Need to check IsValidForNotify again in case first call broke something.
				if (NotifyInfo.bCallEvent1 && NotifyInfo.IsValidForNotify() && NotifyInfo.Info1.Actor.IsValid())
				{
					NotifyInfo.RigidCollisionData.SwapContactOrders();
					NotifyInfo.Info1.Actor->DispatchPhysicsCollisionHit(NotifyInfo.Info1, NotifyInfo.Info0, NotifyInfo.RigidCollisionData);
				}
			}
		}
		PendingCollisionNotifies.Reset();
	}

#if WITH_APEX
	for (const FPendingApexDamageEvent& PendingDamageEvent : PendingApexDamageManager->PendingDamageEvents)
	{
		if(UDestructibleComponent* DestructibleComponent = PendingDamageEvent.DestructibleComponent.Get())
		{
			const NxApexDamageEventReportData & damageEvent = PendingDamageEvent.DamageEvent;
			check(DestructibleComponent == Cast<UDestructibleComponent>(FPhysxUserData::Get<UPrimitiveComponent>(damageEvent.destructible->userData)))	//we store this as a weak pointer above in case one of the callbacks decided to call DestroyComponent
			DestructibleComponent->OnDamageEvent(damageEvent);
		}
	}

	PendingApexDamageManager->PendingDamageEvents.Empty();
#endif

#if WITH_PHYSX
	for (int32 SceneType = 0; SceneType < PST_MAX; ++SceneType)
	{
		for (auto MapItr = PendingSleepEvents[SceneType].CreateIterator(); MapItr; ++MapItr)
		{
			PxActor* Actor = MapItr.Key();
			if(FBodyInstance* BodyInstance = FPhysxUserData::Get<FBodyInstance>(Actor->userData))
			{
				if(UPrimitiveComponent* PrimitiveComponent = BodyInstance->OwnerComponent.Get())
				{
					PrimitiveComponent->DispatchWakeEvents(MapItr.Value(), BodyInstance->BodySetup->BoneName);
				}
			}
		}

		PendingSleepEvents[SceneType].Empty();
	}
#endif
}

void FPhysScene::SetUpForFrame(const FVector* NewGrav, float InDeltaSeconds, float InMaxPhysicsDeltaTime)
{
	DeltaSeconds = InDeltaSeconds;
	MaxPhysicsDeltaTime = InMaxPhysicsDeltaTime;
#if WITH_PHYSX
	if (NewGrav)
	{
		// Loop through scene types to get all scenes
		for (uint32 SceneType = 0; SceneType < NumPhysScenes; ++SceneType)
		{
			PxScene* PScene = GetPhysXScene(SceneType);
			if (PScene != NULL)
			{
				//@todo phys_thread don't do this if gravity changes

				//@todo, to me it looks like we should avoid this if the gravity has not changed, the lock is probably expensive
				// Lock scene lock, in case it is required
				SCENE_LOCK_WRITE(PScene);

				PScene->setGravity(U2PVector(*NewGrav));

#if WITH_APEX_CLOTHING
				NxApexScene* ApexScene = GetApexScene(SceneType);
				if(SceneType == PST_Cloth && ApexScene)
				{
					ApexScene->updateGravity();
				}
#endif

				// Unlock scene lock, in case it is required
				SCENE_UNLOCK_WRITE(PScene);
			}
		}
	}
#endif
}

FAutoConsoleTaskPriority CPrio_PhyXSceneCompletion(
	TEXT("TaskGraph.TaskPriorities.PhyXSceneCompletion"),
	TEXT("Task and thread priority for PhysicsSceneCompletion."),
	ENamedThreads::HighThreadPriority, // if we have high priority task threads, then use them...
	ENamedThreads::HighTaskPriority, // .. at high task priority
	ENamedThreads::HighTaskPriority // if we don't have hi pri threads, then use normal priority threads at high task priority instead
	);



void FPhysScene::StartFrame()
{
	FGraphEventArray FinishPrerequisites;

	//Update the collision disable table before ticking
	FlushDeferredCollisionDisableTableQueue();
#if WITH_PHYSX
	// Flush list of deferred actors to add to the scene
	FlushDeferredActors();
#endif

	// Run the sync scene
	TickPhysScene(PST_Sync, PhysicsSubsceneCompletion[PST_Sync]);
	{
		FGraphEventArray MainScenePrerequisites;
		if (FrameLagAsync() && bAsyncSceneEnabled)
		{
			if (FrameLaggedPhysicsSubsceneCompletion[PST_Async].GetReference() && !FrameLaggedPhysicsSubsceneCompletion[PST_Async]->IsComplete())
			{
				MainScenePrerequisites.Add(FrameLaggedPhysicsSubsceneCompletion[PST_Async]);
				FinishPrerequisites.Add(FrameLaggedPhysicsSubsceneCompletion[PST_Async]);
			}
		}
		if (PhysicsSubsceneCompletion[PST_Sync].GetReference())
		{
			MainScenePrerequisites.Add(PhysicsSubsceneCompletion[PST_Sync]);

			DECLARE_CYCLE_STAT(TEXT("FDelegateGraphTask.ProcessPhysScene_Sync"),
				STAT_FDelegateGraphTask_ProcessPhysScene_Sync,
				STATGROUP_TaskGraphTasks);

			new (FinishPrerequisites)FGraphEventRef(
				FDelegateGraphTask::CreateAndDispatchWhenReady(
					FDelegateGraphTask::FDelegate::CreateRaw(this, &FPhysScene::SceneCompletionTask, PST_Sync),
					GET_STATID(STAT_FDelegateGraphTask_ProcessPhysScene_Sync), &MainScenePrerequisites,
					ENamedThreads::GameThread, ENamedThreads::GameThread
				)
			);
		}
	}

	if (!FrameLagAsync() && bAsyncSceneEnabled)
	{
		TickPhysScene(PST_Async, PhysicsSubsceneCompletion[PST_Async]);
		if (PhysicsSubsceneCompletion[PST_Async].GetReference())
		{
			DECLARE_CYCLE_STAT(TEXT("FDelegateGraphTask.ProcessPhysScene_Async"),
				STAT_FDelegateGraphTask_ProcessPhysScene_Async,
				STATGROUP_TaskGraphTasks);

			new (FinishPrerequisites)FGraphEventRef(
				FDelegateGraphTask::CreateAndDispatchWhenReady(
					FDelegateGraphTask::FDelegate::CreateRaw(this, &FPhysScene::SceneCompletionTask, PST_Async),
					GET_STATID(STAT_FDelegateGraphTask_ProcessPhysScene_Async), PhysicsSubsceneCompletion[PST_Async],
					ENamedThreads::GameThread, ENamedThreads::GameThread
				)
			);
		}
	}

	check(!PhysicsSceneCompletion.GetReference()); // this should have been cleared
	if (FinishPrerequisites.Num())
	{
		if (FinishPrerequisites.Num() > 1)  // we don't need to create a new task if we only have one prerequisite
		{
			DECLARE_CYCLE_STAT(TEXT("FNullGraphTask.ProcessPhysScene_Join"),
				STAT_FNullGraphTask_ProcessPhysScene_Join,
				STATGROUP_TaskGraphTasks);

			PhysicsSceneCompletion = TGraphTask<FNullGraphTask>::CreateTask(&FinishPrerequisites, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(
				GET_STATID(STAT_FNullGraphTask_ProcessPhysScene_Join), PhysSingleThreadedMode() ? ENamedThreads::GameThread : CPrio_PhyXSceneCompletion.Get());
		}
		else
		{
			PhysicsSceneCompletion = FinishPrerequisites[0]; // we don't need a join
		}
	}

	// Record the sync tick time for use with the async tick
	SyncDeltaSeconds = DeltaSeconds;
}

TAutoConsoleVariable<int32> CVarEnableClothPhysics(TEXT("p.ClothPhysics"), 1, TEXT("If 1, physics cloth will be used for simulation."));

void FPhysScene::StartAsync()
{
	FGraphEventArray FinishPrerequisites;

	//If the async scene is lagged we start it here to make sure any cloth in the async scene is using the results of the previous simulation.
	if (FrameLagAsync() && bAsyncSceneEnabled)
	{
		TickPhysScene(PST_Async, PhysicsSubsceneCompletion[PST_Async]);
		if (PhysicsSubsceneCompletion[PST_Async].GetReference())
		{
			DECLARE_CYCLE_STAT(TEXT("FDelegateGraphTask.ProcessPhysScene_Async"),
				STAT_FDelegateGraphTask_ProcessPhysScene_Async,
				STATGROUP_TaskGraphTasks);

			FrameLaggedPhysicsSubsceneCompletion[PST_Async] = FDelegateGraphTask::CreateAndDispatchWhenReady(
				FDelegateGraphTask::FDelegate::CreateRaw(this, &FPhysScene::SceneCompletionTask, PST_Async),
				GET_STATID(STAT_FDelegateGraphTask_ProcessPhysScene_Async), PhysicsSubsceneCompletion[PST_Async],
				ENamedThreads::GameThread, ENamedThreads::GameThread
			);
		}
	}
}

void FPhysScene::EndFrame(ULineBatchComponent* InLineBatcher)
{
	check(IsInGameThread());

	PhysicsSceneCompletion = NULL;

	/**
	* At this point physics simulation has finished. We obtain both scene locks so that the various read/write operations needed can be done quickly.
	* This means that anyone attempting to write on other threads will be blocked. This is OK because acessing any of these game objects from another thread is probably a bad idea!
	*/

	SCOPED_SCENE_WRITE_LOCK(GetPhysXScene(PST_Sync));
	SCOPED_SCENE_WRITE_LOCK(bAsyncSceneEnabled ? GetPhysXScene(PST_Async) : nullptr);

#if ( WITH_PHYSX  && !(UE_BUILD_SHIPPING || WITH_PHYSX_RELEASE))
	GatherPhysXStats_AssumesLocked(GetPhysXScene(PST_Sync), HasAsyncScene() ? GetPhysXScene(PST_Async) : nullptr);
#endif

	if (bAsyncSceneEnabled)
	{
		SyncComponentsToBodies_AssumesLocked(PST_Async);
	}

	SyncComponentsToBodies_AssumesLocked(PST_Sync);

	// Perform any collision notification events
	DispatchPhysNotifications_AssumesLocked();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST || WITH_PHYSX_RELEASE)
	// Handle debug rendering
	if (InLineBatcher)
	{
		AddDebugLines(PST_Sync, InLineBatcher);

		if (bAsyncSceneEnabled)
		{
			AddDebugLines(PST_Async, InLineBatcher);
		}

	}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST || WITH_PHYSX_RELEASE)
}

#if WITH_PHYSX
/** Helper struct that puts all awake actors to sleep and then later wakes them back up */
struct FHelpEnsureCollisionTreeIsBuilt
{
	FHelpEnsureCollisionTreeIsBuilt(PxScene* InPScene)
	: PScene(InPScene)
	{
		if(PScene)
		{
			SCOPED_SCENE_WRITE_LOCK(PScene);
			const int32 NumActors = PScene->getNbActors(PxActorTypeFlag::eRIGID_DYNAMIC);

			if (NumActors)
			{
				ActorBuffer.AddUninitialized(NumActors);
				PScene->getActors(PxActorTypeFlag::eRIGID_DYNAMIC, &ActorBuffer[0], NumActors);

				for (PxActor*& PActor : ActorBuffer)
				{
					if (PActor)
					{
						if (PxRigidDynamic* PDynamic = PActor->isRigidDynamic())
						{
							if (PDynamic->isSleeping() == false)
							{
								PDynamic->putToSleep();
							}
							else
							{
								PActor = nullptr;
							}
						}
					}
				}
			}
		}
	}

	~FHelpEnsureCollisionTreeIsBuilt()
	{
		SCOPED_SCENE_WRITE_LOCK(PScene);
		for (PxActor* PActor : ActorBuffer)
		{
			if (PActor)
			{
				if (PxRigidDynamic* PDynamic = PActor->isRigidDynamic())
				{
					PDynamic->wakeUp();
				}
			}
		}
	}

private:

	TArray<PxActor*> ActorBuffer;
	PxScene* PScene;
};

#endif


DECLARE_CYCLE_STAT(TEXT("EnsureCollisionTreeIsBuilt"), STAT_PhysicsEnsureCollisionTreeIsBuilt, STATGROUP_Physics);

void FPhysScene::EnsureCollisionTreeIsBuilt(UWorld* World)
{
	check(IsInGameThread());
	

	SCOPE_CYCLE_COUNTER(STAT_PhysicsEnsureCollisionTreeIsBuilt);
	//We have to call fetchResults several times to update the internal data structures. PhysX doesn't have an API for this so we have to make all actors sleep before doing this

	SetIsStaticLoading(true);

#if WITH_PHYSX
	FHelpEnsureCollisionTreeIsBuilt SyncSceneHelper(GetPhysXScene(PST_Sync));
	FHelpEnsureCollisionTreeIsBuilt AsyncSceneHelper(HasAsyncScene() ? GetPhysXScene(PST_Async) : nullptr);
#endif

	for (int Iteration = 0; Iteration < 6; ++Iteration)
	{
		World->SetupPhysicsTickFunctions(0.1f);
		StartFrame();
		WaitPhysScenes();
		EndFrame(nullptr);
	}

	SetIsStaticLoading(false);
}

void FPhysScene::SetIsStaticLoading(bool bStaticLoading)
{
#if WITH_PHYSX
	// Loop through scene types to get all scenes
	for (uint32 SceneType = 0; SceneType < NumPhysScenes; ++SceneType)
	{
		PxScene* PScene = GetPhysXScene(SceneType);
		if (PScene != NULL)
		{
			// Lock scene lock, in case it is required
			SCENE_LOCK_WRITE(PScene);

			// Sets the rebuild rate hint, to 1 frame if static loading
			PScene->setDynamicTreeRebuildRateHint(bStaticLoading ? 5 : PhysXSlowRebuildRate);

			// Unlock scene lock, in case it is required
			SCENE_UNLOCK_WRITE(PScene);
		}
	}
#endif
}

#if WITH_PHYSX
/** Utility for looking up the PxScene associated with this FPhysScene. */
PxScene* FPhysScene::GetPhysXScene(uint32 SceneType)
{
	check(SceneType < NumPhysScenes);
	return GetPhysXSceneFromIndex(PhysXSceneIndex[SceneType]);
}

#if WITH_VEHICLE
FPhysXVehicleManager* FPhysScene::GetVehicleManager()
{
	return VehicleManager;
}
#endif

#if WITH_APEX
NxApexScene* FPhysScene::GetApexScene(uint32 SceneType)
{
	check(SceneType < NumPhysScenes);
	return GetApexSceneFromIndex(PhysXSceneIndex[SceneType]);
}
#endif // WITH_APEX

static void BatchPxRenderBufferLines(class ULineBatchComponent& LineBatcherToUse, const PxRenderBuffer& DebugData)
{
	int32 NumPoints = DebugData.getNbPoints();
	if (NumPoints > 0)
	{
		const PxDebugPoint* Points = DebugData.getPoints();
		for (int32 i = 0; i<NumPoints; i++)
		{
			LineBatcherToUse.DrawPoint(P2UVector(Points->pos), FColor((uint32)Points->color), 2, SDPG_World);

			Points++;
		}
	}

	// Build a list of all the lines we want to draw
	TArray<FBatchedLine> DebugLines;

	// Add all the 'lines' from PhysX
	int32 NumLines = DebugData.getNbLines();
	if (NumLines > 0)
	{
		const PxDebugLine* Lines = DebugData.getLines();
		for (int32 i = 0; i<NumLines; i++)
		{
			new(DebugLines)FBatchedLine(P2UVector(Lines->pos0), P2UVector(Lines->pos1), FColor((uint32)Lines->color0), 0.f, 0.0f, SDPG_World);
			Lines++;
		}
	}

	// Add all the 'triangles' from PhysX
	int32 NumTris = DebugData.getNbTriangles();
	if (NumTris > 0)
	{
		const PxDebugTriangle* Triangles = DebugData.getTriangles();
		for (int32 i = 0; i<NumTris; i++)
		{
			new(DebugLines)FBatchedLine(P2UVector(Triangles->pos0), P2UVector(Triangles->pos1), FColor((uint32)Triangles->color0), 0.f, 0.0f, SDPG_World);
			new(DebugLines)FBatchedLine(P2UVector(Triangles->pos1), P2UVector(Triangles->pos2), FColor((uint32)Triangles->color1), 0.f, 0.0f, SDPG_World);
			new(DebugLines)FBatchedLine(P2UVector(Triangles->pos2), P2UVector(Triangles->pos0), FColor((uint32)Triangles->color2), 0.f, 0.0f, SDPG_World);
			Triangles++;
		}
	}

	// Draw them all in one call.
	if (DebugLines.Num() > 0)
	{
		LineBatcherToUse.DrawLines(DebugLines);
	}
}
#endif // WITH_PHYSX


/** Add any debug lines from the physics scene to the supplied line batcher. */
void FPhysScene::AddDebugLines(uint32 SceneType, class ULineBatchComponent* LineBatcherToUse)
{
	check(SceneType < NumPhysScenes);

	if (LineBatcherToUse)
	{
#if WITH_PHYSX
		// Render PhysX debug data
		PxScene* PScene = GetPhysXScene(SceneType);
		const PxRenderBuffer& DebugData = PScene->getRenderBuffer();
		BatchPxRenderBufferLines(*LineBatcherToUse, DebugData);
#if WITH_APEX
		// Render APEX debug data
		NxApexScene* ApexScene = GetApexScene(SceneType);
		const PxRenderBuffer* RenderBuffer = ApexScene->getRenderBuffer();
		if (RenderBuffer != NULL)
		{
			BatchPxRenderBufferLines(*LineBatcherToUse, *RenderBuffer);
			ApexScene->updateRenderResources();
		}
#endif	// WITH_APEX
#endif	// WITH_PHYSX
	}
}

void FPhysScene::ApplyWorldOffset(FVector InOffset)
{
#if WITH_PHYSX
	// Loop through scene types to get all scenes
	for (uint32 SceneType = 0; SceneType < NumPhysScenes; ++SceneType)
	{
		PxScene* PScene = GetPhysXScene(SceneType);
		if (PScene != NULL)
		{
			// Lock scene lock, in case it is required
			SCENE_LOCK_WRITE(PScene);

			PScene->shiftOrigin(U2PVector(-InOffset));

			// Unlock scene lock, in case it is required
			SCENE_UNLOCK_WRITE(PScene);
		}
	}
#endif
}

void FPhysScene::InitPhysScene(uint32 SceneType)
{
	check(SceneType < NumPhysScenes);

#if WITH_PHYSX

	int64 NumPhysxDispatcher = 0;
	FParse::Value(FCommandLine::Get(), TEXT("physxDispatcher="), NumPhysxDispatcher);
	if (NumPhysxDispatcher == 0 && FParse::Param(FCommandLine::Get(), TEXT("physxDispatcher")))
	{
		NumPhysxDispatcher = 4;	//by default give physx 4 threads
	}

	// Create dispatcher for tasks
	if (PhysSingleThreadedMode())
	{
		CPUDispatcher[SceneType] = new FPhysXCPUDispatcherSingleThread();
	}
	else
	{
		if (NumPhysxDispatcher)
		{
			CPUDispatcher[SceneType] = PxDefaultCpuDispatcherCreate(NumPhysxDispatcher);
		}
		else
		{
			if (SceneType == PST_Cloth)
			{
				CPUDispatcher[SceneType] = new FPhysXCPUDispatcher<true>();
			}
			else
			{
				CPUDispatcher[SceneType] = new FPhysXCPUDispatcher<false>();
			}

		}

	}


	PhysxUserData = FPhysxUserData(this);

	// Create sim event callback
	SimEventCallback[SceneType] = SimEventCallbackFactory.IsValid() ? SimEventCallbackFactory->Create(this, SceneType) : new FPhysXSimEventCallback(this, SceneType);

	// Include scene descriptor in loop, so that we might vary it with scene type
	PxSceneDesc PSceneDesc(GPhysXSDK->getTolerancesScale());
	PSceneDesc.cpuDispatcher = CPUDispatcher[SceneType];

	FPhysSceneShaderInfo PhysSceneShaderInfo;
	PhysSceneShaderInfo.PhysScene = this;
	PSceneDesc.filterShaderData = &PhysSceneShaderInfo;
	PSceneDesc.filterShaderDataSize = sizeof(PhysSceneShaderInfo);

	PSceneDesc.filterShader = PhysXSimFilterShader;
	PSceneDesc.simulationEventCallback = SimEventCallback[SceneType];

	if(UPhysicsSettings::Get()->bEnablePCM)
	{
		PSceneDesc.flags |= PxSceneFlag::eENABLE_PCM;
	}
	

	//LOC_MOD enable kinematic vs kinematic for APEX destructibles. This is for the kinematic cube moving horizontally in QA-Destructible map to collide with the destructible.
	// Was this flag turned off in UE4? Do we want to turn it on for both sync and async scenes?
	PSceneDesc.flags |= PxSceneFlag::eENABLE_KINEMATIC_PAIRS;

	// Set bounce threshold
	PSceneDesc.bounceThresholdVelocity = UPhysicsSettings::Get()->BounceThresholdVelocity;

	// Possibly set flags in async scene for better behavior with piles
#if USE_ADAPTIVE_FORCES_FOR_ASYNC_SCENE
	if (SceneType == PST_Async)
	{
		PSceneDesc.flags |= PxSceneFlag::eADAPTIVE_FORCE;
	}
#endif

#if USE_SPECIAL_FRICTION_MODEL_FOR_ASYNC_SCENE
	if (SceneType == PST_Async)
	{
		PSceneDesc.flags |= PxSceneFlag::eENABLE_ONE_DIRECTIONAL_FRICTION;
	}
#endif

	// If we're frame lagging the async scene (truly running it async) then use the scene lock
#if USE_SCENE_LOCK
	if(UPhysicsSettings::Get()->bWarnMissingLocks)
	{
		PSceneDesc.flags |= PxSceneFlag::eREQUIRE_RW_LOCK;
	}
	
#endif

	if(!UPhysicsSettings::Get()->bDisableActiveTransforms)
	{
		// We want to use 'active transforms'
		PSceneDesc.flags |= PxSceneFlag::eENABLE_ACTIVETRANSFORMS;
	}

	// @TODO Should we set up PSceneDesc.limits? How?

	// Do this to improve loading times, esp. for streaming in sublevels
	PSceneDesc.staticStructure = PxPruningStructure::eDYNAMIC_AABB_TREE;
	// Default to rebuilding tree slowly
	PSceneDesc.dynamicTreeRebuildRateHint = PhysXSlowRebuildRate;

	bool bIsValid = PSceneDesc.isValid();
	if (!bIsValid)
	{
		UE_LOG(LogPhysics, Log, TEXT("Invalid PSceneDesc"));
	}

	// Create scene, and add to map
	PxScene* PScene = GPhysXSDK->createScene(PSceneDesc);

#if WITH_APEX
	// Build the APEX scene descriptor for the PhysX scene
	NxApexSceneDesc ApexSceneDesc;
	ApexSceneDesc.scene = PScene;
	// This interface allows us to modify the PhysX simulation filter shader data with contact pair flags 
	ApexSceneDesc.physX3Interface = &GApexPhysX3Interface;

	// Create the APEX scene from our descriptor
	NxApexScene* ApexScene = GApexSDK->createScene(ApexSceneDesc);

	// This enables debug rendering using the "legacy" method, not using the APEX render API
	ApexScene->setUseDebugRenderable(true);

	// Allocate a view matrix for APEX scene LOD
	ApexScene->allocViewMatrix(physx::ViewMatrixType::LOOK_AT_RH);

	// Add the APEX scene to the map instead of the PhysX scene, since we can access the latter through the former
	GPhysXSceneMap.Add(PhysXSceneCount, ApexScene);
#else	// #if WITH_APEX
	GPhysXSceneMap.Add(PhysXSceneCount, PScene);
#endif	// #if WITH_APEX

	// Lock scene lock, in case it is required
	SCENE_LOCK_WRITE(PScene);

	// enable CCD at scene level
	if (UPhysicsSettings::Get()->bDisableCCD == false)
	{
		PScene->setFlag(PxSceneFlag::eENABLE_CCD, true);
	}

	// Need to turn this on to consider kinematics turning into dynamic. Otherwise, you'll need to call resetFiltering to do the expensive broadphase reinserting 
	PScene->setFlag(PxSceneFlag::eENABLE_KINEMATIC_STATIC_PAIRS, true);

	// Unlock scene lock, in case it is required
	SCENE_UNLOCK_WRITE(PScene);

	// Save pointer to FPhysScene in userdata
	PScene->userData = &PhysxUserData;
#if WITH_APEX
	ApexScene->userData = &PhysxUserData;
#endif

	// Store index of PhysX Scene in this FPhysScene
	this->PhysXSceneIndex[SceneType] = PhysXSceneCount;
	DeferredSceneData[SceneType].SceneIndex = PhysXSceneCount;

	// Increment scene count
	PhysXSceneCount++;

#if WITH_VEHICLE
	// Only create PhysXVehicleManager in the sync scene
	if (SceneType == PST_Sync)
	{
		check(VehicleManager == NULL);
		VehicleManager = new FPhysXVehicleManager(PScene);
	}
#endif

	//Initialize substeppers
	//we don't bother sub-stepping cloth
#if WITH_PHYSX
#if WITH_APEX
	PhysSubSteppers[SceneType] = SceneType == PST_Cloth ? NULL : new FPhysSubstepTask(ApexScene);
#else
	PhysSubSteppers[SceneType] = SceneType == PST_Cloth ? NULL : new FPhysSubstepTask(PScene);
#endif
#endif
#if WITH_VEHICLE
	if (SceneType == PST_Sync)
	{
		PhysSubSteppers[SceneType]->SetVehicleManager(VehicleManager);
	}
#endif
	
#endif // WITH_PHYSX
}

void FPhysScene::TermPhysScene(uint32 SceneType)
{
	check(SceneType < NumPhysScenes);

#if WITH_PHYSX
	PxScene* PScene = GetPhysXScene(SceneType);
	if (PScene != NULL)
	{
#if WITH_APEX
		NxApexScene* ApexScene = GetApexScene(SceneType);
		if (ApexScene != NULL)
		{
			GPhysCommandHandler->DeferredRelease(ApexScene);
		}
#endif // #if WITH_APEX

#if WITH_VEHICLE
		if (SceneType == PST_Sync && VehicleManager != NULL)
		{
			delete VehicleManager;
			VehicleManager = NULL;
		}
#endif

		if (SceneType == PST_Sync && PhysSubSteppers[SceneType])
		{
			PhysSubSteppers[SceneType]->SetVehicleManager(NULL);
		}

		delete PhysSubSteppers[SceneType];
		PhysSubSteppers[SceneType] = NULL;

		// @todo block on any running scene before calling this
		GPhysCommandHandler->DeferredRelease(PScene);
		GPhysCommandHandler->DeferredDeleteSimEventCallback(SimEventCallback[SceneType]);

		// Commands may have accumulated as the scene is terminated - flush any commands for this scene.
		GPhysCommandHandler->Flush();

		// Remove from the map
		GPhysXSceneMap.Remove(PhysXSceneIndex[SceneType]);
	}
#endif
}

#if WITH_PHYSX

void FPhysScene::AddPendingSleepingEvent(PxActor* Actor, SleepEvent::Type SleepEventType, int32 SceneType)
{
	PendingSleepEvents[SceneType].FindOrAdd(Actor) = SleepEventType;
}

void FPhysScene::FDeferredSceneData::FlushDeferredActors()
{
	check(AddInstances.Num() == AddActors.Num());

	if (AddInstances.Num() > 0)
	{
		PxScene* Scene = GetPhysXSceneFromIndex(SceneIndex);
		SCOPED_SCENE_WRITE_LOCK(Scene);

		Scene->addActors(AddActors.GetData(), AddActors.Num());
		int32 Idx = -1;
		for (FBodyInstance* Instance : AddInstances)
		{
			++Idx;
			Instance->CurrentSceneState = BodyInstanceSceneState::Added;

			if(Instance->GetPxRigidDynamic_AssumesLocked())
			{
				// Extra setup necessary for dynamic objects.
				Instance->InitDynamicProperties_AssumesLocked();
			}
		}

		AddInstances.Empty();
		AddActors.Empty();
	}

	check(RemoveInstances.Num() == RemoveActors.Num());

	if (RemoveInstances.Num() > 0)
	{
		PxScene* Scene = GetPhysXSceneFromIndex(SceneIndex);
		SCOPED_SCENE_WRITE_LOCK(Scene);

		Scene->removeActors(RemoveActors.GetData(), RemoveActors.Num());

		for (FBodyInstance* Instance : AddInstances)
		{
			Instance->CurrentSceneState = BodyInstanceSceneState::Removed;
		}

		RemoveInstances.Empty();
		RemoveActors.Empty();
	}
}

void FPhysScene::FlushDeferredActors()
{
	check(IsInGameThread());

	for(FDeferredSceneData& Deferred : DeferredSceneData)
	{
		Deferred.FlushDeferredActors();
	}
}



void FPhysScene::FDeferredSceneData::DeferAddActor(FBodyInstance* OwningInstance, PxActor* Actor)
{
	// Allowed to be unadded or awaiting add here (objects can be in more than one scene
	if (OwningInstance->CurrentSceneState == BodyInstanceSceneState::NotAdded ||
		OwningInstance->CurrentSceneState == BodyInstanceSceneState::AwaitingAdd)
	{
		OwningInstance->CurrentSceneState = BodyInstanceSceneState::AwaitingAdd;
		AddInstances.Add(OwningInstance);
		AddActors.Add(Actor);
	}
	else if (OwningInstance->CurrentSceneState == BodyInstanceSceneState::AwaitingRemove)
	{
		// We were waiting to be removed, but we're canceling that

		OwningInstance->CurrentSceneState = BodyInstanceSceneState::Added;
		RemoveInstances.RemoveSingle(OwningInstance);
		RemoveActors.RemoveSingle(Actor);
	}
}

void FPhysScene::DeferAddActor(FBodyInstance* OwningInstance, PxActor* Actor, EPhysicsSceneType SceneType)
{
	check(IsInGameThread());
	check(OwningInstance && Actor && SceneType < PST_MAX);
	DeferredSceneData[SceneType].DeferAddActor(OwningInstance, Actor);
}


void FPhysScene::FDeferredSceneData::DeferAddActors(TArray<FBodyInstance*>& OwningInstances, TArray<PxActor*>& Actors)
{
	int32 Num = OwningInstances.Num();
	AddInstances.Reserve(AddInstances.Num() + Num);
	AddActors.Reserve(AddActors.Num() + Num);

	for (int32 Idx = 0; Idx < Num; ++Idx)
	{
		DeferAddActor(OwningInstances[Idx], Actors[Idx]);
	}
}

void FPhysScene::DeferAddActors(TArray<FBodyInstance*>& OwningInstances, TArray<PxActor*>& Actors, EPhysicsSceneType SceneType)
{
	check(IsInGameThread());
	check(SceneType < PST_MAX);
	DeferredSceneData[SceneType].DeferAddActors(OwningInstances, Actors);
}

void FPhysScene::FDeferredSceneData::DeferRemoveActor(FBodyInstance* OwningInstance, PxActor* Actor)
{
	if (OwningInstance->CurrentSceneState == BodyInstanceSceneState::Added ||
		OwningInstance->CurrentSceneState == BodyInstanceSceneState::AwaitingRemove)
	{
		OwningInstance->CurrentSceneState = BodyInstanceSceneState::AwaitingRemove;
		RemoveInstances.Add(OwningInstance);
		RemoveActors.Add(Actor);
	}
	else if (OwningInstance->CurrentSceneState == BodyInstanceSceneState::AwaitingAdd)
	{
		// We were waiting to add but now we're canceling it
		OwningInstance->CurrentSceneState = BodyInstanceSceneState::Removed;
		AddInstances.RemoveSingle(OwningInstance);
		AddActors.RemoveSingle(Actor);
	}
}

void FPhysScene::DeferRemoveActor(FBodyInstance* OwningInstance, PxActor* Actor, EPhysicsSceneType SceneType)
{
	check(IsInGameThread());
	check(OwningInstance && Actor && SceneType < PST_MAX);
	DeferredSceneData[SceneType].DeferRemoveActor(OwningInstance, Actor);
}

#endif