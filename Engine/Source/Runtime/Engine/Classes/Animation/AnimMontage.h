// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Abstract base class of animation made of multiple sequences.
 *
 */

#include "AnimCompositeBase.h"
#include "Animation/AnimInstance.h"
#include "AnimMontage.generated.h"

class UAnimMontage;

/**
 * Section data for each track. Reference of data will be stored in the child class for the way they want
 * AnimComposite vs AnimMontage have different requirement for the actual data reference
 * This only contains composite section information. (vertical sequences)
 */
USTRUCT()
struct FCompositeSection : public FAnimLinkableElement
{
	GENERATED_USTRUCT_BODY()

	/** Section Name */
	UPROPERTY(EditAnywhere, Category=Section)
	FName SectionName;

	/** Start Time **/
	UPROPERTY()
	float StartTime_DEPRECATED;

	/** Should this animation loop. */
	UPROPERTY()
	FName NextSectionName;

	FCompositeSection()
		: FAnimLinkableElement()
		, SectionName(NAME_None)
		, NextSectionName(NAME_None)
	{
	}
};

/**
 * Each slot data referenced by Animation Slot 
 * contains slot name, and animation data 
 */
USTRUCT()
struct FSlotAnimationTrack
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=Slot)
	FName SlotName;

	UPROPERTY(EditAnywhere, Category=Slot)
	FAnimTrack AnimTrack;

	FSlotAnimationTrack()
		: SlotName(FAnimSlotGroup::DefaultSlotName)
	{}
};

/** 
 * Remove FBranchingPoint when VER_UE4_MONTAGE_BRANCHING_POINT_REMOVAL is removed.
 */
USTRUCT()
struct FBranchingPoint : public FAnimLinkableElement
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=BranchingPoint)
	FName EventName;

	UPROPERTY()
	float DisplayTime_DEPRECATED;

	/** An offset from the DisplayTime to the actual time we will trigger the notify, as we cannot always trigger it exactly at the time the user wants */
	UPROPERTY()
	float TriggerTimeOffset;

	/** Returns the time this branching point should be triggered */
	float GetTriggerTime() const { return GetTime() + TriggerTimeOffset; }
};

/**  */
UENUM()
namespace EAnimNotifyEventType
{
	enum Type
	{
		/** */
		Begin,
		/** */
		End,
	};
}

/** AnimNotifies marked as BranchingPoints will create these markers on their Begin/End times.
	They create stopping points when the Montage is being ticked to dispatch events. */
USTRUCT()
struct FBranchingPointMarker
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	int32 NotifyIndex;
	
	UPROPERTY()
	float TriggerTime;

	UPROPERTY()
	TEnumAsByte<EAnimNotifyEventType::Type> NotifyEventType;

	FBranchingPointMarker()
		: NotifyIndex(INDEX_NONE)
		, TriggerTime(0.f)
		, NotifyEventType(EAnimNotifyEventType::Begin)
	{
	}

	FBranchingPointMarker(int32 InNotifyIndex, float InTriggerTime, EAnimNotifyEventType::Type InNotifyEventType)
		: NotifyIndex(InNotifyIndex)
		, TriggerTime(InTriggerTime)
		, NotifyEventType(InNotifyEventType)
	{
	}
};

/**
 * Delegate for when Montage is completed, whether interrupted or finished
 * Weight of this montage is 0.f, so it stops contributing to output pose
 *
 * bInterrupted = true if it was not property finished
 */
DECLARE_DELEGATE_TwoParams( FOnMontageEnded, class UAnimMontage*, bool /*bInterrupted*/) 
/**
 * Delegate for when Montage started to blend out, whether interrupted or finished
 * DesiredWeight of this montage becomes 0.f, but this still contributes to the output pose
 * 
 * bInterrupted = true if it was not property finished
 */
DECLARE_DELEGATE_TwoParams( FOnMontageBlendingOutStarted, class UAnimMontage*, bool /*bInterrupted*/) 

USTRUCT()
struct FAnimMontageInstance
{
	GENERATED_USTRUCT_BODY()

