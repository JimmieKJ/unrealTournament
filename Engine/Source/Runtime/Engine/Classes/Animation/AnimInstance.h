// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimationAsset.h"
#include "AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "AnimStateMachineTypes.h"
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

/** Enum for controlling which reference frame a controller is applied in. */
UENUM()
enum EBoneControlSpace
{
	/** Set absolute position of bone in world space. */
	BCS_WorldSpace UMETA( DisplayName = "World Space" ),
	/** Set position of bone in SkeletalMeshComponent's reference frame. */
	BCS_ComponentSpace UMETA( DisplayName = "Component Space" ),
	/** Set position of bone relative to parent bone. */
	BCS_ParentBoneSpace UMETA( DisplayName = "Parent Bone Space" ),
	/** Set position of bone in its own reference frame. */
	BCS_BoneSpace UMETA( DisplayName = "Bone Space" ),
	BCS_MAX,
};

/** Enum for specifying the source of a bone's rotation. */
UENUM()
enum EBoneRotationSource
{
	/** Don't change rotation at all. */
	BRS_KeepComponentSpaceRotation UMETA(DisplayName = "No Change (Preserve Existing Component Space Rotation)"),
	/** Keep forward direction vector relative to the parent bone. */
	BRS_KeepLocalSpaceRotation UMETA(DisplayName = "Maintain Local Rotation Relative to Parent"),
	/** Copy rotation of target to bone. */
	BRS_CopyFromTarget UMETA(DisplayName = "Copy Target Rotation"),
};

USTRUCT()
struct FA2Pose
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<FTransform> Bones;

	FA2Pose() {}
};

/** component space poses **/
USTRUCT()
struct ENGINE_API FA2CSPose : public FA2Pose
{
	GENERATED_USTRUCT_BODY()

private:
	/** Pointer to current BoneContainer. */
	const struct FBoneContainer * BoneContainer;

	/** once evaluated to be mesh space, this flag will be set **/
	UPROPERTY()
	TArray<uint8> ComponentSpaceFlags;

public:
	FA2CSPose()
		: BoneContainer(NULL)
	{
	}

	/** constructor - needs LocalPoses **/
	void AllocateLocalPoses(const FBoneContainer& InBoneContainer, const FA2Pose & LocalPose);

	/** constructor - needs LocalPoses **/
	void AllocateLocalPoses(const FBoneContainer& InBoneContainer, const FTransformArrayA2 & LocalBones);

	/** Returns if this struct is valid */
	bool IsValid() const;

	/** Get parent bone index for given bone index. */
	int32 GetParentBoneIndex(const int32& BoneIndex) const;

	/** Returns local transform for the boneindex **/
	FTransform GetLocalSpaceTransform(int32 BoneIndex);

	/**
	 * Do not access Bones array directly but via this 
	 * This will fill up gradually mesh space bases 
	 */
	FTransform GetComponentSpaceTransform(int32 BoneIndex);

	/** convert to local poses **/
	void ConvertToLocalPoses(FA2Pose & LocalPoses) const;

	/** 
	 * Set a bunch of Component Space Bone Transforms.
	 * Do this safely by insuring that Parents are already in Component Space,
	 * and any Component Space children are converted back to Local Space before hand.
	 */
	void SafeSetCSBoneTransforms(const TArray<struct FBoneTransform> & BoneTransforms);

	/** 
	 * Blends Component Space transforms to MeshPose in Local Space. 
	 * Used by SkelControls to apply their transforms.
	 *
	 * The tricky bit is that SkelControls deliver their transforms in Component Space,
	 * But the blending is done in Local Space. Also we need to refresh any Children they have
	 * that has been previously converted to Component Space.
	 */
	void LocalBlendCSBoneTransforms(const TArray<struct FBoneTransform> & BoneTransforms, float Alpha);

private:
	/** Calculate all transform till parent **/
	void CalculateComponentSpaceTransform(int32 Index);
	void SetComponentSpaceTransform(int32 Index, const FTransform& NewTransform);

