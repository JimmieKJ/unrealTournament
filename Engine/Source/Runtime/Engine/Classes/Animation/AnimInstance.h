// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimationAsset.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "AnimStateMachineTypes.h"
#include "BonePose.h"
#include "Animation/AnimTypes.h"
#include "Animation/AnimNotifyQueue.h"
#include "Animation/AnimClassInterface.h"
#include "AnimInstance.generated.h"

struct FAnimMontageInstance;
class UAnimMontage;
class USkeleton;
class AActor;
class UAnimSequenceBase;
class UBlendSpaceBase;
class APawn;
class UAnimationAsset;
class UCanvas;
class UWorld;
struct FTransform;
class FDebugDisplayInfo;
struct FAnimNode_AssetPlayerBase;
struct FAnimNode_Base;
struct FAnimInstanceProxy;

UENUM()
enum class EAnimCurveType : uint8 
{
	AttributeCurve,
	MaterialCurve, 
	MorphTargetCurve, 
	// make sure to update MaxCurve 
	MaxAnimCurveType
};

UENUM()
enum class EMontagePlayReturnType : uint8
{
	//Return value is the length of the montage (in seconds)
	MontageLength,
	//Return value is the play duration of the montage (length / play rate, in seconds)
	Duration,
};

DECLARE_DELEGATE_OneParam(FOnMontageStarted, UAnimMontage*)
DECLARE_DELEGATE_TwoParams(FOnMontageEnded, UAnimMontage*, bool /*bInterrupted*/)
DECLARE_DELEGATE_TwoParams(FOnMontageBlendingOutStarted, UAnimMontage*, bool /*bInterrupted*/)
/**
* Delegate for when Montage is started
*/
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMontageStartedMCDelegate, UAnimMontage*, Montage);

/**
* Delegate for when Montage is completed, whether interrupted or finished
* Weight of this montage is 0.f, so it stops contributing to output pose
*
* bInterrupted = true if it was not property finished
*/
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMontageEndedMCDelegate, UAnimMontage*, Montage, bool, bInterrupted);

/**
* Delegate for when Montage started to blend out, whether interrupted or finished
* DesiredWeight of this montage becomes 0.f, but this still contributes to the output pose
*
* bInterrupted = true if it was not property finished
*/
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMontageBlendingOutStartedMCDelegate, UAnimMontage*, Montage, bool, bInterrupted);

/** Delegate that native code can hook to to provide additional transition logic */
DECLARE_DELEGATE_RetVal(bool, FCanTakeTransition);

/** Delegate that native code can hook into to handle state entry/exit */
DECLARE_DELEGATE_ThreeParams(FOnGraphStateChanged, const struct FAnimNode_StateMachine& /*Machine*/, int32 /*PrevStateIndex*/, int32 /*NextStateIndex*/);

/** Delegate that allows users to insert custom animation curve values - for now, it's only single, not sure how to make this to multi delegate and retrieve value sequentially, so */
DECLARE_DELEGATE_OneParam(FOnAddCustomAnimationCurves, UAnimInstance*)



USTRUCT()
struct FA2Pose
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<FTransform> Bones;

	FA2Pose() {}
};

/** Component space poses. */
USTRUCT()
struct ENGINE_API FA2CSPose : public FA2Pose
{
	GENERATED_USTRUCT_BODY()

private:
	/** Pointer to current BoneContainer. */
	const struct FBoneContainer* BoneContainer;

	/** Once evaluated to be mesh space, this flag will be set. */
	UPROPERTY()
	TArray<uint8> ComponentSpaceFlags;

public:
	FA2CSPose()
		: BoneContainer(NULL)
	{
	}

	/** Constructor - needs LocalPoses. */
	void AllocateLocalPoses(const FBoneContainer& InBoneContainer, const FA2Pose & LocalPose);

	/** Constructor - needs LocalPoses. */
	void AllocateLocalPoses(const FBoneContainer& InBoneContainer, const FTransformArrayA2 & LocalBones);

	/** Returns if this struct is valid. */
	bool IsValid() const;

	/** Get parent bone index for given bone index. */
	int32 GetParentBoneIndex(const int32& BoneIndex) const;

	/** Returns local transform for the bone index. **/
	FTransform GetLocalSpaceTransform(int32 BoneIndex);

	/** Do not access Bones array directly; use this instead. This will fill up gradually mesh space bases. */
	FTransform GetComponentSpaceTransform(int32 BoneIndex);

	/** convert to local poses **/
	void ConvertToLocalPoses(FA2Pose & LocalPoses) const;

private:
	/** Calculate all transform till parent **/
	void CalculateComponentSpaceTransform(int32 Index);
	void SetComponentSpaceTransform(int32 Index, const FTransform& NewTransform);

	/**
	 * Convert Bone to Local Space.
	 */
	void ConvertBoneToLocalSpace(int32 BoneIndex);


	void SetLocalSpaceTransform(int32 Index, const FTransform& NewTransform);

	// This is not really best way to protect SetComponentSpaceTransform, but we'd like to make sure that isn't called by anywhere else.
	friend class FAnimationRuntime;
};



/** Helper struct for Slot node pose evaluation. */
USTRUCT()
struct FSlotEvaluationPose
{
	GENERATED_USTRUCT_BODY()

	/** Type of additive for pose */
	UPROPERTY()
	TEnumAsByte<EAdditiveAnimationType> AdditiveType;

	/** Weight of pose */
	UPROPERTY()
	float Weight;

	/*** ATTENTION *****/
	/* These Pose/Curve is stack allocator. You should not use it outside of stack. */
	FCompactPose Pose;
	FBlendedCurve Curve;

	FSlotEvaluationPose()
	{
	}

	FSlotEvaluationPose(float InWeight, EAdditiveAnimationType InAdditiveType)
		: AdditiveType(InAdditiveType)
		, Weight(InWeight)
	{
	}
};

/** Helper struct to store a Queued Montage BlendingOut event. */
struct FQueuedMontageBlendingOutEvent
{
	class UAnimMontage* Montage;
	bool bInterrupted;
	FOnMontageBlendingOutStarted Delegate;

	FQueuedMontageBlendingOutEvent()
		: Montage(NULL)
		, bInterrupted(false)
	{}

	FQueuedMontageBlendingOutEvent(class UAnimMontage* InMontage, bool InbInterrupted, FOnMontageBlendingOutStarted InDelegate)
		: Montage(InMontage)
		, bInterrupted(InbInterrupted)
		, Delegate(InDelegate)
	{}
};

/** Helper struct to store a Queued Montage Ended event. */
struct FQueuedMontageEndedEvent
{
	class UAnimMontage* Montage;
	bool bInterrupted;
	FOnMontageEnded Delegate;

	FQueuedMontageEndedEvent()
		: Montage(NULL)
		, bInterrupted(false)
	{}

	FQueuedMontageEndedEvent(class UAnimMontage* InMontage, bool InbInterrupted, FOnMontageEnded InDelegate)
		: Montage(InMontage)
		, bInterrupted(InbInterrupted)
		, Delegate(InDelegate)
	{}
};

/** Binding allowing native transition rule evaluation */
struct FNativeTransitionBinding
{
	/** State machine to bind to */
	FName MachineName;

	/** Previous state the transition comes from */
	FName PreviousStateName;

	/** Next state the transition goes to */
	FName NextStateName;

	/** Delegate to use when checking transition */
	FCanTakeTransition NativeTransitionDelegate;

#if WITH_EDITORONLY_DATA
	/** Name of this transition rule */
	FName TransitionName;
#endif

	FNativeTransitionBinding(const FName& InMachineName, const FName& InPreviousStateName, const FName& InNextStateName, const FCanTakeTransition& InNativeTransitionDelegate, const FName& InTransitionName = NAME_None)
		: MachineName(InMachineName)
		, PreviousStateName(InPreviousStateName)
		, NextStateName(InNextStateName)
		, NativeTransitionDelegate(InNativeTransitionDelegate)
#if WITH_EDITORONLY_DATA
		, TransitionName(InTransitionName)
#endif
	{
	}
};

