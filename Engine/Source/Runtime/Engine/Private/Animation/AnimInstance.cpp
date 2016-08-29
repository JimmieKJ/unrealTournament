// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
#include "Animation/AnimInstanceProxy.h"
#include "Animation/Skeleton.h"
#include "Animation/SmartName.h"

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

DEFINE_STAT(STAT_TickAssetPlayerInstances);
DEFINE_STAT(STAT_TickAssetPlayerInstance);

// Define AnimNotify
DEFINE_LOG_CATEGORY(LogAnimNotify);

#define LOCTEXT_NAMESPACE "AnimInstance"

extern TAutoConsoleVariable<int32> CVarUseParallelAnimUpdate;
extern TAutoConsoleVariable<int32> CVarUseParallelAnimationEvaluation;
extern TAutoConsoleVariable<int32> CVarForceUseParallelAnimUpdate;

/////////////////////////////////////////////////////
// UAnimInstance
/////////////////////////////////////////////////////

UAnimInstance::UAnimInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bUpdatingAnimation(false)
	, bPostUpdatingAnimation(false)
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	RootNode = nullptr;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	RootMotionMode = ERootMotionMode::RootMotionFromMontagesOnly;
	bNeedsUpdate = false;
}

void UAnimInstance::MakeSequenceTickRecord(FAnimTickRecord& TickRecord, class UAnimSequenceBase* Sequence, bool bLooping, float PlayRate, float FinalBlendWeight, float& CurrentTime, FMarkerTickRecord& MarkerTickRecord) const
{
	GetProxyOnGameThread<FAnimInstanceProxy>().MakeSequenceTickRecord(TickRecord, Sequence, bLooping, PlayRate, FinalBlendWeight, CurrentTime, MarkerTickRecord);
}

void UAnimInstance::MakeBlendSpaceTickRecord(FAnimTickRecord& TickRecord, class UBlendSpaceBase* BlendSpace, const FVector& BlendInput, TArray<FBlendSampleData>& BlendSampleDataCache, FBlendFilter& BlendFilter, bool bLooping, float PlayRate, float FinalBlendWeight, float& CurrentTime, FMarkerTickRecord& MarkerTickRecord) const
{
	GetProxyOnGameThread<FAnimInstanceProxy>().MakeBlendSpaceTickRecord(TickRecord, BlendSpace, BlendInput, BlendSampleDataCache, BlendFilter, bLooping, PlayRate, FinalBlendWeight, CurrentTime, MarkerTickRecord);
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
	GetProxyOnGameThread<FAnimInstanceProxy>().SequenceAdvanceImmediate(Sequence, bLooping, PlayRate, DeltaSeconds, CurrentTime, MarkerTickRecord);
}

void UAnimInstance::BlendSpaceAdvanceImmediate(class UBlendSpaceBase* BlendSpace, const FVector& BlendInput, TArray<FBlendSampleData>& BlendSampleDataCache, FBlendFilter& BlendFilter, bool bLooping, float PlayRate, float DeltaSeconds, float& CurrentTime, FMarkerTickRecord& MarkerTickRecord)
{
	GetProxyOnGameThread<FAnimInstanceProxy>().BlendSpaceAdvanceImmediate(BlendSpace, BlendInput, BlendSampleDataCache, BlendFilter, bLooping, PlayRate, DeltaSeconds, CurrentTime, MarkerTickRecord);
}

FAnimTickRecord& UAnimInstance::CreateUninitializedTickRecord(int32 GroupIndex, FAnimGroupInstance*& OutSyncGroupPtr)
{
	return GetProxyOnGameThread<FAnimInstanceProxy>().CreateUninitializedTickRecord(GroupIndex, OutSyncGroupPtr);
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

	if (IAnimClassInterface* AnimBlueprintClass = IAnimClassInterface::GetFromClass(GetClass()))
	{
#if WITH_EDITOR
		LifeTimer = 0.0;
		CurrentLifeTimerScrubPosition = 0.0;

		if (UAnimBlueprint* Blueprint = Cast<UAnimBlueprint>(CastChecked<UAnimBlueprintGeneratedClass>(AnimBlueprintClass)->ClassGeneratedBy))
		{
			if (Blueprint->GetObjectBeingDebugged() == this)
			{
				// Reset the snapshot buffer
				CastChecked<UAnimBlueprintGeneratedClass>(AnimBlueprintClass)->GetAnimBlueprintDebugData().ResetSnapshotBuffer();
			}
		}
#endif
	}

	// before initialize, need to recalculate required bone list
	RecalcRequiredBones();

	GetProxyOnGameThread<FAnimInstanceProxy>().Initialize(this);

	ClearMorphTargets();
	NativeInitializeAnimation();
	BlueprintInitializeAnimation();

	GetProxyOnGameThread<FAnimInstanceProxy>().InitializeRootNode();

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	// @todo: remove after deprecation
	RootNode = GetProxyOnGameThread<FAnimInstanceProxy>().GetRootNode();
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// we can bind rules & events now the graph has been initialized
	GetProxyOnGameThread<FAnimInstanceProxy>().BindNativeDelegates();
}

void UAnimInstance::UninitializeAnimation()
{
	NativeUninitializeAnimation();

	GetProxyOnGameThread<FAnimInstanceProxy>().Uninitialize(this);

	StopAllMontages(0.f);

	for(int32 Index = 0; Index < MontageInstances.Num(); ++Index)
	{
		FAnimMontageInstance* MontageInstance = MontageInstances[Index];
		if (ensure(MontageInstance != nullptr))
		{
			ClearMontageInstanceReferences(*MontageInstance);
			delete MontageInstance;
		}
	}

	MontageInstances.Empty();
	ActiveMontagesMap.Empty();

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
			AddCurveValue(ParamsToClearCopy[i], 0.0f, ACF_DriveMaterial);
		}
	}

	ActiveAnimNotifyState.Reset();
	ResetAnimationCurves();
	MaterialParamatersToClear.Reset();
	NotifyQueue.Reset(SkelMeshComp);
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
	GetProxyOnGameThread<FAnimInstanceProxy>().UpdateAnimationNode(DeltaSeconds);
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
		bool bRecordNeedsResetting = true;
		if (MontageInstance->bDidUseMarkerSyncThisTick)
		{
			const int32 GroupIndexToUse = MontageInstance->GetSyncGroupIndex();

			// that is public data, so if anybody decided to play with it
			if (ensure (GroupIndexToUse != INDEX_NONE))
			{
				bRecordNeedsResetting = false;
				FAnimGroupInstance* SyncGroup;
				FAnimTickRecord& TickRecord = GetProxyOnGameThread<FAnimInstanceProxy>().CreateUninitializedTickRecord(GroupIndexToUse, /*out*/ SyncGroup);
				MakeMontageTickRecord(TickRecord, MontageInstance->Montage, MontageInstance->GetPosition(), 
					MontageInstance->GetPreviousPosition(), MontageInstance->GetDeltaMoved(), MontageInstance->GetWeight(), 
					MontageInstance->MarkersPassedThisTick, MontageInstance->MarkerTickRecord);

				// Update the sync group if it exists
				if (SyncGroup != NULL)
				{
					SyncGroup->TestMontageTickRecordForLeadership();
				}
			}
			MontageInstance->bDidUseMarkerSyncThisTick = false;
		}
		if (bRecordNeedsResetting)
		{
			MontageInstance->MarkerTickRecord.Reset();
		}
	}

	// update montage eval data
	UpdateMontageEvaluationData();
}