	/**
	 * Convert Bone to Local Space.
	 */
	void ConvertBoneToLocalSpace(int32 BoneIndex);


	void SetLocalSpaceTransform(int32 Index, const FTransform& NewTransform);

	/** this is not really best way to protect SetComponentSpaceTransform, but we'd like to make sure
	 that isn't called by anywhere else */
	friend class FAnimationRuntime;
};

USTRUCT(BlueprintType)
struct FBoneTransform
{
	GENERATED_USTRUCT_BODY()

	/** @todo anim: should be Skeleton bone index in the future, but right now it's Mesh BoneIndex **/
	UPROPERTY()
	int32 BoneIndex;

	/** Transform to apply **/
	UPROPERTY()
	FTransform Transform;

	FBoneTransform() 
		: BoneIndex(INDEX_NONE)
	{}

	FBoneTransform( int32 InBoneIndex, const FTransform& InTransform) 
		: BoneIndex(InBoneIndex)
		, Transform(InTransform)
	{}
};

USTRUCT(BlueprintType)
struct FPerBoneBlendWeight
{
	GENERATED_USTRUCT_BODY()
	UPROPERTY()
	int32 SourceIndex; // source index of the buffer
	UPROPERTY()
	float BlendWeight; // how much blend weight

	FPerBoneBlendWeight()
		: SourceIndex(0)
		, BlendWeight(0.0f)
	{
	}
};

USTRUCT(BlueprintType)
struct FPerBoneBlendWeights
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<FPerBoneBlendWeight> BoneBlendWeights;


	FPerBoneBlendWeights() {}

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

	/** Pose */
	UPROPERTY()
	FA2Pose Pose;
	
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

	FNativeTransitionBinding(const FName& InMachineName, const FName& InPreviousStateName, const FName& InNextStateName, const FCanTakeTransition& InNativeTransitionDelegate)
		: MachineName(InMachineName)
		, PreviousStateName(InPreviousStateName)
		, NextStateName(InNextStateName)
		, NativeTransitionDelegate(InNativeTransitionDelegate)
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

	FNativeStateBinding(const FName& InMachineName, const FName& InStateName, const FOnGraphStateChanged& InNativeStateDelegate)
		: MachineName(InMachineName)
		, StateName(InStateName)
		, NativeStateDelegate(InNativeStateDelegate)
	{
	}
};

/** Tracks state of active slot nodes in the graph */
struct FMontageActiveSlotTracker
{
	//Weighting of root motion from this montage
	float RootMotionWeight;

	//Is the montage slot part of the active graph this tick
	bool  bIsRelevantThisTick;

	//Was the motnage slot part of the active graph last tick
	bool  bWasRelevantOnPreviousTick;

	FMontageActiveSlotTracker() : RootMotionWeight(0.f), bIsRelevantThisTick(false), bWasRelevantOnPreviousTick(false) {}
};

struct FMontageEvaluationState
{
	FMontageEvaluationState(UAnimMontage* InMontage, float InWeight, float InPosition) : Montage(InMontage), MontageWeight(InWeight), MontagePosition(InPosition) {}

	// The montage to evaluate
	UAnimMontage* Montage;

	// The weight to use for this montage
	float MontageWeight;

	// The position to evaluate this montage at
	float MontagePosition;
};

UCLASS(transient, Blueprintable, hideCategories=AnimInstance, BlueprintType)
class ENGINE_API UAnimInstance : public UObject
{
	GENERATED_UCLASS_BODY()

	/** DeltaTime **/
	UPROPERTY()
	float DeltaTime_DEPRECATED;

	/** This is used to extract animation. If Mesh exists, this will be overwritten by Mesh->Skeleton */
	UPROPERTY(transient)
	USkeleton* CurrentSkeleton;

	// The list of animation assets which are going to be evaluated this frame and need to be ticked (ungrouped)
	UPROPERTY(transient)
	TArray<FAnimTickRecord> UngroupedActivePlayers;