/** Binding allowing native notification of state changes */
struct FNativeStateBinding
{
	/** State machine to bind to */
	FName MachineName;

	/** State to bind to */
	FName StateName;

	/** Delegate to use when checking transition */
	FOnGraphStateChanged NativeStateDelegate;

#if WITH_EDITORONLY_DATA
	/** Name of this binding */
	FName BindingName;
#endif

	FNativeStateBinding(const FName& InMachineName, const FName& InStateName, const FOnGraphStateChanged& InNativeStateDelegate, const FName& InBindingName = NAME_None)
		: MachineName(InMachineName)
		, StateName(InStateName)
		, NativeStateDelegate(InNativeStateDelegate)
#if WITH_EDITORONLY_DATA
		, BindingName(InBindingName)
#endif
	{
	}
};

/** Tracks state of active slot nodes in the graph */
struct FMontageActiveSlotTracker
{
	/** Local weight of Montages being played (local to the slot node) */
	float MontageLocalWeight;

	/** Global weight of this slot node */
	float NodeGlobalWeight;

	//Is the montage slot part of the active graph this tick
	bool  bIsRelevantThisTick;

	//Was the montage slot part of the active graph last tick
	bool  bWasRelevantOnPreviousTick;

	FMontageActiveSlotTracker()
		: MontageLocalWeight(0.f)
		, NodeGlobalWeight(0.f)
		, bIsRelevantThisTick(false)
		, bWasRelevantOnPreviousTick(false) 
	{}
};

struct FMontageEvaluationState
{
	FMontageEvaluationState(UAnimMontage* InMontage, float InWeight, float InDesiredWeight, float InPosition, bool bInIsPlaying, bool bInIsActive) 
		: Montage(InMontage)
		, MontageWeight(InWeight)
		, DesiredWeight(InDesiredWeight)
		, MontagePosition(InPosition)
		, bIsPlaying(bInIsPlaying)
		, bIsActive(bInIsActive)
	{}

	// The montage to evaluate
	UAnimMontage* Montage;

	// The weight to use for this montage
	float MontageWeight;

	// The desired weight of this montage
	float DesiredWeight;

	// The position to evaluate this montage at
	float MontagePosition;

	// Whether this montage is playing
	bool bIsPlaying;

	// Whether this montage is valid and not stopped
	bool bIsActive;
};

UCLASS(transient, Blueprintable, hideCategories=AnimInstance, BlueprintType)
class ENGINE_API UAnimInstance : public UObject
{
	GENERATED_UCLASS_BODY()

	typedef FAnimInstanceProxy ProxyType;

	// Disable compiler-generated deprecation warnings by implementing our own destructor
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	~UAnimInstance() {}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/** DeltaTime **/
	UPROPERTY()
	float DeltaTime_DEPRECATED;

	/** This is used to extract animation. If Mesh exists, this will be overwritten by Mesh->Skeleton */
	UPROPERTY(transient)
	USkeleton* CurrentSkeleton;

	// The list of animation assets which are going to be evaluated this frame and need to be ticked (ungrouped)
	DEPRECATED(4.11, "This cannot be accessed directly as it is potentially in use on worker threads")
	TArray<FAnimTickRecord> UngroupedActivePlayerArrays[2];

	// The set of tick groups for this anim instance
	DEPRECATED(4.11, "This cannot be accessed directly as it is potentially in use on worker threads")
	TArray<FAnimGroupInstance> SyncGroupArrays[2];

	// Sets where this blueprint pulls Root Motion from
	UPROPERTY(Category = RootMotion, EditDefaultsOnly)
	TEnumAsByte<ERootMotionMode::Type> RootMotionMode;

	// Allows this anim instance to update its native update, blend tree, montages and asset players on
	// a worker thread. this requires certain conditions to be met:
	// - All access of variables in the blend tree should be a direct access of a member variable
	// - No BlueprintUpdateAnimation event should be used (i.e. the event graph should be empty). Only native update is permitted.
	UPROPERTY(Category = Optimization, EditDefaultsOnly)
	bool bRunUpdatesInWorkerThreads;

	/** 
	 * Whether we can use parallel updates for our animations.
	 * Conditions affecting this include:
	 * - Use of BlueprintUpdateAnimation
	 * - Use of non 'fast-path' EvaluateGraphExposedInputs in the node graph
	 */
	UPROPERTY()
	bool bCanUseParallelUpdateAnimation;

	/**
	 * Selecting this option will cause the compiler to emit warnings whenever a call into Blueprint
	 * is made from the animation graph. This can help track down optimizations that need to be made.
	 */
	UPROPERTY(Category = Optimization, EditDefaultsOnly)
	bool bWarnAboutBlueprintUsage;

	/** Flag to check back on the game thread that indicates we need to run PostUpdateAnimation() in the post-eval call */
	bool bNeedsUpdate;

public:

	// @todo document
	DEPRECATED(4.11, "This cannot be called directly as it relies on data potentially in use on worker threads")
	void MakeSequenceTickRecord(FAnimTickRecord& TickRecord, UAnimSequenceBase* Sequence, bool bLooping, float PlayRate, float FinalBlendWeight, float& CurrentTime, FMarkerTickRecord& MarkerTickRecord) const;
	DEPRECATED(4.11, "This cannot be called directly as it relies on data potentially in use on worker threads")
	void MakeBlendSpaceTickRecord(FAnimTickRecord& TickRecord, UBlendSpaceBase* BlendSpace, const FVector& BlendInput, TArray<FBlendSampleData>& BlendSampleDataCache, FBlendFilter& BlendFilter, bool bLooping, float PlayRate, float FinalBlendWeight, float& CurrentTime, FMarkerTickRecord& MarkerTickRecord) const;
	void MakeMontageTickRecord(FAnimTickRecord& TickRecord, class UAnimMontage* Montage, float CurrentPosition, float PreviousPosition, float MoveDelta, float Weight, TArray<FPassedMarker>& MarkersPassedThisTick, FMarkerTickRecord& MarkerTickRecord);

	DEPRECATED(4.11, "This cannot be called directly as it relies on data potentially in use on worker threads")
	void SequenceAdvanceImmediate(UAnimSequenceBase* Sequence, bool bLooping, float PlayRate, float DeltaSeconds, /*inout*/ float& CurrentTime, FMarkerTickRecord& MarkerTickRecord);

	// @todo document
	DEPRECATED(4.11, "This cannot be called directly as it relies on data potentially in use on worker threads")
	void BlendSpaceAdvanceImmediate(UBlendSpaceBase* BlendSpace, const FVector& BlendInput, TArray<FBlendSampleData> & BlendSampleDataCache, FBlendFilter & BlendFilter, bool bLooping, float PlayRate, float DeltaSeconds, /*inout*/ float& CurrentTime, FMarkerTickRecord& MarkerTickRecord);

	// Creates an uninitialized tick record in the list for the correct group or the ungrouped array.  If the group is valid, OutSyncGroupPtr will point to the group.
	DEPRECATED(4.11, "This cannot be called directly as it relies on data potentially in use on worker threads")
	FAnimTickRecord& CreateUninitializedTickRecord(int32 GroupIndex, FAnimGroupInstance*& OutSyncGroupPtr);

	/**
	 * Get Slot Node Weight : this returns new Slot Node Weight, Source Weight, Original TotalNodeWeight
	 *							this 3 values can't be derived from each other
	 *
	 * @param SlotNodeName : the name of the slot node you're querying
	 * @param out_SlotNodeWeight : The node weight for this slot node in the range of [0, 1]
	 * @param out_SourceWeight : The Source weight for this node. 
	 * @param out_TotalNodeWeight : Total weight of this node
	 */
	DEPRECATED(4.11, "This cannot be called directly as it relies on data potentially in use on worker threads")
	void GetSlotWeight(FName const& SlotNodeName, float& out_SlotNodeWeight, float& out_SourceWeight, float& out_TotalNodeWeight) const;