	// Montage reference
	UPROPERTY()
	class UAnimMontage* Montage;

public: 
	UPROPERTY()
	float DesiredWeight;

	UPROPERTY()
	float Weight;

	UPROPERTY(transient)
	float BlendTime;

	// Blend Time multiplier to allow extending and narrowing blendtimes
	UPROPERTY(transient)
	float DefaultBlendTimeMultiplier;

	// list of next sections per section - index of array is section id
	UPROPERTY()
	TArray<int32> NextSections;

	// list of prev sections per section - index of array is section id
	UPROPERTY()
	TArray<int32> PrevSections;

	UPROPERTY()
	bool bPlaying;

	// delegates
	FOnMontageEnded OnMontageEnded;
	FOnMontageBlendingOutStarted OnMontageBlendingOutStarted;

	// reference to AnimInstance
	TWeakObjectPtr<UAnimInstance> AnimInstance;

	// need to save if it's interrupted or not
	// this information is crucial for gameplay
	bool bInterrupted;

	// transient PreviousWeight - Weight of previous tick
	float PreviousWeight;

	// transient NotifyWeight   - Weight for spawned notifies, modified slightly to make sure
	//                          - we spawn all notifies
	float NotifyWeight;

	/** Currently Active AnimNotifyState, stored as a copy of the event as we need to
		call NotifyEnd on the event after a deletion in the editor. After this the event
		is removed correctly. */
	UPROPERTY(Transient)
	TArray<FAnimNotifyEvent> ActiveStateBranchingPoints;

private:
	UPROPERTY()
	float Position;

	UPROPERTY()
	float PlayRate;

public:
	/** Montage to Montage Synchronization.
	 *
	 * A montage can only have a single leader. A leader can have multiple followers.
	 * Loops cause no harm.
	 * If Follower gets ticked before Leader, then synchronization will be performed with a frame of lag.
	 *		Essentially correcting the previous frame. Which is enough for simple cases (i.e. no timeline jumps from notifies).
	 * If Follower gets ticked after Leader, then synchronization will be exact and support more complex cases (i.e. timeline jumps).
	 *		This can be enforced by setting up tick pre-requisites if desired.
	 */
	ENGINE_API void MontageSync_Follow(struct FAnimMontageInstance* NewLeaderMontageInstance);
	/** Stop leading, release all followers. */
	void MontageSync_StopLeading();
	/** Stop following our leader */
	void MontageSync_StopFollowing();
	/** PreUpdate - Sync if updated before Leader. */
	void MontageSync_PreUpdate();
	/** PostUpdate - Sync if updated after Leader. */
	void MontageSync_PostUpdate();

private:
	/** Followers this Montage will synchronize */
	TArray<struct FAnimMontageInstance*> MontageSyncFollowers;
	/** Leader this Montage will follow */
	struct FAnimMontageInstance* MontageSyncLeader;
	/** Frame counter to sync montages once per frame */
	uint32 MontageSyncUpdateFrameCounter;

	/** true if montage has been updated this frame */
	bool MontageSync_HasBeenUpdatedThisFrame() const;
	/** This frame's counter, to track which Montages have been updated */
	uint32 MontageSync_GetFrameCounter() const;
	/** Synchronize ourselves to our leader */
	void MontageSync_PerformSyncToLeader();

public:
	FAnimMontageInstance()
		: Montage(NULL)
		, DesiredWeight(0.f)
		, Weight(0.f)
		, BlendTime(0.f)
		, DefaultBlendTimeMultiplier(1.0f)
		, bPlaying(false)	
		, AnimInstance(NULL)
		, bInterrupted(false)
		, PreviousWeight(0.f)
		, Position(0.f)
		, PlayRate(1.f)
		, MontageSyncLeader(NULL)
		, MontageSyncUpdateFrameCounter(INDEX_NONE)
	{
	}

	FAnimMontageInstance(UAnimInstance * InAnimInstance)
		: Montage(NULL)
		, DesiredWeight(0.f)
		, Weight(0.f)
		, BlendTime(0.f)
		, DefaultBlendTimeMultiplier(1.0f)
		, bPlaying(false)		
		, AnimInstance(InAnimInstance)
		, bInterrupted(false)
		, PreviousWeight(0.f)	
		, Position(0.f)
		, PlayRate(1.f)
		, MontageSyncLeader(NULL)
		, MontageSyncUpdateFrameCounter(INDEX_NONE)
	{
	}

