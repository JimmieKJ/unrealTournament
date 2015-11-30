// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimNodeBase.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimNotifyQueue.h"
#include "AnimInstanceProxy.generated.h"

struct FAnimNode_Base;

/** Delegate fired for nodes to gather any data they need on the game thread before */
DECLARE_DELEGATE_OneParam(FGameThreadPreUpdateEvent, const UAnimInstance* /*InAnimInstance*/);

/** Proxy object passed around during animation tree update in lieu of a UAnimInstance */
USTRUCT(meta = (DisplayName = "Native Variables"))
struct ENGINE_API FAnimInstanceProxy
{
	GENERATED_BODY()

public:
	FAnimInstanceProxy()
	{
	}

	FAnimInstanceProxy(UAnimInstance* Instance)
		: AnimInstanceObject(Instance)
		, AnimClassInterface(IAnimClassInterface::GetFromClass(Instance->GetClass()))
		, Skeleton(nullptr)
		, SkeletalMeshComponent(nullptr)
		, CurrentDeltaSeconds(0.0f)
#if WITH_EDITORONLY_DATA
		, bIsBeingDebugged(false)
#endif
		, RootNode(nullptr)
		, SyncGroupWriteIndex(0)
		, RootMotionMode(ERootMotionMode::NoRootMotionExtraction)
		, bShouldExtractRootMotion(false)
		, bBoneCachesInvalidated(false)
	{
	}

	virtual ~FAnimInstanceProxy() {}

	// Get the IAnimClassInterface associated with this context, if there is one.
	// Note: This can return NULL, so check the result.
	IAnimClassInterface* GetAnimClassInterface() const
	{
		return AnimClassInterface;
	}

	// Get the Blueprint Generated Class associated with this context, if there is one.
	// Note: This can return NULL, so check the result.
	DEPRECATED(4.11, "GetAnimBlueprintClass() is deprecated, UAnimBlueprintGeneratedClass should not be directly used at runtime. Please use GetAnimClassInterface() instead.")
	UAnimBlueprintGeneratedClass* GetAnimBlueprintClass() const
	{
		return Cast<UAnimBlueprintGeneratedClass>(IAnimClassInterface::GetActualAnimClass(AnimClassInterface));
	}

	/** Get the last DeltaSeconds passed into PreUpdate() */
	float GetDeltaSeconds() const
	{
		return CurrentDeltaSeconds;
	}

#if WITH_EDITORONLY_DATA
	/** Whether the UAnimInstance this context refers to is currently being debugged in the editor */
	bool IsBeingDebugged() const
	{
		return bIsBeingDebugged;
	}

	/** Record a visited node in the debugger */
	void RecordNodeVisit(int32 TargetNodeIndex, int32 SourceNodeIndex, float BlendWeight)
	{
		new (UpdatedNodesThisFrame) FAnimBlueprintDebugData::FNodeVisit(SourceNodeIndex, TargetNodeIndex, BlendWeight);
	}

	UAnimBlueprint* GetAnimBlueprint() const
	{
		UClass* ActualAnimClass = IAnimClassInterface::GetActualAnimClass(AnimClassInterface);
		return ActualAnimClass ? Cast<UAnimBlueprint>(ActualAnimClass->ClassGeneratedBy) : nullptr;
	}
#endif

	// flip sync group read/write indices
	void TickSyncGroupWriteIndex()
	{ 
		SyncGroupWriteIndex = GetSyncGroupReadIndex();
	}

	/** Get the sync group we are currently reading from */
	const TArray<FAnimGroupInstance>& GetSyncGroupRead() const
	{ 
		return SyncGroupArrays[GetSyncGroupReadIndex()]; 
	}

	/** Get the ungrouped active player we are currently reading from */
	const TArray<FAnimTickRecord>& GetUngroupedActivePlayersRead() 
	{ 
		return UngroupedActivePlayerArrays[GetSyncGroupReadIndex()]; 
	}