void UAnimInstance::UpdateAnimation(float DeltaSeconds, bool bNeedsValidRootMotion)
{
#if DO_CHECK
	checkf(!bUpdatingAnimation, TEXT("UpdateAnimation already in progress, circular detected for SkeletalMeshComponent [%s], AnimInstance [%s]"), *GetNameSafe(GetOwningComponent()),  *GetName());
	TGuardValue<bool> CircularGuard(bUpdatingAnimation, true);
#endif
	SCOPE_CYCLE_COUNTER(STAT_UpdateAnimation);
	FScopeCycleCounterUObject AnimScope(this);

	// acquire the proxy as we need to update
	FAnimInstanceProxy& Proxy = GetProxyOnGameThread<FAnimInstanceProxy>();

#if WITH_EDITOR
	if (GIsEditor)
	{
		// Reset the anim graph visualization
		if (Proxy.HasRootNode())
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

PRAGMA_DISABLE_DEPRECATION_WARNINGS
		// @todo: remove once deprecated - called for backwards-compatibility
		NativeUpdateAnimation_WorkerThread(DeltaSeconds);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
	{
		SCOPE_CYCLE_COUNTER(STAT_BlueprintUpdateAnimation);
		BlueprintUpdateAnimation(DeltaSeconds);
	}

	// need to update montage BEFORE node update
	// so that node knows where montage is
	UpdateMontage(DeltaSeconds);

	if(bNeedsValidRootMotion || NeedsImmediateUpdate(DeltaSeconds))
	{
		// cant use parallel update, so just do the work here
		Proxy.UpdateAnimation();
		PostUpdateAnimation();
	}
}

void UAnimInstance::PreUpdateAnimation(float DeltaSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_PreUpdateAnimation);

	bNeedsUpdate = true;

	NotifyQueue.Reset(GetSkelMeshComponent());
	RootMotionBlendQueue.Reset();

	GetProxyOnGameThread<FAnimInstanceProxy>().PreUpdate(this, DeltaSeconds);
}