	// montage instance interfaces
	void Play(float InPlayRate = 1.f);
	void Stop(float BlendOutDuration, bool bInterrupt=true);
	void Pause();
	void Initialize(class UAnimMontage * InMontage);

	bool JumpToSectionName(FName const & SectionName, bool bEndOfSection = false);
	bool SetNextSectionName(FName const & SectionName, FName const & NewNextSectionName);
	bool SetNextSectionID(int32 const & SectionID, int32 const & NewNextSectionID);

	bool IsValid() const { return (Montage!=NULL); }
	bool IsPlaying() const { return IsValid() && bPlaying; }
	bool IsStopped() const { return DesiredWeight == 0.f; }

	/** Returns true if this montage is active (valid and not blending out) */
	bool IsActive() const { return (IsValid() && !IsStopped()); }

	void Terminate();

	/**
	 *  Getters
	 */
	float GetPosition() const { return Position; };
	float GetPlayRate() const { return PlayRate; }

	/** 
	 * Setters
	 */
	void SetPosition(float const & InPosition) { Position = InPosition; }
	void SetPlayRate(float const & InPlayRate) { PlayRate = InPlayRate; }

	/**
	 * Montage Tick happens in 2 phases
	 *
	 * first is to update weight of current montage only
	 * this will make sure that all nodes will get up-to-date weight information
	 * when update comes in for them
	 *
	 * second is normal tick. This tick has to happen later when all node ticks
	 * to accumulate and update curve data/notifies/branching points
	 */
	void UpdateWeight(float DeltaTime);
	/** Simulate is same as Advance, but without calling any events or touching any of the instance data. So it performs a simulation of advancing the timeline. 
	 * @fixme laurent can we make Advance use that, so we don't have 2 code paths which risk getting out of sync? */
	bool SimulateAdvance(float DeltaTime, float& InOutPosition, struct FRootMotionMovementParams & OutRootMotionParams) const;
	void Advance(float DeltaTime, struct FRootMotionMovementParams * OutRootMotionParams, bool bBlendRootMotion);

	FName GetCurrentSection() const;
	FName GetNextSection() const;
	int32 GetNextSectionID(int32 const & CurrentSectionID) const;
	FName GetSectionNameFromID(int32 const & SectionID) const;

	// reference has to be managed manually
	void AddReferencedObjects( FReferenceCollector& Collector );

	/** Delegate function handlers
	 */
	void HandleEvents(float PreviousTrackPos, float CurrentTrackPos, const FBranchingPointMarker* BranchingPointMarker);
private:
	/** Called by blueprint functions that modify the montages current position. */
	void OnMontagePositionChanged(FName const & ToSectionName);
	
	/** Updates ActiveStateBranchingPoints array and triggers Begin/End notifications based on CurrentTrackPosition */
	void UpdateActiveStateBranchingPoints(float CurrentTrackPosition);

	/** Trigger associated events when Montage ticking reaches given FBranchingPointMarker */
	void BranchingPointEventHandler(const FBranchingPointMarker* BranchingPointMarker);
	void RefreshNextPrevSections();
};

UCLASS(config=Engine, hidecategories=(UObject, Length), MinimalAPI, BlueprintType)
class UAnimMontage : public UAnimCompositeBase
{
	GENERATED_UCLASS_BODY()

	/** Default blend in time. */
	UPROPERTY(EditAnywhere, Category=Montage)
	float BlendInTime;

	/** Default blend out time. */
	UPROPERTY(EditAnywhere, Category=Montage)
	float BlendOutTime;

	/** Time from Sequence End to trigger blend out.
	 * <0 means using BlendOutTime, so BlendOut finishes as Montage ends.
	 * >=0 means using 'SequenceEnd - BlendOutTriggerTime' to trigger blend out. */
	UPROPERTY(EditAnywhere, Category = Montage)
	float BlendOutTriggerTime;