	DEPRECATED(4.11, "This cannot be called directly as it relies on data potentially in use on worker threads")
	void SlotEvaluatePose(FName SlotNodeName, const FCompactPose& SourcePose, const FBlendedCurve& SourceCurve, float InSourceWeight, FCompactPose& BlendedPose, FBlendedCurve& BlendedCurve, float InBlendWeight, float InTotalNodeWeight);

	// slot node run-time functions
	DEPRECATED(4.11, "This cannot be called directly as it relies on data potentially in use on worker threads")
	void ReinitializeSlotNodes();
	DEPRECATED(4.11, "This cannot be called directly as it relies on data potentially in use on worker threads")
	void RegisterSlotNodeWithAnimInstance(FName SlotNodeName);
	DEPRECATED(4.11, "This cannot be called directly as it relies on data potentially in use on worker threads")
	void UpdateSlotNodeWeight(FName SlotNodeName, float InLocalMontageWeight, float InGlobalWeight);
	// if it doesn't tick, it will keep old weight, so we'll have to clear it in the beginning of tick
	DEPRECATED(4.11, "This cannot be called directly as it relies on data potentially in use on worker threads")
	void ClearSlotNodeWeights();
	bool IsSlotNodeRelevantForNotifies(FName SlotNodeName) const;

	/** Get global weight in AnimGraph for this slot node.
	* Note: this is the weight of the node, not the weight of any potential montage it is playing. */
	float GetSlotNodeGlobalWeight(FName SlotNodeName) const;

	// Should Extract Root Motion or not. Return true if we do. 
	bool ShouldExtractRootMotion() const { return RootMotionMode == ERootMotionMode::RootMotionFromEverything || RootMotionMode == ERootMotionMode::IgnoreRootMotion; }

	/** Get Global weight of any montages this slot node is playing.
	* If this slot is not currently playing a montage, it will return 0. */
	float GetSlotMontageGlobalWeight(FName SlotNodeName) const;

	// kismet event functions

	UFUNCTION(BlueprintCallable, Category = "Animation")
	virtual APawn* TryGetPawnOwner() const;

	// Are we being evaluated on a worker thread
	bool IsRunningParallelEvaluation() const;

	// Can does this anim instance need an update (parallel or not)?
	bool NeedsUpdate() const;

private:
	// Does this anim instance need immediate update (rather than parallel)?
	bool NeedsImmediateUpdate(float DeltaSeconds) const;

public:
	/** Returns the owning actor of this AnimInstance */
	UFUNCTION(BlueprintCallable, Category = "Animation")
	AActor* GetOwningActor() const;
	
	// Returns the skeletal mesh component that has created this AnimInstance
	UFUNCTION(BlueprintCallable, Category = "Animation")
	USkeletalMeshComponent* GetOwningComponent() const;

public:

	/** Executed when the Animation is initialized */
	UFUNCTION(BlueprintImplementableEvent)
	void BlueprintInitializeAnimation();

	/** Executed when the Animation is updated */
	UFUNCTION(BlueprintImplementableEvent)
	void BlueprintUpdateAnimation(float DeltaTimeX);

	/** Executed after the Animation is evaluated */
	UFUNCTION(BlueprintImplementableEvent)
	void BlueprintPostEvaluateAnimation();

	bool CanTransitionSignature() const;
	
	/*********************************************************************************************
	* SlotAnimation
	********************************************************************************************* */
public:

	/** DEPRECATED. Use PlaySlotAnimationAsDynamicMontage instead, it returns the UAnimMontage created instead of time, allowing more control */
	/** Play normal animation asset on the slot node. You can only play one asset (whether montage or animsequence) at a time. */
	DEPRECATED(4.9, "This function is deprecated, please use PlaySlotAnimationAsDynamicMontage instead.")
	UFUNCTION(BlueprintCallable, Category="Animation")
	float PlaySlotAnimation(UAnimSequenceBase* Asset, FName SlotNodeName, float BlendInTime = 0.25f, float BlendOutTime = 0.25f, float InPlayRate = 1.f, int32 LoopCount = 1);

	/** Play normal animation asset on the slot node by creating a dynamic UAnimMontage. You can only play one asset (whether montage or animsequence) at a time per SlotGroup. */
	UFUNCTION(BlueprintCallable, Category="Animation")
	UAnimMontage* PlaySlotAnimationAsDynamicMontage(UAnimSequenceBase* Asset, FName SlotNodeName, float BlendInTime = 0.25f, float BlendOutTime = 0.25f, float InPlayRate = 1.f, int32 LoopCount = 1, float BlendOutTriggerTime = -1.f);

	/** Stops currently playing slot animation slot or all*/
	UFUNCTION(BlueprintCallable, Category="Animation")
	void StopSlotAnimation(float InBlendOutTime = 0.25f, FName SlotNodeName = NAME_None);

	/** Return true if it's playing the slot animation */
	UFUNCTION(BlueprintPure, Category="Animation")
	bool IsPlayingSlotAnimation(const UAnimSequenceBase* Asset, FName SlotNodeName) const;

	/** Return true if this instance playing the slot animation, also returning the montage it is playing on */
	bool IsPlayingSlotAnimation(const UAnimSequenceBase* Asset, FName SlotNodeName, UAnimMontage*& OutMontage) const;

	/*********************************************************************************************
	 * AnimMontage
	 ********************************************************************************************* */
public:
	/** Plays an animation montage. Returns the length of the animation montage in seconds. Returns 0.f if failed to play. */
	UFUNCTION(BlueprintCallable, Category = "Animation")
	float Montage_Play(UAnimMontage* MontageToPlay, float InPlayRate = 1.f, EMontagePlayReturnType ReturnValueType = EMontagePlayReturnType::MontageLength);

	/** Stops the animation montage. If reference is NULL, it will stop ALL active montages. */
	UFUNCTION(BlueprintCallable, Category = "Animation")
	void Montage_Stop(float InBlendOutTime, const UAnimMontage* Montage = NULL);

	/** Pauses the animation montage. If reference is NULL, it will pause ALL active montages. */
	UFUNCTION(BlueprintCallable, Category = "Animation")
	void Montage_Pause(const UAnimMontage* Montage = NULL);

	/** Resumes a paused animation montage. If reference is NULL, it will resume ALL active montages. */
	UFUNCTION(BlueprintCallable, Category = "Animation")
	void Montage_Resume(const UAnimMontage* Montage);

	/** Makes a montage jump to a named section. If Montage reference is NULL, it will do that to all active montages. */
	UFUNCTION(BlueprintCallable, Category="Animation")
	void Montage_JumpToSection(FName SectionName, const UAnimMontage* Montage = NULL);

	/** Makes a montage jump to the end of a named section. If Montage reference is NULL, it will do that to all active montages. */
	UFUNCTION(BlueprintCallable, Category="Animation")
	void Montage_JumpToSectionsEnd(FName SectionName, const UAnimMontage* Montage = NULL);

	/** Relink new next section AFTER SectionNameToChange in run-time
	 *	You can link section order the way you like in editor, but in run-time if you'd like to change it dynamically, 
	 *	use this function to relink the next section
	 *	For example, you can have Start->Loop->Loop->Loop.... but when you want it to end, you can relink
	 *	next section of Loop to be End to finish the montage, in which case, it stops looping by Loop->End. 
	 
	 * @param SectionNameToChange : This should be the name of the Montage Section after which you want to insert a new next section
	 * @param NextSection	: new next section 
	 */
	UFUNCTION(BlueprintCallable, Category="Animation")
	void Montage_SetNextSection(FName SectionNameToChange, FName NextSection, const UAnimMontage* Montage = NULL);

	/** Change AnimMontage play rate. NewPlayRate = 1.0 is the default playback rate. */
	UFUNCTION(BlueprintCallable, Category="Animation")
	void Montage_SetPlayRate(const UAnimMontage* Montage, float NewPlayRate = 1.f);