	// The set of tick groups for this anim instance
	UPROPERTY(transient)
	TArray<FAnimGroupInstance> SyncGroups;

	/** Array indicating active vertex anims (by reference) generated by anim instance. */
	UPROPERTY(transient)
	TArray<struct FActiveVertexAnim> VertexAnims;

	// Sets where this blueprint pulls Root Motion from
	UPROPERTY(Category = RootMotion, EditDefaultsOnly)
	TEnumAsByte<ERootMotionMode::Type> RootMotionMode;

public:

	// @todo document
	void MakeSequenceTickRecord(FAnimTickRecord& TickRecord, UAnimSequenceBase* Sequence, bool bLooping, float PlayRate, float FinalBlendWeight, float& CurrentTime) const;
	void MakeBlendSpaceTickRecord(FAnimTickRecord& TickRecord, UBlendSpaceBase* BlendSpace, const FVector& BlendInput, TArray<FBlendSampleData>& BlendSampleDataCache, FBlendFilter& BlendFilter, bool bLooping, float PlayRate, float FinalBlendWeight, float& CurrentTime) const;

	void SequenceAdvanceImmediate(UAnimSequenceBase* Sequence, bool bLooping, float PlayRate, float DeltaSeconds, /*inout*/ float& CurrentTime);

	// @todo document
	void BlendSpaceAdvanceImmediate(UBlendSpaceBase* BlendSpace, const FVector& BlendInput, TArray<FBlendSampleData> & BlendSampleDataCache, FBlendFilter & BlendFilter, bool bLooping, float PlayRate, float DeltaSeconds, /*inout*/ float& CurrentTime);

	// Creates an uninitialized tick record in the list for the correct group or the ungrouped array.  If the group is valid, OutSyncGroupPtr will point to the group.
	FAnimTickRecord& CreateUninitializedTickRecord(int32 GroupIndex, FAnimGroupInstance*& OutSyncGroupPtr);

	void SequenceEvaluatePose(UAnimSequenceBase* Sequence, struct FA2Pose& Pose, const FAnimExtractContext& ExtractionContext);

	void BlendSequences(const struct FA2Pose& Pose1, const struct FA2Pose& Pose2, float Alpha, struct FA2Pose& Blended);

	static void CopyPose(const struct FA2Pose& Source, struct FA2Pose& Destination);

	void ApplyAdditiveSequence(const struct FA2Pose& BasePose, const struct FA2Pose& AdditivePose, float Alpha, struct FA2Pose& Blended);

	void BlendSpaceEvaluatePose(UBlendSpaceBase* BlendSpace, TArray<FBlendSampleData>& BlendSampleDataCache, struct FA2Pose& Pose);

	// skeletal control related functions
	void BlendRotationOffset(const struct FA2Pose& BasePose/* local space base pose */, struct FA2Pose const & RotationOffsetPose/* mesh space rotation only additive **/, float Alpha/*0 means no additive, 1 means whole additive */, struct FA2Pose& Pose /** local space blended pose **/);

	// slotnode interfaces
	void GetSlotWeight(FName const & SlotNodeName, float& out_SlotNodeWeight, float& out_SourceWeight) const;
	void SlotEvaluatePose(FName SlotNodeName, const struct FA2Pose & SourcePose, struct FA2Pose & BlendedPose, float SlotNodeWeight);

	// slot node run-time functions
	void ReinitializeSlotNodes();
	void RegisterSlotNodeWithAnimInstance(FName SlotNodeName);
	void UpdateSlotNodeWeight(FName SlotNodeName, float Weight);
	// if it doesn't tick, it will keep old weight, so we'll have to clear it in the beginning of tick
	void ClearSlotNodeWeights();
	bool IsSlotNodeRelevantForNotifies(FName SlotNodeName) const;

	// Allow slot nodes to store off their root motion weight during ticking
	void UpdateSlotRootMotionWeight(FName SlotNodeName, float Weight);
	// Get the root motion weight for the montage slot
	float GetSlotRootMotionWeight(FName SlotNodeName) const;


