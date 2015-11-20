// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimInstance.cpp: Anim Instance implementation
=============================================================================*/ 

#include "EnginePrivate.h"
#include "AnimationRuntime.h"
#include "AnimationUtils.h"
#include "Animation/AnimStats.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "GameFramework/Character.h"
#include "ParticleDefinitions.h"
#include "DisplayDebugHelpers.h"
#include "MessageLog.h"
#include "Animation/BlendSpaceBase.h"
#include "Animation/AnimComposite.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimNode_StateMachine.h"
#include "Animation/AnimNode_TransitionResult.h"
#include "Animation/AnimNode_AssetPlayerBase.h"
#include "Animation/AnimInstance.h"
/** Anim stats */

DEFINE_STAT(STAT_CalcSkelMeshBounds);
DEFINE_STAT(STAT_MeshObjectUpdate);
DEFINE_STAT(STAT_BlendInPhysics);
DEFINE_STAT(STAT_SkelCompUpdateTransform);
//                         -->  Physics Engine here <--
DEFINE_STAT(STAT_UpdateRBBones);
DEFINE_STAT(STAT_UpdateRBJoints);
DEFINE_STAT(STAT_UpdateLocalToWorldAndOverlaps);
DEFINE_STAT(STAT_GetAnimationPose);
DEFINE_STAT(STAT_AnimTriggerAnimNotifies);
DEFINE_STAT(STAT_RefreshBoneTransforms);
DEFINE_STAT(STAT_InterpolateSkippedFrames);
DEFINE_STAT(STAT_AnimTickTime);
DEFINE_STAT(STAT_SkinnedMeshCompTick);
DEFINE_STAT(STAT_TickUpdateRate);
DEFINE_STAT(STAT_UpdateAnimation);
DEFINE_STAT(STAT_PreUpdateAnimation);
DEFINE_STAT(STAT_PostUpdateAnimation);
DEFINE_STAT(STAT_BlueprintUpdateAnimation);
DEFINE_STAT(STAT_BlueprintPostEvaluateAnimation);
DEFINE_STAT(STAT_NativeUpdateAnimation);
DEFINE_STAT(STAT_Montage_Advance);
DEFINE_STAT(STAT_Montage_UpdateWeight);
DEFINE_STAT(STAT_AnimMontageInstance_Advance);
DEFINE_STAT(STAT_AnimMontageInstance_TickBranchPoints);
DEFINE_STAT(STAT_AnimMontageInstance_Advance_Iteration);
DEFINE_STAT(STAT_UpdateCurves);
DEFINE_STAT(STAT_LocalBlendCSBoneTransforms);

DECLARE_CYCLE_STAT_EXTERN(TEXT("Anim Init Time"), STAT_AnimInitTime, STATGROUP_Anim, );
DEFINE_STAT(STAT_AnimInitTime);

DEFINE_STAT(STAT_AnimStateMachineUpdate);
DEFINE_STAT(STAT_AnimStateMachineFindTransition);

DEFINE_STAT(STAT_SkinPerPolyVertices);
DEFINE_STAT(STAT_UpdateTriMeshVertices);

DEFINE_STAT(STAT_AnimGameThreadTime);

DECLARE_CYCLE_STAT(TEXT("TickAssetPlayerInstances"), STAT_TickAssetPlayerInstances, STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("TickAssetPlayerInstance"), STAT_TickAssetPlayerInstance, STATGROUP_Anim);


#define DO_ANIMSTAT_PROCESSING(StatName) DEFINE_STAT(STAT_ ## StatName)
#include "AnimMTStats.h"
#undef DO_ANIMSTAT_PROCESSING

#define DO_ANIMSTAT_PROCESSING(StatName) DEFINE_STAT(STAT_ ## StatName ## _WorkerThread)
#include "AnimMTStats.h"
#undef DO_ANIMSTAT_PROCESSING

// Define AnimNotify
DEFINE_LOG_CATEGORY(LogAnimNotify);

#define LOCTEXT_NAMESPACE "AnimInstance"

extern TAutoConsoleVariable<int32> CVarUseParallelAnimUpdate;
extern TAutoConsoleVariable<int32> CVarUseParallelAnimationEvaluation;

/////////////////////////////////////////////////////
// UAnimInstance
/////////////////////////////////////////////////////

UAnimInstance::UAnimInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RootNode = NULL;
	RootMotionMode = ERootMotionMode::RootMotionFromMontagesOnly;
	SyncGroupWriteIndex = 0;
	CurrentDeltaSeconds = 0.0f;
	CurrentTimeDilation = 1.0f;
	bNeedsUpdate = false;
}

void UAnimInstance::MakeSequenceTickRecord(FAnimTickRecord& TickRecord, class UAnimSequenceBase* Sequence, bool bLooping, float PlayRate, float FinalBlendWeight, float& CurrentTime, FMarkerTickRecord& MarkerTickRecord) const
{
	TickRecord.SourceAsset = Sequence;
	TickRecord.TimeAccumulator = &CurrentTime;
	TickRecord.MarkerTickRecord = &MarkerTickRecord;
	TickRecord.PlayRateMultiplier = PlayRate;
	TickRecord.EffectiveBlendWeight = FinalBlendWeight;
	TickRecord.bLooping = bLooping;
}

void UAnimInstance::MakeBlendSpaceTickRecord(FAnimTickRecord& TickRecord, class UBlendSpaceBase* BlendSpace, const FVector& BlendInput, TArray<FBlendSampleData>& BlendSampleDataCache, FBlendFilter& BlendFilter, bool bLooping, float PlayRate, float FinalBlendWeight, float& CurrentTime, FMarkerTickRecord& MarkerTickRecord) const
{
	TickRecord.SourceAsset = BlendSpace;
	TickRecord.BlendSpace.BlendSpacePositionX = BlendInput.X;
	TickRecord.BlendSpace.BlendSpacePositionY = BlendInput.Y;
	TickRecord.BlendSpace.BlendSampleDataCache = &BlendSampleDataCache;
	TickRecord.BlendSpace.BlendFilter = &BlendFilter;
	TickRecord.TimeAccumulator = &CurrentTime;
	TickRecord.MarkerTickRecord = &MarkerTickRecord;
	TickRecord.PlayRateMultiplier = PlayRate;
	TickRecord.EffectiveBlendWeight = FinalBlendWeight;
	TickRecord.bLooping = bLooping;
}

// this is only used by montage marker based sync
void UAnimInstance::MakeMontageTickRecord(FAnimTickRecord& TickRecord, class UAnimMontage* Montage, float CurrentPosition, float PreviousPosition, float MoveDelta, float Weight, TArray<FPassedMarker>& MarkersPassedThisTick, FMarkerTickRecord& MarkerTickRecord)
{
	TickRecord.SourceAsset = Montage;
	TickRecord.Montage.CurrentPosition = CurrentPosition;
	TickRecord.Montage.PreviousPosition = PreviousPosition;
	TickRecord.Montage.MoveDelta = MoveDelta;
	TickRecord.Montage.MarkersPassedThisTick = &MarkersPassedThisTick;
	TickRecord.MarkerTickRecord = &MarkerTickRecord;
	TickRecord.PlayRateMultiplier = 1.f; // we don't care here, this is alreayd applied in the montageinstance::Advance
	TickRecord.EffectiveBlendWeight = Weight;
	TickRecord.bLooping = false;
}

void UAnimInstance::SequenceAdvanceImmediate(UAnimSequenceBase* Sequence, bool bLooping, float PlayRate, float DeltaSeconds, float& CurrentTime, FMarkerTickRecord& MarkerTickRecord)
{
	FAnimTickRecord TickRecord;
	MakeSequenceTickRecord(TickRecord, Sequence, bLooping, PlayRate, /*FinalBlendWeight=*/ 1.0f, CurrentTime, MarkerTickRecord);

	FAnimAssetTickContext TickContext(DeltaSeconds, RootMotionMode, true);
	TickRecord.SourceAsset->TickAssetPlayerInstance(TickRecord, this, TickContext);
}

void UAnimInstance::BlendSpaceAdvanceImmediate(class UBlendSpaceBase* BlendSpace, const FVector& BlendInput, TArray<FBlendSampleData>& BlendSampleDataCache, FBlendFilter& BlendFilter, bool bLooping, float PlayRate, float DeltaSeconds, float& CurrentTime, FMarkerTickRecord& MarkerTickRecord)
{
	FAnimTickRecord TickRecord;
	MakeBlendSpaceTickRecord(TickRecord, BlendSpace, BlendInput, BlendSampleDataCache, BlendFilter, bLooping, PlayRate, /*FinalBlendWeight=*/ 1.0f, CurrentTime, MarkerTickRecord);
	
	FAnimAssetTickContext TickContext(DeltaSeconds, RootMotionMode, true);
	TickRecord.SourceAsset->TickAssetPlayerInstance(TickRecord, this, TickContext);
}

// Creates an uninitialized tick record in the list for the correct group or the ungrouped array.  If the group is valid, OutSyncGroupPtr will point to the group.
FAnimTickRecord& UAnimInstance::CreateUninitializedTickRecord(int32 GroupIndex, FAnimGroupInstance*& OutSyncGroupPtr)
{
	// Find or create the sync group if there is one
	OutSyncGroupPtr = NULL;
	if (GroupIndex >= 0)
	{
		TArray<FAnimGroupInstance>& SyncGroups = SyncGroupArrays[GetSyncGroupWriteIndex()];
		while (SyncGroups.Num() <= GroupIndex)
		{
			new (SyncGroups) FAnimGroupInstance();
		}
		OutSyncGroupPtr = &(SyncGroups[GroupIndex]);
	}

	// Create the record
	FAnimTickRecord* TickRecord = new ((OutSyncGroupPtr != NULL) ? OutSyncGroupPtr->ActivePlayers : UngroupedActivePlayerArrays[GetSyncGroupWriteIndex()]) FAnimTickRecord();
	return *TickRecord;
}

AActor* UAnimInstance::GetOwningActor() const
{
	USkeletalMeshComponent* OwnerComponent = GetSkelMeshComponent();
	return OwnerComponent->GetOwner();
}

APawn* UAnimInstance::TryGetPawnOwner() const
{
	USkeletalMeshComponent* OwnerComponent = GetSkelMeshComponent();
	if (AActor* OwnerActor = OwnerComponent->GetOwner())
	{
		return Cast<APawn>(OwnerActor);
	}

	return NULL;
}

USkeletalMeshComponent* UAnimInstance::GetOwningComponent() const
{
	return GetSkelMeshComponent();
}

UWorld* UAnimInstance::GetWorld() const
{
	// The CDO isn't owned by a SkelMeshComponent (and doesn't have a World)
	return (HasAnyFlags(RF_ClassDefaultObject) ? nullptr : GetSkelMeshComponent()->GetWorld());
}

void UAnimInstance::InitializeAnimation()
{
	SCOPE_CYCLE_COUNTER(STAT_AnimInitTime);

	UninitializeAnimation();

	// make sure your skeleton is initialized
	// you can overwrite different skeleton
	USkeletalMeshComponent* OwnerComponent = GetSkelMeshComponent();
	if (OwnerComponent->SkeletalMesh != NULL)
	{
		CurrentSkeleton = OwnerComponent->SkeletalMesh->Skeleton;
	}
	else
	{
		CurrentSkeleton = NULL;
	}

	if (UAnimBlueprintGeneratedClass* AnimBlueprintClass = Cast<UAnimBlueprintGeneratedClass>(GetClass()))
	{
		// Grab a pointer to the root node
		if (AnimBlueprintClass->RootAnimNodeProperty != NULL)
		{
			RootNode = AnimBlueprintClass->RootAnimNodeProperty->ContainerPtrToValuePtr<FAnimNode_Base>(this);
		}
		else
		{
			RootNode = NULL;
		}

		// if no mesh, use Blueprint Skeleton
		if (CurrentSkeleton == NULL)
		{
			CurrentSkeleton = AnimBlueprintClass->TargetSkeleton;
		}

#if WITH_EDITORONLY_DATA
		if (UAnimBlueprint* Blueprint = Cast<UAnimBlueprint>(AnimBlueprintClass->ClassGeneratedBy))
		{
			if (Blueprint->Status == BS_Error)
			{
				RootNode = NULL;
			}
		}
#endif

#if WITH_EDITOR
		LifeTimer = 0.0;
		CurrentLifeTimerScrubPosition = 0.0;

		if (UAnimBlueprint* Blueprint = Cast<UAnimBlueprint>(AnimBlueprintClass->ClassGeneratedBy))
		{
			if (Blueprint->GetObjectBeingDebugged() == this)
			{
				// Reset the snapshot buffer
				AnimBlueprintClass->GetAnimBlueprintDebugData().ResetSnapshotBuffer();
			}
		}
#endif
	}

	// before initialize, need to recalculate required bone list
	RecalcRequiredBones();

	ReinitializeSlotNodes();
	ClearMorphTargets();
	NativeInitializeAnimation();
	BlueprintInitializeAnimation();

	if (RootNode != NULL)
	{
		InitializationCounter.Increment();
		FAnimationInitializeContext InitContext(this);
		RootNode->Initialize(InitContext);
	}

	// we can bind rules & events now the graph has been initialized
	BindNativeDelegates();
}

void UAnimInstance::UninitializeAnimation()
{
	NativeUninitializeAnimation();

	StopAllMontages(0.f);

	for(int32 Index = 0; Index < MontageInstances.Num(); ++Index)
	{
		delete MontageInstances[Index];
	}

	MontageInstances.Empty();

	USkeletalMeshComponent* SkelMeshComp = GetSkelMeshComponent();
	if (SkelMeshComp)
	{
		// Tick currently active AnimNotifyState
		for(int32 Index=0; Index<ActiveAnimNotifyState.Num(); Index++)
		{
			const FAnimNotifyEvent& AnimNotifyEvent = ActiveAnimNotifyState[Index];
			AnimNotifyEvent.NotifyStateClass->NotifyEnd(SkelMeshComp, Cast<UAnimSequenceBase>(AnimNotifyEvent.NotifyStateClass->GetOuter()));
		}

		TArray<FName> ParamsToClearCopy = MaterialParamatersToClear;
		for(int i = 0; i < ParamsToClearCopy.Num(); ++i)
		{
			AddCurveValue(ParamsToClearCopy[i], 0.0f, ACF_DrivesMaterial);
		}
	}

	ActiveAnimNotifyState.Reset();
	EventCurves.Reset();
	MorphTargetCurves.Reset();
	MaterialParameterCurves.Reset();
	MaterialParamatersToClear.Reset();
	AnimNotifies.Reset();
}

#if WITH_EDITORONLY_DATA
bool UAnimInstance::UpdateSnapshotAndSkipRemainingUpdate()
{
#if WITH_EDITOR
	// Avoid updating the instance if we're replaying the past
	if (UAnimBlueprintGeneratedClass* AnimBlueprintClass = Cast<UAnimBlueprintGeneratedClass>(GetClass()))
	{
		FAnimBlueprintDebugData& DebugData = AnimBlueprintClass->GetAnimBlueprintDebugData();
		if (DebugData.IsReplayingSnapshot())
		{
			if (UAnimBlueprint* Blueprint = Cast<UAnimBlueprint>(AnimBlueprintClass->ClassGeneratedBy))
			{
				if (Blueprint->GetObjectBeingDebugged() == this)
				{
					// Find the correct frame
					DebugData.SetSnapshotIndexByTime(this, CurrentLifeTimerScrubPosition);
					return true;
				}
			}
		}
	}
#endif
	return false;
}
#endif

// update animation node, if you have your own anim instance, you could override this to update extra nodes if you'd like
void UAnimInstance::UpdateAnimationNode(float DeltaSeconds)
{
	// Update the anim graph
	if(RootNode != NULL)
	{
		UpdateCounter.Increment();
		FAnimationUpdateContext UpdateContext(this, DeltaSeconds);
		RootNode->Update(UpdateContext);
	}
}

void UAnimInstance::UpdateMontage(float DeltaSeconds)
{
	// update montage weight
	Montage_UpdateWeight(DeltaSeconds);

	// update montage should run in game thread
	// if we do multi threading, make sure this stays in game thread. 
	// This is because branch points need to execute arbitrary code inside this call.
	Montage_Advance(DeltaSeconds);

	// now we know all montage has advanced
	// time to test sync groups
	for (auto& MontageInstance : MontageInstances)
	{
		if (MontageInstance->CanUseMarkerSync())
		{
			const int32 GroupIndexToUse = MontageInstance->GetSyncGroupIndex();

			// that is public data, so if anybody decided to play with it
			if (ensure (GroupIndexToUse != INDEX_NONE))
			{
				FAnimGroupInstance* SyncGroup;
				FAnimTickRecord& TickRecord = CreateUninitializedTickRecord(GroupIndexToUse, /*out*/ SyncGroup);
				MakeMontageTickRecord(TickRecord, MontageInstance->Montage, MontageInstance->GetPosition(), 
					MontageInstance->GetPreviousPosition(), MontageInstance->GetDeltaMoved(), MontageInstance->GetWeight(), 
					MontageInstance->MarkersPassedThisTick, MontageInstance->MarkerTickRecord);

				// Update the sync group if it exists
				if (SyncGroup != NULL)
				{
					SyncGroup->TestMontageTickRecordForLeadership();
				}
			}
		}
	}

	// update montage eval data
	UpdateMontageEvaluationData();
}

void UAnimInstance::UpdateAnimation(float DeltaSeconds, bool bNeedsValidRootMotion)
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateAnimation);
	FScopeCycleCounterUObject AnimScope(this);