	/** Tick active asset players */
	void TickAssetPlayerInstances(float DeltaSeconds);

	/** Queues an Anim Notify from the shared list on our generated class */
	void AddAnimNotifyFromGeneratedClass(int32 NotifyIndex);

	/** Trigger any anim notifies */
	void TriggerAnimNotifies(USkeletalMeshComponent* SkelMeshComp, float DeltaSeconds);

	/** Check whether the supplied skeleton is compatible with this instance's skeleton */
	bool IsSkeletonCompatible(USkeleton const* InSkeleton) const 
	{ 
		return InSkeleton->IsCompatible(Skeleton); 
	}

	/** Check whether we should extract root motion */
	bool ShouldExtractRootMotion() const 
	{ 
		return bShouldExtractRootMotion;
	}

	/** Access various counters */
	const FGraphTraversalCounter& GetInitializationCounter() const { return InitializationCounter; }
	const FGraphTraversalCounter& GetCachedBonesCounter() const  { return CachedBonesCounter; }
	const FGraphTraversalCounter& GetUpdateCounter() const  { return UpdateCounter; }
	const FGraphTraversalCounter& GetEvaluationCounter() const { return EvaluationCounter; }
	const FGraphTraversalCounter& GetSlotNodeInitializationCounter() const { return SlotNodeInitializationCounter; }

	/** Access root motion params */
	FRootMotionMovementParams& GetExtractedRootMotion() { return ExtractedRootMotion; }

	/** Access UObject base of UAnimInstance */
	UObject* GetAnimInstanceObject() { return AnimInstanceObject; }

	/** Gets an unchecked (can return nullptr) node given an index into the node property array */
	FAnimNode_Base* GetNodeFromIndexUntyped(int32 NodeIdx, UScriptStruct* RequiredStructType);

	/** Gets a checked node given an index into the node property array */
	FAnimNode_Base* GetCheckedNodeFromIndexUntyped(int32 NodeIdx, UScriptStruct* RequiredStructType);

	/** Gets a checked node given an index into the node property array */
	template<class NodeType>
	NodeType* GetCheckedNodeFromIndex(int32 NodeIdx)
	{
		return (NodeType*)GetCheckedNodeFromIndexUntyped(NodeIdx, NodeType::StaticStruct());
	}

	/** Gets an unchecked (can return nullptr) node given an index into the node property array */
	template<class NodeType>
	NodeType* GetNodeFromIndex(int32 NodeIdx)
	{
		return (NodeType*)GetNodeFromIndexUntyped(NodeIdx, NodeType::StaticStruct());
	}

	/** const access to required bones array */
	const FBoneContainer& GetRequiredBones() const 
	{ 
		return RequiredBones; 
	}

	/** access to required bones array */
	FBoneContainer& GetRequiredBones() 
	{ 
		return RequiredBones;
	}

	/** Get the current skeleton we are using */
	USkeleton* GetSkeleton() 
	{ 
		return Skeleton; 
	}

	/** Get the current skeletal mesh component we are running on */
	USkeletalMeshComponent* GetSkelMeshComponent() 
	{ 
		return SkeletalMeshComponent; 
	}

	// Creates an uninitialized tick record in the list for the correct group or the ungrouped array.  If the group is valid, OutSyncGroupPtr will point to the group.
	FAnimTickRecord& CreateUninitializedTickRecord(int32 GroupIndex, FAnimGroupInstance*& OutSyncGroupPtr);

	/** Helper function: make a tick record for a sequence */
	void MakeSequenceTickRecord(FAnimTickRecord& TickRecord, UAnimSequenceBase* Sequence, bool bLooping, float PlayRate, float FinalBlendWeight, float& CurrentTime, FMarkerTickRecord& MarkerTickRecord) const;