	// kismet event functions

	UFUNCTION(BlueprintCallable, Category = "Animation")
	virtual APawn* TryGetPawnOwner() const;

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

	bool CanTransitionSignature() const;
	
	UFUNCTION()
	void AnimNotify_Sound(UAnimNotify* Notify);

	/*********************************************************************************************
	* SlotAnimation
	********************************************************************************************* */
public:

	/** DEPRECATED. Use PlaySlotAnimationAsDynamicMontage instead, it returns the UAnimMontage created instead of time, allowing more control */
	/** Play normal animation asset on the slot node. You can only play one asset (whether montage or animsequence) at a time. */
	UFUNCTION(BlueprintCallable, Category="Animation", Meta = (DeprecatedFunction, DeprecationMessage = "Use PlaySlotAnimationAsDynamicMontage instead"))
	float PlaySlotAnimation(UAnimSequenceBase* Asset, FName SlotNodeName, float BlendInTime = 0.25f, float BlendOutTime = 0.25f, float InPlayRate = 1.f, int32 LoopCount = 1);

	/** Play normal animation asset on the slot node by creating a dynamic UAnimMontage. You can only play one asset (whether montage or animsequence) at a time per SlotGroup. */
	UFUNCTION(BlueprintCallable, Category="Animation")
	UAnimMontage* PlaySlotAnimationAsDynamicMontage(UAnimSequenceBase* Asset, FName SlotNodeName, float BlendInTime = 0.25f, float BlendOutTime = 0.25f, float InPlayRate = 1.f, int32 LoopCount = 1, float BlendOutTriggerTime = -1.f);

	/** Stops currently playing slot animation slot or all*/
	UFUNCTION(BlueprintCallable, Category="Animation")
	void StopSlotAnimation(float InBlendOutTime = 0.25f, FName SlotNodeName = NAME_None);

	/** Return true if it's playing the slot animation */
	UFUNCTION(BlueprintCallable, Category="Animation")
	bool IsPlayingSlotAnimation(UAnimSequenceBase* Asset, FName SlotNodeName ); 

	/*********************************************************************************************
	 * AnimMontage
	 ********************************************************************************************* */
public:
	/** Plays an animation montage. Returns the length of the animation montage in seconds. Returns 0.f if failed to play. */
	UFUNCTION(BlueprintCallable, Category = "Animation")
	float Montage_Play(UAnimMontage * MontageToPlay, float InPlayRate = 1.f);

	/** Stops the animation montage. If reference is NULL, it will stop ALL active montages. */
	UFUNCTION(BlueprintCallable, Category = "Animation")
	void Montage_Stop(float InBlendOutTime, UAnimMontage * Montage = NULL);

	/** Pauses the animation montage. If reference is NULL, it will stop ALL active montages. */
	UFUNCTION(BlueprintCallable, Category = "Animation")
	void Montage_Pause(UAnimMontage * Montage = NULL);

	/** Makes a montage jump to a named section. If Montage reference is NULL, it will do that to all active montages. */
	UFUNCTION(BlueprintCallable, Category="Animation")
	void Montage_JumpToSection(FName SectionName, UAnimMontage * Montage = NULL);

	/** Makes a montage jump to the end of a named section. If Montage reference is NULL, it will do that to all active montages. */
	UFUNCTION(BlueprintCallable, Category="Animation")
	void Montage_JumpToSectionsEnd(FName SectionName, UAnimMontage * Montage = NULL);

	/** Relink new next section AFTER SectionNameToChange in run-time
	 *	You can link section order the way you like in editor, but in run-time if you'd like to change it dynamically, 
	 *	use this function to relink the next section
	 *	For example, you can have Start->Loop->Loop->Loop.... but when you want it to end, you can relink
	 *	next section of Loop to be End to finish the montage, in which case, it stops looping by Loop->End. 
	 
	 * @param SectionNameToChange : This should be the name of the Montage Section after which you want to insert a new next section
	 * @param NextSection	: new next section 
	 */
	UFUNCTION(BlueprintCallable, Category="Animation")
	void Montage_SetNextSection(FName SectionNameToChange, FName NextSection, UAnimMontage * Montage = NULL);