	// composite section. 
	UPROPERTY()
	TArray<FCompositeSection> CompositeSections;
	
	// slot data, each slot contains anim track
	UPROPERTY()
	TArray<struct FSlotAnimationTrack> SlotAnimTracks;

	// Remove this when VER_UE4_MONTAGE_BRANCHING_POINT_REMOVAL is removed.
	UPROPERTY()
	TArray<struct FBranchingPoint> BranchingPoints_DEPRECATED;

	/** If this is on, it will allow extracting root motion translation. DEPRECATED in 4.5 root motion is controlled by anim sequences **/
	UPROPERTY()
	bool bEnableRootMotionTranslation;

	/** If this is on, it will allow extracting root motion rotation. DEPRECATED in 4.5 root motion is controlled by anim sequences **/
	UPROPERTY()
	bool bEnableRootMotionRotation;

	/** Root Bone will be locked to that position when extracting root motion. DEPRECATED in 4.5 root motion is controlled by anim sequences **/
	UPROPERTY()
	TEnumAsByte<ERootMotionRootLock::Type> RootMotionRootLock;

#if WITH_EDITORONLY_DATA
	/** Preview Base pose for additive BlendSpace **/
	UPROPERTY(EditAnywhere, Category=AdditiveSettings)
	UAnimSequence* PreviewBasePose;
#endif // WITH_EDITORONLY_DATA

	/** return true if valid slot */
	bool IsValidSlot(FName InSlotName) const;

public:
	// Begin UObject Interface
	virtual void PostLoad() override;

	// Gets the sequence length of the montage by calculating it from the lengths of the segments in the montage
	ENGINE_API float CalculateSequenceLength();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	// End UObject Interface

	// Begin AnimSequenceBase Interface
	virtual bool IsValidAdditive() const override;
#if WITH_EDITOR
	virtual EAnimEventTriggerOffsets::Type CalculateOffsetForNotify(float NotifyDisplayTime) const override;
#endif // WITH_EDITOR
	// End AnimSequenceBase Interface

#if WITH_EDITOR
	// Begin UAnimationAsset interface
	virtual bool GetAllAnimationSequencesReferred(TArray<UAnimSequence*>& AnimationSequences) override;
	virtual void ReplaceReferredAnimations(const TMap<UAnimSequence*, UAnimSequence*>& ReplacementMap) override;
	// End UAnimationAsset interface

	/** Update all linkable elements contained in the montage */
	ENGINE_API void UpdateLinkableElements();

	/** Update linkable elements that rely on a specific segment. This will update linkable elements for the segment specified
	 *	and elements linked to segments after the segment specified
	 *	@param SlotIdx The slot that the segment is contained in
	 *	@param SegmentIdx The index of the segment within the specified slot
	 */
	ENGINE_API void UpdateLinkableElements(int32 SlotIdx, int32 SegmentIdx);
#endif

	/** Get FCompositeSection with InSectionName */
	FCompositeSection& GetAnimCompositeSection(int32 SectionIndex);
	const FCompositeSection& GetAnimCompositeSection(int32 SectionIndex) const;

	// @todo document
	ENGINE_API void GetSectionStartAndEndTime(int32 SectionIndex, float& OutStartTime, float& OutEndTime) const;
	
	// @todo document
	ENGINE_API float GetSectionLength(int32 SectionIndex) const;
	
	/** Get SectionIndex from SectionName */
	ENGINE_API int32 GetSectionIndex(FName InSectionName) const;
	
	/** Get SectionName from SectionIndex in TArray */
	ENGINE_API FName GetSectionName(int32 SectionIndex) const;

	/** @return true if valid section */
	ENGINE_API bool IsValidSectionName(FName InSectionName) const;

	// @todo document
	ENGINE_API bool IsValidSectionIndex(int32 SectionIndex) const;

	/** Return Section Index from Position */
	ENGINE_API int32 GetSectionIndexFromPosition(float Position) const;
	
	/** Get Section Index from CurrentTime with PosWithinCompositeSection */
	int32 GetAnimCompositeSectionIndexFromPos(float CurrentTime, float& PosWithinCompositeSection) const;