	/** Returns true if the animation montage is active. If the Montage reference is NULL, it will return true if any Montage is active. */
	UFUNCTION(BlueprintPure, Category="Animation")
	bool Montage_IsActive(const UAnimMontage* Montage) const;

	/** Returns true if the animation montage is currently active and playing. 
	If reference is NULL, it will return true is ANY montage is currently active and playing. */
	UFUNCTION(BlueprintPure, Category="Animation")
	bool Montage_IsPlaying(const UAnimMontage* Montage) const;

	/** Returns the name of the current animation montage section. */
	UFUNCTION(BlueprintPure, Category="Animation")
	FName Montage_GetCurrentSection(const UAnimMontage* Montage = NULL) const;

	/** Called when a montage starts blending out, whether interrupted or finished */
	UPROPERTY(BlueprintAssignable)
	FOnMontageBlendingOutStartedMCDelegate OnMontageBlendingOut;
	
	/** Called when a montage has started */
	UPROPERTY(BlueprintAssignable)
	FOnMontageStartedMCDelegate OnMontageStarted;

	/** Called when a montage has ended, whether interrupted or finished*/
	UPROPERTY(BlueprintAssignable)
	FOnMontageEndedMCDelegate OnMontageEnded;

	/*********************************************************************************************
	* AnimMontage native C++ interface
	********************************************************************************************* */
public:	
	void Montage_SetEndDelegate(FOnMontageEnded & InOnMontageEnded, UAnimMontage* Montage = NULL);
	
	void Montage_SetBlendingOutDelegate(FOnMontageBlendingOutStarted & InOnMontageBlendingOut, UAnimMontage* Montage = NULL);
	
	/** Get pointer to BlendingOutStarted delegate for Montage.
	If Montage reference is NULL, it will pick the first active montage found. */
	FOnMontageBlendingOutStarted* Montage_GetBlendingOutDelegate(UAnimMontage* Montage = NULL);

	/** Get Current Montage Position */
	float Montage_GetPosition(const UAnimMontage* Montage);
	
	/** Set position. */
	void Montage_SetPosition(const UAnimMontage* Montage, float NewPosition);
	
	/** return true if Montage is not currently active. (not valid or blending out) */
	bool Montage_GetIsStopped(const UAnimMontage* Montage);

	/** Get the current blend time of the Montage.
	If Montage reference is NULL, it will return the current blend time on the first active Montage found. */
	float Montage_GetBlendTime(const UAnimMontage* Montage);

	/** Get PlayRate for Montage.
	If Montage reference is NULL, PlayRate for any Active Montage will be returned.
	If Montage is not playing, 0 is returned. */
	float Montage_GetPlayRate(const UAnimMontage* Montage);

	/** Get next sectionID for given section ID */
	int32 Montage_GetNextSectionID(const UAnimMontage* Montage, int32 const & CurrentSectionID) const;

	/** Returns true if any montage is playing currently. Doesn't mean it's active though, it could be blending out. */
	bool IsAnyMontagePlaying() const;

	/** Get a current Active Montage in this AnimInstance. 
		Note that there might be multiple Active at the same time. This will only return the first active one it finds. **/
	UAnimMontage* GetCurrentActiveMontage();

	/** Get Currently active montage instance.
		Note that there might be multiple Active at the same time. This will only return the first active one it finds. **/
	FAnimMontageInstance* GetActiveMontageInstance() const;

	/** Get Active FAnimMontageInstance for given Montage asset. Will return NULL if Montage is not currently Active. */
	DEPRECATED(4.13, "Please use GetActiveInstanceForMontage(const UAnimMontage* Montage)")
	FAnimMontageInstance* GetActiveInstanceForMontage(UAnimMontage const& Montage) const;

	/** Get Active FAnimMontageInstance for given Montage asset. Will return NULL if Montage is not currently Active. */
	FAnimMontageInstance* GetActiveInstanceForMontage(const UAnimMontage* Montage) const;

	/** Get the FAnimMontageInstance currently running that matches this ID.  Will return NULL if no instance is found. */
	FAnimMontageInstance* GetMontageInstanceForID(int32 MontageInstanceID);

	/** AnimMontage instances that are running currently
	* - only one is primarily active per group, and the other ones are blending out
	*/
	TArray<struct FAnimMontageInstance*> MontageInstances;

	// Cached data for montage evaluation, save us having to access MontageInstances from slot nodes as that isn't thread safe
	DEPRECATED(4.11, "Please use FAnimInstanceProxy::GetMontageEvaluationData()")
	TArray<FMontageEvaluationState> MontageEvaluationData;

	virtual void OnMontageInstanceStopped(FAnimMontageInstance & StoppedMontageInstance);
	void ClearMontageInstanceReferences(FAnimMontageInstance& InMontageInstance);

protected:
	/** Map between Active Montages and their FAnimMontageInstance */
	TMap<class UAnimMontage*, struct FAnimMontageInstance*> ActiveMontagesMap;

	/** Stop all montages that are active **/
	void StopAllMontages(float BlendOut);

	/** Stop all active montages belonging to 'InGroupName' */
	void StopAllMontagesByGroupName(FName InGroupName, const FAlphaBlend& BlendOut);

	/** Update weight of montages  **/
	virtual void Montage_UpdateWeight(float DeltaSeconds);
	/** Advance montages **/
	virtual void Montage_Advance(float DeltaSeconds);

public:
	/** Queue a Montage BlendingOut Event to be triggered. */
	void QueueMontageBlendingOutEvent(const FQueuedMontageBlendingOutEvent& MontageBlendingOutEvent);

	/** Queue a Montage Ended Event to be triggered. */
	void QueueMontageEndedEvent(const FQueuedMontageEndedEvent& MontageEndedEvent);

private:
	/** True when Montages are being ticked, and Montage Events should be queued. 
	 * When Montage are being ticked, we queue AnimNotifies and Events. We trigger notifies first, then Montage events. */
	UPROPERTY(Transient)
	bool bQueueMontageEvents;

	/** Trigger queued Montage events. */
	void TriggerQueuedMontageEvents();

	/** Queued Montage BlendingOut events. */
	TArray<FQueuedMontageBlendingOutEvent> QueuedMontageBlendingOutEvents;

	/** Queued Montage Ended Events */
	TArray<FQueuedMontageEndedEvent> QueuedMontageEndedEvents;

	/** Trigger a Montage BlendingOut event */
	void TriggerMontageBlendingOutEvent(const FQueuedMontageBlendingOutEvent& MontageBlendingOutEvent);

	/** Trigger a Montage Ended event */
	void TriggerMontageEndedEvent(const FQueuedMontageEndedEvent& MontageEndedEvent);

	/** Used to guard against recursive calls to UpdateAnimation */
	bool bUpdatingAnimation;

	/** Used to guard against recursive calls to UpdateAnimation */
	bool bPostUpdatingAnimation;

#if WITH_EDITOR
	/** Delegate for custom animation curve addition */
	TArray<FOnAddCustomAnimationCurves> OnAddAnimationCurves;
public:
	/** Add custom curve delegates */
	void AddDelegate_AddCustomAnimationCurve(FOnAddCustomAnimationCurves& InOnAddCustomAnimationCurves);
	void RemoveDelegate_AddCustomAnimationCurve(FOnAddCustomAnimationCurves& InOnAddCustomAnimationCurves);
#endif // editor only for now
public:

	/** Is this animation currently running post update */
	bool IsPostUpdatingAnimation() const { return bPostUpdatingAnimation; }

	/** Set RootMotionMode */
	UFUNCTION(BlueprintCallable, Category = "Animation")
	void SetRootMotionMode(TEnumAsByte<ERootMotionMode::Type> Value);