	/** Change AnimMontage play rate. NewPlayRate = 1.0 is the default playback rate. */
	UFUNCTION(BlueprintCallable, Category="Animation")
	void Montage_SetPlayRate(UAnimMontage* Montage, float NewPlayRate = 1.f);

	/** Returns true if the animation montage is active. If the Montage reference is NULL, it will return true if any Montage is active. */
	UFUNCTION(BlueprintCallable, Category="Animation")
	bool Montage_IsActive(UAnimMontage* Montage);  

	/** Returns true if the animation montage is currently active and playing. 
	If reference is NULL, it will return true is ANY montage is currently active and playing. */
	UFUNCTION(BlueprintCallable, Category="Animation")
	bool Montage_IsPlaying(UAnimMontage* Montage); 

	/** Returns the name of the current animation montage section. */
	UFUNCTION(BlueprintCallable, Category="Animation")
	FName Montage_GetCurrentSection(UAnimMontage* Montage = NULL);

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
	void Montage_SetEndDelegate(FOnMontageEnded & OnMontageEnded, UAnimMontage * Montage = NULL);
	
	void Montage_SetBlendingOutDelegate(FOnMontageBlendingOutStarted & OnMontageBlendingOut, UAnimMontage* Montage = NULL);
	
	/** Get pointer to BlendingOutStarted delegate for Montage.
	If Montage reference is NULL, it will pick the first active montage found. */
	FOnMontageBlendingOutStarted * Montage_GetBlendingOutDelegate(UAnimMontage * Montage = NULL);

	/** Get Current Montage Position */
	float Montage_GetPosition(UAnimMontage* Montage);
	
	/** Set position. */
	void Montage_SetPosition(UAnimMontage* Montage, float NewPosition);
	
	/** return true if Montage is not currently active. (not valid or blending out) */
	bool Montage_GetIsStopped(UAnimMontage* Montage);

	/** Get the current blend time of the Montage.
	If Montage reference is NULL, it will return the current blend time on the first active Montage found. */
	float Montage_GetBlendTime(UAnimMontage* Montage);

	/** Get PlayRate for Montage.
	If Montage reference is NULL, PlayRate for any Active Montage will be returned.
	If Montage is not playing, 0 is returned. */
	float Montage_GetPlayRate(UAnimMontage* Montage);

	/** Get next sectionID for given section ID */
	int32 Montage_GetNextSectionID(UAnimMontage const * const Montage, int32 const & CurrentSectionID) const;

	/** Get a current Active Montage in this AnimInstance. 
		Note that there might be multiple Active at the same time. This will only return the first active one it finds. **/
	UAnimMontage * GetCurrentActiveMontage();

	/** Get Currently active montage instance.
		Note that there might be multiple Active at the same time. This will only return the first active one it finds. **/
	FAnimMontageInstance * GetActiveMontageInstance();

	/** Get Active FAnimMontageInstance for given Montage asset. Will return NULL if Montage is not currently Active. */
	FAnimMontageInstance * GetActiveInstanceForMontage(UAnimMontage const & Montage) const;

	/** AnimMontage instances that are running currently
	* - only one is primarily active per group, and the other ones are blending out
	*/
	TArray<struct FAnimMontageInstance*> MontageInstances;

	// Cached data for montage evaluation, save us having to access MontageInstances from slot nodes as that isn't thread safe
	TArray<FMontageEvaluationState> MontageEvaluationData;

	void OnMontageInstanceStopped(FAnimMontageInstance & StoppedMontageInstance);

protected:
	/** Map between Active Montages and their FAnimMontageInstance */
	TMap<class UAnimMontage *, struct FAnimMontageInstance*> ActiveMontagesMap;

	/** Stop all montages that are active **/
	void StopAllMontages(float BlendOut);