#if WITH_EDITOR
	if (GIsEditor)
	{
		// Reset the anim graph visualization
		if (RootNode != NULL)
		{
			if (UAnimBlueprintGeneratedClass* AnimBlueprintClass = Cast<UAnimBlueprintGeneratedClass>(GetClass()))
			{
				UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(AnimBlueprintClass->ClassGeneratedBy);
				if (AnimBP && AnimBP->GetObjectBeingDebugged() == this)
				{
					AnimBlueprintClass->GetAnimBlueprintDebugData().ResetNodeVisitSites();
				}
			}
		}

		// Update the lifetimer and see if we should use the snapshot instead
		CurrentLifeTimerScrubPosition += DeltaSeconds;
		LifeTimer = FMath::Max<double>(CurrentLifeTimerScrubPosition, LifeTimer);

		if (UpdateSnapshotAndSkipRemainingUpdate())
		{
			return;
		}
	}
#endif

	PreUpdateAnimation(DeltaSeconds);

	{
		SCOPE_CYCLE_COUNTER(STAT_NativeUpdateAnimation);
		NativeUpdateAnimation(DeltaSeconds);
	}
	{
		SCOPE_CYCLE_COUNTER(STAT_BlueprintUpdateAnimation);
		BlueprintUpdateAnimation(DeltaSeconds);
	}

	// need to update montage BEFORE node update
	// so that node knows where montage is
	UpdateMontage(DeltaSeconds);

	if(NeedsImmediateUpdate(DeltaSeconds))
	{
		// cant use parallel update, so just do the work here
		UpdateAnimationInternal(DeltaSeconds);
		PostUpdateAnimation();
	}
	else if(bNeedsValidRootMotion && RootMotionMode == ERootMotionMode::RootMotionFromMontagesOnly)
	{
		// We may have just partially blended root motion, so make it up to 1 by
		// blending in identity too
		if (ExtractedRootMotion.bHasRootMotion)
		{
			ExtractedRootMotion.MakeUpToFullWeight();
		}
	}
}

void UAnimInstance::PreUpdateAnimation(float DeltaSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_PreUpdateAnimation);

	CurrentDeltaSeconds = DeltaSeconds;

	const UWorld* World = GetSkelMeshComponent()->GetWorld();
	check(World->GetWorldSettings());
	CurrentTimeDilation = World->GetWorldSettings()->GetEffectiveTimeDilation();
	bNeedsUpdate = true;

	AnimNotifies.Reset();

	ClearSlotNodeWeights();

	// Reset the player tick list (but keep it presized)
	TArray<FAnimTickRecord>& UngroupedActivePlayers = UngroupedActivePlayerArrays[GetSyncGroupWriteIndex()];
	UngroupedActivePlayers.Reset();

	TArray<FAnimGroupInstance>& SyncGroups = SyncGroupArrays[GetSyncGroupWriteIndex()];
	for (int32 GroupIndex = 0; GroupIndex < SyncGroups.Num(); ++GroupIndex)
	{
		SyncGroups[GroupIndex].Reset();
	}
}

void UAnimInstance::UpdateAnimationInternal(float DeltaSeconds)
{
	ANIM_MT_SCOPE_CYCLE_COUNTER(UpdateAnimationInternal, IsRunningParallelEvaluation());
	FScopeCycleCounterUObject AnimScope(this);

	// update blueprint and native update
	{
		SCOPE_CYCLE_COUNTER(STAT_NativeUpdateAnimation);
		NativeUpdateAnimation_WorkerThread(DeltaSeconds);
	}

	// update all nodes
	UpdateAnimationNode(DeltaSeconds);

	// tick all our active asset players
	TickAssetPlayerInstances(DeltaSeconds);
}

void UAnimInstance::TickAssetPlayerInstances(float DeltaSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_TickAssetPlayerInstances);

	// Handle all players inside sync groups
	TArray<FAnimGroupInstance>& SyncGroups = SyncGroupArrays[GetSyncGroupWriteIndex()];
	const TArray<FAnimGroupInstance>& PreviousSyncGroups = SyncGroupArrays[GetSyncGroupReadIndex()];
	TArray<FAnimTickRecord>& UngroupedActivePlayers = UngroupedActivePlayerArrays[GetSyncGroupWriteIndex()];

	for (int32 GroupIndex = 0; GroupIndex < SyncGroups.Num(); ++GroupIndex)
	{
		FAnimGroupInstance& SyncGroup = SyncGroups[GroupIndex];
    
		if (SyncGroup.ActivePlayers.Num() > 0)
		{
			const FAnimGroupInstance* PreviousGroup = PreviousSyncGroups.IsValidIndex(GroupIndex) ? &PreviousSyncGroups[GroupIndex] : nullptr;
			SyncGroup.Prepare(PreviousGroup);

			UE_LOG(LogAnimMarkerSync, Log, TEXT("Ticking Group [%d] GroupLeader [%d]"), GroupIndex, SyncGroup.GroupLeaderIndex);

			const bool bOnlyOneAnimationInGroup = SyncGroup.ActivePlayers.Num() == 1;

			// Tick the group leader
			FAnimAssetTickContext TickContext(DeltaSeconds, RootMotionMode, bOnlyOneAnimationInGroup, SyncGroup.ValidMarkers);
			// initialize to invalidate first
			check(SyncGroup.GroupLeaderIndex == INDEX_NONE);
			int32 GroupLeaderIndex = 0;
			for (; GroupLeaderIndex < SyncGroup.ActivePlayers.Num(); ++GroupLeaderIndex)
			{
				FAnimTickRecord& GroupLeader = SyncGroup.ActivePlayers[GroupLeaderIndex];
				// if it has leader score
				SCOPE_CYCLE_COUNTER(STAT_TickAssetPlayerInstance);
				FScopeCycleCounterUObject Scope(GroupLeader.SourceAsset);
				GroupLeader.SourceAsset->TickAssetPlayerInstance(GroupLeader, this, TickContext);

				if (RootMotionMode == ERootMotionMode::RootMotionFromEverything && TickContext.RootMotionMovementParams.bHasRootMotion)
				{
					ExtractedRootMotion.AccumulateWithBlend(TickContext.RootMotionMovementParams.RootMotionTransform, GroupLeader.EffectiveBlendWeight);
				}

				// if we're not using marker based sync, we don't care, get out
				if (TickContext.CanUseMarkerPosition() == false)
				{
					SyncGroup.GroupLeaderIndex = GroupLeaderIndex;
					break;
				}
				// otherwise, the new position should contain the valid position for end, otherwise, we don't know where to sync to
				else if (TickContext.MarkerTickContext.IsMarkerSyncEndValid())
				{
					// if this leader contains correct position, break
					SyncGroup.MarkerTickContext = TickContext.MarkerTickContext;
					SyncGroup.GroupLeaderIndex = GroupLeaderIndex;
					UE_LOG(LogAnimMarkerSync, Log, TEXT("Previous Sync Group Makrer Tick Context :\n%s"), *SyncGroup.MarkerTickContext.ToString());
					UE_LOG(LogAnimMarkerSync, Log, TEXT("New Sync Group Makrer Tick Context :\n%s"), *TickContext.MarkerTickContext.ToString());
					break;
				}
				else
				{
					SyncGroup.GroupLeaderIndex = GroupLeaderIndex;
					UE_LOG(LogAnimMarkerSync, Log, TEXT("Invalid position from Leader %d. Trying next leader"), GroupLeaderIndex);
				}
			} 

			check(SyncGroup.GroupLeaderIndex != INDEX_NONE);
			// we found leader
			SyncGroup.Finalize(PreviousGroup);

			// Update everything else to follow the leader, if there is more followers
			if (SyncGroup.ActivePlayers.Num() > GroupLeaderIndex + 1)
			{
				// if we don't have a good leader, no reason to convert to follower
				// tick as leader
				TickContext.ConvertToFollower();
    
				for (int32 TickIndex = GroupLeaderIndex + 1; TickIndex < SyncGroup.ActivePlayers.Num(); ++TickIndex)
				{
					FAnimTickRecord& AssetPlayer = SyncGroup.ActivePlayers[TickIndex];
					{
						SCOPE_CYCLE_COUNTER(STAT_TickAssetPlayerInstance);
						FScopeCycleCounterUObject Scope(AssetPlayer.SourceAsset);
						TickContext.RootMotionMovementParams.Clear();
						AssetPlayer.SourceAsset->TickAssetPlayerInstance(AssetPlayer, this, TickContext);
					}
					if (RootMotionMode == ERootMotionMode::RootMotionFromEverything && TickContext.RootMotionMovementParams.bHasRootMotion)
					{
						ExtractedRootMotion.AccumulateWithBlend(TickContext.RootMotionMovementParams.RootMotionTransform, AssetPlayer.EffectiveBlendWeight);
					}
				}
			}
		}
	}

	// Handle the remaining ungrouped animation players
	for (int32 TickIndex = 0; TickIndex < UngroupedActivePlayers.Num(); ++TickIndex)
	{
		FAnimTickRecord& AssetPlayerToTick = UngroupedActivePlayers[TickIndex];
		static TArray<FName> TempNames;
		const TArray<FName>* UniqueNames = AssetPlayerToTick.SourceAsset->GetUniqueMarkerNames();
		const TArray<FName>& ValidMarkers = UniqueNames ? *UniqueNames : TempNames;

		const bool bOnlyOneAnimationInGroup = true;
		FAnimAssetTickContext TickContext(DeltaSeconds, RootMotionMode, bOnlyOneAnimationInGroup, ValidMarkers);
		{
			SCOPE_CYCLE_COUNTER(STAT_TickAssetPlayerInstance);
			FScopeCycleCounterUObject Scope(AssetPlayerToTick.SourceAsset);
			AssetPlayerToTick.SourceAsset->TickAssetPlayerInstance(AssetPlayerToTick, this, TickContext);
		}
		if (RootMotionMode == ERootMotionMode::RootMotionFromEverything && TickContext.RootMotionMovementParams.bHasRootMotion)
		{
			ExtractedRootMotion.AccumulateWithBlend(TickContext.RootMotionMovementParams.RootMotionTransform, AssetPlayerToTick.EffectiveBlendWeight);
		}
	}
}

void UAnimInstance::PostUpdateAnimation()
{
	SCOPE_CYCLE_COUNTER(STAT_PostUpdateAnimation);

	bNeedsUpdate = false;

	// We may have just partially blended root motion, so make it up to 1 by
	// blending in identity too
	if (ExtractedRootMotion.bHasRootMotion)
	{
		ExtractedRootMotion.MakeUpToFullWeight();
	}

	// flip read/write index
	TickSyncGroupWriteIndex();

	// now trigger Notifies
	TriggerAnimNotifies(CurrentDeltaSeconds);

	// Trigger Montage end events after notifies. In case Montage ending ends abilities or other states, we make sure notifies are processed before montage events.
	TriggerQueuedMontageEvents();

#if WITH_EDITOR && 0
	{
		// Take a snapshot if the scrub control is locked to the end, we are playing, and we are the one being debugged
		if (UAnimBlueprintGeneratedClass* AnimBlueprintClass = Cast<UAnimBlueprintGeneratedClass>(GetClass()))
		{
			if (UAnimBlueprint* Blueprint = Cast<UAnimBlueprint>(AnimBlueprintClass->ClassGeneratedBy))
			{
				if (Blueprint->GetObjectBeingDebugged() == this)
				{
					if ((CurrentLifeTimerScrubPosition == LifeTimer) && (CurrentDeltaSeconds > 0.0f))
					{
						AnimBlueprintClass->GetAnimBlueprintDebugData().TakeSnapshot(this);
					}
				}
			}
		}
	}
#endif
}

void UAnimInstance::ParallelUpdateAnimation()
{
	UpdateAnimationInternal(CurrentDeltaSeconds);
}

bool UAnimInstance::NeedsImmediateUpdate(float DeltaSeconds) const
{
	return
		CVarUseParallelAnimUpdate.GetValueOnGameThread() == 0 ||
		CVarUseParallelAnimationEvaluation.GetValueOnGameThread() == 0 ||
		IsRunningDedicatedServer() ||
		!bCanUseParallelUpdateAnimation ||
		DeltaSeconds == 0.0f ||
		RootMotionMode == ERootMotionMode::RootMotionFromEverything;
}

bool UAnimInstance::NeedsUpdate() const
{
	return bNeedsUpdate;
}

void UAnimInstance::EvaluateAnimation(FPoseContext& Output)
{
	ANIM_MT_SCOPE_CYCLE_COUNTER(EvaluateAnimInstance, IsRunningParallelEvaluation());

	// If bone caches have been invalidated, have AnimNodes refresh those.
	if( bBoneCachesInvalidated && RootNode )
	{
		bBoneCachesInvalidated = false;

		CachedBonesCounter.Increment();
		FAnimationCacheBonesContext UpdateContext(this);
		RootNode->CacheBones(UpdateContext);
	}

	// Evaluate native code if implemented, otherwise evaluate the node graph
	if (!NativeEvaluateAnimation(Output))
	{
		if (RootNode != NULL)
		{
			ANIM_MT_SCOPE_CYCLE_COUNTER(EvaluateAnimGraph, IsRunningParallelEvaluation());
			EvaluationCounter.Increment();
			RootNode->Evaluate(Output);
		}
		else
		{
			Output.ResetToRefPose();
		}
	}
}

void UAnimInstance::PostEvaluateAnimation()
{
	NativePostEvaluateAnimation();

	{
		SCOPE_CYCLE_COUNTER(STAT_BlueprintPostEvaluateAnimation);
		BlueprintPostEvaluateAnimation();
	}
}

void UAnimInstance::NativeInitializeAnimation()
{
}

void UAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
}

void UAnimInstance::NativeUpdateAnimation_WorkerThread(float DeltaSeconds)
{
}

bool UAnimInstance::NativeEvaluateAnimation(FPoseContext& Output)
{
	return false;
}

void UAnimInstance::NativePostEvaluateAnimation()
{
}

void UAnimInstance::NativeUninitializeAnimation()
{
}

void UAnimInstance::AddNativeTransitionBinding(const FName& MachineName, const FName& PrevStateName, const FName& NextStateName, const FCanTakeTransition& NativeTransitionDelegate, const FName& TransitionName)
{
	NativeTransitionBindings.Add(FNativeTransitionBinding(MachineName, PrevStateName, NextStateName, NativeTransitionDelegate, TransitionName));
}

bool UAnimInstance::HasNativeTransitionBinding(const FName& MachineName, const FName& PrevStateName, const FName& NextStateName, FName& OutBindingName)
{
	for(const auto& Binding : NativeTransitionBindings)
	{
		if(Binding.MachineName == MachineName && Binding.PreviousStateName == PrevStateName && Binding.NextStateName == NextStateName)
		{
			IDelegateInstance* DelegateInstance = Binding.NativeTransitionDelegate.GetDelegateInstance();
			if(DelegateInstance)
			{
				OutBindingName = DelegateInstance->GetFunctionName();
			}
#if WITH_EDITORONLY_DATA
			// if the function wasnt forthcoming with a name, use the explicit name supplied in the binding
			if(OutBindingName == NAME_None)
			{
				OutBindingName = Binding.TransitionName;
			}
#endif
			return true;
		}
	}

	return false;
}

void UAnimInstance::AddNativeStateEntryBinding(const FName& MachineName, const FName& StateName, const FOnGraphStateChanged& NativeEnteredDelegate)
{
	NativeStateEntryBindings.Add(FNativeStateBinding(MachineName, StateName, NativeEnteredDelegate));
}
	
bool UAnimInstance::HasNativeStateEntryBinding(const FName& MachineName, const FName& StateName, FName& OutBindingName)
{
	for(const auto& Binding : NativeStateEntryBindings)
	{
		if(Binding.MachineName == MachineName && Binding.StateName == StateName)
		{
			IDelegateInstance* DelegateInstance = Binding.NativeStateDelegate.GetDelegateInstance();
			if(DelegateInstance)
			{
				OutBindingName = DelegateInstance->GetFunctionName();
			}
			return true;
		}
	}

	return false;
}

void UAnimInstance::AddNativeStateExitBinding(const FName& MachineName, const FName& StateName, const FOnGraphStateChanged& NativeExitedDelegate)
{
	NativeStateExitBindings.Add(FNativeStateBinding(MachineName, StateName, NativeExitedDelegate));
}

bool UAnimInstance::HasNativeStateExitBinding(const FName& MachineName, const FName& StateName, FName& OutBindingName)
{
	for(const auto& Binding : NativeStateExitBindings)
	{
		if(Binding.MachineName == MachineName && Binding.StateName == StateName)
		{
			IDelegateInstance* DelegateInstance = Binding.NativeStateDelegate.GetDelegateInstance();
			if(DelegateInstance)
			{
				OutBindingName = DelegateInstance->GetFunctionName();
			}
			return true;
		}
	}

	return false;
}