	/** 
	 * NOTE: Derived anim getters
	 *
	 * Anim getter functions can be defined for any instance deriving UAnimInstance.
	 * To do this the function must be marked BlueprintPure, and have the AnimGetter metadata entry set to
	 * "true". Following the instructions below, getters should appear correctly in the blueprint node context
	 * menu for the derived classes
	 *
	 * A context string can be provided in the GetterContext metadata and can contain any (or none) of the
	 * following entries separated by a pipe (|)
	 * Transition  - Only available in a transition rule
	 * AnimGraph   - Only available in an animgraph (also covers state anim graphs)
	 * CustomBlend - Only available in a custom blend graph
	 *
	 * Anim getters support a number of automatic parameters that will be baked at compile time to be passed
	 * to the functions. They will not appear as pins on the graph node. They are as follows:
	 * AssetPlayerIndex - Index of an asset player node to operate on, one getter will be added to the blueprint action list per asset node available
	 * MachineIndex     - Index of a state machine in the animation blueprint, one getter will be added to the blueprint action list per state machine
	 * StateIndex       - Index of a state inside a state machine, also requires MachineIndex. One getter will be added to the blueprint action list per state
	 * TransitionIndex  - Index of a transition inside a state machine, also requires MachineIndex. One getter will be added to the blueprint action list per transition
	 */

	/** Gets the length in seconds of the asset referenced in an asset player node */
	UFUNCTION(BlueprintPure, Category="Asset Player", meta=(DisplayName="Length", BlueprintInternalUseOnly="true", AnimGetter="true"))
	float GetInstanceAssetPlayerLength(int32 AssetPlayerIndex);

	/** Get the current accumulated time in seconds for an asset player node */
	UFUNCTION(BlueprintPure, Category="Asset Player", meta = (DisplayName = "Current Time", BlueprintInternalUseOnly = "true", AnimGetter = "true"))
	float GetInstanceAssetPlayerTime(int32 AssetPlayerIndex);

	/** Get the current accumulated time as a fraction for an asset player node */
	UFUNCTION(BlueprintPure, Category="Asset Player", meta=(DisplayName="Current Time (ratio)", BlueprintInternalUseOnly="true", AnimGetter="true"))
	float GetInstanceAssetPlayerTimeFraction(int32 AssetPlayerIndex);

	/** Get the time in seconds from the end of an animation in an asset player node */
	UFUNCTION(BlueprintPure, Category="Asset Player", meta=(DisplayName="Time Remaining", BlueprintInternalUseOnly="true", AnimGetter="true"))
	float GetInstanceAssetPlayerTimeFromEnd(int32 AssetPlayerIndex);

	/** Get the time as a fraction of the asset length of an animation in an asset player node */
	UFUNCTION(BlueprintPure, Category="Asset Player", meta=(DisplayName="Time Remaining (ratio)", BlueprintInternalUseOnly="true", AnimGetter="true"))
	float GetInstanceAssetPlayerTimeFromEndFraction(int32 AssetPlayerIndex);

	/** Get the blend weight of a specified state machine */
	UFUNCTION(BlueprintPure, Category = "States", meta = (DisplayName = "Machine Weight", BlueprintInternalUseOnly = "true", AnimGetter = "true"))
	float GetInstanceMachineWeight(int32 MachineIndex);

	/** Get the blend weight of a specified state */
	UFUNCTION(BlueprintPure, Category="States", meta = (DisplayName="State Weight", BlueprintInternalUseOnly = "true", AnimGetter="true"))
	float GetInstanceStateWeight(int32 MachineIndex, int32 StateIndex);

	/** Get the current elapsed time of a state within the specified state machine */
	UFUNCTION(BlueprintPure, Category="States", meta = (DisplayName="Current State Time", BlueprintInternalUseOnly = "true", AnimGetter="true", GetterContext="Transition"))
	float GetInstanceCurrentStateElapsedTime(int32 MachineIndex);

	/** Get the crossfade duration of a specified transition */
	UFUNCTION(BlueprintPure, Category="Transitions", meta = (DisplayName="Get Transition Crossfade Duration", BlueprintInternalUseOnly = "true", AnimGetter="true"))
	float GetInstanceTransitionCrossfadeDuration(int32 MachineIndex, int32 TransitionIndex);

	/** Get the elapsed time in seconds of a specified transition */
	UFUNCTION(BlueprintPure, Category="Transitions", meta = (DisplayName="Get Transition Time Elapsed", BlueprintInternalUseOnly = "true", AnimGetter="true", GetterContext="CustomBlend"))
	float GetInstanceTransitionTimeElapsed(int32 MachineIndex, int32 TransitionIndex);

	/** Get the elapsed time as a fraction of the crossfade duration of a specified transition */
	UFUNCTION(BlueprintPure, Category="Transitions", meta = (DisplayName="Get Transition Time Elapsed (ratio)", BlueprintInternalUseOnly = "true", AnimGetter="true", GetterContext="CustomBlend"))
	float GetInstanceTransitionTimeElapsedFraction(int32 MachineIndex, int32 TransitionIndex);

	/** Get the time remaining in seconds for the most relevant animation in the source state */
	UFUNCTION(BlueprintPure, Category="Asset Player", meta = (BlueprintInternalUseOnly = "true", AnimGetter="true", GetterContext="Transition"))
	float GetRelevantAnimTimeRemaining(int32 MachineIndex, int32 StateIndex);

	/** Get the time remaining as a fraction of the duration for the most relevant animation in the source state */
	UFUNCTION(BlueprintPure, Category = "Asset Player", meta = (BlueprintInternalUseOnly = "true", AnimGetter = "true", GetterContext = "Transition"))
	float GetRelevantAnimTimeRemainingFraction(int32 MachineIndex, int32 StateIndex);

	/** Get the length in seconds of the most relevant animation in the source state */
	UFUNCTION(BlueprintPure, Category = "Asset Player", meta = (BlueprintInternalUseOnly = "true", AnimGetter = "true", GetterContext = "Transition"))
	float GetRelevantAnimLength(int32 MachineIndex, int32 StateIndex);

	/** Get the current accumulated time in seconds for the most relevant animation in the source state */
	UFUNCTION(BlueprintPure, Category = "Asset Player", meta = (BlueprintInternalUseOnly = "true", AnimGetter = "true", GetterContext = "Transition"))
	float GetRelevantAnimTime(int32 MachineIndex, int32 StateIndex);

	/** Get the current accumulated time as a fraction of the length of the most relevant animation in the source state */
	UFUNCTION(BlueprintPure, Category = "Asset Player", meta = (BlueprintInternalUseOnly = "true", AnimGetter = "true", GetterContext = "Transition"))
	float GetRelevantAnimTimeFraction(int32 MachineIndex, int32 StateIndex);

	/** Gets an unchecked (can return nullptr) node given an index into the node property array */
	DEPRECATED(4.11, "Please use FAnimInstanceProxy::GetNodeFromIndexUntyped")
	FAnimNode_Base* GetNodeFromIndexUntyped(int32 NodeIdx, UScriptStruct* RequiredStructType);

	/** Gets a checked node given an index into the node property array */
	DEPRECATED(4.11, "Please use FAnimInstanceProxy::GetCheckedNodeFromIndexUntyped")
	FAnimNode_Base* GetCheckedNodeFromIndexUntyped(int32 NodeIdx, UScriptStruct* RequiredStructType);

	/** Gets a checked node given an index into the node property array */
	template<class NodeType>
	DEPRECATED(4.11, "Please use FAnimInstanceProxy::GetCheckedNodeFromIndex")
	NodeType* GetCheckedNodeFromIndex(int32 NodeIdx)
	{
		return (NodeType*)GetCheckedNodeFromIndexUntyped(NodeIdx, NodeType::StaticStruct());
	}

	/** Gets an unchecked (can return nullptr) node given an index into the node property array */
	template<class NodeType>
	DEPRECATED(4.11, "Please use FAnimInstanceProxy::GetNodeFromIndex")
	NodeType* GetNodeFromIndex(int32 NodeIdx)
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return (NodeType*)GetNodeFromIndexUntyped(NodeIdx, NodeType::StaticStruct());
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	/** Gets the runtime instance of the specified state machine by Name */
	FAnimNode_StateMachine* GetStateMachineInstanceFromName(FName MachineName);

