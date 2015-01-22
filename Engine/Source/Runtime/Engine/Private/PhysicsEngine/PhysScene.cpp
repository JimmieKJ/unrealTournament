// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "EnginePrivate.h"
#include "PhysicsPublic.h"

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

#define USE_ADAPTIVE_FORCES_FOR_ASYNC_SCENE			1
#define USE_SPECIAL_FRICTION_MODEL_FOR_ASYNC_SCENE	0

static int32 PhysXSceneCount = 1;
static const int PhysXSlowRebuildRate = 10;

// Cvars
static TAutoConsoleVariable<float> CVarBounceThresholdVelocity(
	TEXT("p.BounceThresholdVelocity"),
	200.f,
	TEXT("A contact with a relative velocity below this will not bounce. Default: 200"),
	ECVF_Default);

FORCEINLINE EPhysicsSceneType SceneType(const FBodyInstance* BodyInstance)
{
#if WITH_PHYSX
	//This is a helper function for dynamic actors - static actors are in both scenes
	check(BodyInstance->GetPxRigidBody());
	return UPhysicsSettings::Get()->bEnableAsyncScene && BodyInstance->UseAsyncScene() ? PST_Async : PST_Sync;
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


/** Exposes creation of physics-engine scene outside Engine (for use with PhAT for example). */
FPhysScene::FPhysScene()
{
	LineBatcher = NULL;
	OwningWorld = NULL;
#if WITH_PHYSX
#if WITH_VEHICLE
	VehicleManager = NULL;
#endif
	PhysxUserData = FPhysxUserData(this);

	// Create dispatcher for tasks
	if (PhysSingleThreadedMode())
	{
		CPUDispatcher = new FPhysXCPUDispatcherSingleThread();
	}
	else
	{
		CPUDispatcher = new FPhysXCPUDispatcher();
	}
	// Create sim event callback
	SimEventCallback = new FPhysXSimEventCallback();
#endif	//#if WITH_PHYSX

	// initialize console variable - this console variable change requires it to restart scene. 
	static bool bInitializeConsoleVariable = true;
	static float InitialAverageFrameRate = 0.016f;


	UPhysicsSettings * PhysSetting = UPhysicsSettings::Get();
	if (bInitializeConsoleVariable)
	{
		InitialAverageFrameRate = PhysSetting->InitialAverageFrameRate;
		FrameTimeSmoothingFactor[PST_Sync] = PhysSetting->SyncSceneSmoothingFactor;
		FrameTimeSmoothingFactor[PST_Async] = PhysSetting->AsyncSceneSmoothingFactor;
		bInitializeConsoleVariable = false;
	}

#if WITH_SUBSTEPPING
	bSubstepping = PhysSetting->bSubstepping;
	bSubsteppingAsync = PhysSetting->bSubsteppingAsync;
#endif
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
		AveragedFrameTime[SceneType] = InitialAverageFrameRate;

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
#endif
}



/** Exposes destruction of physics-engine scene outside Engine. */
FPhysScene::~FPhysScene()
{
	// Make sure no scenes are left simulating (no-ops if not simulating)
	WaitPhysScenes();
	// Loop through scene types to get all scenes
	for (uint32 SceneType = 0; SceneType < NumPhysScenes; ++SceneType)
	{

		// Destroy the physics scene
		TermPhysScene(SceneType);
	}

#if WITH_PHYSX
	GPhysCommandHandler->DeferredDeleteCPUDispathcer(CPUDispatcher);
	GPhysCommandHandler->DeferredDeleteSimEventCallback(SimEventCallback);
#endif	//#if WITH_PHYSX
}

namespace
{

bool UseSyncTime(uint32 SceneType)
{
	return (FrameLagAsync() && SceneType == PST_Async);
}

}

bool FPhysScene::GetKinematicTarget(const FBodyInstance* BodyInstance, FTransform& OutTM) const
{
#if WITH_PHYSX
	if (PxRigidDynamic * PRigidDynamic = BodyInstance->GetPxRigidDynamic())
	{
#if WITH_SUBSTEPPING
		uint32 BodySceneType = SceneType(BodyInstance);
		if (IsSubstepping(BodySceneType))
		{
			FPhysSubstepTask * PhysSubStepper = PhysSubSteppers[BodySceneType];
			return PhysSubStepper->GetKinematicTarget(BodyInstance, OutTM);
		}
		else
#endif
		{

			SCOPED_SCENE_READ_LOCK(PRigidDynamic->getScene());
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

void FPhysScene::SetKinematicTarget(FBodyInstance* BodyInstance, const FTransform& TargetTransform, bool bAllowSubstepping)
{
	TargetTransform.DiagnosticCheckNaN_All();

#if WITH_PHYSX
	if (PxRigidDynamic * PRigidDynamic = BodyInstance->GetPxRigidDynamic())
	{
#if WITH_SUBSTEPPING
		uint32 BodySceneType = SceneType(BodyInstance);
		if (bAllowSubstepping && IsSubstepping(BodySceneType))
		{
			FPhysSubstepTask * PhysSubStepper = PhysSubSteppers[BodySceneType];
			PhysSubStepper->SetKinematicTarget(BodyInstance, TargetTransform);
		}
		else
#endif
		{
			const PxTransform PNewPose = U2PTransform(TargetTransform);
			check(PNewPose.isValid());

			SCOPED_SCENE_WRITE_LOCK(PRigidDynamic->getScene());
			PRigidDynamic->setKinematicTarget(PNewPose);
		}
	}
#endif
}

void FPhysScene::AddCustomPhysics(FBodyInstance* BodyInstance, FCalculateCustomPhysics& CalculateCustomPhysics)
{
#if WITH_PHYSX
#if WITH_SUBSTEPPING
	uint32 BodySceneType = SceneType(BodyInstance);
	if (IsSubstepping(BodySceneType))
	{
		FPhysSubstepTask * PhysSubStepper = PhysSubSteppers[SceneType(BodyInstance)];
		PhysSubStepper->AddCustomPhysics(BodyInstance, CalculateCustomPhysics);
	}
	else
#endif
	{
		// Since physics frame is set up before "pre-physics" tick group is called, can just fetch delta time from there
		CalculateCustomPhysics.ExecuteIfBound(this->DeltaSeconds, BodyInstance);
	}
#endif
}

void FPhysScene::AddForce(FBodyInstance* BodyInstance, const FVector& Force, bool bAllowSubstepping)
{
#if WITH_PHYSX

	if (PxRigidBody * PRigidBody = BodyInstance->GetPxRigidBody())
	{
#if WITH_SUBSTEPPING
		uint32 BodySceneType = SceneType(BodyInstance);
		if (bAllowSubstepping && IsSubstepping(BodySceneType))
		{
			FPhysSubstepTask * PhysSubStepper = PhysSubSteppers[BodySceneType];
			PhysSubStepper->AddForce(BodyInstance, Force);
		}
		else
#endif
		{
			SCOPED_SCENE_WRITE_LOCK(PRigidBody->getScene());
			PRigidBody->addForce(U2PVector(Force), PxForceMode::eFORCE, true);
		}
	}
#endif
}

void FPhysScene::AddForceAtPosition(FBodyInstance* BodyInstance, const FVector& Force, const FVector& Position, bool bAllowSubstepping)
{
#if WITH_PHYSX

	if (PxRigidBody * PRigidBody = BodyInstance->GetPxRigidBody())
	{
#if WITH_SUBSTEPPING
		uint32 BodySceneType = SceneType(BodyInstance);
		if (bAllowSubstepping && IsSubstepping(BodySceneType))
		{
			FPhysSubstepTask * PhysSubStepper = PhysSubSteppers[BodySceneType];
			PhysSubStepper->AddForceAtPosition(BodyInstance, Force, Position);
		}
		else
#endif
		{
			SCOPED_SCENE_WRITE_LOCK(PRigidBody->getScene());
			PxRigidBodyExt::addForceAtPos(*PRigidBody, U2PVector(Force), U2PVector(Position), PxForceMode::eFORCE, true);
		}
	}
#endif
}

void FPhysScene::AddTorque(FBodyInstance* BodyInstance, const FVector& Torque, bool bAllowSubstepping)
{
#if WITH_PHYSX

	if (PxRigidBody * PRigidBody = BodyInstance->GetPxRigidBody())
	{
#if WITH_SUBSTEPPING
		uint32 BodySceneType = SceneType(BodyInstance);
		if (bAllowSubstepping && IsSubstepping(BodySceneType))
		{
			FPhysSubstepTask * PhysSubStepper = PhysSubSteppers[BodySceneType];
			PhysSubStepper->AddTorque(BodyInstance, Torque);
		}
		else
#endif
		{
			SCOPED_SCENE_WRITE_LOCK(PRigidBody->getScene());
			PRigidBody->addTorque(U2PVector(Torque), PxForceMode::eFORCE, true);
		}
	}
#endif
}

#if WITH_PHYSX
void FPhysScene::RemoveActiveBody(FBodyInstance* BodyInstance, uint32 SceneType)
{
	int32 BodyIndex = ActiveBodyInstances[SceneType].Find(BodyInstance);
	if (BodyIndex != INDEX_NONE)
	{
		ActiveBodyInstances[SceneType][BodyIndex] = nullptr;
	}
}
#endif
void FPhysScene::TermBody(FBodyInstance* BodyInstance)
{
#if WITH_SUBSTEPPING
	if (PxRigidBody * PRigidBody = BodyInstance->GetPxRigidBody())
	{
		FPhysSubstepTask * PhysSubStepper = PhysSubSteppers[SceneType(BodyInstance)];
		PhysSubStepper->RemoveBodyInstance(BodyInstance);
	}
#endif
#if WITH_PHYSX
	RemoveActiveBody(BodyInstance, PST_Sync);
	RemoveActiveBody(BodyInstance, PST_Async);
#endif
}

#if WITH_SUBSTEPPING

#if WITH_APEX
void FPhysScene::DeferredDestructibleDamageNotify(const NxApexDamageEventReportData& damageEvent)
{
	//TODO: waiting for support on this from NVIDIA
	//DestructibleDamageEventQueue.Add(damageEvent);
}
#endif

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
		PhysXCompletionTask* Task = new PhysXCompletionTask(InOutCompletionEvent, PScene->getTaskManager());
		ENamedThreads::Type NamedThread = PhysSingleThreadedMode() ? ENamedThreads::GameThread : ENamedThreads::AnyThread;

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

#endif //#if WITH_SUBSTEPPING

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

void GatherPhysXStats(PxScene* PScene, uint32 SceneType)
{
	/** Gather PhysX stats */
	if (SceneType == 0)
	{
		if (PScene)
		{
			PxSimulationStatistics SimStats;
			PScene->getSimulationStatistics(SimStats);

			SET_DWORD_STAT(STAT_NumActiveConstraints, SimStats.nbActiveConstraints);
			SET_DWORD_STAT(STAT_NumActiveSimulatedBodies, SimStats.nbActiveDynamicBodies);
			SET_DWORD_STAT(STAT_NumActiveKinematicBodies, SimStats.nbActiveKinematicBodies);
			SET_DWORD_STAT(STAT_NumStaticBodies, SimStats.nbStaticBodies);
			SET_DWORD_STAT(STAT_NumMobileBodies, SimStats.nbDynamicBodies);
			
			SET_DWORD_STAT(STAT_NumBroadphaseAdds, SimStats.getNbBroadPhaseAdds(PxSimulationStatistics::VolumeType::eRIGID_BODY));
			SET_DWORD_STAT(STAT_NumBroadphaseRemoves, SimStats.getNbBroadPhaseRemoves(PxSimulationStatistics::VolumeType::eRIGID_BODY));

			uint32 NumShapes = 0;
			for (int32 GeomType = 0; GeomType < PxGeometryType::eGEOMETRY_COUNT; ++GeomType)
			{
				NumShapes += SimStats.nbShapes[GeomType];
			}

			SET_DWORD_STAT(STAT_NumShapes, NumShapes);

		}

	}

#if 0	//this is not reliable right now
	else if (SceneType == 1 & UPhysicsSettings::Get()->bEnableAsyncScene)
	{
		//Having to duplicate because of macros. In theory we can fix this but need to get this quickly
		PxSimulationStatistics SimStats;
		PScene->getSimulationStatistics(SimStats);

		SET_DWORD_STAT(STAT_NumActiveConstraintsAsync, SimStats.nbActiveConstraints);
		SET_DWORD_STAT(STAT_NumActiveSimulatedBodiesAsync, SimStats.nbActiveDynamicBodies);
		SET_DWORD_STAT(STAT_NumActiveKinematicBodiesAsync, SimStats.nbActiveKinematicBodies);
		SET_DWORD_STAT(STAT_NumStaticBodiesAsync, SimStats.nbStaticBodies);
		SET_DWORD_STAT(STAT_NumMobileBodiesAsync, SimStats.nbDynamicBodies);

		SET_DWORD_STAT(STAT_NumBroadphaseAddsAsync, SimStats.getNbBroadPhaseAdds(PxSimulationStatistics::VolumeType::eRIGID_BODY));
		SET_DWORD_STAT(STAT_NumBroadphaseRemovesAsync, SimStats.getNbBroadPhaseRemoves(PxSimulationStatistics::VolumeType::eRIGID_BODY));

		uint32 NumShapes = 0;
		for (int32 GeomType = 0; GeomType < PxGeometryType::eGEOMETRY_COUNT; ++GeomType)
		{
			NumShapes += SimStats.nbShapes[GeomType];
		}

		SET_DWORD_STAT(STAT_NumShapesAsync, NumShapes);
	}
#endif
}

/** Exposes ticking of physics-engine scene outside Engine. */
void FPhysScene::TickPhysScene(uint32 SceneType, FGraphEventRef& InOutCompletionEvent)
{
	SCOPE_CYCLE_COUNTER(STAT_TotalPhysicsTime);
	SCOPE_CYCLE_COUNTER(STAT_PhysicsKickOffDynamicsTime);

	check(SceneType < NumPhysScenes);

	if (bPhysXSceneExecuting[SceneType] != 0)
	{
		// Already executing this scene, must call WaitPhysScene before calling this function again.
		UE_LOG(LogPhysics, Log, TEXT("TickPhysScene: Already executing scene (%d) - aborting."), SceneType);
		return;
	}

#if WITH_SUBSTEPPING
	if (IsSubstepping(SceneType))	//we don't bother sub-stepping cloth
	{
		//We're about to start stepping so swap buffers. Might want to find a better place for this?
		PhysSubSteppers[SceneType]->SwapBuffers();
	}
#endif

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

#if WITH_PHYSX
	GatherPhysXStats(GetPhysXScene(SceneType), SceneType);
#endif



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

#if WITH_PHYSX

#if WITH_VEHICLE
	if (VehicleManager && SceneType == PST_Sync)
	{
		float TickTime = AveragedFrameTime[SceneType];
#if WITH_SUBSTEPPING
		if (IsSubstepping(SceneType))
		{
			TickTime = UseSyncTime(SceneType) ? SyncDeltaSeconds : DeltaSeconds;
		}
#endif
		VehicleManager->PreTick(TickTime);
#if WITH_SUBSTEPPING
		if (IsSubstepping(SceneType) == false)
#endif
		{
			VehicleManager->Update(AveragedFrameTime[SceneType]);
		}
	}
#endif

#if !WITH_APEX
	PxScene* PScene = GetPhysXScene(SceneType);
	if (PScene && (UseDelta > 0.f))
	{
		PhysXCompletionTask* Task = new PhysXCompletionTask(InOutCompletionEvent, PScene->getTaskManager());
		PScene->lockWrite();
		PScene->simulate(AveragedFrameTime[SceneType], Task);
		PScene->unlockWrite();
		Task->removeReference();
		bTaskOutstanding = true;
	}
#else	//	#if !WITH_APEX
	// The APEX scene calls the simulate function for the PhysX scene, so we only call ApexScene->simulate().
	NxApexScene* ApexScene = GetApexScene(SceneType);
	if(ApexScene && UseDelta > 0.f)
	{
#if WITH_SUBSTEPPING
		if (IsSubstepping(SceneType)) //we don't bother sub-stepping cloth
		{
			bTaskOutstanding = SubstepSimulation(SceneType, InOutCompletionEvent);
		}else
#endif
		{
			PhysXCompletionTask* Task = new PhysXCompletionTask(InOutCompletionEvent, ApexScene->getTaskManager());
			ApexScene->simulate(AveragedFrameTime[SceneType], true, Task);
			Task->removeReference();
			bTaskOutstanding = true;
		}
	}
#endif	//	#if !WITH_APEX
#endif // WITH_PHYSX
	if (!bTaskOutstanding)
	{
		InOutCompletionEvent->DispatchSubsequents(); // nothing to do, so nothing to wait for
	}
#if WITH_SUBSTEPPING
	bSubstepping = UPhysicsSettings::Get()->bSubstepping;
	bSubsteppingAsync = UPhysicsSettings::Get()->bSubsteppingAsync;
#endif
}

void FPhysScene::WaitPhysScenes()
{
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
		FTaskGraphInterface::Get().WaitUntilTasksComplete(ThingsToComplete, ENamedThreads::GameThread);
	}
}

void FPhysScene::WaitClothScene()
{
	FGraphEventArray ThingsToComplete;
	if (PhysicsSubsceneCompletion[PST_Cloth].GetReference())
	{
		ThingsToComplete.Add(PhysicsSubsceneCompletion[PST_Cloth]);
	}

	if (ThingsToComplete.Num())
	{
		FTaskGraphInterface::Get().WaitUntilTasksComplete(ThingsToComplete, ENamedThreads::GameThread);
	}
}

void FPhysScene::SceneCompletionTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent, EPhysicsSceneType SceneType)
{
	ProcessPhysScene(SceneType);
}

void FPhysScene::ProcessPhysScene(uint32 SceneType)
{
	SCOPE_CYCLE_COUNTER(STAT_TotalPhysicsTime);
	SCOPE_CYCLE_COUNTER(STAT_PhysicsFetchDynamicsTime);

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

//This fetches and gets active transforms. It's important that the function that calls this locks because getting the transforms and using the data must be an atomic operation
#if WITH_PHYSX
	PxScene* PScene = GetPhysXScene(SceneType);
	check(PScene);
	PxU32 OutErrorCode = 0;

#if !WITH_APEX
	PScene->lockWrite();
	PScene->fetchResults(true, &OutErrorCode);
	PScene->unlockWrite();
#else	//	#if !WITH_APEX
	// The APEX scene calls the fetchResults function for the PhysX scene, so we only call ApexScene->fetchResults().
	NxApexScene* ApexScene = GetApexScene(SceneType);
	check(ApexScene);
	ApexScene->fetchResults(true, &OutErrorCode);
#endif	//	#if !WITH_APEX

	UpdateActiveTransforms(SceneType);
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
	if (SceneType == PST_Cloth)	//cloth doesn't bother with updating components to bodies so we don't need to store any transforms
	{
		return;
	}
	PxScene* PScene = GetPhysXScene(SceneType);
	check(PScene);
	SCOPED_SCENE_WRITE_LOCK(PScene);

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

DEFINE_STAT(STAT_SyncComponentsToBodies);

void FPhysScene::SyncComponentsToBodies(uint32 SceneType)
{
	SCOPE_CYCLE_COUNTER(STAT_TotalPhysicsTime);
	SCOPE_CYCLE_COUNTER(STAT_SyncComponentsToBodies);

	for (FBodyInstance* BodyInstance : ActiveBodyInstances[SceneType])
	{
		if (BodyInstance == nullptr) { continue; }

		check(BodyInstance->OwnerComponent->IsRegistered()); // shouldn't have a physics body for a non-registered component!

		AActor* Owner = BodyInstance->OwnerComponent->GetOwner();

		// See if the transform is actually different, and if so, move the component to match physics
		const FTransform NewTransform = BodyInstance->GetUnrealWorldTransform();
		if (!NewTransform.EqualsNoScale(BodyInstance->OwnerComponent->ComponentToWorld))
		{
			const FVector MoveBy = NewTransform.GetLocation() - BodyInstance->OwnerComponent->ComponentToWorld.GetLocation();
			const FRotator NewRotation = NewTransform.Rotator();

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
		SCOPED_SCENE_READ_LOCK(GetPhysXScene(SceneType));
		UDestructibleComponent::UpdateDestructibleChunkTM(ActiveDestructibleActors[SceneType]);
	}
#endif
}

void FPhysScene::DispatchPhysNotifications()
{
	SCOPE_CYCLE_COUNTER(STAT_PhysicsEventTime);

	//Collision notification
	{
		// Let the game-specific PhysicsCollisionHandler process any physics collisions that took place
		if (OwningWorld != NULL && OwningWorld->PhysicsCollisionHandler != NULL)
		{
			OwningWorld->PhysicsCollisionHandler->HandlePhysicsCollisions(PendingCollisionNotifies);
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
		PendingCollisionNotifies.Empty();
	}

#if WITH_SUBSTEPPING
#if WITH_APEX
	//TODO: Queue is always empty for now - waiting for support from NVIDIA
	//Destructible notification
	{
		for (int32 i = 0; i < DestructibleDamageEventQueue.Num(); ++i)
		{
			const NxApexDamageEventReportData & damageEvent = DestructibleDamageEventQueue[i];
			UDestructibleComponent* DestructibleComponent = Cast<UDestructibleComponent>(FPhysxUserData::Get<UPrimitiveComponent>(damageEvent.destructible->userData));
			DestructibleComponent->OnDamageEvent(damageEvent);
		}

		DestructibleDamageEventQueue.Empty();
	}
#endif
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

				// Unlock scene lock, in case it is required
				SCENE_UNLOCK_WRITE(PScene);
			}
		}
	}
#endif
}

void FPhysScene::StartFrame()
{
	FGraphEventArray FinishPrerequisites;

	//Update the collision disable table before ticking
	FlushDeferredCollisionDisableTableQueue();

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
				GET_STATID(STAT_FNullGraphTask_ProcessPhysScene_Join), PhysSingleThreadedMode() ? ENamedThreads::GameThread : ENamedThreads::AnyThread);
		}
		else
		{
			PhysicsSceneCompletion = FinishPrerequisites[0]; // we don't need a join
		}
	}

	// Record the sync tick time for use with the async tick
	SyncDeltaSeconds = DeltaSeconds;
}