void UAnimInstance::BindNativeDelegates()
{
	auto ForEachStateLambda = [&](UAnimBlueprintGeneratedClass* AnimBlueprintGeneratedClass, const FName& MachineName, const FName& StateName, TFunctionRef<void (FAnimNode_StateMachine*, const FBakedAnimationState&, int32)> Predicate)
	{
		for(UStructProperty* Property : AnimBlueprintGeneratedClass->AnimNodeProperties)
		{
			if(Property && Property->Struct == FAnimNode_StateMachine::StaticStruct())
			{
				FAnimNode_StateMachine* StateMachine = Property->ContainerPtrToValuePtr<FAnimNode_StateMachine>(this);
				if(StateMachine)
				{
					const FBakedAnimationStateMachine* MachineDescription = GetMachineDescription(AnimBlueprintGeneratedClass, StateMachine);
					if(MachineDescription && MachineName == MachineDescription->MachineName)
					{
						// check each state transition for a match
						int32 StateIndex = 0;
						for(const FBakedAnimationState& State : MachineDescription->States)
						{
							if(State.StateName == StateName)
							{
								Predicate(StateMachine, State, StateIndex);
							}
							StateIndex++;
						}
					}
				}
			}
		}
	};

	UAnimBlueprintGeneratedClass* AnimBlueprintGeneratedClass = Cast<UAnimBlueprintGeneratedClass>(GetClass());
	if(AnimBlueprintGeneratedClass)
	{
		// transition delegates
		for(const auto& Binding : NativeTransitionBindings)
		{
			ForEachStateLambda(AnimBlueprintGeneratedClass, Binding.MachineName, Binding.PreviousStateName, 
				[&](FAnimNode_StateMachine* StateMachine, const FBakedAnimationState& State, int32 StateIndex)
				{
					for(const FBakedStateExitTransition& TransitionExit : State.Transitions)
					{
						if(TransitionExit.CanTakeDelegateIndex != INDEX_NONE)
						{
							// In case the state machine hasn't been initilized, we need to re-get the desc
							const FBakedAnimationStateMachine* MachineDesc = GetMachineDescription(AnimBlueprintGeneratedClass, StateMachine);
							const FAnimationTransitionBetweenStates& Transition = MachineDesc->Transitions[TransitionExit.TransitionIndex];
							const FBakedAnimationState& BakedState = MachineDesc->States[Transition.NextState];
							
							if(BakedState.StateName == Binding.NextStateName)
							{
								FAnimNode_TransitionResult* ResultNode = GetNodeFromPropertyIndex<FAnimNode_TransitionResult>(this, AnimBlueprintGeneratedClass, TransitionExit.CanTakeDelegateIndex);
								if(ResultNode)
								{
									ResultNode->NativeTransitionDelegate = Binding.NativeTransitionDelegate;
								}
							}
						}
					}
				});
		}

		// state entry delegates
		for(const auto& Binding : NativeStateEntryBindings)
		{
			ForEachStateLambda(AnimBlueprintGeneratedClass, Binding.MachineName, Binding.StateName, 
				[&](FAnimNode_StateMachine* StateMachine, const FBakedAnimationState& State, int32 StateIndex)
				{
					// allocate enough space for all our states we need so far
					StateMachine->OnGraphStatesEntered.SetNum(FMath::Max(StateIndex + 1, StateMachine->OnGraphStatesEntered.Num()));
					StateMachine->OnGraphStatesEntered[StateIndex] = Binding.NativeStateDelegate;
				});
		}

		// state exit delegates
		for(const auto& Binding : NativeStateExitBindings)
		{
			ForEachStateLambda(AnimBlueprintGeneratedClass, Binding.MachineName, Binding.StateName, 
				[&](FAnimNode_StateMachine* StateMachine, const FBakedAnimationState& State, int32 StateIndex)
				{
					// allocate enough space for all our states we need so far
					StateMachine->OnGraphStatesExited.SetNum(FMath::Max(StateIndex + 1, StateMachine->OnGraphStatesExited.Num()));
					StateMachine->OnGraphStatesExited[StateIndex] = Binding.NativeStateDelegate;
				});
		}
	}
}

void OutputCurveMap(TMap<FName, float>& CurveMap, UCanvas* Canvas, FDisplayDebugManager& DisplayDebugManager, float Indent)
{
	TArray<FName> Names;
	CurveMap.GetKeys(Names);
	Names.Sort();
	for (FName CurveName : Names)
	{
		FString CurveEntry = FString::Printf(TEXT("%s: %.3f"), *CurveName.ToString(), CurveMap[CurveName]);
		DisplayDebugManager.DrawString(CurveEntry, Indent);
	}
}

void OutputTickRecords(const TArray<FAnimTickRecord>& Records, UCanvas* Canvas, float Indent, const int32 HighlightIndex, FLinearColor TextColor, FLinearColor HighlightColor, FLinearColor InactiveColor, FDisplayDebugManager& DisplayDebugManager, bool bFullBlendspaceDisplay)
{
	for (int32 PlayerIndex = 0; PlayerIndex < Records.Num(); ++PlayerIndex)
	{
		const FAnimTickRecord& Player = Records[PlayerIndex];

		DisplayDebugManager.SetLinearDrawColor((PlayerIndex == HighlightIndex) ? HighlightColor : TextColor);

		FString PlayerEntry;

		// Part of a sync group
		if (HighlightIndex != INDEX_NONE)
		{
			PlayerEntry = FString::Printf(TEXT("%i) %s (%s) W:%.1f%% P:%.2f, Prev(i:%d, t:%.3f) Next(i:%d, t:%.3f)"),
				PlayerIndex, *Player.SourceAsset->GetName(), *Player.SourceAsset->GetClass()->GetName(), Player.EffectiveBlendWeight*100.f, Player.TimeAccumulator != nullptr ? *Player.TimeAccumulator : 0.f
				, Player.MarkerTickRecord->PreviousMarker.MarkerIndex, Player.MarkerTickRecord->PreviousMarker.TimeToMarker, Player.MarkerTickRecord->NextMarker.MarkerIndex, Player.MarkerTickRecord->NextMarker.TimeToMarker);
		}
		// not part of a sync group
		else
		{
			PlayerEntry = FString::Printf(TEXT("%i) %s (%s) W:%.1f%% P:%.2f"),
				PlayerIndex, *Player.SourceAsset->GetName(), *Player.SourceAsset->GetClass()->GetName(), Player.EffectiveBlendWeight*100.f, Player.TimeAccumulator != nullptr ? *Player.TimeAccumulator : 0.f);
		}

		DisplayDebugManager.DrawString(PlayerEntry, Indent);

		if (UBlendSpaceBase* BlendSpace = Cast<UBlendSpaceBase>(Player.SourceAsset))
		{
			if (bFullBlendspaceDisplay && Player.BlendSpace.BlendSampleDataCache && Player.BlendSpace.BlendSampleDataCache->Num() > 0)
			{
				TArray<FBlendSampleData> SampleData = *Player.BlendSpace.BlendSampleDataCache;
				SampleData.Sort([](const FBlendSampleData& L, const FBlendSampleData& R) { return L.SampleDataIndex < R.SampleDataIndex; });

				FIndenter BlendspaceIndent(Indent);
				const FVector BlendSpacePosition(Player.BlendSpace.BlendSpacePositionX, Player.BlendSpace.BlendSpacePositionY, 0.f);
				FString BlendspaceHeader = FString::Printf(TEXT("Blendspace Input (%s)"), *BlendSpacePosition.ToString());
				DisplayDebugManager.DrawString(BlendspaceHeader, Indent);

				const TArray<FBlendSample>& BlendSamples = BlendSpace->GetBlendSamples();

				int32 WeightedSampleIndex = 0;

				for (int32 SampleIndex = 0; SampleIndex < BlendSamples.Num(); ++SampleIndex)
				{
					const FBlendSample& BlendSample = BlendSamples[SampleIndex];

					float Weight = 0.f;
					for (; WeightedSampleIndex < SampleData.Num(); ++WeightedSampleIndex)
					{
						FBlendSampleData& WeightedSample = SampleData[WeightedSampleIndex];
						if (WeightedSample.SampleDataIndex == SampleIndex)
						{
							Weight += WeightedSample.GetWeight();
						}
						else if (WeightedSample.SampleDataIndex > SampleIndex)
						{
							break;
						}
					}

					FIndenter SampleIndent(Indent);

					DisplayDebugManager.SetLinearDrawColor((Weight > 0.f) ? TextColor : InactiveColor);

					FString SampleEntry = FString::Printf(TEXT("%s W:%.1f%%"), *BlendSample.Animation->GetName(), Weight*100.f);
					DisplayDebugManager.DrawString(SampleEntry, Indent);
				}
			}
		}
	}
}

void UAnimInstance::DisplayDebug(class UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos)
{
	float Indent = 0.f;

	FLinearColor TextYellow(0.86f, 0.69f, 0.f);
	FLinearColor TextWhite(0.9f, 0.9f, 0.9f);
	FLinearColor ActiveColor(0.1f, 0.6f, 0.1f);
	FLinearColor InactiveColor(0.2f, 0.2f, 0.2f);
	FLinearColor PoseSourceColor(0.5f, 0.25f, 0.5f);

	FDisplayDebugManager& DisplayDebugManager = Canvas->DisplayDebugManager;
	DisplayDebugManager.SetFont(GEngine->GetSmallFont());
	DisplayDebugManager.SetLinearDrawColor(TextYellow);

	static FName CAT_SyncGroups(TEXT("SyncGroups"));
	static FName CAT_Montages(TEXT("Montages"));
	static FName CAT_Graph(TEXT("Graph"));
	static FName CAT_Curves(TEXT("Curves"));
	static FName CAT_Notifies(TEXT("Notifies"));
	static FName CAT_FullAnimGraph(TEXT("FullGraph"));
	static FName CAT_FullBlendspaceDisplay(TEXT("FullBlendspaceDisplay"));

	const bool bShowSyncGroups = DebugDisplay.IsCategoryToggledOn(CAT_SyncGroups, true);
	const bool bShowMontages = DebugDisplay.IsCategoryToggledOn(CAT_Montages, true);
	const bool bShowGraph = DebugDisplay.IsCategoryToggledOn(CAT_Graph, true);
	const bool bShowCurves = DebugDisplay.IsCategoryToggledOn(CAT_Curves, true);
	const bool bShowNotifies = DebugDisplay.IsCategoryToggledOn(CAT_Notifies, true);
	const bool bFullGraph = DebugDisplay.IsCategoryToggledOn(CAT_FullAnimGraph, false);
	const bool bFullBlendspaceDisplay = DebugDisplay.IsCategoryToggledOn(CAT_FullBlendspaceDisplay, true);

	FString Heading = FString::Printf(TEXT("Animation: %s"), *GetName());
	DisplayDebugManager.DrawString(Heading, Indent);

	if (bShowGraph && RootNode != nullptr)
	{
		DisplayDebugManager.SetLinearDrawColor(TextYellow);

		Heading = FString::Printf(TEXT("Anim Node Tree"));
		DisplayDebugManager.DrawString(Heading, Indent);

		const float NodeIndent = 8.f;
		const float LineIndent = 4.f;
		const float AttachLineLength = NodeIndent - LineIndent;

		FIndenter AnimNodeTreeIndent(Indent);

		DebugDataCounter.Increment();
		FNodeDebugData NodeDebugData(this);
		RootNode->GatherDebugData(NodeDebugData);

		TArray<FNodeDebugData::FFlattenedDebugData> FlattenedData = NodeDebugData.GetFlattenedDebugData();

		TArray<FVector2D> IndentLineStartCoord; // Index represents indent level, track the current starting point for that 

		int32 PrevChainID = -1;

		for (FNodeDebugData::FFlattenedDebugData& Line : FlattenedData)
		{
			if (!Line.IsOnActiveBranch() && !bFullGraph)
			{
				continue;
			}
			float CurrIndent = Indent + (Line.Indent * NodeIndent);
			float CurrLineYBase = DisplayDebugManager.GetYPos() + DisplayDebugManager.GetMaxCharHeight();

			if (PrevChainID != Line.ChainID)
			{
				const int32 HalfStep = int32(DisplayDebugManager.GetMaxCharHeight() / 2);
				DisplayDebugManager.ShiftYDrawPosition(float(HalfStep)); // Extra spacing to delimit different chains, CurrLineYBase now 
				// roughly represents middle of text line, so we can use it for line drawing

				//Handle line drawing
				int32 VerticalLineIndex = Line.Indent - 1;
				if (IndentLineStartCoord.IsValidIndex(VerticalLineIndex))
				{
					FVector2D LineStartCoord = IndentLineStartCoord[VerticalLineIndex];
					IndentLineStartCoord[VerticalLineIndex] = FVector2D(DisplayDebugManager.GetXPos(), CurrLineYBase);

					// If indent parent is not in same column, ignore line.
					if (FMath::IsNearlyEqual(LineStartCoord.X, DisplayDebugManager.GetXPos()))
					{
						float EndX = DisplayDebugManager.GetXPos() + CurrIndent;
						float StartX = EndX - AttachLineLength;

						//horizontal line to node
						DrawDebugCanvas2DLine(Canvas, FVector(StartX, CurrLineYBase, 0.f), FVector(EndX, CurrLineYBase, 0.f), ActiveColor);

						//vertical line
						DrawDebugCanvas2DLine(Canvas, FVector(StartX, LineStartCoord.Y, 0.f), FVector(StartX, CurrLineYBase, 0.f), ActiveColor);
					}
				}

				CurrLineYBase += HalfStep; // move CurrYLineBase back to base of line
			}

			// Update our base position for subsequent line drawing
			if (!IndentLineStartCoord.IsValidIndex(Line.Indent))
			{
				IndentLineStartCoord.AddZeroed(Line.Indent + 1 - IndentLineStartCoord.Num());
			}
			IndentLineStartCoord[Line.Indent] = FVector2D(DisplayDebugManager.GetXPos(), CurrLineYBase);

			PrevChainID = Line.ChainID;
			FLinearColor ItemColor = Line.bPoseSource ? PoseSourceColor : ActiveColor;
			DisplayDebugManager.SetLinearDrawColor(Line.IsOnActiveBranch() ? ItemColor : InactiveColor);
			DisplayDebugManager.DrawString(Line.DebugLine, CurrIndent);
		}
	}

	if (bShowSyncGroups)
	{
		FIndenter AnimIndent(Indent);

		//Display Sync Groups
		const TArray<FAnimGroupInstance>& SyncGroups = SyncGroupArrays[GetSyncGroupReadIndex()];
		const TArray<FAnimTickRecord>& UngroupedActivePlayers = UngroupedActivePlayerArrays[GetSyncGroupReadIndex()];

		Heading = FString::Printf(TEXT("SyncGroups: %i"), SyncGroups.Num());
		DisplayDebugManager.DrawString(Heading, Indent);

		for (int32 GroupIndex = 0; GroupIndex < SyncGroups.Num(); ++GroupIndex)
		{
			FIndenter GroupIndent(Indent);
			const FAnimGroupInstance& SyncGroup = SyncGroups[GroupIndex];

			DisplayDebugManager.SetLinearDrawColor(TextYellow);

			FString GroupLabel = FString::Printf(TEXT("Group %i - Players %i"), GroupIndex, SyncGroup.ActivePlayers.Num());
			DisplayDebugManager.DrawString(GroupLabel, Indent);

			if (SyncGroup.ActivePlayers.Num() > 0)
			{
				check(SyncGroup.GroupLeaderIndex != -1);
				OutputTickRecords(SyncGroup.ActivePlayers, Canvas, Indent, SyncGroup.GroupLeaderIndex, TextWhite, ActiveColor, InactiveColor, DisplayDebugManager, bFullBlendspaceDisplay);
			}
		}

		DisplayDebugManager.SetLinearDrawColor(TextYellow);

		Heading = FString::Printf(TEXT("Ungrouped: %i"), UngroupedActivePlayers.Num());
		DisplayDebugManager.DrawString(Heading, Indent);

		DisplayDebugManager.SetLinearDrawColor(TextWhite);

		OutputTickRecords(UngroupedActivePlayers, Canvas, Indent, -1, TextWhite, ActiveColor, InactiveColor, DisplayDebugManager, bFullBlendspaceDisplay);
	}

	if (bShowMontages)
	{
		DisplayDebugManager.SetLinearDrawColor(TextYellow);

		Heading = FString::Printf(TEXT("Montages: %i"), MontageInstances.Num());
		DisplayDebugManager.DrawString(Heading, Indent);

		for (int32 MontageIndex = 0; MontageIndex < MontageInstances.Num(); ++MontageIndex)
		{
			FIndenter PlayerIndent(Indent);

			FAnimMontageInstance* MontageInstance = MontageInstances[MontageIndex];

			DisplayDebugManager.SetLinearDrawColor((MontageInstance->IsActive()) ? ActiveColor : TextWhite);

			FString MontageEntry = FString::Printf(TEXT("%i) %s CurrSec: %s NextSec: %s W:%.3f DW:%.3f"), MontageIndex, *MontageInstance->Montage->GetName(), *MontageInstance->GetCurrentSection().ToString(), *MontageInstance->GetNextSection().ToString(), MontageInstance->GetWeight(), MontageInstance->GetDesiredWeight());
			DisplayDebugManager.DrawString(MontageEntry, Indent);
		}
	}

	if (bShowNotifies)
	{
		DisplayDebugManager.SetLinearDrawColor(TextYellow);

		Heading = FString::Printf(TEXT("Active Notify States: %i"), ActiveAnimNotifyState.Num());
		DisplayDebugManager.DrawString(Heading, Indent);

		DisplayDebugManager.SetLinearDrawColor(TextWhite);

		for (int32 NotifyIndex = 0; NotifyIndex < ActiveAnimNotifyState.Num(); ++NotifyIndex)
		{
			FIndenter NotifyIndent(Indent);

			const FAnimNotifyEvent& NotifyState = ActiveAnimNotifyState[NotifyIndex];

			FString NotifyEntry = FString::Printf(TEXT("%i) %s Class: %s Dur:%.3f"), NotifyIndex, *NotifyState.NotifyName.ToString(), *NotifyState.NotifyStateClass->GetName(), NotifyState.GetDuration());
			DisplayDebugManager.DrawString(NotifyEntry, Indent);
		}
	}

	if (bShowCurves)
	{
		DisplayDebugManager.SetLinearDrawColor(TextYellow);

		Heading = FString::Printf(TEXT("Curves"));
		DisplayDebugManager.DrawString(Heading, Indent);

		{
			FIndenter CurveIndent(Indent);

			Heading = FString::Printf(TEXT("Morph Curves: %i"), MorphTargetCurves.Num());
			DisplayDebugManager.DrawString(Heading, Indent);

			DisplayDebugManager.SetLinearDrawColor(TextWhite);

			{
				FIndenter MorphCurveIndent(Indent);
				OutputCurveMap(MorphTargetCurves, Canvas, DisplayDebugManager, Indent);
			}

			DisplayDebugManager.SetLinearDrawColor(TextYellow);

			Heading = FString::Printf(TEXT("Material Curves: %i"), MaterialParameterCurves.Num());
			DisplayDebugManager.DrawString(Heading, Indent);

			DisplayDebugManager.SetLinearDrawColor(TextWhite);

			{
				FIndenter MaterialCurveIndent(Indent);
				OutputCurveMap(MaterialParameterCurves, Canvas, DisplayDebugManager, Indent);
			}

			DisplayDebugManager.SetLinearDrawColor(TextYellow);

			Heading = FString::Printf(TEXT("Event Curves: %i"), EventCurves.Num());
			DisplayDebugManager.DrawString(Heading, Indent);

			DisplayDebugManager.SetLinearDrawColor(TextWhite);

			{
				FIndenter EventCurveIndent(Indent);
				OutputCurveMap(EventCurves, Canvas, DisplayDebugManager, Indent);
			}
		}
	}
}