	/** Get the machine description for the specified instance. Does not rely on PRIVATE_MachineDescription being initialized */
	const FBakedAnimationStateMachine* GetMachineDescription(IAnimClassInterface* AnimBlueprintClass, FAnimNode_StateMachine* MachineInstance);

	void GetStateMachineIndexAndDescription(FName InMachineName, int32& OutMachineIndex, const FBakedAnimationStateMachine** OutMachineDescription);

	/** Returns the baked sync group index from the compile step */
	int32 GetSyncGroupIndexFromName(FName SyncGroupName) const;
protected:

	/** Gets the index of the state machine matching MachineName */
	int32 GetStateMachineIndex(FName MachineName);

	/** Gets the runtime instance of the specified state machine */
	FAnimNode_StateMachine* GetStateMachineInstance(int32 MachineIndex);

	/** 
	 * Get the index of the specified instance asset player. Useful to pass to GetInstanceAssetPlayerLength (etc.).
	 * Passing NAME_None to InstanceName will return the first (assumed only) player instance index found.
	 */
	int32 GetInstanceAssetPlayerIndex(FName MachineName, FName StateName, FName InstanceName = NAME_None);

	/** Gets the runtime instance desc of the state machine specified by name */
	const FBakedAnimationStateMachine* GetStateMachineInstanceDesc(FName MachineName);

	/** Gets the most relevant asset player in a specified state */
	FAnimNode_AssetPlayerBase* GetRelevantAssetPlayerFromState(int32 MachineIndex, int32 StateIndex);

	//////////////////////////////////////////////////////////////////////////

public:
	/** Returns the value of a named curve. */
	UFUNCTION(BlueprintPure, Category="Animation")
	float GetCurveValue(FName CurveName);

	/** Returns the length (in seconds) of an animation AnimAsset. */
	DEPRECATED(4.9, "GetAnimAssetPlayerLength is deprecated, use GetInstanceAssetPlayerLength instead")
	UFUNCTION(BlueprintPure, Category="Animation", meta=(BlueprintInternalUseOnly = "true"))
	static float GetAnimAssetPlayerLength(UAnimationAsset* AnimAsset);

	//** Returns how far through the animation AnimAsset we are (as a proportion between 0.0 and 1.0). */
	DEPRECATED(4.9, "GetAnimAssetPlayerTimeFraction is deprecated, use GetInstanceAssetPlayerTimeFraction instead")
	UFUNCTION(BlueprintPure, Category="Animation", meta=(BlueprintInternalUseOnly = "true"))
	static float GetAnimAssetPlayerTimeFraction(UAnimationAsset* AnimAsset, float CurrentTime);

	/** Returns how long until the end of the animation AnimAsset (in seconds). */
	DEPRECATED(4.9, "GetAnimAssetPlayerTimeFromEnd is deprecated, use GetInstanceAssetPlayerTimeFromEnd instead")
	UFUNCTION(BlueprintPure, Category="Animation", meta=(BlueprintInternalUseOnly = "true"))
	float GetAnimAssetPlayerTimeFromEnd(UAnimationAsset* AnimAsset, float CurrentTime);

	/** Returns how long until the end of the animation AnimAsset we are (as a proportion between 0.0 and 1.0). */
	DEPRECATED(4.9, "GetAnimAssetPlayerTimeFromEndFraction is deprecated, use GetInstanceAssetPlayerTimeFromEndFraction instead")
	UFUNCTION(BlueprintPure, Category="Animation", meta=(BlueprintInternalUseOnly = "true"))
	static float GetAnimAssetPlayerTimeFromEndFraction(UAnimationAsset* AnimAsset, float CurrentTime);

	/** Returns the weight of a state in a state machine. */
	DEPRECATED(4.9, "GetStateWeight is deprecated, use GetInstanceStateWeight instead")
	UFUNCTION(BlueprintPure, Category="Animation", meta=(BlueprintInternalUseOnly = "true"))
	float GetStateWeight(int32 MachineIndex, int32 StateIndex);

	/** Returns (in seconds) the time a state machine has been active. */
	DEPRECATED(4.9, "GetCurrentStateElapsedTime is deprecated, use GetInstanceCurrentStateElapsedTime instead")
	UFUNCTION(BlueprintPure, Category="Animation", meta=(BlueprintInternalUseOnly = "true"))
	float GetCurrentStateElapsedTime(int32 MachineIndex);

	/** Returns the name of a currently active state in a state machine. */
	UFUNCTION(BlueprintPure, Category="Animation", meta=(BlueprintInternalUseOnly = "true", AnimGetter = "true"))
	FName GetCurrentStateName(int32 MachineIndex);

	/** Sets a morph target to a certain weight. */
	UFUNCTION(BlueprintCallable, Category="Animation")
	void SetMorphTarget(FName MorphTargetName, float Value);

	/** Clears the current morph targets. */
	UFUNCTION(BlueprintCallable, Category="Animation")
	void ClearMorphTargets();

	/** 
	 * Returns degree of the angle betwee velocity and Rotation forward vector
	 * The range of return will be from [-180, 180], and this can be used to feed blendspace directional value
	 */
	UFUNCTION(BlueprintCallable, Category="Animation")
	float CalculateDirection(const FVector& Velocity, const FRotator& BaseRotation);

	//--- AI communication start ---//
	/** locks indicated AI resources of animated pawn
	 *	DEPRECATED. Use LockAIResourcesWithAnimation instead */
	UFUNCTION(BlueprintCallable, Category = "Animation", BlueprintAuthorityOnly, Meta=(DeprecatedFunction, DeprecationMessage="Use LockAIResourcesWithAnimation instead"))
	void LockAIResources(bool bLockMovement, bool LockAILogic);

	/** unlocks indicated AI resources of animated pawn. Will unlock only animation-locked resources.
	 *	DEPRECATED. Use UnlockAIResourcesWithAnimation instead */
	UFUNCTION(BlueprintCallable, Category = "Animation", BlueprintAuthorityOnly, Meta=(DeprecatedFunction, DeprecationMessage="Use UnlockAIResourcesWithAnimation instead"))
	void UnlockAIResources(bool bUnlockMovement, bool UnlockAILogic);
	//--- AI communication end ---//

	UFUNCTION(BlueprintCallable, Category = "SyncGroup")
	bool GetTimeToClosestMarker(FName SyncGroup, FName MarkerName, float& OutMarkerTime) const;

	UFUNCTION(BlueprintCallable, Category = "SyncGroup")
	bool HasMarkerBeenHitThisFrame(FName SyncGroup, FName MarkerName) const;

	UFUNCTION(BlueprintCallable, Category = "SyncGroup")
	bool IsSyncGroupBetweenMarkers(FName InSyncGroupName, FName PreviousMarker, FName NextMarker, bool bRespectMarkerOrder = true) const;

	UFUNCTION(BlueprintCallable, Category = "SyncGroup")
	FMarkerSyncAnimPosition GetSyncGroupPosition(FName InSyncGroupName) const;

public:
	// Root node of animation graph
	DEPRECATED(4.11, "RootNode access has been moved to FAnimInstanceProxy")
	struct FAnimNode_Base* RootNode;

public:
	//~ Begin UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void BeginDestroy() override;
	virtual void PostInitProperties() override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	//~ End UObject Interface

	virtual void OnUROSkipTickAnimation() {}
	virtual void OnUROPreInterpolation() {}

	// Animation phase trigger
	// start with initialize
	// update happens in every tick. Can happen in parallel with others if conditions are right.
	// evaluate happens when condition is met - i.e. depending on your skeletalmeshcomponent update flag
	// post eval happens after evaluation is done
	// uninitialize happens when owner is unregistered
	void InitializeAnimation();
	void UpdateAnimation(float DeltaSeconds, bool bNeedsValidRootMotion);

	/** Run update animation work on a worker thread */
	void ParallelUpdateAnimation();

	/** Called after updates are completed, dispatches notifies etc. */
	void PostUpdateAnimation();

	/** Called on the game thread pre-evaluation. */
	void PreEvaluateAnimation();