void UAnimInstance::PostUpdateAnimation()
{
#if DO_CHECK
	checkf(!bPostUpdatingAnimation, TEXT("PostUpdateAnimation already in progress, recursion detected for SkeletalMeshComponent [%s], AnimInstance [%s]"), *GetNameSafe(GetOwningComponent()), *GetName());
	TGuardValue<bool> CircularGuard(bPostUpdatingAnimation, true);
#endif

	SCOPE_CYCLE_COUNTER(STAT_PostUpdateAnimation);
	check(!IsRunningParallelEvaluation());

	bNeedsUpdate = false;

	// acquire the proxy as we need to update
	FAnimInstanceProxy& Proxy = GetProxyOnGameThread<FAnimInstanceProxy>();

	// flip read/write index
	// Do this first, as we'll be reading cached slot weights, and we want this to be up to date for this frame.
	Proxy.TickSyncGroupWriteIndex();

	Proxy.PostUpdate(this);

	FRootMotionMovementParams& ExtractedRootMotion = Proxy.GetExtractedRootMotion();

	// blend in any montage-blended root motion that we now have correct weights for
	for(const FQueuedRootMotionBlend& RootMotionBlend : RootMotionBlendQueue)
	{
		const float RootMotionSlotWeight = GetSlotNodeGlobalWeight(RootMotionBlend.SlotName);
		const float RootMotionInstanceWeight = RootMotionBlend.Weight * RootMotionSlotWeight;
		ExtractedRootMotion.AccumulateWithBlend(RootMotionBlend.Transform, RootMotionInstanceWeight);
	}

	// We may have just partially blended root motion, so make it up to 1 by
	// blending in identity too
	if (ExtractedRootMotion.bHasRootMotion)
	{
		ExtractedRootMotion.MakeUpToFullWeight();
	}

	/////////////////////////////////////////////////////////////////////////////
	// Notify / Event Handling!
	// This can do anything to our component (including destroy it) 
	// Any code added after this point needs to take that into account
	/////////////////////////////////////////////////////////////////////////////
	{
		// now trigger Notifies
		TriggerAnimNotifies(Proxy.GetDeltaSeconds());

		// Trigger Montage end events after notifies. In case Montage ending ends abilities or other states, we make sure notifies are processed before montage events.
		TriggerQueuedMontageEvents();
	}

#if WITH_EDITOR && 0
	{
		// Take a snapshot if the scrub control is locked to the end, we are playing, and we are the one being debugged
		if (UAnimBlueprintGeneratedClass* AnimBlueprintClass = Cast<UAnimBlueprintGeneratedClass>(GetClass()))
		{
			if (UAnimBlueprint* Blueprint = Cast<UAnimBlueprint>(AnimBlueprintClass->ClassGeneratedBy))
			{
				if (Blueprint->GetObjectBeingDebugged() == this)
				{
					if ((CurrentLifeTimerScrubPosition == LifeTimer) && (Proxy.GetDeltaSeconds() > 0.0f))
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
	GetProxyOnAnyThread<FAnimInstanceProxy>().UpdateAnimation();
}

bool UAnimInstance::NeedsImmediateUpdate(float DeltaSeconds) const
{
	// If Evaluation Phase is skipped, PostUpdateAnimation() will not get called, so we can't use ParallelUpdateAnimation then.
	USkeletalMeshComponent* SkelMeshComp = GetSkelMeshComponent();
	const bool bEvaluationPhaseSkipped = SkelMeshComp && !SkelMeshComp->bRecentlyRendered && (SkelMeshComp->MeshComponentUpdateFlag > EMeshComponentUpdateFlag::AlwaysTickPoseAndRefreshBones);

	const bool bUseParallelUpdateAnimation = bCanUseParallelUpdateAnimation || (CVarForceUseParallelAnimUpdate.GetValueOnGameThread() != 0);

	return
		GIntraFrameDebuggingGameThread ||
		bEvaluationPhaseSkipped || 
		CVarUseParallelAnimUpdate.GetValueOnGameThread() == 0 ||
		CVarUseParallelAnimationEvaluation.GetValueOnGameThread() == 0 ||
		!bUseParallelUpdateAnimation ||
		DeltaSeconds == 0.0f ||
		RootMotionMode == ERootMotionMode::RootMotionFromEverything;
}

bool UAnimInstance::NeedsUpdate() const
{
	return bNeedsUpdate;
}

void UAnimInstance::PreEvaluateAnimation()
{
	GetProxyOnGameThread<FAnimInstanceProxy>().PreEvaluateAnimation(this);
}

void UAnimInstance::EvaluateAnimation(FPoseContext& Output)
{
	GetProxyOnGameThread<FAnimInstanceProxy>().EvaluateAnimation(Output);
}

bool UAnimInstance::ParallelCanEvaluate(const USkeletalMesh* InSkeletalMesh) const
{
	const FAnimInstanceProxy& Proxy = GetProxyOnAnyThread<FAnimInstanceProxy>();
	return Proxy.GetRequiredBones().IsValid() && (Proxy.GetRequiredBones().GetAsset() == InSkeletalMesh);
}

void UAnimInstance::ParallelEvaluateAnimation(bool bForceRefPose, const USkeletalMesh* InSkeletalMesh, TArray<FTransform>& OutBoneSpaceTransforms, FBlendedHeapCurve& OutCurve)
{
	FMemMark Mark(FMemStack::Get());
	FAnimInstanceProxy& Proxy = GetProxyOnAnyThread<FAnimInstanceProxy>();

	if( !bForceRefPose )
	{
		// Create an evaluation context
		FPoseContext EvaluationContext(&Proxy);
		EvaluationContext.ResetToRefPose();
			
		// Run the anim blueprint
		Proxy.EvaluateAnimation(EvaluationContext);
		// Move the curves
		OutCurve.CopyFrom(EvaluationContext.Curve);
			
		// can we avoid that copy?
		if( EvaluationContext.Pose.GetNumBones() > 0 )
		{
			// Make sure rotations are normalized to account for accumulation of errors.
			EvaluationContext.Pose.NormalizeRotations();
			for (const FCompactPoseBoneIndex BoneIndex : EvaluationContext.Pose.ForEachBoneIndex())
			{
				FMeshPoseBoneIndex MeshPoseBoneIndex = EvaluationContext.Pose.GetBoneContainer().MakeMeshPoseIndex(BoneIndex);
				OutBoneSpaceTransforms[MeshPoseBoneIndex.GetInt()] = EvaluationContext.Pose[BoneIndex];
			}
		}
		else
		{
			FAnimationRuntime::FillWithRefPose(OutBoneSpaceTransforms, Proxy.GetRequiredBones());
		}
	}
	else
	{
		FAnimationRuntime::FillWithRefPose(OutBoneSpaceTransforms, Proxy.GetRequiredBones());
	}
}

void UAnimInstance::PostEvaluateAnimation()
{
	NativePostEvaluateAnimation();

	{
		SCOPE_CYCLE_COUNTER(STAT_BlueprintPostEvaluateAnimation);
		BlueprintPostEvaluateAnimation();
	}

	GetProxyOnGameThread<FAnimInstanceProxy>().ClearObjects();
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
	GetProxyOnGameThread<FAnimInstanceProxy>().AddNativeTransitionBinding(MachineName, PrevStateName, NextStateName, NativeTransitionDelegate, TransitionName);
}

bool UAnimInstance::HasNativeTransitionBinding(const FName& MachineName, const FName& PrevStateName, const FName& NextStateName, FName& OutBindingName)
{
	return GetProxyOnGameThread<FAnimInstanceProxy>().HasNativeTransitionBinding(MachineName, PrevStateName, NextStateName, OutBindingName);
}

void UAnimInstance::AddNativeStateEntryBinding(const FName& MachineName, const FName& StateName, const FOnGraphStateChanged& NativeEnteredDelegate)
{
	GetProxyOnGameThread<FAnimInstanceProxy>().AddNativeStateEntryBinding(MachineName, StateName, NativeEnteredDelegate);
}
	
bool UAnimInstance::HasNativeStateEntryBinding(const FName& MachineName, const FName& StateName, FName& OutBindingName)
{
	return GetProxyOnGameThread<FAnimInstanceProxy>().HasNativeStateEntryBinding(MachineName, StateName, OutBindingName);
}

void UAnimInstance::AddNativeStateExitBinding(const FName& MachineName, const FName& StateName, const FOnGraphStateChanged& NativeExitedDelegate)
{
	GetProxyOnGameThread<FAnimInstanceProxy>().AddNativeStateExitBinding(MachineName, StateName, NativeExitedDelegate);
}

bool UAnimInstance::HasNativeStateExitBinding(const FName& MachineName, const FName& StateName, FName& OutBindingName)
{
	return GetProxyOnGameThread<FAnimInstanceProxy>().HasNativeStateExitBinding(MachineName, StateName, OutBindingName);
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
	FAnimInstanceProxy& Proxy = GetProxyOnGameThread<FAnimInstanceProxy>();

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

	if (bShowGraph && Proxy.HasRootNode())
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
		Proxy.GatherDebugData(NodeDebugData);

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
		const TArray<FAnimGroupInstance>& SyncGroups = GetProxyOnGameThread<FAnimInstanceProxy>().GetSyncGroupRead();
		const TArray<FAnimTickRecord>& UngroupedActivePlayers = GetProxyOnGameThread<FAnimInstanceProxy>().GetUngroupedActivePlayersRead();

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

			Heading = FString::Printf(TEXT("Morph Curves: %i"), AnimationCurves[(uint8)EAnimCurveType::MorphTargetCurve].Num());
			DisplayDebugManager.DrawString(Heading, Indent);

			DisplayDebugManager.SetLinearDrawColor(TextWhite);

			{
				FIndenter MorphCurveIndent(Indent);
				OutputCurveMap(AnimationCurves[(uint8)EAnimCurveType::MorphTargetCurve], Canvas, DisplayDebugManager, Indent);
			}

			DisplayDebugManager.SetLinearDrawColor(TextYellow);

			Heading = FString::Printf(TEXT("Material Curves: %i"), AnimationCurves[(uint8)EAnimCurveType::MaterialCurve].Num());
			DisplayDebugManager.DrawString(Heading, Indent);

			DisplayDebugManager.SetLinearDrawColor(TextWhite);

			{
				FIndenter MaterialCurveIndent(Indent);
				OutputCurveMap(AnimationCurves[(uint8)EAnimCurveType::MaterialCurve], Canvas, DisplayDebugManager, Indent);
			}

			DisplayDebugManager.SetLinearDrawColor(TextYellow);

			Heading = FString::Printf(TEXT("Event Curves: %i"), AnimationCurves[(uint8)EAnimCurveType::AttributeCurve].Num());
			DisplayDebugManager.DrawString(Heading, Indent);

			DisplayDebugManager.SetLinearDrawColor(TextWhite);

			{
				FIndenter EventCurveIndent(Indent);
				OutputCurveMap(AnimationCurves[(uint8)EAnimCurveType::AttributeCurve], Canvas, DisplayDebugManager, Indent);
			}
		}
	}
}

void UAnimInstance::ResetDynamics()
{
	GetProxyOnGameThread<FAnimInstanceProxy>().ResetDynamics();
}

void UAnimInstance::RecalcRequiredBones()
{
	USkeletalMeshComponent* SkelMeshComp = GetSkelMeshComponent();
	check( SkelMeshComp )

	if( SkelMeshComp->SkeletalMesh && SkelMeshComp->SkeletalMesh->Skeleton )
	{
		GetProxyOnGameThread<FAnimInstanceProxy>().RecalcRequiredBones(SkelMeshComp, SkelMeshComp->SkeletalMesh);
	}
	else if( CurrentSkeleton != NULL )
	{
		GetProxyOnGameThread<FAnimInstanceProxy>().RecalcRequiredBones(SkelMeshComp, CurrentSkeleton);
	}
}

void UAnimInstance::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (!Ar.IsLoading() || !Ar.IsSaving())
	{
		Ar << GetProxyOnAnyThread<FAnimInstanceProxy>().GetRequiredBones();
	}
}

bool UAnimInstance::CanTransitionSignature() const
{
	return false;
}

void UAnimInstance::BeginDestroy()
{
#if DO_CHECK
	if(GetOuter() && GetOuter()->IsA<USkeletalMeshComponent>())
	{
		check(!IsRunningParallelEvaluation());
	}
#endif
	if(AnimInstanceProxy != nullptr)
	{
		DestroyAnimInstanceProxy(AnimInstanceProxy);
		AnimInstanceProxy = nullptr;
	}

	Super::BeginDestroy();
}

void UAnimInstance::PostInitProperties()
{
	Super::PostInitProperties();

	if(AnimInstanceProxy == nullptr)
	{
		AnimInstanceProxy = CreateAnimInstanceProxy();
		check(AnimInstanceProxy != nullptr);
	}
}

bool UAnimInstance::PassesFiltering(const FAnimNotifyEvent* Notify) const
{
	return NotifyQueue.PassesFiltering(Notify);
}

bool UAnimInstance::PassesChanceOfTriggering(const FAnimNotifyEvent* Event) const
{
	return NotifyQueue.PassesChanceOfTriggering(Event);
}

void UAnimInstance::AddAnimNotifyFromGeneratedClass(int32 NotifyIndex)
{
	GetProxyOnGameThread<FAnimInstanceProxy>().AddAnimNotifyFromGeneratedClass(NotifyIndex);
}

void UAnimInstance::AddCurveValue(const FName& CurveName, float Value, int32 CurveTypeFlags)
{
	FAnimInstanceProxy& Proxy = GetProxyOnGameThread<FAnimInstanceProxy>();

	// save curve value, it will overwrite if same exists, 
	//CurveValues.Add(CurveName, Value);
	if (CurveTypeFlags & ACF_DriveAttribute)
	{
		float *CurveValPtr = AnimationCurves[(uint8)EAnimCurveType::AttributeCurve].Find(CurveName);
		if ( CurveValPtr )
		{
			// sum up, in the future we might normalize, but for now this just sums up
			// this won't work well if all of them have full weight - i.e. additive 
			*CurveValPtr = Value;
		}
		else
		{
			AnimationCurves[(uint8)EAnimCurveType::AttributeCurve].Add(CurveName, Value);
		}
	}

	if (CurveTypeFlags & ACF_DriveMorphTarget)
	{
		float *CurveValPtr = AnimationCurves[(uint8)EAnimCurveType::MorphTargetCurve].Find(CurveName);
		if ( CurveValPtr )
		{
			// sum up, in the future we might normalize, but for now this just sums up
			// this won't work well if all of them have full weight - i.e. additive 
			*CurveValPtr = Value;
		}
		else
		{
			AnimationCurves[(uint8)EAnimCurveType::MorphTargetCurve].Add(CurveName, Value);
		}
	}

	if (CurveTypeFlags & ACF_DriveMaterial)
	{
		MaterialParamatersToClear.RemoveSwap(CurveName);
		float* CurveValPtr = AnimationCurves[(uint8)EAnimCurveType::MaterialCurve].Find(CurveName);
		if( CurveValPtr)
		{
			*CurveValPtr = Value;
		}
		else
		{
			AnimationCurves[(uint8)EAnimCurveType::MaterialCurve].Add(CurveName, Value);
		}
	}
}

void UAnimInstance::AddCurveValue(const USkeleton::AnimCurveUID Uid, float Value, int32 CurveTypeFlags)
{
	FName CurrentCurveName;
	// Grab the smartname mapping from our current skeleton and resolve the curve name. We cannot cache
	// the smart name mapping as the skeleton can change at any time.
	if(const FSmartNameMapping* NameMapping = CurrentSkeleton->GetSmartNameContainer(USkeleton::AnimCurveMappingName))
	{
		NameMapping->GetName(Uid, CurrentCurveName);
	}
	AddCurveValue(CurrentCurveName, Value, CurveTypeFlags);
}

int32 UAnimInstance::GetSyncGroupReadIndex() const
{ 
	return GetProxyOnGameThread<FAnimInstanceProxy>().GetSyncGroupReadIndex(); 
}

int32 UAnimInstance::GetSyncGroupWriteIndex() const 
{ 
	return GetProxyOnGameThread<FAnimInstanceProxy>().GetSyncGroupWriteIndex(); 
}

void UAnimInstance::TickSyncGroupWriteIndex() 
{
	GetProxyOnGameThread<FAnimInstanceProxy>().TickSyncGroupWriteIndex();
}

void UAnimInstance::UpdateCurvesToComponents(USkeletalMeshComponent* Component /*= nullptr*/)
{
	// update curves to component
	if (Component)
	{
		Component->ApplyAnimationCurvesToComponent(&AnimationCurves[(uint8)EAnimCurveType::MaterialCurve], &AnimationCurves[(uint8)EAnimCurveType::MorphTargetCurve]);
	}
}

void UAnimInstance::GetAnimationCurveList(int32 CurveFlags, TMap<FName, float>& OutCurveList) const
{
	OutCurveList.Reset();

	if (CurveFlags & ACF_DriveAttribute)
	{
		OutCurveList.Append(AnimationCurves[(uint8)EAnimCurveType::AttributeCurve]);
	}

	if (CurveFlags & ACF_DriveMorphTarget)
	{
		OutCurveList.Append(AnimationCurves[(uint8)EAnimCurveType::MorphTargetCurve]);
	}

	if (CurveFlags & ACF_DriveMaterial)
	{
		OutCurveList.Append(AnimationCurves[(uint8)EAnimCurveType::MaterialCurve]);
	}
}

void UAnimInstance::RefreshCurves(USkeletalMeshComponent* Component)
{
	UpdateCurvesToComponents(Component);
}

void UAnimInstance::ResetAnimationCurves()
{
	for (uint8 Index = 0; Index < (uint8)EAnimCurveType::MaxAnimCurveType; ++Index)
	{
		AnimationCurves[Index].Reset();
	}
}

void UAnimInstance::UpdateCurves(const FBlendedHeapCurve& InCurve)
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateCurves);

	FAnimInstanceProxy& Proxy = GetProxyOnGameThread<FAnimInstanceProxy>();

	//Track material params we set last time round so we can clear them if they aren't set again.
	MaterialParamatersToClear.Reset();
	for(auto Iter = AnimationCurves[(uint8)EAnimCurveType::MaterialCurve].CreateConstIterator(); Iter; ++Iter)
	{
		if(Iter.Value() > 0.0f)
		{
			MaterialParamatersToClear.Add(Iter.Key());
		}
	}

	ResetAnimationCurves();

	if (InCurve.UIDList != nullptr)
	{
		const TArray<FSmartNameMapping::UID>& UIDList = *InCurve.UIDList;
		for (int32 CurveId = 0; CurveId < InCurve.UIDList->Num(); ++CurveId)
		{
			// had to add to another data type
			AddCurveValue(UIDList[CurveId], InCurve.Elements[CurveId].Value, InCurve.Elements[CurveId].Flags);
		}
	}	

	// Add 0.0 curves to clear parameters that we have previously set but didn't set this tick.
	//   - Make a copy of MaterialParametersToClear as it will be modified by AddCurveValue
	TArray<FName> ParamsToClearCopy = MaterialParamatersToClear;
	for(int i = 0; i < ParamsToClearCopy.Num(); ++i)
	{
		AddCurveValue(ParamsToClearCopy[i], 0.0f, ACF_DriveMaterial);
	}

	// @todo: delete me later when james g's change goes in
	// this won't work well because pose needs to be handled in evaluate
	// the question is that if we'd like to support preview in anim graph
	// that will need better handling of the curves - currently UI curves are inserted to 
	// SignleNodeInstance->PreviewOverride