void UAnimInstance::RecalcRequiredBones()
{
	USkeletalMeshComponent* SkelMeshComp = GetSkelMeshComponent();
	check( SkelMeshComp )

	if( SkelMeshComp->SkeletalMesh && SkelMeshComp->SkeletalMesh->Skeleton )
	{
		RequiredBones.InitializeTo(SkelMeshComp->RequiredBones, *SkelMeshComp->SkeletalMesh);
	}
	else if( CurrentSkeleton != NULL )
	{
		RequiredBones.InitializeTo(SkelMeshComp->RequiredBones, *CurrentSkeleton);
	}

	// When RequiredBones mapping has changed, AnimNodes need to update their bones caches. 
	bBoneCachesInvalidated = true;
}

void UAnimInstance::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (!Ar.IsLoading() || !Ar.IsSaving())
	{
		Ar << RequiredBones;
	}
}

bool UAnimInstance::CanTransitionSignature() const
{
	return false;
}

void UAnimInstance::AddAnimNotifies(const TArray<const FAnimNotifyEvent*>& NewNotifies, const float InstanceWeight)
{
	// for now there is no filter whatsoever, it just adds everything requested
	for (const FAnimNotifyEvent* Notify : NewNotifies)
	{
		// only add if it is over TriggerWeightThreshold
		const bool bPassesDedicatedServerCheck = Notify->bTriggerOnDedicatedServer || !IsRunningDedicatedServer();
		if (bPassesDedicatedServerCheck && Notify->TriggerWeightThreshold <= InstanceWeight && PassesFiltering(Notify) && PassesChanceOfTriggering(Notify))
		{
			// Only add unique AnimNotifyState instances just once. We can get multiple triggers if looping over an animation.
			// It is the same state, so just report it once.
			Notify->NotifyStateClass ? AnimNotifies.AddUnique(Notify) : AnimNotifies.Add(Notify);
		}
	}
}

bool UAnimInstance::PassesFiltering(const FAnimNotifyEvent* Notify) const
{
	switch (Notify->NotifyFilterType)
	{
		case ENotifyFilterType::NoFiltering:
		{
			return true;
		}
		case ENotifyFilterType::LOD:
		{
			USkeletalMeshComponent* Comp = GetSkelMeshComponent();
			return Comp ? Notify->NotifyFilterLOD > Comp->PredictedLODLevel : true;
		}
		default:
		{
			ensure(false); // Unknown Filter Type
		}
	}
	return true;
}

bool UAnimInstance::PassesChanceOfTriggering(const FAnimNotifyEvent* Event) const
{
	return Event->NotifyStateClass ? true : FMath::FRandRange(0.f, 1.f) < Event->NotifyTriggerChance;
}

void UAnimInstance::AddAnimNotifyFromGeneratedClass(int32 NotifyIndex)
{
	if(NotifyIndex==INDEX_NONE)
	{
		return;
	}

	if (UAnimBlueprintGeneratedClass* AnimBlueprintClass = Cast<UAnimBlueprintGeneratedClass>(GetClass()))
	{
		check(AnimBlueprintClass->AnimNotifies.IsValidIndex(NotifyIndex));
		const FAnimNotifyEvent* Notify =& AnimBlueprintClass->AnimNotifies[NotifyIndex];
		AnimNotifies.Add(Notify);
	}
}

void UAnimInstance::AddCurveValue(const FName& CurveName, float Value, int32 CurveTypeFlags)
{
	// save curve value, it will overwrite if same exists, 
	//CurveValues.Add(CurveName, Value);
	if (CurveTypeFlags & ACF_TriggerEvent)
	{
		float *CurveValPtr = EventCurves.Find(CurveName);
		if ( CurveValPtr )
		{
			// sum up, in the future we might normalize, but for now this just sums up
			// this won't work well if all of them have full weight - i.e. additive 
			*CurveValPtr += Value;
		}
		else
		{
			EventCurves.Add(CurveName, Value);
		}
	}

	if (CurveTypeFlags & ACF_DrivesMorphTarget)
	{
		float *CurveValPtr = MorphTargetCurves.Find(CurveName);
		if ( CurveValPtr )
		{
			// sum up, in the future we might normalize, but for now this just sums up
			// this won't work well if all of them have full weight - i.e. additive 
			*CurveValPtr += Value;
		}
		else
		{
			MorphTargetCurves.Add(CurveName, Value);
		}
	}

	if (CurveTypeFlags & ACF_DrivesMaterial)
	{
		MaterialParamatersToClear.RemoveSwap(CurveName);
		float* CurveValPtr = MaterialParameterCurves.Find(CurveName);
		if( CurveValPtr)
		{
			*CurveValPtr += Value;
		}
		else
		{
			MaterialParameterCurves.Add(CurveName, Value);
		}
	}
}

void UAnimInstance::AddCurveValue(const USkeleton::AnimCurveUID Uid, float Value, int32 CurveTypeFlags)
{
	FName CurrentCurveName;
	// Grab the smartname mapping from our current skeleton and resolve the curve name. We cannot cache
	// the smart name mapping as the skeleton can change at any time.
	if(FSmartNameMapping* NameMapping = CurrentSkeleton->SmartNames.GetContainer(USkeleton::AnimCurveMappingName))
	{
		NameMapping->GetName(Uid, CurrentCurveName);
	}
	AddCurveValue(CurrentCurveName, Value, CurveTypeFlags);
}

void UAnimInstance::UpdateCurves(const FBlendedCurve& InCurve)
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateCurves);

	//Track material params we set last time round so we can clear them if they aren't set again.
	MaterialParamatersToClear.Reset();
	for(auto Iter = MaterialParameterCurves.CreateConstIterator(); Iter; ++Iter)
	{
		if(Iter.Value() > 0.0f)
		{
			MaterialParamatersToClear.Add(Iter.Key());
		}
	}

	EventCurves.Reset();
	MorphTargetCurves.Reset();
	MaterialParameterCurves.Reset();

	for(int32 CurveId=0; CurveId<InCurve.UIDList.Num(); ++CurveId)
	{
		// had to add to anotehr data type
		AddCurveValue(InCurve.UIDList[CurveId], InCurve.Elements[CurveId].Value, InCurve.Elements[CurveId].Flags);
	}

	// Add 0.0 curves to clear parameters that we have previously set but didn't set this tick.
	//   - Make a copy of MaterialParametersToClear as it will be modified by AddCurveValue
	TArray<FName> ParamsToClearCopy = MaterialParamatersToClear;
	for(int i = 0; i < ParamsToClearCopy.Num(); ++i)
	{
		AddCurveValue(ParamsToClearCopy[i], 0.0f, ACF_DrivesMaterial);
	}
}

void UAnimInstance::TriggerAnimNotifies(float DeltaSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_AnimTriggerAnimNotifies);
	USkeletalMeshComponent* SkelMeshComp = GetSkelMeshComponent();

	// Array that will replace the 'ActiveAnimNotifyState' at the end of this function.
	TArray<FAnimNotifyEvent> NewActiveAnimNotifyState;
	// AnimNotifyState freshly added that need their 'NotifyBegin' event called.
	TArray<const FAnimNotifyEvent *> NotifyStateBeginEvent;

	for (int32 Index=0; Index<AnimNotifies.Num(); Index++)
	{
		const FAnimNotifyEvent* AnimNotifyEvent = AnimNotifies[Index];

		// AnimNotifyState
		if( AnimNotifyEvent->NotifyStateClass )
		{
			if( !ActiveAnimNotifyState.RemoveSingleSwap(*AnimNotifyEvent) )
			{
				// Queue up calls to 'NotifyBegin', so they happen after 'NotifyEnd'.
				NotifyStateBeginEvent.Add(AnimNotifyEvent);
			}
			NewActiveAnimNotifyState.Add(*AnimNotifyEvent);
			continue;
		}

		// Trigger non 'state' AnimNotifies
		TriggerSingleAnimNotify(AnimNotifyEvent);
	}

	// Send end notification to AnimNotifyState not active anymore.
	for(int32 Index=0; Index<ActiveAnimNotifyState.Num(); Index++)
	{
		const FAnimNotifyEvent& AnimNotifyEvent = ActiveAnimNotifyState[Index];
		AnimNotifyEvent.NotifyStateClass->NotifyEnd(SkelMeshComp, Cast<UAnimSequenceBase>(AnimNotifyEvent.NotifyStateClass->GetOuter()));
	}

	// Call 'NotifyBegin' event on freshly added AnimNotifyState.
	for (int32 Index = 0; Index < NotifyStateBeginEvent.Num(); Index++)
	{
		const FAnimNotifyEvent* AnimNotifyEvent = NotifyStateBeginEvent[Index];
		AnimNotifyEvent->NotifyStateClass->NotifyBegin(SkelMeshComp, Cast<UAnimSequenceBase>(AnimNotifyEvent->NotifyStateClass->GetOuter()), AnimNotifyEvent->GetDuration());
	}

	// Switch our arrays.
	ActiveAnimNotifyState = MoveTemp(NewActiveAnimNotifyState);

	// Tick currently active AnimNotifyState
	for(int32 Index=0; Index<ActiveAnimNotifyState.Num(); Index++)
	{
		const FAnimNotifyEvent& AnimNotifyEvent = ActiveAnimNotifyState[Index];
		AnimNotifyEvent.NotifyStateClass->NotifyTick(SkelMeshComp, Cast<UAnimSequenceBase>(AnimNotifyEvent.NotifyStateClass->GetOuter()), DeltaSeconds);
	}
}

void UAnimInstance::TriggerSingleAnimNotify(const FAnimNotifyEvent* AnimNotifyEvent)
{
	// This is for non 'state' anim notifies.
	if (AnimNotifyEvent && (AnimNotifyEvent->NotifyStateClass == NULL))
	{
		if (AnimNotifyEvent->Notify != NULL)
		{
			// Implemented notify: just call Notify. UAnimNotify will forward this to the event which will do the work.
			AnimNotifyEvent->Notify->Notify(GetSkelMeshComponent(), Cast<UAnimSequenceBase>(AnimNotifyEvent->Notify->GetOuter()));
		}
		else if (AnimNotifyEvent->NotifyName != NAME_None)
		{
			// Custom Event based notifies. These will call a AnimNotify_* function on the AnimInstance.
			FString FuncName = FString::Printf(TEXT("AnimNotify_%s"), *AnimNotifyEvent->NotifyName.ToString());
			FName FuncFName = FName(*FuncName);

			UFunction* Function = FindFunction(FuncFName);
			if (Function)
			{
				// if parameter is none, add event
				if (Function->NumParms == 0)
				{
					ProcessEvent(Function, NULL);
				}
				else if ((Function->NumParms == 1) && (Cast<UObjectProperty>(Function->PropertyLink) != NULL))
				{
					struct FAnimNotifierHandler_Parms
					{
						UAnimNotify* Notify;
					};

					FAnimNotifierHandler_Parms Parms;
					Parms.Notify = AnimNotifyEvent->Notify;
					ProcessEvent(Function, &Parms);
				}
				else
				{
					// Actor has event, but with different parameters. Print warning
					UE_LOG(LogAnimNotify, Warning, TEXT("Anim notifier named %s, but the parameter number does not match or not of the correct type"), *FuncName);
				}
			}
		}
	}
}

//to debug montage weight
#define DEBUGMONTAGEWEIGHT 0

void UAnimInstance::GetSlotWeight(FName const& SlotNodeName, float& out_SlotNodeWeight, float& out_SourceWeight, float& out_TotalNodeWeight) const
{
	// node total weight 
	float NewSlotNodeWeight = 0.f;
	// this is required to track, because it will be 1-SourceWeight
	// if additive, it can be applied more
	float NonAdditiveTotalWeight = 0.f;

#if DEBUGMONTAGEWEIGHT
	float TotalDesiredWeight = 0.f;
#endif
	// first get all the montage instance weight this slot node has
	for (const FMontageEvaluationState& EvalState : MontageEvaluationData)
	{
		if (EvalState.Montage->IsValidSlot(SlotNodeName))
		{
			NewSlotNodeWeight += EvalState.MontageWeight;

			if( !EvalState.Montage->IsValidAdditiveSlot(SlotNodeName) )
			{
				NonAdditiveTotalWeight += EvalState.MontageWeight;
			}

#if DEBUGMONTAGEWEIGHT			
			TotalDesiredWeight += EvalState->DesiredWeight;
#endif
			UE_LOG(LogAnimMontage, Verbose, TEXT("GetSlotWeight : Owner: %s, AnimMontage: %s,  (DesiredWeight:%0.2f, Weight:%0.2f)"),
						*GetNameSafe(GetOwningActor()), *EvalState.Montage->GetName(), EvalState.DesiredWeight, EvalState.MontageWeight);
		}
	}

	// save the total node weight, it can be more than 1
	// we need this so that when we eval, we normalized by this weight
	// calculating there can cause inconsistency if some data changes
	out_TotalNodeWeight = NewSlotNodeWeight;

	// this can happen when it's blending in OR when newer animation comes in with shorter blendtime
	// say #1 animation was blending out time with current blendtime 1.0 #2 animation was blending in with 1.0 (old) but got blend out with new blendtime 0.2f
	// #3 animation was blending in with the new blendtime 0.2f, you'll have sum of #1, 2, 3 exceeds 1.f
	if (NewSlotNodeWeight > (1.f + ZERO_ANIMWEIGHT_THRESH))
	{
		// you don't want to change weight of montage instance since it can play multiple slots
		// if you change one, it will apply to all slots in that montage
		// instead we should renormalize when we eval
		// this should happen in the eval phase
		NonAdditiveTotalWeight /= NewSlotNodeWeight;
		// since we normalized, we reset
		NewSlotNodeWeight = 1.f;
	}
#if DEBUGMONTAGEWEIGHT
	else if (TotalDesiredWeight == 1.f && TotalSum < 1.f - ZERO_ANIMWEIGHT_THRESH)
	{
		// this can happen when it's blending in OR when newer animation comes in with longer blendtime
		// say #1 animation was blending out time with current blendtime 0.2 #2 animation was blending in with 0.2 (old) but got blend out with new blendtime 1.f
		// #3 animation was blending in with the new blendtime 1.f, you'll have sum of #1, 2, 3 doesn't meet 1.f
		UE_LOG(LogAnimMontage, Warning, TEXT("[%s] Montage has less weight. Blending in?(%f)"), *SlotNodeName.ToString(), TotalSum);
	}
#endif

	out_SlotNodeWeight = NewSlotNodeWeight;
	out_SourceWeight = 1.f - NonAdditiveTotalWeight;
}

// for now disable becauase it will not work with single node instance
#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
#define DEBUG_MONTAGEINSTANCE_WEIGHT 0
#else
#define DEBUG_MONTAGEINSTANCE_WEIGHT 1
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