	DEPRECATED(4.11, "This function should no longer be used. Use ParallelEvaluateAnimation")
	void EvaluateAnimation(struct FPoseContext& Output);

	/** Check whether evaluation can be performed on the supplied skeletal mesh. Can be called from worker threads. */
	bool ParallelCanEvaluate(const USkeletalMesh* InSkeletalMesh) const;

	/** Perform evaluation. Can be called from worker threads. */
	void ParallelEvaluateAnimation(bool bForceRefPose, const USkeletalMesh* InSkeletalMesh, TArray<FTransform>& OutBoneSpaceTransforms, FBlendedHeapCurve& OutCurve);

	void PostEvaluateAnimation();
	void UninitializeAnimation();

	// the below functions are the native overrides for each phase
	// Native initialization override point
	virtual void NativeInitializeAnimation();
	// Native update override point. It is usually a good idea to simply gather data in this step and 
	// for the bulk of the work to be done in NativeUpdateAnimation.
	virtual void NativeUpdateAnimation(float DeltaSeconds);
	// Native update override point. Can be called from a worker thread. This is a good place to do any
	// heavy lifting (as opposed to NativeUpdateAnimation_GameThread()).
	// This function should not be used. Worker thread updates should be performed in the FAnimInstanceProxy attached to this instance.
	virtual void NativeUpdateAnimation_WorkerThread(float DeltaSeconds);
	// Native evaluate override point.
	// @return true if this function is implemented, false otherwise.
	// Note: the node graph will not be evaluated if this function returns true
	DEPRECATED(4.11, "Please use FAnimInstanceProxy::Evaluate")
	virtual bool NativeEvaluateAnimation(FPoseContext& Output);
	// Native Post Evaluate override point
	virtual void NativePostEvaluateAnimation();
	// Native Uninitialize override point
	virtual void NativeUninitializeAnimation();

	// Sets up a native transition delegate between states with PrevStateName and NextStateName, in the state machine with name MachineName.
	// Note that a transition already has to exist for this to succeed
	void AddNativeTransitionBinding(const FName& MachineName, const FName& PrevStateName, const FName& NextStateName, const FCanTakeTransition& NativeTransitionDelegate, const FName& TransitionName = NAME_None);

	// Check for whether a native rule is bound to the specified transition
	bool HasNativeTransitionBinding(const FName& MachineName, const FName& PrevStateName, const FName& NextStateName, FName& OutBindingName);

	// Sets up a native state entry delegate from state with StateName, in the state machine with name MachineName.
	void AddNativeStateEntryBinding(const FName& MachineName, const FName& StateName, const FOnGraphStateChanged& NativeEnteredDelegate);
	
	// Check for whether a native entry delegate is bound to the specified state
	bool HasNativeStateEntryBinding(const FName& MachineName, const FName& StateName, FName& OutBindingName);

	// Sets up a native state exit delegate from state with StateName, in the state machine with name MachineName.
	void AddNativeStateExitBinding(const FName& MachineName, const FName& StateName, const FOnGraphStateChanged& NativeExitedDelegate);

	// Check for whether a native exit delegate is bound to the specified state
	bool HasNativeStateExitBinding(const FName& MachineName, const FName& StateName, FName& OutBindingName);

	// Debug output for this anim instance 
	void DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos);

	/** Reset any dynamics running simulation-style updates (e.g. on teleport, time skip etc.) */
	void ResetDynamics();

public:

	/** Access the required bones array */
	FBoneContainer& GetRequiredBones();	

	/** Temporary array of bone indices required this frame. Should be subset of Skeleton and Mesh's RequiredBones */
	DEPRECATED(4.11, "This cannot be accessed directly, use UnimInstance::GetRequiredBones")
	FBoneContainer RequiredBones;

	/** Animation Notifies that has been triggered in the latest tick **/
	DEPRECATED(4.11, "Use UAnimInstance::NotifyQueue")
	TArray<const struct FAnimNotifyEvent *> AnimNotifies;

	/** Animation Notifies that has been triggered in the latest tick **/
	FAnimNotifyQueue NotifyQueue;

	/** Currently Active AnimNotifyState, stored as a copy of the event as we need to
		call NotifyEnd on the event after a deletion in the editor. After this the event
		is removed correctly. */
	UPROPERTY(transient)
	TArray<FAnimNotifyEvent> ActiveAnimNotifyState;

protected:
	DEPRECATED(4.11, "This cannot be called directly as it relies on data potentially in use on worker threads")
	int32 GetSyncGroupReadIndex() const;
	DEPRECATED(4.11, "This cannot be called directly as it relies on data potentially in use on worker threads")
	int32 GetSyncGroupWriteIndex() const;
	DEPRECATED(4.11, "This cannot be called directly as it relies on data potentially in use on worker threads")
	void TickSyncGroupWriteIndex();

private:
	/** This is if you want to separate to another array**/
	TMap<FName, float>	AnimationCurves[(uint8)EAnimCurveType::MaxAnimCurveType];

	/** Reset Animation Curves */
	void ResetAnimationCurves();

	/** Material parameters that we had been changing and now need to clear */
	TArray<FName> MaterialParamatersToClear;

	//This frames marker sync data
	FMarkerTickContext MarkerTickContext;

public: 
	/** Update all internal curves from Blended Curve */
	void UpdateCurves(const FBlendedHeapCurve& InCurves);

	/** Refresh currently existing curves */
	void RefreshCurves(USkeletalMeshComponent* Component);

	/** Check whether we have active morph target curves */
	bool HasMorphTargetCurves() const;

	/** 
	 * Retrieve animation curve list by Curve Flags, it will return list of {UID, value} 
	 * It will clear the OutCurveList before adding
	 */
	void GetAnimationCurveList(int32 CurveFlags, TMap<FName, float>& OutCurveList) const;

#if WITH_EDITORONLY_DATA
	// Maximum playback position ever reached (only used when debugging in Persona)
	double LifeTimer;

	// Current scrubbing playback position (only used when debugging in Persona)
	double CurrentLifeTimerScrubPosition;
#endif

public:
	DEPRECATED(4.11, "This cannot be accessed directly as it is potentially in use on worker threads")
	FGraphTraversalCounter InitializationCounter;
	DEPRECATED(4.11, "This cannot be accessed directly as it is potentially in use on worker threads")
	FGraphTraversalCounter CachedBonesCounter;
	DEPRECATED(4.11, "This cannot be accessed directly as it is potentially in use on worker threads")
	FGraphTraversalCounter UpdateCounter;
	DEPRECATED(4.11, "This cannot be accessed directly as it is potentially in use on worker threads")
	FGraphTraversalCounter EvaluationCounter;
	FGraphTraversalCounter DebugDataCounter;
	DEPRECATED(4.11, "This cannot be accessed directly as it is potentially in use on worker threads")
	FGraphTraversalCounter SlotNodeInitializationCounter;

private:
	TMap<FName, FMontageActiveSlotTracker> SlotWeightTracker;

