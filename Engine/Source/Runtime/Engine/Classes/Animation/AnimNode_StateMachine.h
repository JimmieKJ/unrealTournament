// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "AnimNodeBase.h"
#include "AnimStateMachineTypes.h"
#include "AnimNode_StateMachine.generated.h"

struct FAnimNode_StateMachine;
struct FAnimNode_TransitionPoseEvaluator;

// Information about an active transition on the transition stack
USTRUCT()
struct FAnimationActiveTransitionEntry
{
	GENERATED_USTRUCT_BODY()

	// Elapsed time for this transition
	float ElapsedTime;

	// The transition alpha between next and previous states
	float Alpha;

	// Duration of this cross-fade (may be shorter than the nominal duration specified by the state machine if the target state had non-zero weight at the start)
	float CrossfadeDuration;

	// Blend type (type of curve applied to time)
	TEnumAsByte<ETransitionBlendMode::Type> CrossfadeMode;

	// Is this transition active?
	bool bActive;

	// Cached Pose for this transition
	TArray<FTransform> InputPose;

	// Graph to run that determines the final pose for this transition
	FPoseLink CustomTransitionGraph;

	// To and from state ids
	int32 NextState;

	int32 PreviousState;

	// Notifies are copied from the reference transition info
	int32 StartNotify;

	int32 EndNotify;

	int32 InterruptNotify;
	
	TEnumAsByte<ETransitionLogicType::Type> LogicType;

	TArray<FAnimNode_TransitionPoseEvaluator*> PoseEvaluators;

#if WITH_EDITORONLY_DATA
	TArray<int32, TInlineAllocator<3>> SourceTransitionIndices;
#endif


public:
	FAnimationActiveTransitionEntry();
	FAnimationActiveTransitionEntry(int32 NextStateID, float ExistingWeightOfNextState, int32 PreviousStateID, const FAnimationTransitionBetweenStates& ReferenceTransitionInfo);
	
	void InitializeCustomGraphLinks(const FAnimationUpdateContext& Context, const FBakedStateExitTransition& TransitionRule);

	void Update(const FAnimationUpdateContext& Context, int32 CurrentStateIndex, bool &OutFinished);
	
	void UpdateCustomTransitionGraph(const FAnimationUpdateContext& Context, FAnimNode_StateMachine& StateMachine, int32 ActiveTransitionIndex);
	void EvaluateCustomTransitionGraph(FPoseContext& Output, FAnimNode_StateMachine& StateMachine, bool IntermediatePoseIsValid, int32 ActiveTransitionIndex);

	bool Serialize(FArchive& Ar);

protected:
	float CalculateInverseAlpha(ETransitionBlendMode::Type BlendType, float InFraction) const;
	float CalculateAlpha(float InFraction) const;
};

USTRUCT()
struct FAnimationPotentialTransition
{
	GENERATED_USTRUCT_BODY()

	int32 TargetState;

	const FBakedStateExitTransition* TransitionRule;

#if WITH_EDITORONLY_DATA
	TArray<int32, TInlineAllocator<3>> SourceTransitionIndices;
#endif

public:
	FAnimationPotentialTransition();
	bool IsValid() const;
	void Clear();
};

//@TODO: ANIM: Need to implement WithSerializer and Identical for FAnimationActiveTransitionEntry?

// State machine node
USTRUCT()
struct ENGINE_API FAnimNode_StateMachine : public FAnimNode_Base
{
	GENERATED_USTRUCT_BODY()

public:
	// Index into the BakedStateMachines array in the owning UAnimBlueprintGeneratedClass
	UPROPERTY()
	int32 StateMachineIndexInClass;

	// The maximum number of transitions that can be taken by this machine 'simultaneously' in a single frame
	UPROPERTY(EditAnywhere, Category=Settings)
	int32 MaxTransitionsPerFrame;
public:

	int32 GetCurrentState() const
	{
		return CurrentState;
	}

	float GetCurrentStateElapsedTime() const
	{
		return ElapsedTime;
	}

#if WITH_EDITORONLY_DATA
	bool IsTransitionActive(int32 TransIndex) const;
#endif

protected:
	// The state machine description this is an instance of
	FBakedAnimationStateMachine* PRIVATE_MachineDescription;

	// The current state within the state machine
	UPROPERTY()
	int32 CurrentState;

	// Elapsed time since entering the current state
	UPROPERTY()
	float ElapsedTime;

	// Current Transition Index being evaluated
	int32 EvaluatingTransitionIndex;

	// The set of active transitions, if there are any
	TArray<FAnimationActiveTransitionEntry> ActiveTransitionArray;

	// The set of states in this state machine
	TArray<FPoseLink> StatePoseLinks;
	
	// Used during transitions to make sure we don't double tick a state if it appears multiple times
	TArray<int32> StatesUpdated;

	// Delegates that native code can hook into to handle state entry
	TArray<FOnGraphStateChanged> OnGraphStatesEntered;

	// Delegates that native code can hook into to handle state exits
	TArray<FOnGraphStateChanged> OnGraphStatesExited;

private:
	// true if it is the first update.
	bool bFirstUpdate;

public:
	FAnimNode_StateMachine()
		: MaxTransitionsPerFrame(3)
		, PRIVATE_MachineDescription(NULL)
		, CurrentState(INDEX_NONE)
		, bFirstUpdate(true)
	{
	}

	// FAnimNode_Base interface
	virtual void Initialize(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones(const FAnimationCacheBonesContext& Context) override;
	virtual void Update(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface

	// Returns the blend weight of the specified state, as calculated by the last call to Update()
	float GetStateWeight(int32 StateIndex) const;

	const FBakedAnimationState& GetStateInfo(int32 StateIndex) const;
	const FAnimationTransitionBetweenStates& GetTransitionInfo(int32 TransIndex) const;
	
protected:
	// Tries to get the instance information for the state machine
	FBakedAnimationStateMachine* GetMachineDescription();

	void SetState(const FAnimationBaseContext& Context, int32 NewStateIndex);
	void SetStateInternal(int32 NewStateIndex);

	const FBakedAnimationState& GetStateInfo() const;
	const int32 GetStateIndex(const FBakedAnimationState& StateInfo) const;
	
	// finds the highest priority valid transition, information pass via the OutPotentialTransition variable.
	// OutVisitedStateIndices will let you know what states were checked, but is also used to make sure we don't get stuck in an infinite loop or recheck states
	bool FindValidTransition(const FAnimationUpdateContext& Context, 
							const FBakedAnimationState& StateInfo,
							/*OUT*/ FAnimationPotentialTransition& OutPotentialTransition,
							/*OUT*/ TArray<int32, TInlineAllocator<4>>& OutVisitedStateIndices);

	// Helper function that will update the states associated with a transition
	void UpdateTransitionStates(const FAnimationUpdateContext& Context, FAnimationActiveTransitionEntry& Transition);

	// helper function to test if a state is a conduit
	bool IsAConduitState(int32 StateIndex) const;

	// helper functions for calling update and evaluate on state nodes
	void UpdateState(int32 StateIndex, const FAnimationUpdateContext& Context);
	void EvaluateState(int32 StateIndex, FPoseContext& Output);

	// transition type evaluation functions
	void EvaluateTransitionStandardBlend(FPoseContext& Output, FAnimationActiveTransitionEntry& Transition, bool bIntermediatePoseIsValid);
	void EvaluateTransitionCustomBlend(FPoseContext& Output, FAnimationActiveTransitionEntry& Transition, bool bIntermediatePoseIsValid);

public:
	friend class UAnimInstance;
};