void UAnimInstance::SlotEvaluatePose(FName SlotNodeName, const FCompactPose& SourcePose, const FBlendedCurve& SourceCurve, float InSourceWeight, FCompactPose& BlendedPose, FBlendedCurve& BlendedCurve, float InBlendWeight, float InTotalNodeWeight)
{
	//Accessing MontageInstances from this function is not safe (as this can be called during Parallel Anim Evaluation!
	//Any montage data you need to add should be part of MontageEvaluationData
	// nothing to blend, just get it out
	if (InBlendWeight <= ZERO_ANIMWEIGHT_THRESH)
	{
		BlendedPose = SourcePose;
		BlendedCurve = SourceCurve;
		return;
	}

	// Split our data into additive and non additive.
	TArray<FSlotEvaluationPose> AdditivePoses;
	TArray<FSlotEvaluationPose> NonAdditivePoses;

	// first pass we go through collect weights and valid montages. 
#if DEBUG_MONTAGEINSTANCE_WEIGHT
	float TotalWeight = 0.f;
#endif // DEBUG_MONTAGEINSTANCE_WEIGHT
	for (const FMontageEvaluationState& EvalState : MontageEvaluationData)
	{
		if (EvalState.Montage->IsValidSlot(SlotNodeName))
		{
			FAnimTrack const* const AnimTrack = EvalState.Montage->GetAnimationData(SlotNodeName);

			// Find out additive type for pose.
			EAdditiveAnimationType const AdditiveAnimType = AnimTrack->IsAdditive() 
				? (AnimTrack->IsRotationOffsetAdditive() ? AAT_RotationOffsetMeshSpace : AAT_LocalSpaceBase)
				: AAT_None;

			FSlotEvaluationPose NewPose(EvalState.MontageWeight, AdditiveAnimType);
			
			// Bone array has to be allocated prior to calling GetPoseFromAnimTrack
			NewPose.Pose.SetBoneContainer(&RequiredBones);
			NewPose.Curve.InitFrom(this);

			// Extract pose from Track
			FAnimExtractContext ExtractionContext(EvalState.MontagePosition, EvalState.Montage->HasRootMotion() && RootMotionMode != ERootMotionMode::NoRootMotionExtraction);
			AnimTrack->GetAnimationPose(NewPose.Pose, NewPose.Curve, ExtractionContext);

			// add montage curves 
			FBlendedCurve MontageCurve;
			MontageCurve.InitFrom(this);
			EvalState.Montage->EvaluateCurveData(MontageCurve, EvalState.MontagePosition);
			NewPose.Curve.Combine(MontageCurve);

#if DEBUG_MONTAGEINSTANCE_WEIGHT
			TotalWeight += EvalState.MontageWeight;
#endif // DEBUG_MONTAGEINSTANCE_WEIGHT
			if (AdditiveAnimType == AAT_None)
			{
				NonAdditivePoses.Add(NewPose);
			}
			else
			{
				AdditivePoses.Add(NewPose);
			}
		}
	}

	// allocate for blending
	// If source has any weight, add it to the blend array.
	float const SourceWeight = FMath::Clamp<float>(InSourceWeight, 0.f, 1.f);

#if DEBUG_MONTAGEINSTANCE_WEIGHT
	ensure (FMath::IsNearlyEqual(InTotalNodeWeight, TotalWeight, KINDA_SMALL_NUMBER));
#endif // DEBUG_MONTAGEINSTANCE_WEIGHT
	ensure (InTotalNodeWeight > ZERO_ANIMWEIGHT_THRESH);

	if (InTotalNodeWeight > (1.f + ZERO_ANIMWEIGHT_THRESH))
	{
		// Re-normalize additive poses
		for (int32 Index = 0; Index < AdditivePoses.Num(); Index++)
		{
			AdditivePoses[Index].Weight /= InTotalNodeWeight;
		}
		// Re-normalize non-additive poses
		for (int32 Index = 0; Index < NonAdditivePoses.Num(); Index++)
		{
			NonAdditivePoses[Index].Weight /= InTotalNodeWeight;
		}
	}

	// Make sure we have at least one montage here.
	check((AdditivePoses.Num() > 0) || (NonAdditivePoses.Num() > 0));

	// Second pass, blend non additive poses together
	{
		// If we're only playing additive animations, just copy source for base pose.
		if (NonAdditivePoses.Num() == 0)
		{
			BlendedPose = SourcePose;
			BlendedCurve = SourceCurve;
		}
		// Otherwise we need to blend non additive poses together
		else
		{
			int32 const NumPoses = NonAdditivePoses.Num() + ((SourceWeight > ZERO_ANIMWEIGHT_THRESH) ? 1 : 0);

			TArray<const FCompactPose*, TInlineAllocator<8>> BlendingPoses;
			BlendingPoses.AddUninitialized(NumPoses);

			TArray<float, TInlineAllocator<8>> BlendWeights;
			BlendWeights.AddUninitialized(NumPoses);

			TArray<const FBlendedCurve*, TInlineAllocator<8>> BlendingCurves;
			BlendingCurves.AddUninitialized(NumPoses);

			for (int32 Index = 0; Index < NonAdditivePoses.Num(); Index++)
			{
				BlendingPoses[Index] = &NonAdditivePoses[Index].Pose;
				BlendingCurves[Index] = &NonAdditivePoses[Index].Curve;
				BlendWeights[Index] = NonAdditivePoses[Index].Weight;
			}

			if (SourceWeight > ZERO_ANIMWEIGHT_THRESH)
			{
				int32 const SourceIndex = BlendWeights.Num() - 1;
				BlendingPoses[SourceIndex] = &SourcePose;
				BlendingCurves[SourceIndex] = &SourceCurve;
				BlendWeights[SourceIndex] = SourceWeight;
			}

			// now time to blend all montages
			FAnimationRuntime::BlendPosesTogetherIndirect(BlendingPoses, BlendingCurves, BlendWeights, BlendedPose, BlendedCurve);
		}
	}

	// Third pass, layer on weighted additive poses.
	if (AdditivePoses.Num() > 0)
	{
		for (int32 Index = 0; Index < AdditivePoses.Num(); Index++)
		{
			FSlotEvaluationPose const& AdditivePose = AdditivePoses[Index];
			FAnimationRuntime::AccumulateAdditivePose(BlendedPose, AdditivePose.Pose, BlendedCurve, AdditivePose.Curve, AdditivePose.Weight, AdditivePose.AdditiveType);
		}
	}

	// Normalize rotations after blending/accumulation
	BlendedPose.NormalizeRotations();
}

void UAnimInstance::ReinitializeSlotNodes()
{
	SlotWeightTracker.Reset();
	
	// Increment counter
	SlotNodeInitializationCounter.Increment();
}

void UAnimInstance::RegisterSlotNodeWithAnimInstance(FName SlotNodeName)
{
	// verify if same slot node name exists
	// then warn users, this is invalid
	if (SlotWeightTracker.Contains(SlotNodeName))
	{
		UClass* InstanceClass = GetClass();

		FString ClassNameString = InstanceClass ? InstanceClass->GetName() : FString("Unavailable");

		FMessageLog("AnimBlueprint").Warning(FText::Format(LOCTEXT("AnimInstance_SlotNode", "SLOTNODE: '{0}' in animation instance class {1} already exists. Remove duplicates from the animation graph for this class."), FText::FromString(SlotNodeName.ToString()), FText::FromString(ClassNameString)));
		return;
	}

	SlotWeightTracker.Add(SlotNodeName, FMontageActiveSlotTracker());
}

void UAnimInstance::UpdateSlotNodeWeight(FName SlotNodeName, float Weight)
{
	FMontageActiveSlotTracker* Tracker = SlotWeightTracker.Find(SlotNodeName);
	if (Tracker)
	{
		// Count as relevant if we are weighted in
		Tracker->bIsRelevantThisTick = Tracker->bIsRelevantThisTick || Weight > ZERO_ANIMWEIGHT_THRESH;
	}
}

void UAnimInstance::ClearSlotNodeWeights()
{
	for (auto Iter = SlotWeightTracker.CreateIterator(); Iter; ++Iter)
	{
		FMontageActiveSlotTracker& Tracker = Iter.Value();
		Tracker.bWasRelevantOnPreviousTick = Tracker.bIsRelevantThisTick;
		Tracker.bIsRelevantThisTick = false;
		Tracker.RootMotionWeight = 0.f;
	}
}

bool UAnimInstance::IsSlotNodeRelevantForNotifies(FName SlotNodeName) const
{
	const FMontageActiveSlotTracker* Tracker = SlotWeightTracker.Find(SlotNodeName);
	return ( Tracker && (Tracker->bIsRelevantThisTick || Tracker->bWasRelevantOnPreviousTick) );
}

void UAnimInstance::UpdateSlotRootMotionWeight(FName SlotNodeName, float Weight)
{
	FMontageActiveSlotTracker* Tracker = SlotWeightTracker.Find(SlotNodeName);
	if (Tracker)
	{
		Tracker->RootMotionWeight += Weight;
	}
}

float UAnimInstance::GetSlotRootMotionWeight(FName SlotNodeName) const
{
	const FMontageActiveSlotTracker* Tracker = SlotWeightTracker.Find(SlotNodeName);
	return Tracker ? Tracker->RootMotionWeight : 0.f;
}

float UAnimInstance::GetCurveValue(FName CurveName)
{
	float* Value = EventCurves.Find(CurveName);
	if (Value)
	{
		return *Value;
	}

	return 0.f;
}

float UAnimInstance::GetAnimAssetPlayerLength(class UAnimationAsset* AnimAsset)
{
	if (AnimAsset)
	{
		return AnimAsset->GetMaxCurrentTime();
	}

	return 0.f;
}

float UAnimInstance::GetAnimAssetPlayerTimeFraction(class UAnimationAsset* AnimAsset, float CurrentTime)
{
	float Length = (AnimAsset)? AnimAsset->GetMaxCurrentTime() : 0.f;
	if (Length > 0.f)
	{
		return CurrentTime / Length;
	}

	return 0.f;
}

float UAnimInstance::GetAnimAssetPlayerTimeFromEnd(class UAnimationAsset* AnimAsset, float CurrentTime)
{
	if (AnimAsset)
	{
		return AnimAsset->GetMaxCurrentTime() - CurrentTime;
	}

	return 0.f;
}

float UAnimInstance::GetAnimAssetPlayerTimeFromEndFraction(class UAnimationAsset* AnimAsset, float CurrentTime)
{
	float Length = (AnimAsset)? AnimAsset->GetMaxCurrentTime() : 0.f;
	if ( Length > 0.f )
	{
		return (Length- CurrentTime) / Length;
	}

	return 0.f;
}

float UAnimInstance::GetStateWeight(int32 MachineIndex, int32 StateIndex)
{
	if (UAnimBlueprintGeneratedClass* AnimBlueprintClass = Cast<UAnimBlueprintGeneratedClass>((UObject*)GetClass()))
	{
		if ((MachineIndex >= 0) && (MachineIndex < AnimBlueprintClass->AnimNodeProperties.Num()))
		{
			const int32 InstancePropertyIndex = AnimBlueprintClass->AnimNodeProperties.Num() - 1 - MachineIndex; //@TODO: ANIMREFACTOR: Reverse indexing

			UStructProperty* MachineInstanceProperty = AnimBlueprintClass->AnimNodeProperties[InstancePropertyIndex];
			checkSlow(MachineInstanceProperty->Struct->IsChildOf(FAnimNode_StateMachine::StaticStruct()));

			FAnimNode_StateMachine* MachineInstance = MachineInstanceProperty->ContainerPtrToValuePtr<FAnimNode_StateMachine>(this);

			return MachineInstance->GetStateWeight(StateIndex);
		}
	}

	return 0.0f;
}

float UAnimInstance::GetCurrentStateElapsedTime(int32 MachineIndex)
{
	if (UAnimBlueprintGeneratedClass* AnimBlueprintClass = Cast<UAnimBlueprintGeneratedClass>((UObject*)GetClass()))
	{
		if ((MachineIndex >= 0) && (MachineIndex < AnimBlueprintClass->AnimNodeProperties.Num()))
		{
			const int32 InstancePropertyIndex = AnimBlueprintClass->AnimNodeProperties.Num() - 1 - MachineIndex; //@TODO: ANIMREFACTOR: Reverse indexing

			UStructProperty* MachineInstanceProperty = AnimBlueprintClass->AnimNodeProperties[InstancePropertyIndex];
			checkSlow(MachineInstanceProperty->Struct->IsChildOf(FAnimNode_StateMachine::StaticStruct()));

			FAnimNode_StateMachine* MachineInstance = MachineInstanceProperty->ContainerPtrToValuePtr<FAnimNode_StateMachine>(this);

			return MachineInstance->GetCurrentStateElapsedTime();
		}
	}

	return 0.0f;
}

FName UAnimInstance::GetCurrentStateName(int32 MachineIndex)
{
	if (UAnimBlueprintGeneratedClass* AnimBlueprintClass = Cast<UAnimBlueprintGeneratedClass>((UObject*)GetClass()))
	{
		if ((MachineIndex >= 0) && (MachineIndex < AnimBlueprintClass->AnimNodeProperties.Num()))
		{
			const int32 InstancePropertyIndex = AnimBlueprintClass->AnimNodeProperties.Num() - 1 - MachineIndex; //@TODO: ANIMREFACTOR: Reverse indexing

			UStructProperty* MachineInstanceProperty = AnimBlueprintClass->AnimNodeProperties[InstancePropertyIndex];
			checkSlow(MachineInstanceProperty->Struct->IsChildOf(FAnimNode_StateMachine::StaticStruct()));

			FAnimNode_StateMachine* MachineInstance = MachineInstanceProperty->ContainerPtrToValuePtr<FAnimNode_StateMachine>(this);

			return MachineInstance->GetCurrentStateName();
		}
	}

	return NAME_None;
}

void UAnimInstance::Montage_UpdateWeight(float DeltaSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_Montage_UpdateWeight);

	// go through all montage instances, and update them
	// and make sure their weight is updated properly
	for (int32 I=0; I<MontageInstances.Num(); ++I)
	{
		if ( MontageInstances[I] )
		{
			MontageInstances[I]->UpdateWeight(DeltaSeconds);
		}
	}
}

void UAnimInstance::Montage_Advance(float DeltaSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_Montage_Advance);

	// We're about to tick montages, queue their events to they're triggered after batched anim notifies.
	bQueueMontageEvents = true;

	// go through all montage instances, and update them
	// and make sure their weight is updated properly
	for (int32 InstanceIndex = 0; InstanceIndex<MontageInstances.Num(); InstanceIndex++)
	{
		// should never be NULL
		FAnimMontageInstance* MontageInstance = MontageInstances[InstanceIndex];
		ensure(MontageInstance);
		if (MontageInstance)
		{
			bool const bUsingBlendedRootMotion = RootMotionMode == ERootMotionMode::RootMotionFromEverything;
			bool const bNoRootMotionExtraction = RootMotionMode == ERootMotionMode::NoRootMotionExtraction;

			// Extract root motion if we are using blend root motion (RootMotionFromEverything) or if we are set to extract root 
			// motion AND we are the active root motion instance. This is so we can make root motion deterministic for networking when
			// we are not using RootMotionFromEverything
			bool const bExtractRootMotion = bUsingBlendedRootMotion || (!bNoRootMotionExtraction && MontageInstance == GetRootMotionMontageInstance());

			FRootMotionMovementParams LocalExtractedRootMotion;
			FRootMotionMovementParams* RootMotionParams = NULL;
			if (bExtractRootMotion)
			{
				RootMotionParams = (RootMotionMode != ERootMotionMode::IgnoreRootMotion) ? &ExtractedRootMotion : &LocalExtractedRootMotion;
			}

			MontageInstance->MontageSync_PreUpdate();
			MontageInstance->Advance(DeltaSeconds, RootMotionParams, bUsingBlendedRootMotion);
			MontageInstance->MontageSync_PostUpdate();

			if (!MontageInstance->IsValid())
			{
				delete MontageInstance;
				MontageInstances.RemoveAt(InstanceIndex);
				--InstanceIndex;
			}
#if DO_CHECK && WITH_EDITORONLY_DATA && 0
			else
			{
				FAnimMontageInstance* AnimMontageInstance = MontageInstances(I);
				// print blending time and weight and montage name
				UE_LOG(LogAnimMontage, Warning, TEXT("%d. Montage (%s), DesiredWeight(%0.2f), CurrentWeight(%0.2f), BlendingTime(%0.2f)"), 
					I+1, *AnimMontageInstance->Montage->GetName(), AnimMontageInstance->GetDesiredWeight(), AnimMontageInstance->GetWeight(),  
					AnimMontageInstance->GetBlendTime() );
			}
#endif
		}
	}
}

void UAnimInstance::QueueMontageBlendingOutEvent(const FQueuedMontageBlendingOutEvent& MontageBlendingOutEvent)
{
	if (bQueueMontageEvents)
	{
		QueuedMontageBlendingOutEvents.Add(MontageBlendingOutEvent);
	}
	else
	{
		TriggerMontageBlendingOutEvent(MontageBlendingOutEvent);
	}
}

void UAnimInstance::TriggerMontageBlendingOutEvent(const FQueuedMontageBlendingOutEvent& MontageBlendingOutEvent)
{
	MontageBlendingOutEvent.Delegate.ExecuteIfBound(MontageBlendingOutEvent.Montage, MontageBlendingOutEvent.bInterrupted);
	OnMontageBlendingOut.Broadcast(MontageBlendingOutEvent.Montage, MontageBlendingOutEvent.bInterrupted);
}

void UAnimInstance::QueueMontageEndedEvent(const FQueuedMontageEndedEvent& MontageEndedEvent)
{
	if (bQueueMontageEvents)
	{
		QueuedMontageEndedEvents.Add(MontageEndedEvent);
	}
	else
	{
		TriggerMontageEndedEvent(MontageEndedEvent);
	}
}

void UAnimInstance::TriggerMontageEndedEvent(const FQueuedMontageEndedEvent& MontageEndedEvent)
{
	MontageEndedEvent.Delegate.ExecuteIfBound(MontageEndedEvent.Montage, MontageEndedEvent.bInterrupted);
	OnMontageEnded.Broadcast(MontageEndedEvent.Montage, MontageEndedEvent.bInterrupted);
}