// #if WITH_EDITOR
// 	// if we're supporting this in-game, this code has to change to work with UID
// 	for (auto& AddAnimCurveDelegate : OnAddAnimationCurves)
// 	{
// 		if (AddAnimCurveDelegate.IsBound())
// 		{
// 			AddAnimCurveDelegate.Execute(this);
// 		}
// 	}
// 
// #endif
	// update curves to component
	UpdateCurvesToComponents(GetOwningComponent());
}

bool UAnimInstance::HasMorphTargetCurves() const
{
	return AnimationCurves[(uint8)EAnimCurveType::MorphTargetCurve].Num() > 0;
}

void UAnimInstance::TriggerAnimNotifies(float DeltaSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_AnimTriggerAnimNotifies);
	USkeletalMeshComponent* SkelMeshComp = GetSkelMeshComponent();

	// Array that will replace the 'ActiveAnimNotifyState' at the end of this function.
	TArray<FAnimNotifyEvent> NewActiveAnimNotifyState;
	// AnimNotifyState freshly added that need their 'NotifyBegin' event called.
	TArray<const FAnimNotifyEvent *> NotifyStateBeginEvent;

	for (int32 Index=0; Index<NotifyQueue.AnimNotifies.Num(); Index++)
	{
		const FAnimNotifyEvent* AnimNotifyEvent = NotifyQueue.AnimNotifies[Index];

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
	GetProxyOnGameThread<FAnimInstanceProxy>().GetSlotWeight(SlotNodeName, out_SlotNodeWeight, out_SourceWeight, out_TotalNodeWeight);
}