	/** Return time left to end of section from given position. -1.f if not a valid position */
	ENGINE_API float GetSectionTimeLeftFromPos(float Position);

	/** Utility function to calculate Animation Pos from Section, PosWithinCompositeSection */
	float CalculatePos(FCompositeSection &Section, float PosWithinCompositeSection) const;
	
	/** Prototype function to get animation data - this will need rework */
	const FAnimTrack* GetAnimationData(FName SlotName) const;

	/** Returns whether the anim sequences this montage have root motion enabled */
	virtual bool HasRootMotion() const override;

	/** Extract RootMotion Transform from a contiguous Track position range.
	 * *CONTIGUOUS* means that if playing forward StartTractPosition < EndTrackPosition.
	 * No wrapping over if looping. No jumping across different sections.
	 * So the AnimMontage has to break the update into contiguous pieces to handle those cases.
	 *
	 * This does handle Montage playing backwards (StartTrackPosition > EndTrackPosition).
	 *
	 * It will break down the range into steps if needed to handle looping animations, or different animations.
	 * These steps will be processed sequentially, and output the RootMotion transform in component space.
	 */
	ENGINE_API FTransform ExtractRootMotionFromTrackRange(float StartTrackPosition, float EndTrackPosition) const;

	/** Get the Montage's Group Name. This is the group from the first slot.  */
	ENGINE_API FName GetGroupName() const;

	/** true if valid, false otherwise. Will log warning if not valid. */
	bool HasValidSlotSetup() const;

private:
	/** 
	 * Utility function to check if CurrentTime is between FirstIndex and SecondIndex of CompositeSections
	 * return true if it is
	 */
	bool IsWithinPos(int32 FirstIndex, int32 SecondIndex, float CurrentTime) const;

	/** Calculates a trigger offset based on the supplied time taking into account only the montages sections */
	EAnimEventTriggerOffsets::Type CalculateOffsetFromSections(float Time) const;


public:
	// UAnimSequenceBase Interface
	virtual void EvaluateCurveData(class UAnimInstance* Instance, float CurrentTime, float BlendWeight) const override;
	// End of UAnimSequenceBase Interface

#if WITH_EDITOR
	/**
	 * Add Composite section with InSectionName
	 * returns index of added item 
	 * returns INDEX_NONE if failed. - i.e. InSectionName is not unique
	 */
	ENGINE_API int32 AddAnimCompositeSection(FName InSectionName, float StartPos);
	
	/** 
	 * Delete Composite section with InSectionName
	 * return true if success, false otherwise
	 */
	ENGINE_API bool DeleteAnimCompositeSection(int32 SectionIndex);

private:
	/** Sort CompositeSections in the order of StartPos */
	void SortAnimCompositeSectionByPos();

#endif	//WITH_EDITOR

private:

	/** Convert all branching points to AnimNotifies */
	void ConvertBranchingPointsToAnimNotifies();
	/** Recreate BranchingPoint markers from AnimNotifies marked 'BranchingPoints' */
	void RefreshBranchingPointMarkers();
	void AddBranchingPointMarker(FBranchingPointMarker Marker, TMap<float, FAnimNotifyEvent*>& TriggerTimes);
	
	/** Cached list of Branching Point markers */
	UPROPERTY()
	TArray<FBranchingPointMarker> BranchingPointMarkers;

	UPROPERTY(Transient)
	// @remove me: temporary variable to do sort while property window changed
	// this should be fixed when we have tool to do so.
	bool bAnimBranchingPointNeedsSort;

public:

	/** Keep track of which AnimNotify_State are marked as BranchingPoints, so we can update their state when the Montage is ticked */
	UPROPERTY()
	TArray<int32> BranchingPointStateNotifyIndices;

	/** Find first branching point marker between track positions */
	const FBranchingPointMarker* FindFirstBranchingPointMarker(float StartTrackPos, float EndTrackPos);
	/** Filter out notifies from array that are marked as 'BranchingPoints' */
	void FilterOutNotifyBranchingPoints(TArray<const FAnimNotifyEvent*>& InAnimNotifies);
};