void UAnimInstance::TriggerQueuedMontageEvents()
{
	// We don't need to queue montage events anymore.
	bQueueMontageEvents = false;

	// Trigger Montage blending out before Ended events.
	if (QueuedMontageBlendingOutEvents.Num() > 0)
	{
		for (auto MontageBlendingOutEvent : QueuedMontageBlendingOutEvents)
		{
			TriggerMontageBlendingOutEvent(MontageBlendingOutEvent);
		}
		QueuedMontageBlendingOutEvents.Reset();
	}

	if (QueuedMontageEndedEvents.Num() > 0)
	{
		for (auto MontageEndedEvent : QueuedMontageEndedEvents)
		{
			TriggerMontageEndedEvent(MontageEndedEvent);
		}
		QueuedMontageEndedEvents.Reset();
	}
}

float UAnimInstance::PlaySlotAnimation(UAnimSequenceBase* Asset, FName SlotNodeName, float BlendInTime, float BlendOutTime, float InPlayRate, int32 LoopCount)
{
	// create temporary montage and play
	bool bValidAsset = Asset && !Asset->IsA(UAnimMontage::StaticClass());
	if (!bValidAsset)
	{
		// user warning
		UE_LOG(LogAnimMontage, Warning, TEXT("Invalid Asset. If Montage, use Montage_Play"));
		return 0.f;
	}

	if (SlotNodeName == NAME_None)
	{
		// user warning
		UE_LOG(LogAnimMontage, Warning, TEXT("SlotNode Name is required. Make sure to add Slot Node in your anim graph and name it."));
		return 0.f;
	}

	USkeleton* AssetSkeleton = Asset->GetSkeleton();
	if (!CurrentSkeleton->IsCompatible(AssetSkeleton))
	{
		UE_LOG(LogAnimMontage, Warning, TEXT("The Skeleton isn't compatible"));
		return 0.f;
	}

	// now play
	UAnimMontage* NewMontage = NewObject<UAnimMontage>();
	NewMontage->SetSkeleton(AssetSkeleton);

	// add new track
	FSlotAnimationTrack NewTrack;
	NewTrack.SlotName = SlotNodeName;
	FAnimSegment NewSegment;
	NewSegment.AnimReference = Asset;
	NewSegment.AnimStartTime = 0.f;
	NewSegment.AnimEndTime = Asset->SequenceLength;
	NewSegment.AnimPlayRate = 1.f;
	NewSegment.StartPos = 0.f;
	NewSegment.LoopingCount = LoopCount;
	NewMontage->SequenceLength = NewSegment.GetLength();
	NewTrack.AnimTrack.AnimSegments.Add(NewSegment);
		
	FCompositeSection NewSection;
	NewSection.SectionName = TEXT("Default");
	NewSection.SetTime(0.0f);

	// add new section
	NewMontage->CompositeSections.Add(NewSection);
	NewMontage->BlendIn.SetBlendTime(BlendInTime);
	NewMontage->BlendOut.SetBlendTime(BlendOutTime);
	NewMontage->SlotAnimTracks.Add(NewTrack);

	return Montage_Play(NewMontage, InPlayRate);
}

UAnimMontage* UAnimInstance::PlaySlotAnimationAsDynamicMontage(UAnimSequenceBase* Asset, FName SlotNodeName, float BlendInTime, float BlendOutTime, float InPlayRate, int32 LoopCount, float BlendOutTriggerTime)
{
	// create temporary montage and play
	bool bValidAsset = Asset && !Asset->IsA(UAnimMontage::StaticClass());
	if (!bValidAsset)
	{
		// user warning
		UE_LOG(LogAnimMontage, Warning, TEXT("Invalid Asset. If Montage, use Montage_Play"));
		return NULL;
	}

	if (SlotNodeName == NAME_None)
	{
		// user warning
		UE_LOG(LogAnimMontage, Warning, TEXT("SlotNode Name is required. Make sure to add Slot Node in your anim graph and name it."));
		return NULL;
	}

	USkeleton* AssetSkeleton = Asset->GetSkeleton();
	if (!CurrentSkeleton->IsCompatible(AssetSkeleton))
	{
		UE_LOG(LogAnimMontage, Warning, TEXT("The Skeleton isn't compatible"));
		return NULL;
	}

	// now play
	UAnimMontage* NewMontage = NewObject<UAnimMontage>();
	NewMontage->SetSkeleton(AssetSkeleton);

	// add new track
	FSlotAnimationTrack NewTrack;
	NewTrack.SlotName = SlotNodeName;
	FAnimSegment NewSegment;
	NewSegment.AnimReference = Asset;
	NewSegment.AnimStartTime = 0.f;
	NewSegment.AnimEndTime = Asset->SequenceLength;
	NewSegment.AnimPlayRate = 1.f;
	NewSegment.StartPos = 0.f;
	NewSegment.LoopingCount = LoopCount;
	NewMontage->SequenceLength = NewSegment.GetLength();
	NewTrack.AnimTrack.AnimSegments.Add(NewSegment);

	FCompositeSection NewSection;
	NewSection.SectionName = TEXT("Default");
	NewSection.SetTime(0.0f);

	// add new section
	NewMontage->CompositeSections.Add(NewSection);
	NewMontage->BlendIn.SetBlendTime(BlendInTime);
	NewMontage->BlendOut.SetBlendTime(BlendOutTime);
	NewMontage->BlendOutTriggerTime = BlendOutTriggerTime;
	NewMontage->SlotAnimTracks.Add(NewTrack);

	// if playing is successful, return the montage to allow more control if needed
	float PlayTime = Montage_Play(NewMontage, InPlayRate);
	return PlayTime > 0.0f ? NewMontage : NULL;
}

void UAnimInstance::StopSlotAnimation(float InBlendOutTime, FName SlotNodeName)
{
	// stop temporary montage
	// when terminate (in the Montage_Advance), we have to lose reference to the temporary montage
	if (SlotNodeName != NAME_None)
	{
		for (int32 InstanceIndex = 0; InstanceIndex < MontageInstances.Num(); InstanceIndex++)
		{
			// check if this is playing
			FAnimMontageInstance* MontageInstance = MontageInstances[InstanceIndex];
			// make sure what is active right now is transient that we created by request
			if (MontageInstance && MontageInstance->IsActive() && MontageInstance->IsPlaying())
			{
				UAnimMontage* CurMontage = MontageInstance->Montage;
				if (CurMontage && CurMontage->GetOuter() == GetTransientPackage())
				{
					// Check each track, in practice there should only be one on these
					for (int32 SlotTrackIndex = 0; SlotTrackIndex < CurMontage->SlotAnimTracks.Num(); SlotTrackIndex++)
					{
						const FSlotAnimationTrack* AnimTrack = &CurMontage->SlotAnimTracks[SlotTrackIndex];
						if (AnimTrack && AnimTrack->SlotName == SlotNodeName)
						{
							// Found it
							MontageInstance->Stop(FAlphaBlend(InBlendOutTime));
							break;
						}
					}
				}
			}
		}
	}
	else
	{
		// Stop all
		Montage_Stop(InBlendOutTime);
	}
}

bool UAnimInstance::IsPlayingSlotAnimation(UAnimSequenceBase* Asset, FName SlotNodeName )
{
	for (int32 InstanceIndex = 0; InstanceIndex < MontageInstances.Num(); InstanceIndex++)
	{
		// check if this is playing
		FAnimMontageInstance* MontageInstance = MontageInstances[InstanceIndex];
		// make sure what is active right now is transient that we created by request
		if (MontageInstance && MontageInstance->IsActive() && MontageInstance->IsPlaying())
		{
			UAnimMontage* CurMontage = MontageInstance->Montage;
			if (CurMontage && CurMontage->GetOuter() == GetTransientPackage())
			{
				const FAnimTrack* AnimTrack = CurMontage->GetAnimationData(SlotNodeName);
				if (AnimTrack && AnimTrack->AnimSegments.Num() == 1)
				{
					// find if the 
					return (AnimTrack->AnimSegments[0].AnimReference == Asset);
				}
			}
		}
	}

	return false;
}

/** Play a Montage. Returns Length of Montage in seconds. Returns 0.f if failed to play. */
float UAnimInstance::Montage_Play(UAnimMontage* MontageToPlay, float InPlayRate/*= 1.f*/)
{
	if (MontageToPlay && (MontageToPlay->SequenceLength > 0.f) && MontageToPlay->HasValidSlotSetup())
	{
		if (CurrentSkeleton->IsCompatible(MontageToPlay->GetSkeleton()))
		{
			// Enforce 'a single montage at once per group' rule
			FName NewMontageGroupName = MontageToPlay->GetGroupName();
			StopAllMontagesByGroupName(NewMontageGroupName, MontageToPlay->BlendIn);

			// Enforce 'a single root motion montage at once' rule.
			if (MontageToPlay->bEnableRootMotionTranslation || MontageToPlay->bEnableRootMotionRotation)
			{
				FAnimMontageInstance* ActiveRootMotionMontageInstance = GetRootMotionMontageInstance();
				if (ActiveRootMotionMontageInstance)
				{
					ActiveRootMotionMontageInstance->Stop(MontageToPlay->BlendIn);
				}
			}

			FAnimMontageInstance* NewInstance = new FAnimMontageInstance(this);
			check(NewInstance);

			NewInstance->Initialize(MontageToPlay);
			NewInstance->Play(InPlayRate);
			MontageInstances.Add(NewInstance);
			ActiveMontagesMap.Add(MontageToPlay, NewInstance);

			// If we are playing root motion, set this instance as the one providing root motion.
			if (MontageToPlay->HasRootMotion())
			{
				RootMotionMontageInstance = NewInstance;
			}

			OnMontageStarted.Broadcast(MontageToPlay);

			UE_LOG(LogAnimMontage, Verbose, TEXT("Montage_Play: AnimMontage: %s,  (DesiredWeight:%0.2f, Weight:%0.2f)"),
						*NewInstance->Montage->GetName(), NewInstance->GetDesiredWeight(), NewInstance->GetWeight());
			return NewInstance->Montage->SequenceLength;
		}
		else
		{
			UE_LOG(LogAnimMontage, Warning, TEXT("Playing a Montage (%s) for the wrong Skeleton (%s) instead of (%s)."),
				*GetNameSafe(MontageToPlay), *GetNameSafe(CurrentSkeleton), *GetNameSafe(MontageToPlay->GetSkeleton()));
		}
	}

	return 0.f;
}

void UAnimInstance::Montage_Stop(float InBlendOutTime, UAnimMontage* Montage)
{
	if (Montage)
	{
		FAnimMontageInstance* MontageInstance = GetActiveInstanceForMontage(*Montage);
		if (MontageInstance)
		{
			MontageInstance->Stop(FAlphaBlend(Montage->BlendOut, InBlendOutTime));
		}
	}
	else
	{
		// If no Montage reference, do it on all active ones.
		for (int32 InstanceIndex = 0; InstanceIndex < MontageInstances.Num(); InstanceIndex++)
		{
			FAnimMontageInstance* MontageInstance = MontageInstances[InstanceIndex];
			if (MontageInstance && MontageInstance->IsActive())
			{
				MontageInstance->Stop(FAlphaBlend(MontageInstance->Montage->BlendOut, InBlendOutTime));
			}
		}
	}
}

void UAnimInstance::Montage_Pause(UAnimMontage* Montage)
{
	if (Montage)
	{
		FAnimMontageInstance* MontageInstance = GetActiveInstanceForMontage(*Montage);
		if (MontageInstance)
		{
			MontageInstance->Pause();
		}
	}
	else
	{
		// If no Montage reference, do it on all active ones.
		for (int32 InstanceIndex = 0; InstanceIndex < MontageInstances.Num(); InstanceIndex++)
		{
			FAnimMontageInstance* MontageInstance = MontageInstances[InstanceIndex];
			if (MontageInstance && MontageInstance->IsActive())
			{
				MontageInstance->Pause();
			}
		}
	}
}

void UAnimInstance::Montage_JumpToSection(FName SectionName, UAnimMontage* Montage)
{
	if (Montage)
	{
		FAnimMontageInstance* MontageInstance = GetActiveInstanceForMontage(*Montage);
		if (MontageInstance)
		{
			bool const bEndOfSection = (MontageInstance->GetPlayRate() < 0.f);
			MontageInstance->JumpToSectionName(SectionName, bEndOfSection);
		}
	}
	else
	{
		// If no Montage reference, do it on all active ones.
		for (int32 InstanceIndex = 0; InstanceIndex < MontageInstances.Num(); InstanceIndex++)
		{
			FAnimMontageInstance* MontageInstance = MontageInstances[InstanceIndex];
			if (MontageInstance && MontageInstance->IsActive())
			{
				bool const bEndOfSection = (MontageInstance->GetPlayRate() < 0.f);
				MontageInstance->JumpToSectionName(SectionName, bEndOfSection);
			}
		}
	}
}

void UAnimInstance::Montage_JumpToSectionsEnd(FName SectionName, UAnimMontage* Montage)
{
	if (Montage)
	{
		FAnimMontageInstance* MontageInstance = GetActiveInstanceForMontage(*Montage);
		if (MontageInstance)
		{
			bool const bEndOfSection = (MontageInstance->GetPlayRate() >= 0.f);
			MontageInstance->JumpToSectionName(SectionName, bEndOfSection);
		}
	}
	else
	{
		// If no Montage reference, do it on all active ones.
		for (int32 InstanceIndex = 0; InstanceIndex < MontageInstances.Num(); InstanceIndex++)
		{
			FAnimMontageInstance* MontageInstance = MontageInstances[InstanceIndex];
			if (MontageInstance && MontageInstance->IsActive())
			{
				bool const bEndOfSection = (MontageInstance->GetPlayRate() >= 0.f);
				MontageInstance->JumpToSectionName(SectionName, bEndOfSection);
			}
		}
	}
}

void UAnimInstance::Montage_SetNextSection(FName SectionNameToChange, FName NextSection, UAnimMontage* Montage)
{
	if (Montage)
	{
		FAnimMontageInstance* MontageInstance = GetActiveInstanceForMontage(*Montage);
		if (MontageInstance)
		{
			MontageInstance->SetNextSectionName(SectionNameToChange, NextSection);
		}
	}
	else
	{
		bool bFoundOne = false;

		// If no Montage reference, do it on all active ones.
		for (int32 InstanceIndex = 0; InstanceIndex < MontageInstances.Num(); InstanceIndex++)
		{
			FAnimMontageInstance* MontageInstance = MontageInstances[InstanceIndex];
			if (MontageInstance && MontageInstance->IsActive())
			{
				MontageInstance->SetNextSectionName(SectionNameToChange, NextSection);
				bFoundOne = true;
			}
		}

		if (!bFoundOne)
		{
			bFoundOne = true;
		}
	}
}

void UAnimInstance::Montage_SetPlayRate(UAnimMontage* Montage, float NewPlayRate)
{
	if (Montage)
	{
		FAnimMontageInstance* MontageInstance = GetActiveInstanceForMontage(*Montage);
		if (MontageInstance)
		{
			MontageInstance->SetPlayRate(NewPlayRate);
		}
	}
	else
	{
		// If no Montage reference, do it on all active ones.
		for (int32 InstanceIndex = 0; InstanceIndex < MontageInstances.Num(); InstanceIndex++)
		{
			FAnimMontageInstance* MontageInstance = MontageInstances[InstanceIndex];
			if (MontageInstance && MontageInstance->IsActive())
			{
				MontageInstance->SetPlayRate(NewPlayRate);
			}
		}
	}
}

bool UAnimInstance::Montage_IsActive(UAnimMontage* Montage)
{
	if (Montage)
	{
		FAnimMontageInstance* MontageInstance = GetActiveInstanceForMontage(*Montage);
		if (MontageInstance)
		{
			return true;
		}
	}
	else
	{
		// If no Montage reference, return true if there is any active montage.
		for (int32 InstanceIndex = 0; InstanceIndex < MontageInstances.Num(); InstanceIndex++)
		{
			FAnimMontageInstance* MontageInstance = MontageInstances[InstanceIndex];
			if (MontageInstance && MontageInstance->IsActive())
			{
				return true;
			}
		}
	}

	return false;
}

bool UAnimInstance::Montage_IsPlaying(UAnimMontage* Montage)
{
	if (Montage)
	{
		FAnimMontageInstance* MontageInstance = GetActiveInstanceForMontage(*Montage);
		if (MontageInstance)
		{
			return MontageInstance->IsPlaying();
		}
	}
	else
	{
		// If no Montage reference, return true if there is any active playing montage.
		for (int32 InstanceIndex = 0; InstanceIndex < MontageInstances.Num(); InstanceIndex++)
		{
			FAnimMontageInstance* MontageInstance = MontageInstances[InstanceIndex];
			if (MontageInstance && MontageInstance->IsActive() && MontageInstance->IsPlaying())
			{
				return true;
			}
		}
	}

	return false;
}

FName UAnimInstance::Montage_GetCurrentSection(UAnimMontage* Montage)
{
	if (Montage)
	{
		FAnimMontageInstance* MontageInstance = GetActiveInstanceForMontage(*Montage);
		if (MontageInstance)
		{
			return MontageInstance->GetCurrentSection();
		}
	}
	else
	{
		// If no Montage reference, get first active one.
		for (int32 InstanceIndex = 0; InstanceIndex < MontageInstances.Num(); InstanceIndex++)
		{
			FAnimMontageInstance* MontageInstance = MontageInstances[InstanceIndex];
			if (MontageInstance && MontageInstance->IsActive())
			{
				return MontageInstance->GetCurrentSection();
			}
		}
	}

	return NAME_None;
}