	/** Stop all active montages belonging to 'InGroupName' */
	void StopAllMontagesByGroupName(FName InGroupName, float BlendOutTime);

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

public:
	/** Returns the value of a named curve. */
	UFUNCTION(BlueprintPure, Category="Animation")
	float GetCurveValue(FName CurveName);

	/** Returns the length (in seconds) of an animation AnimAsset. */
	UFUNCTION(BlueprintPure, Category="Animation", meta=(BlueprintInternalUseOnly = "true"))
	static float GetAnimAssetPlayerLength(UAnimationAsset* AnimAsset);

	//** Returns how far through the animation AnimAsset we are (as a proportion between 0.0 and 1.0). */
	UFUNCTION(BlueprintPure, Category="Animation", meta=(BlueprintInternalUseOnly = "true"))
	static float GetAnimAssetPlayerTimeFraction(UAnimationAsset* AnimAsset, float CurrentTime);

	/** Returns how long until the end of the animation AnimAsset (in seconds). */
	UFUNCTION(BlueprintPure, Category="Animation", meta=(BlueprintInternalUseOnly = "true"))
	static float GetAnimAssetPlayerTimeFromEnd(UAnimationAsset* AnimAsset, float CurrentTime);

	/** Returns how long until the end of the animation AnimAsset we are (as a proportion between 0.0 and 1.0). */
	UFUNCTION(BlueprintPure, Category="Animation", meta=(BlueprintInternalUseOnly = "true"))
	static float GetAnimAssetPlayerTimeFromEndFraction(UAnimationAsset* AnimAsset, float CurrentTime);

	/** Returns the weight of a state in a state machine. */
	UFUNCTION(BlueprintPure, Category="Animation", meta=(BlueprintInternalUseOnly = "true"))
	float GetStateWeight(int32 MachineIndex, int32 StateIndex);

	/** Returns (in seconds) the time a state machine has been active. */
	UFUNCTION(BlueprintPure, Category="Animation", meta=(BlueprintInternalUseOnly = "true"))
	float GetCurrentStateElapsedTime(int32 MachineIndex);

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

public:
	// Root node of animation graph
	struct FAnimNode_Base* RootNode;

public:
	// Begin UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	// End UObject Interface

	// Updates the montage data used for evaluation based on the current playing montages
	void UpdateMontageEvaluationData();

	//@TODO: Better comments
	virtual void EvaluateAnimation(struct FPoseContext& Output);
	virtual void PostAnimEvaluation() {}

	void InitializeAnimation();
	void UpdateAnimation(float DeltaSeconds);
	void UninitializeAnimation();

	// Native initialization override point
	virtual void NativeInitializeAnimation();

	// Native update override point
	virtual void NativeUpdateAnimation(float DeltaSeconds);

	// Native evaluate override point.
	// @return true if this function is implemented, false otherwise.
	// Note: the node graph will not be evaluated if this function returns true
	virtual bool NativeEvaluateAnimation(FPoseContext& Output);

	// Sets up a native transition delegate between states with PrevStateName and NextStateName, in the state machine with name MachineName.
	// Note that a transition already has to exist for this to succeed
	void AddNativeTransitionBinding(const FName& MachineName, const FName& PrevStateName, const FName& NextStateName, const FCanTakeTransition& NativeTransitionDelegate);

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
public:

	/** Temporary array of bone indices required this frame. Should be subset of Skeleton and Mesh's RequiredBones */
	FBoneContainer RequiredBones;

	/** Animation Notifies that has been triggered in the latest tick **/
	TArray<const struct FAnimNotifyEvent *> AnimNotifies;

	/** Currently Active AnimNotifyState, stored as a copy of the event as we need to
		call NotifyEnd on the event after a deletion in the editor. After this the event
		is removed correctly. */
	UPROPERTY(transient)
	TArray<FAnimNotifyEvent> ActiveAnimNotifyState;