	/** Helper function: make a tick record for a blend space */
	void MakeBlendSpaceTickRecord(FAnimTickRecord& TickRecord, UBlendSpaceBase* BlendSpace, const FVector& BlendInput, TArray<FBlendSampleData>& BlendSampleDataCache, FBlendFilter& BlendFilter, bool bLooping, float PlayRate, float FinalBlendWeight, float& CurrentTime, FMarkerTickRecord& MarkerTickRecord) const;

	/**
	 * Get Slot Node Weight : this returns new Slot Node Weight, Source Weight, Original TotalNodeWeight
	 *							this 3 values can't be derived from each other
	 *
	 * @param SlotNodeName : the name of the slot node you're querying
	 * @param out_SlotNodeWeight : The node weight for this slot node in the range of [0, 1]
	 * @param out_SourceWeight : The Source weight for this node. 
	 * @param out_TotalNodeWeight : Total weight of this node
	 */
	void GetSlotWeight(FName const& SlotNodeName, float& out_SlotNodeWeight, float& out_SourceWeight, float& out_TotalNodeWeight) const;

	/** Evaluate a pose for a named montage slot */
	void SlotEvaluatePose(FName SlotNodeName, const FCompactPose& SourcePose, const FBlendedCurve& SourceCurve, float InSourceWeight, FCompactPose& BlendedPose, FBlendedCurve& BlendedCurve, float InBlendWeight, float InTotalNodeWeight);
	
	// Allow slot nodes to store off their weight during ticking
	void UpdateSlotNodeWeight(FName SlotNodeName, float Weight);

	// Allow slot nodes to store off their root motion weight during ticking
	void UpdateSlotRootMotionWeight(FName SlotNodeName, float Weight);

	/** Register a named slot */
	void RegisterSlotNodeWithAnimInstance(FName SlotNodeName);

	/** 
	 * Add a pre-update event. Events can be added more than once. Subsequent adds will be ignored.
	 * Events are called from PreUpdate() on the game thread.
	 * This is intended to be used by FAnimNode_Base-derived classes that need to perform some Update() work on the game thread.
	 * @param	InEvent		The event to register
	 */
	void AddGameThreadPreUpdateEvent(const FGameThreadPreUpdateEvent& InEvent);

	/** Check whether we have a valid root node */
	bool HasRootNode() const
	{ 
		return RootNode != nullptr; 
	}

	/** @todo: remove after deprecation */
	FAnimNode_Base* GetRootNode() 
	{ 
		return RootNode;
	}

	/** Gather debug data from this instance proxy and the blend tree for display */
	void GatherDebugData(FNodeDebugData& DebugData);

#if !NO_LOGGING
	const FString& GetActorName() const 
	{
		return ActorName; 
	}
#endif

#if DO_CHECK
	const FString& GetAnimInstanceName() const 
	{ 
		return AnimInstanceName;
	}
#endif

	/** Gets the runtime instance of the specified state machine by Name */
	FAnimNode_StateMachine* GetStateMachineInstanceFromName(FName MachineName);

	/** Get the machine description for the specified instance. Does not rely on PRIVATE_MachineDescription being initialized */
	static const FBakedAnimationStateMachine* GetMachineDescription(IAnimClassInterface* AnimBlueprintClass, FAnimNode_StateMachine* MachineInstance);

	/** 
	 * Get the index of the specified instance asset player. Useful to pass to GetInstanceAssetPlayerLength (etc.).
	 * Passing NAME_None to InstanceName will return the first (assumed only) player instance index found.
	 */
	int32 GetInstanceAssetPlayerIndex(FName MachineName, FName StateName, FName InstanceName = NAME_None);

	/** Only restricted classes can access the protected interface */
	friend class UAnimInstance;
	friend class UAnimSingleNodeInstance;
	friend class USkeletalMeshComponent;

protected:
	/** Called when our anim instance is being initialized */
	virtual void Initialize(UAnimInstance* InAnimInstance);

	/** Called when our anim instance is being uninitialized */
	virtual void Uninitialize(UAnimInstance* InAnimInstance);

	/** Called before update so we can copy any data we need */
	virtual void PreUpdate(const UAnimInstance* InAnimInstance, float DeltaSeconds);