void UAnimInstance::Montage_SetEndDelegate(FOnMontageEnded& InOnMontageEnded, UAnimMontage* Montage)
{
	if (Montage)
	{
		FAnimMontageInstance* MontageInstance = GetActiveInstanceForMontage(*Montage);
		if (MontageInstance)
		{
			MontageInstance->OnMontageEnded = InOnMontageEnded;
		}
	}
	else
	{
		// If no Montage reference, do it on all active ones.
		for (int32 InstanceIndex = 0; InstanceIndex < MontageInstances.Num(); InstanceIndex++)
		{
			FAnimMontageInstance* MontageInstance = MontageInstances[InstanceIndex];
			if (MontageInstance && MontageInstance->IsActive())
			{
				MontageInstance->OnMontageEnded = InOnMontageEnded;
			}
		}
	}
}

void UAnimInstance::Montage_SetBlendingOutDelegate(FOnMontageBlendingOutStarted& InOnMontageBlendingOut, UAnimMontage* Montage)
{
	if (Montage)
	{
		FAnimMontageInstance* MontageInstance = GetActiveInstanceForMontage(*Montage);
		if (MontageInstance)
		{
			MontageInstance->OnMontageBlendingOutStarted = InOnMontageBlendingOut;
		}
	}
	else
	{
		// If no Montage reference, do it on all active ones.
		for (int32 InstanceIndex = 0; InstanceIndex < MontageInstances.Num(); InstanceIndex++)
		{
			FAnimMontageInstance* MontageInstance = MontageInstances[InstanceIndex];
			if (MontageInstance && MontageInstance->IsActive())
			{
				MontageInstance->OnMontageBlendingOutStarted = InOnMontageBlendingOut;
			}
		}
	}
}

FOnMontageBlendingOutStarted* UAnimInstance::Montage_GetBlendingOutDelegate(UAnimMontage* Montage)
{
	if (Montage)
	{
		FAnimMontageInstance* MontageInstance = GetActiveInstanceForMontage(*Montage);
		if (MontageInstance)
		{
			return &MontageInstance->OnMontageBlendingOutStarted;
		}
	}
	else
	{
		// If no Montage reference, use first active one found.
		for (int32 InstanceIndex = 0; InstanceIndex < MontageInstances.Num(); InstanceIndex++)
		{
			FAnimMontageInstance* MontageInstance = MontageInstances[InstanceIndex];
			if (MontageInstance && MontageInstance->IsActive())
			{
				return &MontageInstance->OnMontageBlendingOutStarted;
			}
		}
	}

	return NULL;
}

void UAnimInstance::Montage_SetPosition(UAnimMontage* Montage, float NewPosition)
{
	if (Montage)
	{
		FAnimMontageInstance* MontageInstance = GetActiveInstanceForMontage(*Montage);
		if (MontageInstance)
		{
			MontageInstance->SetPosition(NewPosition);
		}
	}
	else
	{
		// If no Montage reference, do it on all active ones.
		for (int32 InstanceIndex = 0; InstanceIndex < MontageInstances.Num(); InstanceIndex++)
		{
			FAnimMontageInstance* MontageInstance = MontageInstances[InstanceIndex];
			if (MontageInstance && MontageInstance->IsActive())
			{
				MontageInstance->SetPosition(NewPosition);
			}
		}
	}
}

float UAnimInstance::Montage_GetPosition(UAnimMontage* Montage)
{
	if (Montage)
	{
		FAnimMontageInstance* MontageInstance = GetActiveInstanceForMontage(*Montage);
		if (MontageInstance)
		{
			return MontageInstance->GetPosition();
		}
	}
	else
	{
		// If no Montage reference, use first active one found.
		for (int32 InstanceIndex = 0; InstanceIndex < MontageInstances.Num(); InstanceIndex++)
		{
			FAnimMontageInstance* MontageInstance = MontageInstances[InstanceIndex];
			if (MontageInstance && MontageInstance->IsActive())
			{
				return MontageInstance->GetPosition();
			}
		}
	}

	return 0.f;
}

bool UAnimInstance::Montage_GetIsStopped(UAnimMontage* Montage)
{
	if (Montage)
	{
		FAnimMontageInstance* MontageInstance = GetActiveInstanceForMontage(*Montage);
		return (!MontageInstance); // Not active == Stopped.
	}
	return true;
}

float UAnimInstance::Montage_GetBlendTime(UAnimMontage* Montage)
{
	if (Montage)
	{
		FAnimMontageInstance* MontageInstance = GetActiveInstanceForMontage(*Montage);
		if (MontageInstance)
		{
			return MontageInstance->GetBlendTime();
		}
	}
	else
	{
		// If no Montage reference, use first active one found.
		for (int32 InstanceIndex = 0; InstanceIndex < MontageInstances.Num(); InstanceIndex++)
		{
			FAnimMontageInstance* MontageInstance = MontageInstances[InstanceIndex];
			if (MontageInstance && MontageInstance->IsActive())
			{
				return MontageInstance->GetBlendTime();
			}
		}
	}

	return 0.f;
}

float UAnimInstance::Montage_GetPlayRate(UAnimMontage* Montage)
{
	if (Montage)
	{
		FAnimMontageInstance* MontageInstance = GetActiveInstanceForMontage(*Montage);
		if (MontageInstance)
		{
			return MontageInstance->GetPlayRate();
		}
	}
	else
	{
		// If no Montage reference, use first active one found.
		for (int32 InstanceIndex = 0; InstanceIndex < MontageInstances.Num(); InstanceIndex++)
		{
			FAnimMontageInstance* MontageInstance = MontageInstances[InstanceIndex];
			if (MontageInstance && MontageInstance->IsActive())
			{
				return MontageInstance->GetPlayRate();
			}
		}
	}

	return 0.f;
}

int32 UAnimInstance::Montage_GetNextSectionID(UAnimMontage const* const Montage, int32 const& CurrentSectionID) const
{
	if (Montage)
	{
		FAnimMontageInstance* MontageInstance = GetActiveInstanceForMontage(*Montage);
		if (MontageInstance)
		{
			return MontageInstance->GetNextSectionID(CurrentSectionID);
		}
	}
	else
	{
		// If no Montage reference, use first active one found.
		for (int32 InstanceIndex = 0; InstanceIndex < MontageInstances.Num(); InstanceIndex++)
		{
			FAnimMontageInstance* MontageInstance = MontageInstances[InstanceIndex];
			if (MontageInstance && MontageInstance->IsActive())
			{
				return MontageInstance->GetNextSectionID(CurrentSectionID);
			}
		}
	}

	return INDEX_NONE;
}

bool UAnimInstance::IsAnyMontagePlaying() const
{
	return (MontageInstances.Num() > 0);
}

UAnimMontage* UAnimInstance::GetCurrentActiveMontage()
{
	// Start from end, as most recent instances are added at the end of the queue.
	int32 const NumInstances = MontageInstances.Num();
	for (int32 InstanceIndex = NumInstances - 1; InstanceIndex >= 0; InstanceIndex--)
	{
		FAnimMontageInstance* MontageInstance = MontageInstances[InstanceIndex];
		if (MontageInstance && MontageInstance->IsActive())
		{
			return MontageInstance->Montage;
		}
	}

	return NULL;
}

FAnimMontageInstance* UAnimInstance::GetActiveMontageInstance()
{
	// Start from end, as most recent instances are added at the end of the queue.
	int32 const NumInstances = MontageInstances.Num();
	for (int32 InstanceIndex = NumInstances - 1; InstanceIndex >= 0; InstanceIndex--)
	{
		FAnimMontageInstance* MontageInstance = MontageInstances[InstanceIndex];
		if (MontageInstance && MontageInstance->IsActive())
		{
			return MontageInstance;
		}
	}

	return NULL;
}

void UAnimInstance::StopAllMontages(float BlendOut)
{
	for (int32 Index = MontageInstances.Num() - 1; Index >= 0; Index--)
	{
		MontageInstances[Index]->Stop(FAlphaBlend(BlendOut), true);
	}
}

void UAnimInstance::StopAllMontagesByGroupName(FName InGroupName, const FAlphaBlend& BlendOut)
{
	for (int32 InstanceIndex = MontageInstances.Num() - 1; InstanceIndex >= 0; InstanceIndex--)
	{
		FAnimMontageInstance* MontageInstance = MontageInstances[InstanceIndex];
		if (MontageInstance && MontageInstance->IsActive() && MontageInstance->Montage && (MontageInstance->Montage->GetGroupName() == InGroupName))
		{
			MontageInstances[InstanceIndex]->Stop(BlendOut, true);
		}
	}
}

void UAnimInstance::OnMontageInstanceStopped(FAnimMontageInstance& StoppedMontageInstance)
{
	UAnimMontage* MontageStopped = StoppedMontageInstance.Montage;
	ensure(MontageStopped != NULL);

	// Remove instance for Active List.
	FAnimMontageInstance** AnimInstancePtr = ActiveMontagesMap.Find(MontageStopped);
	if (AnimInstancePtr && (*AnimInstancePtr == &StoppedMontageInstance))
	{
		ActiveMontagesMap.Remove(MontageStopped);
	}

	// Clear RootMotionMontageInstance
	if (RootMotionMontageInstance == &StoppedMontageInstance)
	{
		RootMotionMontageInstance = NULL;
	}
}

FAnimMontageInstance* UAnimInstance::GetActiveInstanceForMontage(UAnimMontage const& Montage) const
{
	FAnimMontageInstance* const* FoundInstancePtr = ActiveMontagesMap.Find(&Montage);
	return FoundInstancePtr ? *FoundInstancePtr : NULL;
}

FAnimMontageInstance* UAnimInstance::GetRootMotionMontageInstance() const
{
	return RootMotionMontageInstance;
}

FRootMotionMovementParams UAnimInstance::ConsumeExtractedRootMotion(float Alpha)
{
	if (Alpha < ZERO_ANIMWEIGHT_THRESH)
	{
		return FRootMotionMovementParams();
	}
	else if (Alpha > (1.f - ZERO_ANIMWEIGHT_THRESH))
	{
		FRootMotionMovementParams RootMotion = ExtractedRootMotion;
		ExtractedRootMotion.Clear();
		return RootMotion;
	}
	else
	{
		return ExtractedRootMotion.ConsumeRootMotion(Alpha);
	}
}

void UAnimInstance::SetMorphTarget(FName MorphTargetName, float Value)
{
	USkeletalMeshComponent* Component = GetOwningComponent();
	if (Component)
	{
		Component->SetMorphTarget(MorphTargetName, Value);
	}
}

void UAnimInstance::ClearMorphTargets()
{
	USkeletalMeshComponent* Component = GetOwningComponent();
	if (Component)
	{
		Component->ClearMorphTargets();
	}
}

float UAnimInstance::CalculateDirection(const FVector& Velocity, const FRotator& BaseRotation)
{
	FMatrix RotMatrix = FRotationMatrix(BaseRotation);
	FVector ForwardVector = RotMatrix.GetScaledAxis(EAxis::X);
	FVector RightVector = RotMatrix.GetScaledAxis(EAxis::Y);
	FVector NormalizedVel = Velocity.GetSafeNormal();
	ForwardVector.Z = RightVector.Z = NormalizedVel.Z = 0.f;

	// get a cos(alpha) of forward vector vs velocity
	float ForwardCosAngle = FVector::DotProduct(ForwardVector, NormalizedVel);
	// now get the alpha and convert to degree
	float ForwardDeltaDegree = FMath::RadiansToDegrees( FMath::Acos(ForwardCosAngle) );

	// depending on where right vector is, flip it
	float RightCosAngle = FVector::DotProduct(RightVector, NormalizedVel);
	if ( RightCosAngle < 0 )
	{
		ForwardDeltaDegree *= -1;
	}

	return ForwardDeltaDegree;
}

void UAnimInstance::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UAnimInstance* This = CastChecked<UAnimInstance>(InThis);
	if (This)
	{
		// go through all montage instances, and update them
		// and make sure their weight is updated properly
		for (int32 I=0; I<This->MontageInstances.Num(); ++I)
		{
			if( This->MontageInstances[I] )
			{
				This->MontageInstances[I]->AddReferencedObjects(Collector);
			}
		}
	}

	Super::AddReferencedObjects(This, Collector);
}
// 
void UAnimInstance::LockAIResources(bool bLockMovement, bool LockAILogic)
{
	UE_LOG(LogAnimation, Error, TEXT("%s: LockAIResources is no longer supported. Please use LockAIResourcesWithAnimation instead."), *GetName());	
}

void UAnimInstance::UnlockAIResources(bool bUnlockMovement, bool UnlockAILogic)
{
	UE_LOG(LogAnimation, Error, TEXT("%s: UnlockAIResources is no longer supported. Please use UnlockAIResourcesWithAnimation instead."), *GetName());
}

bool UAnimInstance::GetTimeToClosestMarker(FName SyncGroup, FName MarkerName, float& OutMarkerTime) const
{
	const int32 SyncGroupIndex = GetSyncGroupIndexFromName(SyncGroup);
	const TArray<FAnimGroupInstance>& SyncGroups = SyncGroupArrays[GetSyncGroupReadIndex()];

	if (SyncGroups.IsValidIndex(SyncGroupIndex))
	{
		const FAnimGroupInstance& SyncGroupInstance = SyncGroups[SyncGroupIndex];
		if (SyncGroupInstance.bCanUseMarkerSync && SyncGroupInstance.ActivePlayers.IsValidIndex(SyncGroupInstance.GroupLeaderIndex))
		{
			const FMarkerSyncAnimPosition& EndPosition = SyncGroupInstance.MarkerTickContext.GetMarkerSyncEndPosition();
			const FAnimTickRecord& Leader = SyncGroupInstance.ActivePlayers[SyncGroupInstance.GroupLeaderIndex];
			if (EndPosition.PreviousMarkerName == MarkerName)
			{
				OutMarkerTime = Leader.MarkerTickRecord->PreviousMarker.TimeToMarker;
				return true;
			}
			else if (EndPosition.NextMarkerName == MarkerName)
			{
				OutMarkerTime = Leader.MarkerTickRecord->NextMarker.TimeToMarker;
				return true;
			}
		}
	}
	return false;
}

bool UAnimInstance::HasMarkerBeenHitThisFrame(FName SyncGroup, FName MarkerName) const
{
	const int32 SyncGroupIndex = GetSyncGroupIndexFromName(SyncGroup);
	const TArray<FAnimGroupInstance>& SyncGroups = SyncGroupArrays[GetSyncGroupReadIndex()];

	if (SyncGroups.IsValidIndex(SyncGroupIndex))
	{
		const FAnimGroupInstance& SyncGroupInstance = SyncGroups[SyncGroupIndex];
		if (SyncGroupInstance.bCanUseMarkerSync)
		{
			return SyncGroupInstance.MarkerTickContext.MarkersPassedThisTick.ContainsByPredicate([&MarkerName](const FPassedMarker& PassedMarker) -> bool
			{
				return PassedMarker.PassedMarkerName == MarkerName;
			});
		}
	}
	return false;
}

bool UAnimInstance::IsSyncGroupBetweenMarkers(FName InSyncGroupName, FName PreviousMarker, FName NextMarker, bool bRespectMarkerOrder) const
{
	const FMarkerSyncAnimPosition& SyncGroupPosition = GetSyncGroupPosition(InSyncGroupName);
	if ((SyncGroupPosition.PreviousMarkerName == PreviousMarker) && (SyncGroupPosition.NextMarkerName == NextMarker))
	{
		return true;
	}

	if (!bRespectMarkerOrder)
	{
		return ((SyncGroupPosition.PreviousMarkerName == NextMarker) && (SyncGroupPosition.NextMarkerName == PreviousMarker));
	}

	return false;
}

FMarkerSyncAnimPosition UAnimInstance::GetSyncGroupPosition(FName InSyncGroupName) const
{
	const int32 SyncGroupIndex = GetSyncGroupIndexFromName(InSyncGroupName);
	const TArray<FAnimGroupInstance>& SyncGroups = SyncGroupArrays[GetSyncGroupReadIndex()];

	if (SyncGroups.IsValidIndex(SyncGroupIndex))
	{
		const FAnimGroupInstance& SyncGroupInstance = SyncGroups[SyncGroupIndex];
		if (SyncGroupInstance.bCanUseMarkerSync)
		{
			return SyncGroupInstance.MarkerTickContext.GetMarkerSyncEndPosition();
		}
	}

	return FMarkerSyncAnimPosition();
}

void UAnimInstance::UpdateMontageEvaluationData()
{
	MontageEvaluationData.Reset(MontageInstances.Num());
	UE_LOG(LogAnimMontage, Verbose, TEXT("UpdateMontageEvaluationData Strting: Owner: %s"),	*GetNameSafe(GetOwningActor()));

	for (FAnimMontageInstance* MontageInstance : MontageInstances)
	{
		// although montage can advance with 0.f weight, it is fine to filter by weight here
		// because we don't want to evaluate them if 0 weight
		if (MontageInstance->Montage && MontageInstance->GetWeight() > ZERO_ANIMWEIGHT_THRESH)
		{
			UE_LOG(LogAnimMontage, Verbose, TEXT("UpdateMontageEvaluationData : AnimMontage: %s,  (DesiredWeight:%0.2f, Weight:%0.2f)"),
						*MontageInstance->Montage->GetName(), MontageInstance->GetDesiredWeight(), MontageInstance->GetWeight());
			MontageEvaluationData.Add(FMontageEvaluationState(MontageInstance->Montage, MontageInstance->GetWeight(), MontageInstance->GetDesiredWeight(), MontageInstance->GetPosition()));
		}
	}
}

