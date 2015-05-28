// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimStateMachineTypes.generated.h"

//@TODO: Document
UENUM()
namespace ETransitionBlendMode
{
	enum Type
	{
		TBM_Linear UMETA(DisplayName="Linear"),
		TBM_Cubic UMETA(DisplayName="Cubic")
	};
}

//@TODO: Document
UENUM()
namespace ETransitionLogicType
{
	enum Type
	{
		TLT_StandardBlend UMETA(DisplayName="Standard Blend"),
		TLT_Custom UMETA(DisplayName="Custom")
	};
}

// This structure represents a baked transition rule inside a state
USTRUCT()
struct FAnimationTransitionRule
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FName RuleToExecute;

	/** What RuleToExecute must return to take transition (for bidirectional transitions) */
	UPROPERTY()
	bool TransitionReturnVal;

	UPROPERTY()
	int32 TransitionIndex;

	FAnimationTransitionRule()
		: TransitionReturnVal(true)
		, TransitionIndex(INDEX_NONE)
	{}

	FAnimationTransitionRule(int32 InTransitionState)
		: TransitionIndex(InTransitionState)
	{
	}
};

// This is the base class that both baked states and transitions use
USTRUCT()
struct FAnimationStateBase
{
	GENERATED_USTRUCT_BODY()

	// The name of this state
	UPROPERTY()
	FName StateName;

	FAnimationStateBase()
	{}
};

//
USTRUCT()
struct FAnimationState : public FAnimationStateBase
{
	GENERATED_USTRUCT_BODY()

	// Set of legal transitions out of this state; already in priority order
	UPROPERTY()
	TArray<FAnimationTransitionRule> Transitions;

	// The root node index (into the AnimNodeProperties array of the UAnimBlueprintGeneratedClass)
	UPROPERTY()
	int32 StateRootNodeIndex;

	// The index of the notify to fire when this state is first entered (weight within the machine becomes non-zero)
	UPROPERTY()
	int32 StartNotify;

	// The index of the notify to fire when this state is finished exiting (weight within the machine becomes zero)
	UPROPERTY()
	int32 EndNotify;

	// The index of the notify to fire when this state is fully entered (weight within the machine becomes one)
	UPROPERTY()
	int32 FullyBlendedNotify;

	FAnimationState()
		: FAnimationStateBase()
		, StartNotify(INDEX_NONE)
		, EndNotify(INDEX_NONE)
		, FullyBlendedNotify(INDEX_NONE)
	{}
};

// This represents a baked transition
USTRUCT()
struct FAnimationTransitionBetweenStates : public FAnimationStateBase
{
	GENERATED_USTRUCT_BODY()

	// Transition-only: State being transitioned from
	UPROPERTY()
	int32 PreviousState;

	// Transition-only: State being transitioned to
	UPROPERTY()
	int32 NextState;

	UPROPERTY()
	float CrossfadeDuration;

	UPROPERTY()
	int32 StartNotify;

	UPROPERTY()
	int32 EndNotify;

	UPROPERTY()
	int32 InterruptNotify;

	UPROPERTY()
	TEnumAsByte<ETransitionBlendMode::Type> CrossfadeMode;

	UPROPERTY()
	TEnumAsByte<ETransitionLogicType::Type> LogicType;

#if WITH_EDITORONLY_DATA
	// This is only needed for the baking process, to denote which baked transitions need to reverse their prev/next state in the final step
	bool ReverseTransition;
#endif

	FAnimationTransitionBetweenStates()
		: FAnimationStateBase()
		, PreviousState(INDEX_NONE)
		, NextState(INDEX_NONE)
		, StartNotify(INDEX_NONE)
		, EndNotify(INDEX_NONE)
		, InterruptNotify(INDEX_NONE)
		, CrossfadeMode(ETransitionBlendMode::TBM_Cubic)
		, LogicType(ETransitionLogicType::TLT_StandardBlend)
#if WITH_EDITOR
		, ReverseTransition(false)
#endif
	{}
};


USTRUCT()
struct FBakedStateExitTransition
{
	GENERATED_USTRUCT_BODY()

	// The node property index for this rule
	UPROPERTY()
	int32 CanTakeDelegateIndex;

	// The blend graph result node index
	UPROPERTY()
	int32 CustomResultNodeIndex;

	// The index into the machine table of transitions
	UPROPERTY()
	int32 TransitionIndex;

	// For transitions that go automatically at the end of the sequence, this is the index for the sole sequence player node in the graph
	UPROPERTY()
	int32 StateSequencePlayerToQueryIndex;

	// What the transition rule node needs to return to take this transition (for bidirectional transitions)
	UPROPERTY()
	bool bDesiredTransitionReturnValue;
	
	UPROPERTY()
	TArray<int32> PoseEvaluatorLinks;

	FBakedStateExitTransition()
		: CanTakeDelegateIndex(INDEX_NONE)
		, CustomResultNodeIndex(INDEX_NONE)
		, TransitionIndex(INDEX_NONE)
		, StateSequencePlayerToQueryIndex(INDEX_NONE)
		, bDesiredTransitionReturnValue(true)
	{
	}
};


//
USTRUCT()
struct FBakedAnimationState
{
	GENERATED_USTRUCT_BODY()

	// The name of this state
	UPROPERTY()
	FName StateName;

	// Set of legal transitions out of this state; already in priority order
	UPROPERTY()
	TArray<FBakedStateExitTransition> Transitions;

	// The root node index (into the AnimNodeProperties array of the UAnimBlueprintGeneratedClass)
	UPROPERTY()
	int32 StateRootNodeIndex;

	UPROPERTY()
	int32 StartNotify;

	UPROPERTY()
	int32 EndNotify;

	UPROPERTY()
	int32 FullyBlendedNotify;

	UPROPERTY()
	bool bIsAConduit;

	UPROPERTY()
	int32 EntryRuleNodeIndex;


public:
	FBakedAnimationState()
		: StartNotify(INDEX_NONE)
		, EndNotify(INDEX_NONE)
		, FullyBlendedNotify(INDEX_NONE)
		, bIsAConduit(false)
		, EntryRuleNodeIndex(INDEX_NONE)
	{}
};

USTRUCT()
struct FBakedAnimationStateMachine
{
	GENERATED_USTRUCT_BODY()

	// Name of this machine (primarily for debugging purposes)
	UPROPERTY()
	FName MachineName;

	// Index of the initial state that the machine will start in
	UPROPERTY()
	int32 InitialState;

	// List of all states this machine can be in
	UPROPERTY()
	TArray<FBakedAnimationState> States;

	// List of all transitions between states
	UPROPERTY()
	TArray<FAnimationTransitionBetweenStates> Transitions;

public:
	FBakedAnimationStateMachine()
		: InitialState(INDEX_NONE)
	{}

	// Finds a state by name or NULL if no such state exists
	int32 FindStateIndex(FName StateName);
};






UCLASS()
class UAnimStateMachineTypes : public UObject
{
	GENERATED_UCLASS_BODY()
};