	/** Update override point */
	virtual void Update(float DeltaSeconds) {}

	/** Updates the anim graph */
	virtual void UpdateAnimationNode(float DeltaSeconds);

	/** 
	 * Evaluate override point 
	 * @return true if this function is implemented, false otherwise.
	 * Note: the node graph will not be evaluated if this function returns true
	 */
	virtual bool Evaluate(FPoseContext& Output) { return false; }

	/** Called after update so we can copy any data we need */
	virtual void PostUpdate(UAnimInstance* InAnimInstance) const;

	/** Calls Update(), updates the anim graph, ticks asset players */
	void UpdateAnimation();

	/** Evaluates the anim graph */
	void EvaluateAnimation(FPoseContext& Output);

	// @todo document
	void SequenceAdvanceImmediate(UAnimSequenceBase* Sequence, bool bLooping, float PlayRate, float DeltaSeconds, /*inout*/ float& CurrentTime, FMarkerTickRecord& MarkerTickRecord);

	// @todo document
	void BlendSpaceAdvanceImmediate(UBlendSpaceBase* BlendSpace, const FVector& BlendInput, TArray<FBlendSampleData> & BlendSampleDataCache, FBlendFilter & BlendFilter, bool bLooping, float PlayRate, float DeltaSeconds, /*inout*/ float& CurrentTime, FMarkerTickRecord& MarkerTickRecord);

	// Gets the sync group we should be reading from
	int32 GetSyncGroupReadIndex() const 
	{ 
		return 1 - SyncGroupWriteIndex; 
	}

	// Gets the sync group we should be writing to
	int32 GetSyncGroupWriteIndex() const 
	{ 
		return SyncGroupWriteIndex; 
	}

	/** Add anim notifier **/
	void AddAnimNotifies(const TArray<const FAnimNotifyEvent*>& NewNotifies, const float InstanceWeight);

	/** Returns the baked sync group index from the compile step */
	int32 GetSyncGroupIndexFromName(FName SyncGroupName) const;

	bool GetTimeToClosestMarker(FName SyncGroup, FName MarkerName, float& OutMarkerTime) const;

	bool HasMarkerBeenHitThisFrame(FName SyncGroup, FName MarkerName) const;

	bool IsSyncGroupBetweenMarkers(FName InSyncGroupName, FName PreviousMarker, FName NextMarker, bool bRespectMarkerOrder = true) const;

	FMarkerSyncAnimPosition GetSyncGroupPosition(FName InSyncGroupName) const;

	// slot node run-time functions
	void ReinitializeSlotNodes();

	// if it doesn't tick, it will keep old weight, so we'll have to clear it in the beginning of tick
	void ClearSlotNodeWeights();
	bool IsSlotNodeRelevantForNotifies(FName SlotNodeName) const;

	// Get the root motion weight for the montage slot
	float GetSlotRootMotionWeight(FName SlotNodeName) const;

	/** 
	 * Recalculate Required Bones [RequiredBones]
	 * Is called when bRequiredBonesUpToDate = false
	 */
	void RecalcRequiredBones(USkeletalMeshComponent* Component, UObject* Asset);

	/** Update morph target curves for external map (from USkeletalMeshComponent) */
	void UpdateMorphTargetCurves(const TMap<FName, float>& InMorphTargetCurves);

	/** Update the material parameters of the supplied component from this instance */
	void UpdateComponentsMaterialParameters(UPrimitiveComponent* Component);

	/** Get Currently active montage evaluation state.
		Note that there might be multiple Active at the same time. This will only return the first active one it finds. **/
	const FMontageEvaluationState* GetActiveMontageEvaluationState() const;

	/** Access montage array data */
	TArray<FMontageEvaluationState>& GetMontageEvaluationData() { return MontageEvaluationData; }

	/** Access montage array data */
	const TArray<FMontageEvaluationState>& GetMontageEvaluationData() const { return MontageEvaluationData; }