float UAnimInstance::GetInstanceAssetPlayerLength(int32 AssetPlayerIndex)
{
	if(FAnimNode_AssetPlayerBase* PlayerNode = GetNodeFromIndex<FAnimNode_AssetPlayerBase>(AssetPlayerIndex))
	{
		return PlayerNode->GetAnimAsset()->GetMaxCurrentTime();
	}

	return 0.0f;
}

float UAnimInstance::GetInstanceAssetPlayerTime(int32 AssetPlayerIndex)
{
	if(FAnimNode_AssetPlayerBase* PlayerNode = GetNodeFromIndex<FAnimNode_AssetPlayerBase>(AssetPlayerIndex))
	{
		return PlayerNode->GetAccumulatedTime();
	}

	return 0.0f;
}

float UAnimInstance::GetInstanceAssetPlayerTimeFraction(int32 AssetPlayerIndex)
{
	if(FAnimNode_AssetPlayerBase* PlayerNode = GetNodeFromIndex<FAnimNode_AssetPlayerBase>(AssetPlayerIndex))
	{
		UAnimationAsset* Asset = PlayerNode->GetAnimAsset();

		float Length = Asset ? Asset->GetMaxCurrentTime() : 0.0f;

		if(Length > 0.0f)
		{
			return PlayerNode->GetAccumulatedTime() / Length;
		}
	}

	

	return 0.0f;
}

float UAnimInstance::GetInstanceAssetPlayerTimeFromEndFraction(int32 AssetPlayerIndex)
{
	if(FAnimNode_AssetPlayerBase* PlayerNode = GetNodeFromIndex<FAnimNode_AssetPlayerBase>(AssetPlayerIndex))
	{
		UAnimationAsset* Asset = PlayerNode->GetAnimAsset();
		
		float Length = Asset ? Asset->GetMaxCurrentTime() : 0.f;

		if(Length > 0.f)
		{
			return (Length - PlayerNode->GetAccumulatedTime()) / Length;
		}
	}

	return 1.0f;
}

float UAnimInstance::GetInstanceAssetPlayerTimeFromEnd(int32 AssetPlayerIndex)
{
	if(FAnimNode_AssetPlayerBase* PlayerNode = GetNodeFromIndex<FAnimNode_AssetPlayerBase>(AssetPlayerIndex))
	{
		UAnimationAsset* Asset = PlayerNode->GetAnimAsset();
		if(Asset)
		{
			return PlayerNode->GetAnimAsset()->GetMaxCurrentTime() - PlayerNode->GetAccumulatedTime();
		}
	}

	return MAX_flt;
}

float UAnimInstance::GetInstanceStateWeight(int32 MachineIndex, int32 StateIndex)
{
	if(FAnimNode_StateMachine* MachineInstance = GetStateMachineInstance(MachineIndex))
	{
		return MachineInstance->GetStateWeight(StateIndex);
	}
	
	return 0.0f;
}

float UAnimInstance::GetInstanceCurrentStateElapsedTime(int32 MachineIndex)
{
	if(FAnimNode_StateMachine* MachineInstance = GetStateMachineInstance(MachineIndex))
	{
		return MachineInstance->GetCurrentStateElapsedTime();
	}

	return 0.0f;
}

float UAnimInstance::GetInstanceTransitionCrossfadeDuration(int32 MachineIndex, int32 TransitionIndex)
{
	if(FAnimNode_StateMachine* MachineInstance = GetStateMachineInstance(MachineIndex))
	{
		if(MachineInstance->IsValidTransitionIndex(TransitionIndex))
		{
			return MachineInstance->GetTransitionInfo(TransitionIndex).CrossfadeDuration;
		}
	}

	return 0.0f;
}

float UAnimInstance::GetInstanceTransitionTimeElapsed(int32 MachineIndex, int32 TransitionIndex)
{
	// Just an alias for readability in the anim graph
	return GetInstanceCurrentStateElapsedTime(MachineIndex);
}

float UAnimInstance::GetInstanceTransitionTimeElapsedFraction(int32 MachineIndex, int32 TransitionIndex)
{
	if(FAnimNode_StateMachine* MachineInstance = GetStateMachineInstance(MachineIndex))
	{
		if(MachineInstance->IsValidTransitionIndex(TransitionIndex))
		{
			return MachineInstance->GetCurrentStateElapsedTime() / MachineInstance->GetTransitionInfo(TransitionIndex).CrossfadeDuration;
		}
	}

	return 0.0f;
}

float UAnimInstance::GetRelevantAnimTimeRemaining(int32 MachineIndex, int32 StateIndex)
{
	if(FAnimNode_AssetPlayerBase* AssetPlayer = GetRelevantAssetPlayerFromState(MachineIndex, StateIndex))
	{
		if(AssetPlayer->GetAnimAsset())
		{
			return AssetPlayer->GetAnimAsset()->GetMaxCurrentTime() - AssetPlayer->GetAccumulatedTime();
		}
	}

	return MAX_flt;
}

float UAnimInstance::GetRelevantAnimTimeRemainingFraction(int32 MachineIndex, int32 StateIndex)
{
	if(FAnimNode_AssetPlayerBase* AssetPlayer = GetRelevantAssetPlayerFromState(MachineIndex, StateIndex))
	{
		if(AssetPlayer->GetAnimAsset())
		{
			float Length = AssetPlayer->GetAnimAsset()->GetMaxCurrentTime();
			if(Length > 0.0f)
			{
				return (Length - AssetPlayer->GetAccumulatedTime()) / Length;
			}
		}
	}

	return 1.0f;
}

float UAnimInstance::GetRelevantAnimLength(int32 MachineIndex, int32 StateIndex)
{
	if(FAnimNode_AssetPlayerBase* AssetPlayer = GetRelevantAssetPlayerFromState(MachineIndex, StateIndex))
	{
		if(AssetPlayer->GetAnimAsset())
		{
			return AssetPlayer->GetAnimAsset()->GetMaxCurrentTime();
		}
	}

	return 0.0f;
}

float UAnimInstance::GetRelevantAnimTime(int32 MachineIndex, int32 StateIndex)
{
	if(FAnimNode_AssetPlayerBase* AssetPlayer = GetRelevantAssetPlayerFromState(MachineIndex, StateIndex))
	{
		return AssetPlayer->GetAccumulatedTime();
	}

	return 0.0f;
}

float UAnimInstance::GetRelevantAnimTimeFraction(int32 MachineIndex, int32 StateIndex)
{
	if(FAnimNode_AssetPlayerBase* AssetPlayer = GetRelevantAssetPlayerFromState(MachineIndex, StateIndex))
	{
		float Length = AssetPlayer->GetAnimAsset()->GetMaxCurrentTime();
		if(Length > 0.0f)
		{
			return AssetPlayer->GetAccumulatedTime() / Length;
		}
	}

	return 0.0f;
}

FAnimNode_StateMachine* UAnimInstance::GetStateMachineInstance(int32 MachineIndex)
{
	if(UAnimBlueprintGeneratedClass* AnimBlueprintClass = Cast<UAnimBlueprintGeneratedClass>((UObject*)GetClass()))
	{
		if((MachineIndex >= 0) && (MachineIndex < AnimBlueprintClass->AnimNodeProperties.Num()))
		{
			const int32 InstancePropertyIndex = AnimBlueprintClass->AnimNodeProperties.Num() - 1 - MachineIndex;

			UStructProperty* MachineInstanceProperty = AnimBlueprintClass->AnimNodeProperties[InstancePropertyIndex];
			checkSlow(MachineInstanceProperty->Struct->IsChildOf(FAnimNode_StateMachine::StaticStruct()));

			return MachineInstanceProperty->ContainerPtrToValuePtr<FAnimNode_StateMachine>(this);
		}
	}

	return nullptr;
}

FAnimNode_StateMachine* UAnimInstance::GetStateMachineInstanceFromName(FName MachineName)
{
	if (UAnimBlueprintGeneratedClass* AnimBlueprintClass = Cast<UAnimBlueprintGeneratedClass>((UObject*)GetClass()))
	{
		for (int32 MachineIndex = 0; MachineIndex < AnimBlueprintClass->AnimNodeProperties.Num(); MachineIndex++)
		{
			UStructProperty* Property = AnimBlueprintClass->AnimNodeProperties[AnimBlueprintClass->AnimNodeProperties.Num() - 1 - MachineIndex];
			if (Property && Property->Struct == FAnimNode_StateMachine::StaticStruct())
			{
				FAnimNode_StateMachine* StateMachine = Property->ContainerPtrToValuePtr<FAnimNode_StateMachine>(this);
				if (StateMachine)
				{
					if (const FBakedAnimationStateMachine* MachineDescription = GetMachineDescription(AnimBlueprintClass, StateMachine))
					{
						if (MachineDescription->MachineName == MachineName)
						{
							return StateMachine;
						}
					}
				}
			}
		}
	}

	return nullptr;
}

const FBakedAnimationStateMachine* UAnimInstance::GetStateMachineInstanceDesc(FName MachineName)
{
	if(UAnimBlueprintGeneratedClass* AnimBlueprintClass = Cast<UAnimBlueprintGeneratedClass>((UObject*)GetClass()))
	{
		for(int32 MachineIndex = 0; MachineIndex < AnimBlueprintClass->AnimNodeProperties.Num(); MachineIndex++)
		{
			UStructProperty* Property = AnimBlueprintClass->AnimNodeProperties[AnimBlueprintClass->AnimNodeProperties.Num() - 1 - MachineIndex];
			if(Property && Property->Struct == FAnimNode_StateMachine::StaticStruct())
			{
				FAnimNode_StateMachine* StateMachine = Property->ContainerPtrToValuePtr<FAnimNode_StateMachine>(this);
				if(StateMachine)
				{
					if(const FBakedAnimationStateMachine* MachineDescription = GetMachineDescription(AnimBlueprintClass, StateMachine))
					{
						if(MachineDescription->MachineName == MachineName)
						{
							return MachineDescription;
						}
					}
				}
			}
		}
	}

	return nullptr;
}

int32 UAnimInstance::GetStateMachineIndex(FName MachineName)
{
	if(UAnimBlueprintGeneratedClass* AnimBlueprintClass = Cast<UAnimBlueprintGeneratedClass>((UObject*)GetClass()))
	{
		for(int32 MachineIndex = 0; MachineIndex < AnimBlueprintClass->AnimNodeProperties.Num(); MachineIndex++)
		{
			UStructProperty* Property = AnimBlueprintClass->AnimNodeProperties[AnimBlueprintClass->AnimNodeProperties.Num() - 1 - MachineIndex];
			if(Property && Property->Struct == FAnimNode_StateMachine::StaticStruct())
			{
				FAnimNode_StateMachine* StateMachine = Property->ContainerPtrToValuePtr<FAnimNode_StateMachine>(this);
				if(StateMachine)
				{
					if(const FBakedAnimationStateMachine* MachineDescription = GetMachineDescription(AnimBlueprintClass, StateMachine))
					{
						if(MachineDescription->MachineName == MachineName)
						{
							return MachineIndex;
						}
					}
				}
			}
		}
	}

	return INDEX_NONE;
}

FAnimNode_Base* UAnimInstance::GetCheckedNodeFromIndexUntyped(int32 NodeIdx, UScriptStruct* RequiredStructType)
{
	FAnimNode_Base* NodePtr = nullptr;
	if (UAnimBlueprintGeneratedClass* AnimBlueprintClass = Cast<UAnimBlueprintGeneratedClass>(GetClass()))
	{
		const int32 InstanceIdx = AnimBlueprintClass->AnimNodeProperties.Num() - 1 - NodeIdx;

		if (AnimBlueprintClass->AnimNodeProperties.IsValidIndex(InstanceIdx))
		{
			UStructProperty* NodeProperty = AnimBlueprintClass->AnimNodeProperties[InstanceIdx];

			if (NodeProperty->Struct->IsChildOf(RequiredStructType))
			{
				NodePtr = NodeProperty->ContainerPtrToValuePtr<FAnimNode_Base>(this);
			}
			else
			{
				checkfSlow(false, TEXT("Requested a node of type %s but found node of type %s"), *RequiredStructType->GetName(), *NodeProperty->Struct->GetName());
			}
		}
		else
		{
			checkfSlow(false, TEXT("Requested node of type %s at index %d/%d, index out of bounds."), *RequiredStructType->GetName(), NodeIdx, InstanceIdx);
		}
	}

	checkfSlow(NodePtr, TEXT("Requested node at index %d not found!"), NodeIdx);

	return NodePtr;
}

FAnimNode_Base* UAnimInstance::GetNodeFromIndexUntyped(int32 NodeIdx, UScriptStruct* RequiredStructType)
{
	FAnimNode_Base* NodePtr = nullptr;
	if (UAnimBlueprintGeneratedClass* AnimBlueprintClass = Cast<UAnimBlueprintGeneratedClass>(GetClass()))
	{
		const int32 InstanceIdx = AnimBlueprintClass->AnimNodeProperties.Num() - 1 - NodeIdx;

		if (AnimBlueprintClass->AnimNodeProperties.IsValidIndex(InstanceIdx))
		{
			UStructProperty* NodeProperty = AnimBlueprintClass->AnimNodeProperties[InstanceIdx];

			if (NodeProperty->Struct->IsChildOf(RequiredStructType))
			{
				NodePtr = NodeProperty->ContainerPtrToValuePtr<FAnimNode_Base>(this);
			}
		}
	}

	return NodePtr;
}

const FBakedAnimationStateMachine* UAnimInstance::GetMachineDescription(UAnimBlueprintGeneratedClass* AnimBlueprintClass, FAnimNode_StateMachine* MachineInstance)
{
	return AnimBlueprintClass->BakedStateMachines.IsValidIndex(MachineInstance->StateMachineIndexInClass) ? &(AnimBlueprintClass->BakedStateMachines[MachineInstance->StateMachineIndexInClass]) : nullptr;
}

int32 UAnimInstance::GetInstanceAssetPlayerIndex(FName MachineName, FName StateName, FName AssetName)
{
	if(UAnimBlueprintGeneratedClass* AnimBlueprintClass = Cast<UAnimBlueprintGeneratedClass>((UObject*)GetClass()))
	{
		if(const FBakedAnimationStateMachine* MachineDescription = GetStateMachineInstanceDesc(MachineName))
		{
			for(int32 StateIndex = 0; StateIndex < MachineDescription->States.Num(); StateIndex++)
			{
				const FBakedAnimationState& State = MachineDescription->States[StateIndex];
				if(State.StateName == StateName)
				{
					for(int32 PlayerIndex = 0; PlayerIndex < State.PlayerNodeIndices.Num(); PlayerIndex++)
					{
						checkSlow(State.PlayerNodeIndices[PlayerIndex] < AnimBlueprintClass->AnimNodeProperties.Num());
						UStructProperty* AssetPlayerProperty = AnimBlueprintClass->AnimNodeProperties[AnimBlueprintClass->AnimNodeProperties.Num() - 1 - State.PlayerNodeIndices[PlayerIndex]];
						if(AssetPlayerProperty && AssetPlayerProperty->Struct->IsChildOf(FAnimNode_AssetPlayerBase::StaticStruct()))
						{
							FAnimNode_AssetPlayerBase* AssetPlayer = AssetPlayerProperty->ContainerPtrToValuePtr<FAnimNode_AssetPlayerBase>(this);
							if(AssetPlayer)
							{
								if(AssetName == NAME_None || AssetPlayer->GetAnimAsset()->GetFName() == AssetName)
								{
									return State.PlayerNodeIndices[PlayerIndex];
								}
							}
						}
					}
				}
			}
		}
	}

	return INDEX_NONE;
}

FAnimNode_AssetPlayerBase* UAnimInstance::GetRelevantAssetPlayerFromState(int32 MachineIndex, int32 StateIndex)
{
	FAnimNode_AssetPlayerBase* ResultPlayer = nullptr;
	if(FAnimNode_StateMachine* MachineInstance = GetStateMachineInstance(MachineIndex))
	{
		float MaxWeight = 0.0f;
		const FBakedAnimationState& State = MachineInstance->GetStateInfo(StateIndex);
		for(const int32& PlayerIdx : State.PlayerNodeIndices)
		{
			if(FAnimNode_AssetPlayerBase* Player = GetNodeFromIndex<FAnimNode_AssetPlayerBase>(PlayerIdx))
			{
				if(!Player->bIgnoreForRelevancyTest && Player->GetCachedBlendWeight() > MaxWeight)
				{
					MaxWeight = Player->GetCachedBlendWeight();
					ResultPlayer = Player;
				}
			}
		}
	}
	return ResultPlayer;
}

int32 UAnimInstance::GetSyncGroupIndexFromName(FName SyncGroupName) const
{

	if (UAnimBlueprintGeneratedClass* AnimBlueprintClass = Cast<UAnimBlueprintGeneratedClass>(GetClass()))
	{
		return AnimBlueprintClass->GetSyncGroupIndex(SyncGroupName);
	}
	return INDEX_NONE;
}

bool UAnimInstance::IsRunningParallelEvaluation() const
{
	USkeletalMeshComponent* Comp = GetOwningComponent();
	if (Comp)
	{
		return Comp->IsRunningParallelEvaluation();
	}
	return false;
}

#undef LOCTEXT_NAMESPACE 