	/** Curve Values that are added to trigger in event**/
	TMap<FName, float>	EventCurves;
	/** Morph Target Curves that will be used for SkeletalMeshComponent **/
	TMap<FName, float>	MorphTargetCurves;
	/** Material Curves that will be used for SkeletalMeshComponent **/
	TMap<FName, float>	MaterialParameterCurves;
	/** Material parameters that we had been changing and now need to clear */
	TArray<FName> MaterialParamatersToClear;



#if WITH_EDITORONLY_DATA
	// Maximum playback position ever reached (only used when debugging in Persona)
	double LifeTimer;

	// Current scrubbing playback position (only used when debugging in Persona)
	double CurrentLifeTimerScrubPosition;
#endif

public:
	int16 GetSlotNodeInitializationCounter() const { return SlotNodeInitializationCounter; }
	int16 GetGraphTraversalCounter() const { return GraphTraversalCounter; }

	/** Increment traversal counter every time the graph is about to be traversed, to ensure every node is only touched once. */
	void IncrementGraphTraversalCounter();

private:
	/** Counter so we register Slot nodes just once per Initialization pass 
	 * State can trigger initialization later, and we don't want to register these nodes multiple times. */
	UPROPERTY(Transient)
	int16 SlotNodeInitializationCounter;

	/** Counter incremented every time the graph is about to be traversed, to ensure every node is only touched once. */
	UPROPERTY(Transient)
	int16 GraphTraversalCounter;

	TMap<FName, FMontageActiveSlotTracker> SlotWeightTracker;

public:
	/** 
	 * Recalculate Required Bones [RequiredBones]
	 * Is called when bRequiredBonesUpToDate = false
	 */
	void RecalcRequiredBones();

	/** When RequiredBones mapping has changed, AnimNodes need to update their bones caches. */
	UPROPERTY(Transient)
	bool bBoneCachesInvalidated;

	// @todo document
	inline USkeletalMeshComponent* GetSkelMeshComponent() const { return CastChecked<USkeletalMeshComponent>(GetOuter()); }

	virtual UWorld* GetWorld() const override;

	/** Add anim notifier **/
	void AddAnimNotifies(const TArray<const FAnimNotifyEvent*>& NewNotifies, const float InstanceWeight);

	/** Should the notifies current filtering mode stop it from triggering */
	bool PassesFiltering(const FAnimNotifyEvent* Notify) const;

	/** Work out whether this notify should be triggered based on its chance of triggering value */
	bool PassesChanceOfTriggering(const FAnimNotifyEvent* Event) const;

	/** Queues an Anim Notify from the shared list on our generated class */
	void AddAnimNotifyFromGeneratedClass(int32 NotifyIndex);

	/** Trigger AnimNotifies **/
	void TriggerAnimNotifies(float DeltaSeconds);
	void TriggerSingleAnimNotify(const FAnimNotifyEvent* AnimNotifyEvent);

	/** Add curve float data using a curve Uid, the name of the curve will be resolved from the skeleton **/
	void AddCurveValue(const USkeleton::AnimCurveUID Uid, float Value, int32 CurveTypeFlags);

protected:
	/** 
	 * Add curve float data, using a curve name. External values should all be added using
	 * The curve UID to the public version of this method
	 */
	void AddCurveValue(const FName& CurveName, float Value, int32 CurveTypeFlags);

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

private:
	/** Active Root Motion Montage Instance, if any. */
	struct FAnimMontageInstance * RootMotionMontageInstance;

	// Root motion extracted from animation since the last time ConsumeExtractedRootMotion was called
	FRootMotionMovementParams ExtractedRootMotion;

	// Native bindings
private:
	/** Bind any native delegates that we have set up */
	void BindNativeDelegates();

	/** Native transition rules */
	TArray<FNativeTransitionBinding> NativeTransitionBindings;

	/** Native state entry bindings */
	TArray<FNativeStateBinding> NativeStateEntryBindings;

	/** Native state exit bindings */
	TArray<FNativeStateBinding> NativeStateExitBindings;
};

