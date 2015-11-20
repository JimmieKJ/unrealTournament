// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "ClothManager.h"
#include "PhysicsPublic.h"
#include "Components/SkeletalMeshComponent.h"

FClothManager::FClothManager(UWorld* InAssociatedWorld)
: AssociatedWorld(InAssociatedWorld)
{
}

TAutoConsoleVariable<int32> CVarParallelCloth(TEXT("p.ParallelCloth"), 0, TEXT("Whether to prepare and start cloth sim off the game thread"));

void FClothManagerData::PrepareCloth(float DeltaTime, FTickFunction& TickFunction)
{
#if WITH_APEX_CLOTHING
	const bool bParallelCloth = !!CVarParallelCloth.GetValueOnAnyThread() && !PhysSingleThreadedMode();
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FClothManager_PrepareCloth);
	if (SkeletalMeshComponents.Num())
	{
		// TODO: OriC, I took the earlier version but you should verify which is correct. You probably want the above check at this location :)
		//const bool bParallelCloth = !!CVarParallelCloth.GetValueOnAnyThread();
		IsPreparingClothInParallel.AtomicSet(bParallelCloth);
		for (USkeletalMeshComponent* SkeletalMeshComponent : SkeletalMeshComponents)
		{
			FClothSimulationContext& ClothSimulationContext = SkeletalMeshComponent->InternalClothSimulationContext;
			if(bParallelCloth)
			{
				SkeletalMeshComponent->ParallelTickClothing(DeltaTime, ClothSimulationContext);
			}else
			{
				check(IsInGameThread());
				SkeletalMeshComponent->TickClothing(DeltaTime, TickFunction);
			}
			
		}

		IsPreparingClothInParallel.AtomicSet(false);
	}
#endif
}

void FClothManager::RegisterForPrepareCloth(USkeletalMeshComponent* SkeletalMeshComponent, PrepareClothSchedule PrepSchedule)
{
	check(PrepSchedule != PrepareClothSchedule::MAX);
	PrepareClothDataArray[(int32)PrepSchedule].SkeletalMeshComponents.Add(SkeletalMeshComponent);
}

void FClothManager::StartCloth(float DeltaTime)
{
	//First we must prepare any cloth that waits on physics sim
	PrepareClothDataArray[(int32)PrepareClothSchedule::WaitOnPhysics].PrepareCloth(DeltaTime, StartClothTickFunction);

	//Reset skeletal mesh components
	bool bNeedSimulateCloth = false;
	for (FClothManagerData& PrepareData : PrepareClothDataArray)
	{
		if (PrepareData.SkeletalMeshComponents.Num())
		{
			bNeedSimulateCloth = true;
			PrepareData.SkeletalMeshComponents.Reset();
		}
		PrepareData.PrepareCompletion.SafeRelease();
	}

	if (bNeedSimulateCloth)
	{
		if (FPhysScene* PhysScene = AssociatedWorld->GetPhysicsScene())
		{
			PhysScene->StartCloth();
		}
	}
}

void FClothManager::SetupClothTickFunction(bool bTickClothSim)
{
	check(IsInGameThread());
	check(!IsPreparingClothAsync());

	bool bTickOnAny = !!CVarParallelCloth.GetValueOnGameThread() && !PhysSingleThreadedMode();
	StartIgnorePhysicsClothTickFunction.bRunOnAnyThread = bTickOnAny;
	StartClothTickFunction.bRunOnAnyThread = bTickOnAny;

	/** Need to register tick function and we are not currently registered */
	if (bTickClothSim && !StartClothTickFunction.IsTickFunctionRegistered())
	{
		//prepare cloth data ticks
		StartIgnorePhysicsClothTickFunction.TickGroup = TG_StartPhysics;
		StartIgnorePhysicsClothTickFunction.Target = this;
		StartIgnorePhysicsClothTickFunction.bCanEverTick = true;
		StartIgnorePhysicsClothTickFunction.RegisterTickFunction(AssociatedWorld->PersistentLevel);

		//simulate cloth tick
		StartClothTickFunction.TickGroup = TG_StartCloth;
		StartClothTickFunction.Target = this;
		StartClothTickFunction.bCanEverTick = true;
		StartClothTickFunction.AddPrerequisite(AssociatedWorld, StartIgnorePhysicsClothTickFunction);
		StartClothTickFunction.RegisterTickFunction(AssociatedWorld->PersistentLevel);

		//wait for cloth sim to finish
		EndClothTickFunction.TickGroup = TG_EndCloth;
		EndClothTickFunction.Target = this;
		EndClothTickFunction.bCanEverTick = true;
		EndClothTickFunction.bRunOnAnyThread = false;	//this HAS to run on game thread because we need to block if cloth sim is not done
		EndClothTickFunction.AddPrerequisite(AssociatedWorld, StartClothTickFunction);
		EndClothTickFunction.RegisterTickFunction(AssociatedWorld->PersistentLevel);
	}
	else if (!bTickClothSim && StartClothTickFunction.IsTickFunctionRegistered())	//Need to unregister tick function and we current are registered
	{
		StartIgnorePhysicsClothTickFunction.UnRegisterTickFunction();
		StartClothTickFunction.UnRegisterTickFunction();
		EndClothTickFunction.UnRegisterTickFunction();
	}
}


void FClothManager::EndCloth()
{
	QUICK_SCOPE_CYCLE_COUNTER(FClothManager_ExecuteTick);
	if (FPhysScene* PhysScene = AssociatedWorld->GetPhysicsScene())
	{
		PhysScene->WaitClothScene();
	}
}

void FStartIgnorePhysicsClothTickFunction::ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	Target->PrepareClothDataArray[(int32)PrepareClothSchedule::IgnorePhysics].PrepareCloth(DeltaTime, *this);
}

FString FStartIgnorePhysicsClothTickFunction::DiagnosticMessage()
{
	return TEXT("FStartIgnorePhysicsClothTickFunction");
}

void FStartClothTickFunction::ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	Target->StartCloth(DeltaTime);
}

FString FStartClothTickFunction::DiagnosticMessage()
{
	return TEXT("FStartClothTickFunction");
}


void FEndClothTickFunction::ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	Target->EndCloth();
}

FString FEndClothTickFunction::DiagnosticMessage()
{
	return TEXT("FEndClothTickFunction");
}