void UAnimInstance::SlotEvaluatePose(FName SlotNodeName, const FCompactPose& SourcePose, const FBlendedCurve& SourceCurve, float InSourceWeight, FCompactPose& BlendedPose, FBlendedCurve& BlendedCurve, float InBlendWeight, float InTotalNodeWeight)
{
	GetProxyOnGameThread<FAnimInstanceProxy>().SlotEvaluatePose(SlotNodeName, SourcePose, SourceCurve, InSourceWeight, BlendedPose, BlendedCurve, InBlendWeight, InTotalNodeWeight);
}

void UAnimInstance::ReinitializeSlotNodes()
{
	GetProxyOnGameThread<FAnimInstanceProxy>().ReinitializeSlotNodes();
}

void UAnimInstance::RegisterSlotNodeWithAnimInstance(FName SlotNodeName)
{
	GetProxyOnGameThread<FAnimInstanceProxy>().RegisterSlotNodeWithAnimInstance(SlotNodeName);
}

void UAnimInstance::UpdateSlotNodeWeight(FName SlotNodeName, float InLocalMontageWeight, float InGlobalWeight)
{
	GetProxyOnGameThread<FAnimInstanceProxy>().UpdateSlotNodeWeight(SlotNodeName, InLocalMontageWeight, InGlobalWeight);
}

void UAnimInstance::ClearSlotNodeWeights()
{
	GetProxyOnGameThread<FAnimInstanceProxy>().ClearSlotNodeWeights();
}

bool UAnimInstance::IsSlotNodeRelevantForNotifies(FName SlotNodeName) const
{
	return GetProxyOnGameThread<FAnimInstanceProxy>().IsSlotNodeRelevantForNotifies(SlotNodeName);
}

float UAnimInstance::GetSlotNodeGlobalWeight(FName SlotNodeName) const
{
	return GetProxyOnGameThread<FAnimInstanceProxy>().GetSlotNodeGlobalWeight(SlotNodeName);
}

float UAnimInstance::GetSlotMontageGlobalWeight(FName SlotNodeName) const
{
	return GetProxyOnAnyThread<FAnimInstanceProxy>().GetSlotMontageGlobalWeight(SlotNodeName);
}

float UAnimInstance::GetCurveValue(FName CurveName)
{
	float* Value = AnimationCurves[(uint8)EAnimCurveType::AttributeCurve].Find(CurveName);
	if (Value)
	{
		return *Value;
	}

	return 0.f;
}

void UAnimInstance::SetRootMotionMode(TEnumAsByte<ERootMotionMode::Type> Value)
{
	RootMotionMode = Value;
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
	if (IAnimClassInterface* AnimBlueprintClass = IAnimClassInterface::GetFromClass(GetClass()))
	{
		const TArray<UStructProperty*>& AnimNodeProperties = AnimBlueprintClass->GetAnimNodeProperties();
		if ((MachineIndex >= 0) && (MachineIndex < AnimNodeProperties.Num()))
		{
			const int32 InstancePropertyIndex = AnimNodeProperties.Num() - 1 - MachineIndex; //@TODO: ANIMREFACTOR: Reverse indexing

			UStructProperty* MachineInstanceProperty = AnimNodeProperties[InstancePropertyIndex];
			checkSlow(MachineInstanceProperty->Struct->IsChildOf(FAnimNode_StateMachine::StaticStruct()));

			FAnimNode_StateMachine* MachineInstance = MachineInstanceProperty->ContainerPtrToValuePtr<FAnimNode_StateMachine>(this);

			return MachineInstance->GetStateWeight(StateIndex);
		}
	}

	return 0.0f;
}

float UAnimInstance::GetCurrentStateElapsedTime(int32 MachineIndex)
{
	if (IAnimClassInterface* AnimBlueprintClass = IAnimClassInterface::GetFromClass(GetClass()))
	{
		const TArray<UStructProperty*>& AnimNodeProperties = AnimBlueprintClass->GetAnimNodeProperties();
		if ((MachineIndex >= 0) && (MachineIndex < AnimNodeProperties.Num()))
		{
			const int32 InstancePropertyIndex = AnimNodeProperties.Num() - 1 - MachineIndex; //@TODO: ANIMREFACTOR: Reverse indexing

			UStructProperty* MachineInstanceProperty = AnimNodeProperties[InstancePropertyIndex];
			checkSlow(MachineInstanceProperty->Struct->IsChildOf(FAnimNode_StateMachine::StaticStruct()));

			FAnimNode_StateMachine* MachineInstance = MachineInstanceProperty->ContainerPtrToValuePtr<FAnimNode_StateMachine>(this);

			return MachineInstance->GetCurrentStateElapsedTime();
		}
	}

	return 0.0f;
}