	/** Check whether we have active morph target curves */
	bool HasMorphTargetCurves() const { return MorphTargetCurves.Num() > 0; }

	/** Access the active morph target curves */
	TMap<FName, float>& GetMorphTargetCurves() { return MorphTargetCurves; }

	/** Access the active material parameter curves */
	TMap<FName, float>& GetMaterialParameterCurves() { return MaterialParameterCurves; }

	/** Check whether we have active morph target curves */
	TArray<struct FActiveVertexAnim>& GetVertexAnims() { return VertexAnims; }

	/** Adds an active vertex anim */
	void AddVertexAnim(const FActiveVertexAnim& VertexAnim);

	/** Gets the most relevant asset player in a specified state */
	FAnimNode_AssetPlayerBase* GetRelevantAssetPlayerFromState(int32 MachineIndex, int32 StateIndex);

	/** Gets the runtime instance of the specified state machine */
	FAnimNode_StateMachine* GetStateMachineInstance(int32 MachineIndex);

	/** Gets an unchecked (can return nullptr) node given a property of the anim instance */
	template<class NodeType>
	NodeType* GetNodeFromProperty(UProperty* Property)
	{
		return (NodeType*)Property->ContainerPtrToValuePtr<NodeType>(AnimInstanceObject);
	}

	/** Gets the length in seconds of the asset referenced in an asset player node */
	float GetInstanceAssetPlayerLength(int32 AssetPlayerIndex);

	/** Get the current accumulated time in seconds for an asset player node */
	float GetInstanceAssetPlayerTime(int32 AssetPlayerIndex);

	/** Get the current accumulated time as a fraction for an asset player node */
	float GetInstanceAssetPlayerTimeFraction(int32 AssetPlayerIndex);

	/** Get the time in seconds from the end of an animation in an asset player node */
	float GetInstanceAssetPlayerTimeFromEnd(int32 AssetPlayerIndex);

	/** Get the time as a fraction of the asset length of an animation in an asset player node */
	float GetInstanceAssetPlayerTimeFromEndFraction(int32 AssetPlayerIndex);

	/** Get the blend weight of a specified state */
	float GetInstanceStateWeight(int32 MachineIndex, int32 StateIndex);

	/** Get the current elapsed time of a state within the specified state machine */
	float GetInstanceCurrentStateElapsedTime(int32 MachineIndex);

	/** Get the crossfade duration of a specified transition */
	float GetInstanceTransitionCrossfadeDuration(int32 MachineIndex, int32 TransitionIndex);

	/** Get the elapsed time in seconds of a specified transition */
	float GetInstanceTransitionTimeElapsed(int32 MachineIndex, int32 TransitionIndex);

	/** Get the elapsed time as a fraction of the crossfade duration of a specified transition */
	float GetInstanceTransitionTimeElapsedFraction(int32 MachineIndex, int32 TransitionIndex);

	/** Get the time remaining in seconds for the most relevant animation in the source state */
	float GetRelevantAnimTimeRemaining(int32 MachineIndex, int32 StateIndex);

	/** Get the time remaining as a fraction of the duration for the most relevant animation in the source state */
	float GetRelevantAnimTimeRemainingFraction(int32 MachineIndex, int32 StateIndex);

	/** Get the length in seconds of the most relevant animation in the source state */
	float GetRelevantAnimLength(int32 MachineIndex, int32 StateIndex);

	/** Get the current accumulated time in seconds for the most relevant animation in the source state */
	float GetRelevantAnimTime(int32 MachineIndex, int32 StateIndex);

	/** Get the current accumulated time as a fraction of the length of the most relevant animation in the source state */
	float GetRelevantAnimTimeFraction(int32 MachineIndex, int32 StateIndex);

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

	/** Bind any native delegates that we have set up */
	void BindNativeDelegates();

	/** Gets the runtime instance desc of the state machine specified by name */
	const FBakedAnimationStateMachine* GetStateMachineInstanceDesc(FName MachineName);