void FPhysScene::StartCloth()
{
	FGraphEventArray FinishPrerequisites;
	TickPhysScene(PST_Cloth, PhysicsSubsceneCompletion[PST_Cloth]);
	{
		if (PhysicsSubsceneCompletion[PST_Cloth].GetReference())
		{
			DECLARE_CYCLE_STAT(TEXT("FDelegateGraphTask.ProcessPhysScene_Cloth"),
				STAT_FDelegateGraphTask_ProcessPhysScene_Cloth,
				STATGROUP_TaskGraphTasks);

			new (FinishPrerequisites)FGraphEventRef(
				FDelegateGraphTask::CreateAndDispatchWhenReady(
					FDelegateGraphTask::FDelegate::CreateRaw(this, &FPhysScene::SceneCompletionTask, PST_Cloth),
					GET_STATID(STAT_FDelegateGraphTask_ProcessPhysScene_Cloth), PhysicsSubsceneCompletion[PST_Cloth],
					ENamedThreads::GameThread, ENamedThreads::GameThread
				)
			);
		}
	}

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
	PhysicsSceneCompletion = NULL;

	if (bAsyncSceneEnabled)
	{
		SyncComponentsToBodies(PST_Async);
	}

	SyncComponentsToBodies(PST_Sync);

	// Perform any collision notification events
	DispatchPhysNotifications();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// Handle debug rendering
	if (InLineBatcher)
	{
		AddDebugLines(PST_Sync, InLineBatcher);

		if (bAsyncSceneEnabled)
		{
			AddDebugLines(PST_Async, InLineBatcher);
		}

	}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
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
	PhysxUserData = FPhysxUserData(this);

	// Include scene descriptor in loop, so that we might vary it with scene type
	PxSceneDesc PSceneDesc(GPhysXSDK->getTolerancesScale());
	PSceneDesc.cpuDispatcher = CPUDispatcher;

	FPhysSceneShaderInfo PhysSceneShaderInfo;
	PhysSceneShaderInfo.PhysScene = this;
	PSceneDesc.filterShaderData = &PhysSceneShaderInfo;
	PSceneDesc.filterShaderDataSize = sizeof(PhysSceneShaderInfo);

	PSceneDesc.filterShader = PhysXSimFilterShader;
	PSceneDesc.simulationEventCallback = SimEventCallback;

	//LOC_MOD enable kinematic vs kinematic for APEX destructibles. This is for the kinematic cube moving horizontally in QA-Destructible map to collide with the destructible.
	// Was this flag turned off in UE4? Do we want to turn it on for both sync and async scenes?
	PSceneDesc.flags |= PxSceneFlag::eENABLE_KINEMATIC_PAIRS;

	// Set bounce threshold
	PSceneDesc.bounceThresholdVelocity = CVarBounceThresholdVelocity.GetValueOnGameThread();

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
	if (SceneType == PST_Async)
	{
		PSceneDesc.flags |= PxSceneFlag::eREQUIRE_RW_LOCK;
	}
#endif

	// We want to use 'active transforms'
	PSceneDesc.flags |= PxSceneFlag::eENABLE_ACTIVETRANSFORMS;

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
	if (bGlobalCCD)
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

#if WITH_SUBSTEPPING
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

#if WITH_SUBSTEPPING

		if (SceneType == PST_Sync && PhysSubSteppers[SceneType])
		{
			PhysSubSteppers[SceneType]->SetVehicleManager(NULL);
		}

		delete PhysSubSteppers[SceneType];
		PhysSubSteppers[SceneType] = NULL;
#endif

		// @todo block on any running scene before calling this
		GPhysCommandHandler->DeferredRelease(PScene);

		// Commands may have accumulated as the scene is terminated - flush any commands for this scene.
		DeferredCommandHandler.Flush();

		// Remove from the map
		GPhysXSceneMap.Remove(PhysXSceneIndex[SceneType]);
	}
#endif
}