FName UAnimInstance::GetCurrentStateName(int32 MachineIndex)
{
	if (IAnimClassInterface* AnimBlueprintClass = IAnimClassInterface::GetFromClass(GetClass()))
	{
		const TArray<UStructProperty*>& AnimNodeProperties = AnimBlueprintClass->GetAnimNodeProperties();
		if ((MachineIndex >= 0) && (MachineIndex < AnimNodeProperties.Num()))
		{
			const int32 InstancePropertyIndex = AnimNodeProperties.Num() - 1 - MachineIndex; //@TODO: ANIMREFACTOR: Reverse indexing

			UStructProperty* MachineInstanceProperty = AnimNodeProperties[InstancePropertyIndex];
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
				RootMotionParams = (RootMotionMode != ERootMotionMode::IgnoreRootMotion) ? &GetProxyOnGameThread<FAnimInstanceProxy>().GetExtractedRootMotion() : &LocalExtractedRootMotion;
			}

			MontageInstance->MontageSync_PreUpdate();
			MontageInstance->Advance(DeltaSeconds, RootMotionParams, bUsingBlendedRootMotion);
			MontageInstance->MontageSync_PostUpdate();

			if (!MontageInstance->IsValid())
			{
				// Make sure we've cleared our references before deleting memory
				ClearMontageInstanceReferences(*MontageInstance);

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

	if (!Asset->CanBeUsedInMontage())
	{
		UE_LOG(LogAnimMontage, Warning, TEXT("This animation isn't supported to play as montage"));
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
		return nullptr;
	}

	if (SlotNodeName == NAME_None)
	{
		// user warning
		UE_LOG(LogAnimMontage, Warning, TEXT("SlotNode Name is required. Make sure to add Slot Node in your anim graph and name it."));
		return nullptr;
	}

	USkeleton* AssetSkeleton = Asset->GetSkeleton();
	if (!CurrentSkeleton->IsCompatible(AssetSkeleton))
	{
		UE_LOG(LogAnimMontage, Warning, TEXT("The Skeleton isn't compatible"));
		return nullptr;
	}

	if (!Asset->CanBeUsedInMontage())
	{
		UE_LOG(LogAnimMontage, Warning, TEXT("This animation isn't supported to play as montage"));
		return nullptr;
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

bool UAnimInstance::IsPlayingSlotAnimation(const UAnimSequenceBase* Asset, FName SlotNodeName) const
{
	UAnimMontage* Montage = nullptr;
	return IsPlayingSlotAnimation(Asset, SlotNodeName, Montage);
}

bool UAnimInstance::IsPlayingSlotAnimation(const UAnimSequenceBase* Asset, FName SlotNodeName, UAnimMontage*& OutMontage) const
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
					OutMontage = CurMontage;
					return (AnimTrack->AnimSegments[0].AnimReference == Asset);
				}
			}
		}
	}

	return false;
}

/** Play a Montage. Returns Length of Montage in seconds. Returns 0.f if failed to play. */
float UAnimInstance::Montage_Play(UAnimMontage* MontageToPlay, float InPlayRate/*= 1.f*/, EMontagePlayReturnType ReturnValueType)
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
			
			const float MontageLength = NewInstance->Montage->SequenceLength;
			
			return (ReturnValueType == EMontagePlayReturnType::MontageLength) ? MontageLength : (MontageLength/(InPlayRate*MontageToPlay->RateScale));
		}
		else
		{
			UE_LOG(LogAnimMontage, Warning, TEXT("Playing a Montage (%s) for the wrong Skeleton (%s) instead of (%s)."),
				*GetNameSafe(MontageToPlay), *GetNameSafe(CurrentSkeleton), *GetNameSafe(MontageToPlay->GetSkeleton()));
		}
	}

	return 0.f;
}