	/** Gets the index of the state machine matching MachineName */
	int32 GetStateMachineIndex(FName MachineName);

	/** Initialize the root node - split into a separate function for backwards compatibility (initialization order) reasons */
	void InitializeRootNode();

private:
	/** Object ptr to our UAnimInstance */
	mutable UObject* AnimInstanceObject;

	/** Our anim blueprint generated class */
	IAnimClassInterface* AnimClassInterface;

	/** Skeleton we are using, only used for comparison purposes */
	USkeleton* Skeleton;

	/** Skeletal mesh component we are attached to */
	USkeletalMeshComponent* SkeletalMeshComponent;

	/** The last time passed into PreUpdate() */
	float CurrentDeltaSeconds;

#if WITH_EDITORONLY_DATA
	/** Whether this UAnimInstance is currently being debugged in the editor */
	bool bIsBeingDebugged;

	/** Array of visited nodes this frame */
	TArray<FAnimBlueprintDebugData::FNodeVisit> UpdatedNodesThisFrame;
#endif

#if !NO_LOGGING
	/** Actor name for debug logging purposes */
	FString ActorName;
#endif

#if DO_CHECK
	/** Anim instance name for debug purposes */
	FString AnimInstanceName;
#endif

	/** Anim graph */
	FAnimNode_Base* RootNode;

	/** The list of animation assets which are going to be evaluated this frame and need to be ticked (ungrouped) */
	TArray<FAnimTickRecord> UngroupedActivePlayerArrays[2];

	/** The set of tick groups for this anim instance */
	TArray<FAnimGroupInstance> SyncGroupArrays[2];

	// Current sync group buffer index
	int32 SyncGroupWriteIndex;

	/** Animation Notifies that has been triggered in the latest tick **/
	FAnimNotifyQueue NotifyQueue;

	// Root motion mode duplicated from the anim instance
	ERootMotionMode::Type RootMotionMode;

	// Diplicate of bool result of ShouldExtractRootMotion()
	bool bShouldExtractRootMotion;

	// Tracker map for slot name->weights/relevancy
	TMap<FName, FMontageActiveSlotTracker> SlotWeightTracker;

	// Counters for synchronization
	FGraphTraversalCounter InitializationCounter;
	FGraphTraversalCounter CachedBonesCounter;
	FGraphTraversalCounter UpdateCounter;
	FGraphTraversalCounter EvaluationCounter;
	FGraphTraversalCounter SlotNodeInitializationCounter;

	// Root motion extracted from animation since the last time ConsumeExtractedRootMotion was called
	FRootMotionMovementParams ExtractedRootMotion;

	/** Temporary array of bone indices required this frame. Should be subset of Skeleton and Mesh's RequiredBones */
	FBoneContainer RequiredBones;

	/** When RequiredBones mapping has changed, AnimNodes need to update their bones caches. */
	bool bBoneCachesInvalidated;

	/** Morph Target Curves that will be used for SkeletalMeshComponent **/
	TMap<FName, float> MorphTargetCurves;

	/** Material Curves that will be used for SkeletalMeshComponent **/
	TMap<FName, float> MaterialParameterCurves;

	/** Copy of UAnimInstance::MontageInstances data used for update & evaluation */
	TArray<FMontageEvaluationState> MontageEvaluationData;

	/** Array indicating active vertex anims (by reference) generated by anim instance. */
	TArray<struct FActiveVertexAnim> VertexAnims;

	/** Delegate fired on the game thread before update occurs */
	TArray<FGameThreadPreUpdateEvent> GameThreadPreUpdateEvents;

	/** Native transition rules */
	TArray<FNativeTransitionBinding> NativeTransitionBindings;

	/** Native state entry bindings */
	TArray<FNativeStateBinding> NativeStateEntryBindings;

	/** Native state exit bindings */
	TArray<FNativeStateBinding> NativeStateExitBindings;

};