public:
	/** 
	 * Recalculate Required Bones [RequiredBones]
	 * Is called when bRequiredBonesUpToDate = false
	 */
	void RecalcRequiredBones();

	/** When RequiredBones mapping has changed, AnimNodes need to update their bones caches. */
	DEPRECATED(4.11, "This cannot be accessed directly, use FAnimInstanceProxy::bBoneCachesInvalidated")
	bool bBoneCachesInvalidated;

	// @todo document
	inline USkeletalMeshComponent* GetSkelMeshComponent() const { return CastChecked<USkeletalMeshComponent>(GetOuter()); }

	virtual UWorld* GetWorld() const override;

	/** Add anim notifier **/
	DEPRECATED(4.11, "Please use NotifyQueue.AddAnimNotifies")
	void AddAnimNotifies(const TArray<const FAnimNotifyEvent*>& NewNotifies, const float InstanceWeight);

	/** Should the notifies current filtering mode stop it from triggering */
	DEPRECATED(4.11, "This cannot be called directly as it relies on data potentially in use on worker threads")
	bool PassesFiltering(const FAnimNotifyEvent* Notify) const;

	/** Work out whether this notify should be triggered based on its chance of triggering value */
	DEPRECATED(4.11, "This cannot be called directly as it relies on data potentially in use on worker threads")
	bool PassesChanceOfTriggering(const FAnimNotifyEvent* Event) const;

	/** Queues an Anim Notify from the shared list on our generated class */
	DEPRECATED(4.11, "This cannot be called directly as it relies on data potentially in use on worker threads")
	void AddAnimNotifyFromGeneratedClass(int32 NotifyIndex);

	/** Trigger AnimNotifies **/
	void TriggerAnimNotifies(float DeltaSeconds);
	void TriggerSingleAnimNotify(const FAnimNotifyEvent* AnimNotifyEvent);

	/** Add curve float data using a curve Uid, the name of the curve will be resolved from the skeleton **/
	void AddCurveValue(const USkeleton::AnimCurveUID Uid, float Value, int32 CurveTypeFlags);

	/** Given a machine index, record a state machine weight for this frame */
	void RecordMachineWeight(const int32& InMachineClassIndex, const float& InMachineWeight);
	/** 
	 * Add curve float data, using a curve name. External values should all be added using
	 * The curve UID to the public version of this method
	 */
	void AddCurveValue(const FName& CurveName, float Value, int32 CurveTypeFlags);

	/** Given a machine and state index, record a state weight for this frame */
	void RecordStateWeight(const int32& InMachineClassIndex, const int32& InStateIndex, const float& InStateWeight);

protected:
#if WITH_EDITORONLY_DATA
	// Returns true if a snapshot is being played back and the remainder of Update should be skipped.
	bool UpdateSnapshotAndSkipRemainingUpdate();
#endif

	// Root Motion
public:
	/** Get current RootMotion FAnimMontageInstance if any. NULL otherwise. */
	FAnimMontageInstance * GetRootMotionMontageInstance() const;

	/** Get current accumulated root motion, removing it from the AnimInstance in the process */
	FRootMotionMovementParams ConsumeExtractedRootMotion(float Alpha);

	/**  
	 * Queue blended root motion. This is used to blend in root motion transforms according to 
	 * the correctly-updated slot weight (after the animation graph has been updated).
	 */
	void QueueRootMotionBlend(const FTransform& RootTransform, const FName& SlotName, float Weight);

private:
	/** Active Root Motion Montage Instance, if any. */
	struct FAnimMontageInstance* RootMotionMontageInstance;

	/** Temporarily queued root motion blend */
	struct FQueuedRootMotionBlend
	{
		FQueuedRootMotionBlend(const FTransform& InTransform, const FName& InSlotName, float InWeight)
			: Transform(InTransform)
			, SlotName(InSlotName)
			, Weight(InWeight)
		{}

		FTransform Transform;
		FName SlotName;
		float Weight;
	};

	/** 
	 * Blend queue for blended root motion. This is used to blend in root motion transforms according to 
	 * the correctly-updated slot weight (after the animation graph has been updated).
	 */
	TArray<FQueuedRootMotionBlend> RootMotionBlendQueue;

private:
	// update montage
	void UpdateMontage(float DeltaSeconds);

protected:
	/** Update all animation node */
	DEPRECATED(4.11, "This cannot be called directly as it relies on data potentially in use on worker threads")
	virtual void UpdateAnimationNode(float DeltaSeconds);

	// Updates the montage data used for evaluation based on the current playing montages
	void UpdateMontageEvaluationData();

	/** Called to setup for updates */
	void PreUpdateAnimation(float DeltaSeconds);

	/** Actually does the update work, can be called from a worker thread  */
	void UpdateAnimationInternal_Concurrent(float DeltaSeconds, FAnimInstanceProxy& Proxy);

	/** update animation curves to component */
	void UpdateCurvesToComponents(USkeletalMeshComponent* Component);

	/** Override point for derived classes to create their own proxy objects (allows custom allocation) */
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy();

	/** Override point for derived classes to destroy their own proxy objects (allows custom allocation) */
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy);

	/** Access the proxy but block if a task is currently in progress as it wouldn't be safe to access it */
	template <typename T /*= FAnimInstanceProxy*/>	// @TODO: Cant default parameters to this function on Xbox One until we move off the VS2012 compiler
	FORCEINLINE T& GetProxyOnGameThread()
	{
		check(IsInGameThread());
		if(GetOuter() && GetOuter()->IsA<USkeletalMeshComponent>())
		{
			bool bBlockOnTask = true;
			bool bPerformPostAnimEvaluation = true;
			GetSkelMeshComponent()->HandleExistingParallelEvaluationTask(bBlockOnTask, bPerformPostAnimEvaluation);
		}
		if(AnimInstanceProxy == nullptr)
		{
			AnimInstanceProxy = CreateAnimInstanceProxy();
		}
		return *static_cast<T*>(AnimInstanceProxy);
	}

	/** Access the proxy but block if a task is currently in progress as it wouldn't be safe to access it */
	template <typename T/* = FAnimInstanceProxy*/>	// @TODO: Cant default parameters to this function on Xbox One until we move off the VS2012 compiler
	FORCEINLINE const T& GetProxyOnGameThread() const
	{
		check(IsInGameThread());
		if(GetOuter() && GetOuter()->IsA<USkeletalMeshComponent>())
		{
			bool bBlockOnTask = true;
			bool bPerformPostAnimEvaluation = true;
			GetSkelMeshComponent()->HandleExistingParallelEvaluationTask(bBlockOnTask, bPerformPostAnimEvaluation);
		}
		if(AnimInstanceProxy == nullptr)
		{
			AnimInstanceProxy = const_cast<UAnimInstance*>(this)->CreateAnimInstanceProxy();
		}
		return *static_cast<const T*>(AnimInstanceProxy);
	}

	/** Access the proxy but block if a task is currently in progress (and we are on the game thread) as it wouldn't be safe to access it */
	template <typename T/* = FAnimInstanceProxy*/>	// @TODO: Cant default parameters to this function on Xbox One until we move off the VS2012 compiler
	FORCEINLINE T& GetProxyOnAnyThread()
	{
		if(GetOuter() && GetOuter()->IsA<USkeletalMeshComponent>())
		{
			if(IsInGameThread())
			{
				bool bBlockOnTask = true;
				bool bPerformPostAnimEvaluation = true;
				GetSkelMeshComponent()->HandleExistingParallelEvaluationTask(bBlockOnTask, bPerformPostAnimEvaluation);
			}
		}
		if(AnimInstanceProxy == nullptr)
		{
			AnimInstanceProxy = CreateAnimInstanceProxy();
		}
		return *static_cast<T*>(AnimInstanceProxy);
	}

	/** Access the proxy but block if a task is currently in progress (and we are on the game thread) as it wouldn't be safe to access it */
	template <typename T/* = FAnimInstanceProxy*/>	// @TODO: Cant default parameters to this function on Xbox One until we move off the VS2012 compiler
	FORCEINLINE const T& GetProxyOnAnyThread() const
	{
		if(GetOuter() && GetOuter()->IsA<USkeletalMeshComponent>())
		{
			if(IsInGameThread())
			{
				bool bBlockOnTask = true;
				bool bPerformPostAnimEvaluation = true;
				GetSkelMeshComponent()->HandleExistingParallelEvaluationTask(bBlockOnTask, bPerformPostAnimEvaluation);
			}
		}
		if(AnimInstanceProxy == nullptr)
		{
			AnimInstanceProxy = const_cast<UAnimInstance*>(this)->CreateAnimInstanceProxy();
		}
		return *static_cast<const T*>(AnimInstanceProxy);
	}

	// TODO: Remove after deprecation (4.11)
	friend struct FAnimationBaseContext;
	
	friend struct FAnimNode_SubInstance;

protected:
	/** Proxy object, nothing should access this from an externally-callable API as it is used as a scratch area on worker threads */
	mutable FAnimInstanceProxy* AnimInstanceProxy;
};