void UAnimInstance::Montage_Stop(float InBlendOutTime, const UAnimMontage* Montage)
{
	if (Montage)
	{
		FAnimMontageInstance* MontageInstance = GetActiveInstanceForMontage(Montage);
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

void UAnimInstance::Montage_Pause(const UAnimMontage* Montage)
{
	if (Montage)
	{
		FAnimMontageInstance* MontageInstance = GetActiveInstanceForMontage(Montage);
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

void UAnimInstance::Montage_Resume(const UAnimMontage* Montage)
{
	if (Montage)
	{
		FAnimMontageInstance* MontageInstance = GetActiveInstanceForMontage(Montage);
		if (MontageInstance && !MontageInstance->IsPlaying())
		{
			MontageInstance->SetPlaying(true);
		}
	}
	else
	{
		// If no Montage reference, do it on all active ones.
		for (int32 InstanceIndex = 0; InstanceIndex < MontageInstances.Num(); InstanceIndex++)
		{
			FAnimMontageInstance* MontageInstance = MontageInstances[InstanceIndex];
			if (MontageInstance && MontageInstance->IsActive() && !MontageInstance->IsPlaying())
			{
				MontageInstance->SetPlaying(true);
			}
		}
	}
}

void UAnimInstance::Montage_JumpToSection(FName SectionName, const UAnimMontage* Montage)
{
	if (Montage)
	{
		FAnimMontageInstance* MontageInstance = GetActiveInstanceForMontage(Montage);
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

void UAnimInstance::Montage_JumpToSectionsEnd(FName SectionName, const UAnimMontage* Montage)
{
	if (Montage)
	{
		FAnimMontageInstance* MontageInstance = GetActiveInstanceForMontage(Montage);
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

void UAnimInstance::Montage_SetNextSection(FName SectionNameToChange, FName NextSection, const UAnimMontage* Montage)
{
	if (Montage)
	{
		FAnimMontageInstance* MontageInstance = GetActiveInstanceForMontage(Montage);
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

void UAnimInstance::Montage_SetPlayRate(const UAnimMontage* Montage, float NewPlayRate)
{
	if (Montage)
	{
		FAnimMontageInstance* MontageInstance = GetActiveInstanceForMontage(Montage);
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

bool UAnimInstance::Montage_IsActive(const UAnimMontage* Montage) const
{
	if (Montage)
	{
		FAnimMontageInstance* MontageInstance = GetActiveInstanceForMontage(Montage);
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

bool UAnimInstance::Montage_IsPlaying(const UAnimMontage* Montage) const
{
	if (Montage)
	{
		FAnimMontageInstance* MontageInstance = GetActiveInstanceForMontage(Montage);
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

FName UAnimInstance::Montage_GetCurrentSection(const UAnimMontage* Montage) const
{ 
	if (Montage)
	{
		FAnimMontageInstance* MontageInstance = GetActiveInstanceForMontage(Montage);
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
		FAnimMontageInstance* MontageInstance = GetActiveInstanceForMontage(Montage);
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
		FAnimMontageInstance* MontageInstance = GetActiveInstanceForMontage(Montage);
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
		FAnimMontageInstance* MontageInstance = GetActiveInstanceForMontage(Montage);
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

void UAnimInstance::Montage_SetPosition(const UAnimMontage* Montage, float NewPosition)
{
	if (Montage)
	{
		FAnimMontageInstance* MontageInstance = GetActiveInstanceForMontage(Montage);
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

float UAnimInstance::Montage_GetPosition(const UAnimMontage* Montage)
{
	if (Montage)
	{
		FAnimMontageInstance* MontageInstance = GetActiveInstanceForMontage(Montage);
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

bool UAnimInstance::Montage_GetIsStopped(const UAnimMontage* Montage)
{
	if (Montage)
	{
		FAnimMontageInstance* MontageInstance = GetActiveInstanceForMontage(Montage);
		return (!MontageInstance); // Not active == Stopped.
	}
	return true;
}

float UAnimInstance::Montage_GetBlendTime(const UAnimMontage* Montage)
{
	if (Montage)
	{
		FAnimMontageInstance* MontageInstance = GetActiveInstanceForMontage(Montage);
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

float UAnimInstance::Montage_GetPlayRate(const UAnimMontage* Montage)
{
	if (Montage)
	{
		FAnimMontageInstance* MontageInstance = GetActiveInstanceForMontage(Montage);
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
		FAnimMontageInstance* MontageInstance = GetActiveInstanceForMontage(Montage);
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

FAnimMontageInstance* UAnimInstance::GetActiveMontageInstance() const
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
		if (MontageInstance && MontageInstance->Montage && (MontageInstance->Montage->GetGroupName() == InGroupName))
		{
			MontageInstances[InstanceIndex]->Stop(BlendOut, true);
		}
	}
}

void UAnimInstance::OnMontageInstanceStopped(FAnimMontageInstance& StoppedMontageInstance)
{
	ClearMontageInstanceReferences(StoppedMontageInstance);
}

void UAnimInstance::ClearMontageInstanceReferences(FAnimMontageInstance& InMontageInstance)
{
	if (UAnimMontage* MontageStopped = InMontageInstance.Montage)
	{
		// Remove instance for Active List.
		FAnimMontageInstance** AnimInstancePtr = ActiveMontagesMap.Find(MontageStopped);
		if (AnimInstancePtr && (*AnimInstancePtr == &InMontageInstance))
		{
			ActiveMontagesMap.Remove(MontageStopped);
		}
	}
	else
	{
		// If Montage ref is nullptr, it's possible the instance got terminated already and that is fine.
		// Make sure it's been removed from our ActiveMap though
		if (ActiveMontagesMap.FindKey(&InMontageInstance) != nullptr)
		{
			UE_LOG(LogAnimation, Warning, TEXT("%s: null montage found in the montage instance array!!"), *GetName());
		}
	}

	// Clear RootMotionMontageInstance
	if (RootMotionMontageInstance == &InMontageInstance)
	{
		RootMotionMontageInstance = nullptr;
	}

	// Clear any active synchronization
	InMontageInstance.MontageSync_StopFollowing();
	InMontageInstance.MontageSync_StopLeading();
}

FAnimMontageInstance* UAnimInstance::GetActiveInstanceForMontage(UAnimMontage const& Montage) const
{
	return GetActiveInstanceForMontage(&Montage);
}

FAnimMontageInstance* UAnimInstance::GetActiveInstanceForMontage(const UAnimMontage* Montage) const
{
	FAnimMontageInstance* const* FoundInstancePtr = ActiveMontagesMap.Find(Montage);
	return FoundInstancePtr ? *FoundInstancePtr : nullptr;
}

FAnimMontageInstance* UAnimInstance::GetMontageInstanceForID(int32 MontageInstanceID)
{
	for (FAnimMontageInstance* MontageInstance : MontageInstances)
	{
		if (MontageInstance && MontageInstance->GetInstanceID() == MontageInstanceID)
		{
			return MontageInstance;
		}
	}

	return nullptr;
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
		FRootMotionMovementParams RootMotion = GetProxyOnGameThread<FAnimInstanceProxy>().GetExtractedRootMotion();
		GetProxyOnGameThread<FAnimInstanceProxy>().GetExtractedRootMotion().Clear();
		return RootMotion;
	}
	else
	{
		return GetProxyOnGameThread<FAnimInstanceProxy>().GetExtractedRootMotion().ConsumeRootMotion(Alpha);
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
	return GetProxyOnAnyThread<FAnimInstanceProxy>().GetTimeToClosestMarker(SyncGroup, MarkerName, OutMarkerTime);
}

bool UAnimInstance::HasMarkerBeenHitThisFrame(FName SyncGroup, FName MarkerName) const
{
	return GetProxyOnAnyThread<FAnimInstanceProxy>().HasMarkerBeenHitThisFrame(SyncGroup, MarkerName);
}

bool UAnimInstance::IsSyncGroupBetweenMarkers(FName InSyncGroupName, FName PreviousMarker, FName NextMarker, bool bRespectMarkerOrder) const
{
	return GetProxyOnAnyThread<FAnimInstanceProxy>().IsSyncGroupBetweenMarkers(InSyncGroupName, PreviousMarker, NextMarker, bRespectMarkerOrder);
}

FMarkerSyncAnimPosition UAnimInstance::GetSyncGroupPosition(FName InSyncGroupName) const
{
	return GetProxyOnAnyThread<FAnimInstanceProxy>().GetSyncGroupPosition(InSyncGroupName);
}

void UAnimInstance::UpdateMontageEvaluationData()
{
	FAnimInstanceProxy& Proxy = GetProxyOnGameThread<FAnimInstanceProxy>();

	Proxy.GetMontageEvaluationData().Reset(MontageInstances.Num());
	UE_LOG(LogAnimMontage, Verbose, TEXT("UpdateMontageEvaluationData Strting: Owner: %s"),	*GetNameSafe(GetOwningActor()));

	for (FAnimMontageInstance* MontageInstance : MontageInstances)
	{
		// although montage can advance with 0.f weight, it is fine to filter by weight here
		// because we don't want to evaluate them if 0 weight
		if (MontageInstance->Montage && MontageInstance->GetWeight() > ZERO_ANIMWEIGHT_THRESH)
		{
			UE_LOG(LogAnimMontage, Verbose, TEXT("UpdateMontageEvaluationData : AnimMontage: %s,  (DesiredWeight:%0.2f, Weight:%0.2f)"),
						*MontageInstance->Montage->GetName(), MontageInstance->GetDesiredWeight(), MontageInstance->GetWeight());
			Proxy.GetMontageEvaluationData().Add(FMontageEvaluationState(MontageInstance->Montage, MontageInstance->GetWeight(), MontageInstance->GetDesiredWeight(), MontageInstance->GetPosition(), MontageInstance->bPlaying, MontageInstance->IsActive()));
		}
	}
}

float UAnimInstance::GetInstanceAssetPlayerLength(int32 AssetPlayerIndex)
{
	return GetProxyOnAnyThread<FAnimInstanceProxy>().GetInstanceAssetPlayerLength(AssetPlayerIndex);
}

float UAnimInstance::GetInstanceAssetPlayerTime(int32 AssetPlayerIndex)
{
	return GetProxyOnAnyThread<FAnimInstanceProxy>().GetInstanceAssetPlayerTime(AssetPlayerIndex);
}

float UAnimInstance::GetInstanceAssetPlayerTimeFraction(int32 AssetPlayerIndex)
{
	return GetProxyOnAnyThread<FAnimInstanceProxy>().GetInstanceAssetPlayerTimeFraction(AssetPlayerIndex);
}

float UAnimInstance::GetInstanceAssetPlayerTimeFromEndFraction(int32 AssetPlayerIndex)
{
	return GetProxyOnAnyThread<FAnimInstanceProxy>().GetInstanceAssetPlayerTimeFromEndFraction(AssetPlayerIndex);
}

float UAnimInstance::GetInstanceAssetPlayerTimeFromEnd(int32 AssetPlayerIndex)
{
	return GetProxyOnAnyThread<FAnimInstanceProxy>().GetInstanceAssetPlayerTimeFromEnd(AssetPlayerIndex);
}

float UAnimInstance::GetInstanceMachineWeight(int32 MachineIndex)
{
	return GetProxyOnAnyThread<FAnimInstanceProxy>().GetInstanceMachineWeight(MachineIndex);
}

float UAnimInstance::GetInstanceStateWeight(int32 MachineIndex, int32 StateIndex)
{
	return GetProxyOnAnyThread<FAnimInstanceProxy>().GetInstanceStateWeight(MachineIndex, StateIndex);
}

float UAnimInstance::GetInstanceCurrentStateElapsedTime(int32 MachineIndex)
{
	return GetProxyOnAnyThread<FAnimInstanceProxy>().GetInstanceCurrentStateElapsedTime(MachineIndex);
}

float UAnimInstance::GetInstanceTransitionCrossfadeDuration(int32 MachineIndex, int32 TransitionIndex)
{
	return GetProxyOnAnyThread<FAnimInstanceProxy>().GetInstanceTransitionCrossfadeDuration(MachineIndex, TransitionIndex);
}

float UAnimInstance::GetInstanceTransitionTimeElapsed(int32 MachineIndex, int32 TransitionIndex)
{
	// Just an alias for readability in the anim graph
	return GetInstanceCurrentStateElapsedTime(MachineIndex);
}

float UAnimInstance::GetInstanceTransitionTimeElapsedFraction(int32 MachineIndex, int32 TransitionIndex)
{
	return GetProxyOnAnyThread<FAnimInstanceProxy>().GetInstanceTransitionTimeElapsedFraction(MachineIndex, TransitionIndex);
}

float UAnimInstance::GetRelevantAnimTimeRemaining(int32 MachineIndex, int32 StateIndex)
{
	return GetProxyOnAnyThread<FAnimInstanceProxy>().GetRelevantAnimTimeRemaining(MachineIndex, StateIndex);
}

float UAnimInstance::GetRelevantAnimTimeRemainingFraction(int32 MachineIndex, int32 StateIndex)
{
	return GetProxyOnAnyThread<FAnimInstanceProxy>().GetRelevantAnimTimeRemainingFraction(MachineIndex, StateIndex);
}

float UAnimInstance::GetRelevantAnimLength(int32 MachineIndex, int32 StateIndex)
{
	return GetProxyOnAnyThread<FAnimInstanceProxy>().GetRelevantAnimLength(MachineIndex, StateIndex);
}

float UAnimInstance::GetRelevantAnimTime(int32 MachineIndex, int32 StateIndex)
{
	return GetProxyOnAnyThread<FAnimInstanceProxy>().GetRelevantAnimTime(MachineIndex, StateIndex);
}

float UAnimInstance::GetRelevantAnimTimeFraction(int32 MachineIndex, int32 StateIndex)
{
	return GetProxyOnAnyThread<FAnimInstanceProxy>().GetRelevantAnimTimeFraction(MachineIndex, StateIndex);
}

FAnimNode_StateMachine* UAnimInstance::GetStateMachineInstance(int32 MachineIndex)
{
	return GetProxyOnAnyThread<FAnimInstanceProxy>().GetStateMachineInstance(MachineIndex);
}

FAnimNode_StateMachine* UAnimInstance::GetStateMachineInstanceFromName(FName MachineName)
{
	return GetProxyOnAnyThread<FAnimInstanceProxy>().GetStateMachineInstanceFromName(MachineName);
}

const FBakedAnimationStateMachine* UAnimInstance::GetStateMachineInstanceDesc(FName MachineName)
{
	return GetProxyOnAnyThread<FAnimInstanceProxy>().GetStateMachineInstanceDesc(MachineName);
}

int32 UAnimInstance::GetStateMachineIndex(FName MachineName)
{
	return GetProxyOnAnyThread<FAnimInstanceProxy>().GetStateMachineIndex(MachineName);
}

void UAnimInstance::GetStateMachineIndexAndDescription(FName InMachineName, int32& OutMachineIndex, const FBakedAnimationStateMachine** OutMachineDescription)
{
	return GetProxyOnAnyThread<FAnimInstanceProxy>().GetStateMachineIndexAndDescription(InMachineName, OutMachineIndex, OutMachineDescription);
}

FAnimNode_Base* UAnimInstance::GetCheckedNodeFromIndexUntyped(int32 NodeIdx, UScriptStruct* RequiredStructType)
{
	return GetProxyOnGameThread<FAnimInstanceProxy>().GetCheckedNodeFromIndexUntyped(NodeIdx, RequiredStructType);
}

FAnimNode_Base* UAnimInstance::GetNodeFromIndexUntyped(int32 NodeIdx, UScriptStruct* RequiredStructType)
{
	return GetProxyOnGameThread<FAnimInstanceProxy>().GetNodeFromIndexUntyped(NodeIdx, RequiredStructType);
}

const FBakedAnimationStateMachine* UAnimInstance::GetMachineDescription(IAnimClassInterface* AnimBlueprintClass, FAnimNode_StateMachine* MachineInstance)
{
	return GetProxyOnGameThread<FAnimInstanceProxy>().GetMachineDescription(AnimBlueprintClass, MachineInstance);
}

int32 UAnimInstance::GetInstanceAssetPlayerIndex(FName MachineName, FName StateName, FName AssetName)
{
	return GetProxyOnGameThread<FAnimInstanceProxy>().GetInstanceAssetPlayerIndex(MachineName, StateName, AssetName);
}

FAnimNode_AssetPlayerBase* UAnimInstance::GetRelevantAssetPlayerFromState(int32 MachineIndex, int32 StateIndex)
{
	return GetProxyOnGameThread<FAnimInstanceProxy>().GetRelevantAssetPlayerFromState(MachineIndex, StateIndex);
}

int32 UAnimInstance::GetSyncGroupIndexFromName(FName SyncGroupName) const
{
	return GetProxyOnGameThread<FAnimInstanceProxy>().GetSyncGroupIndexFromName(SyncGroupName);
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

FAnimInstanceProxy* UAnimInstance::CreateAnimInstanceProxy()
{
	return new FAnimInstanceProxy(this);
}

void UAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete InProxy;
}

void UAnimInstance::RecordMachineWeight(const int32& InMachineClassIndex, const float& InMachineWeight)
{
	GetProxyOnAnyThread<FAnimInstanceProxy>().RecordMachineWeight(InMachineClassIndex, InMachineWeight);
}

void UAnimInstance::RecordStateWeight(const int32& InMachineClassIndex, const int32& InStateIndex, const float& InStateWeight)
{
	GetProxyOnAnyThread<FAnimInstanceProxy>().RecordStateWeight(InMachineClassIndex, InStateIndex, InStateWeight);
}

FBoneContainer& UAnimInstance::GetRequiredBones()
{
	return GetProxyOnGameThread<FAnimInstanceProxy>().GetRequiredBones();
}

void UAnimInstance::QueueRootMotionBlend(const FTransform& RootTransform, const FName& SlotName, float Weight)
{
	RootMotionBlendQueue.Add(FQueuedRootMotionBlend(RootTransform, SlotName, Weight));
}

#if WITH_EDITOR
void UAnimInstance::AddDelegate_AddCustomAnimationCurve(FOnAddCustomAnimationCurves& InOnAddCustomAnimationCurves)
{
	if (InOnAddCustomAnimationCurves.IsBound())
	{
		OnAddAnimationCurves.Add(InOnAddCustomAnimationCurves);
	}
}

void UAnimInstance::RemoveDelegate_AddCustomAnimationCurve(FOnAddCustomAnimationCurves& InOnAddCustomAnimationCurves)
{
	for (int32 DelegateId = 0; DelegateId < OnAddAnimationCurves.Num(); ++DelegateId)
	{
		if (InOnAddCustomAnimationCurves.GetHandle() == OnAddAnimationCurves[DelegateId].GetHandle())
		{
			InOnAddCustomAnimationCurves.Unbind();
			OnAddAnimationCurves.RemoveAt(DelegateId);
			break;
		}
	}
}

#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE 
