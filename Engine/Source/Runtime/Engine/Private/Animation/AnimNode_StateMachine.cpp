// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Animation/AnimNode_StateMachine.h"
#include "Animation/AnimNode_TransitionResult.h"
#include "Animation/AnimNode_TransitionPoseEvaluator.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "AnimationRuntime.h"
#include "AnimTree.h"

//////////////////////////////////////////////////////////////////////////
// FAnimationActiveTransitionEntry

FAnimationActiveTransitionEntry::FAnimationActiveTransitionEntry()
	: ElapsedTime(0.0f)
	, Alpha(0.0f)
	, CrossfadeDuration(0.0f)
	, CrossfadeMode(ETransitionBlendMode::TBM_Cubic)
	, bActive(false)
	, NextState(INDEX_NONE)
	, PreviousState(INDEX_NONE)
	, StartNotify(INDEX_NONE)
	, EndNotify(INDEX_NONE)
	, InterruptNotify(INDEX_NONE)
	, LogicType(ETransitionLogicType::TLT_StandardBlend)
{
}

FAnimationActiveTransitionEntry::FAnimationActiveTransitionEntry(int32 NextStateID, float ExistingWeightOfNextState, int32 PreviousStateID, const FAnimationTransitionBetweenStates& ReferenceTransitionInfo)
	: ElapsedTime(0.0f)
	, Alpha(0.0f)
	, CrossfadeMode(ReferenceTransitionInfo.CrossfadeMode)
	, bActive(true)
	, NextState(NextStateID)
	, PreviousState(PreviousStateID)
	, StartNotify(ReferenceTransitionInfo.StartNotify)
	, EndNotify(ReferenceTransitionInfo.EndNotify)
	, InterruptNotify(ReferenceTransitionInfo.InterruptNotify)
	, LogicType(ReferenceTransitionInfo.LogicType)
{
	// Adjust the duration of the blend based on the target weight to get us there quicker if we're closer
	const float Scaler = 1.0f - ExistingWeightOfNextState;
	CrossfadeDuration = ReferenceTransitionInfo.CrossfadeDuration * CalculateInverseAlpha(CrossfadeMode, Scaler);
}

float FAnimationActiveTransitionEntry::CalculateInverseAlpha(ETransitionBlendMode::Type BlendType, float InFraction) const
{
	if (BlendType == ETransitionBlendMode::TBM_Cubic)
	{
		const float A = 4.0f/3.0f;
		const float B = -2.0f;
		const float C = 5.0f/3.0f;

		const float T = InFraction;
		const float TT = InFraction*InFraction;
		const float TTT = InFraction*InFraction*InFraction;

		return TTT*A + TT*B + T*C;
	}
	else
	{
		return FMath::Clamp<float>(InFraction, 0.0f, 1.0f);
	}
}

void FAnimationActiveTransitionEntry::InitializeCustomGraphLinks(const FAnimationUpdateContext& Context, const FBakedStateExitTransition& TransitionRule)
{
	if (TransitionRule.CustomResultNodeIndex != INDEX_NONE)
	{
		const UAnimBlueprintGeneratedClass* AnimBlueprintClass = Context.GetAnimBlueprintClass();
		CustomTransitionGraph.LinkID = AnimBlueprintClass->AnimNodeProperties.Num() - 1 - TransitionRule.CustomResultNodeIndex; //@TODO: Crazysauce
		FAnimationInitializeContext InitContext(Context.AnimInstance);
		CustomTransitionGraph.Initialize(InitContext);

		for (int32 Index = 0; Index < TransitionRule.PoseEvaluatorLinks.Num(); ++Index)
		{
			FAnimNode_TransitionPoseEvaluator* PoseEvaluator = GetNodeFromPropertyIndex<FAnimNode_TransitionPoseEvaluator>(Context.AnimInstance, AnimBlueprintClass, TransitionRule.PoseEvaluatorLinks[Index]);
			PoseEvaluators.Add(PoseEvaluator);
		}
	}
}

void FAnimationActiveTransitionEntry::Update(const FAnimationUpdateContext& Context, int32 CurrentStateIndex, bool& bOutFinished)
{
	bOutFinished = false;

	// Advance time
	if (bActive)
	{
		ElapsedTime += Context.GetDeltaTime();
		if (ElapsedTime >= CrossfadeDuration)
		{
			bActive = false;
			bOutFinished = true;
		}

		if(CrossfadeDuration <= 0.0f)
		{
			Alpha = 1.0f;
		}
		else
		{
			Alpha = CalculateAlpha(ElapsedTime / CrossfadeDuration);
		}

	}
}

float FAnimationActiveTransitionEntry::CalculateAlpha(float InFraction) const
{
	if (CrossfadeMode == ETransitionBlendMode::TBM_Cubic)
	{
		return FMath::SmoothStep(0.0f, 1.0f, InFraction);
	}
	else
	{
		return FMath::Clamp<float>(InFraction, 0.0f, 1.0f);
	}
}

bool FAnimationActiveTransitionEntry::Serialize(FArchive& Ar)
{
	Ar << ElapsedTime;
	Ar << Alpha;
	Ar << CrossfadeDuration;
	Ar << bActive;
	Ar << NextState;
	Ar << PreviousState;

	return true;
}

/////////////////////////////////////////////////////
// FAnimationPotentialTransition

FAnimationPotentialTransition::FAnimationPotentialTransition()
: 	TargetState(INDEX_NONE)
,	TransitionRule(NULL)
{
}

bool FAnimationPotentialTransition::IsValid() const
{
	return (TargetState != INDEX_NONE) && (TransitionRule != NULL) && (TransitionRule->TransitionIndex != INDEX_NONE);
}

void FAnimationPotentialTransition::Clear()
{
	TargetState = INDEX_NONE;
	TransitionRule = NULL;
#if WITH_EDITORONLY_DATA
	SourceTransitionIndices.Reset();
#endif
}


/////////////////////////////////////////////////////
// FAnimNode_StateMachine

// Tries to get the instance information for the state machine
FBakedAnimationStateMachine* FAnimNode_StateMachine::GetMachineDescription()
{
	if (PRIVATE_MachineDescription != NULL)
	{
		return PRIVATE_MachineDescription;
	}
	else
	{
		UE_LOG(LogAnimation, Warning, TEXT("FAnimNode_StateMachine: Bad machine ptr"));
		return NULL;
	}
}

void FAnimNode_StateMachine::Initialize(const FAnimationInitializeContext& Context)
{
	UAnimBlueprintGeneratedClass* AnimBlueprintClass = Context.GetAnimBlueprintClass();
	PRIVATE_MachineDescription = AnimBlueprintClass->BakedStateMachines.IsValidIndex(StateMachineIndexInClass) ? &(AnimBlueprintClass->BakedStateMachines[StateMachineIndexInClass]) : NULL;

	if (FBakedAnimationStateMachine* Machine = GetMachineDescription())
	{
		ElapsedTime = 0.0f;

		CurrentState = INDEX_NONE;

		if (Machine->States.Num() > 0)
		{
			// Create a pose link for each state we can reach
			StatePoseLinks.Empty(Machine->States.Num());
			for (int32 StateIndex = 0; StateIndex < Machine->States.Num(); ++StateIndex)
			{
				FPoseLink* StatePoseLink = new (StatePoseLinks) FPoseLink();

				// because conduits don't contain bound graphs, this link is no longer guaranteed to be valid
				if (Machine->States[StateIndex].StateRootNodeIndex != INDEX_NONE)
				{
					StatePoseLink->LinkID = AnimBlueprintClass->AnimNodeProperties.Num() - 1 - Machine->States[StateIndex].StateRootNodeIndex; //@TODO: Crazysauce
				}
			}

			// Reset transition related variables
			StatesUpdated.Empty(StatesUpdated.Num());
			ActiveTransitionArray.Empty(ActiveTransitionArray.Num());
		
			// Move to the default state
			SetState(Context, Machine->InitialState);

			// initialize first update
			bFirstUpdate = true;
		}
	}
}

void FAnimNode_StateMachine::CacheBones(const FAnimationCacheBonesContext& Context) 
{
	if (FBakedAnimationStateMachine* Machine = GetMachineDescription())
	{
		for (int32 StateIndex = 0; StateIndex < Machine->States.Num(); ++StateIndex)
		{
			if( GetStateWeight(StateIndex) > 0.f) 
			{
				StatePoseLinks[StateIndex].CacheBones(Context);
			}
		}
	}

	// @TODO GetStateWeight is O(N) transitions.
}

const FBakedAnimationState& FAnimNode_StateMachine::GetStateInfo() const
{
	return PRIVATE_MachineDescription->States[CurrentState];
}

const FBakedAnimationState& FAnimNode_StateMachine::GetStateInfo(int32 StateIndex) const
{
	return PRIVATE_MachineDescription->States[StateIndex];
}

const int32 FAnimNode_StateMachine::GetStateIndex( const FBakedAnimationState& StateInfo ) const
{
	for (int32 Index = 0; Index < PRIVATE_MachineDescription->States.Num(); ++Index)
	{
		if( &PRIVATE_MachineDescription->States[Index] == &StateInfo )
		{
			return Index;
		}
	}

	return INDEX_NONE;
}


const FAnimationTransitionBetweenStates& FAnimNode_StateMachine::GetTransitionInfo(int32 TransIndex) const
{
	return PRIVATE_MachineDescription->Transitions[TransIndex];
}

void FAnimNode_StateMachine::Update(const FAnimationUpdateContext& Context)
{
	if (FBakedAnimationStateMachine* Machine = GetMachineDescription())
	{
		if (Machine->States.Num() == 0)
		{
			return;
		}
		else if(!Machine->States.IsValidIndex(CurrentState))
		{
			// Attempting to catch a crash where the state machine has been freed.
			UE_LOG(LogAnimation, Warning, TEXT("FAnimNode_StateMachine::Update - Invalid current state, please report. Attempting to use state %d in state machine %d"), CurrentState, StateMachineIndexInClass);
			UE_LOG(LogAnimation, Warning, TEXT("\t\tWhen updating AnimInstance: %s"), *Context.AnimInstance->GetName())

			return;
		}
	}
	else
	{
		return;
	}

	SCOPE_CYCLE_COUNTER(STAT_AnimStateMachineUpdate);

	bool bFoundValidTransition = false;
	int32 TransitionCountThisFrame = 0;
	int32 TransitionIndex = INDEX_NONE;

	// Look for legal transitions to take; can move across multiple states in one frame (up to MaxTransitionsPerFrame)
	do
	{
		bFoundValidTransition = false;
		FAnimationPotentialTransition PotentialTransition;
		
		{
			SCOPE_CYCLE_COUNTER(STAT_AnimStateMachineFindTransition);

			// Evaluate possible transitions out of this state
			//@TODO: Evaluate if a set is better than an array for the probably low N encountered here
			TArray<int32, TInlineAllocator<4>> VisitedStateIndices;
			FindValidTransition(Context, GetStateInfo(), /*Out*/ PotentialTransition, /*Out*/ VisitedStateIndices);
		}
				
		// If transition is valid and not waiting on other conditions
		if (PotentialTransition.IsValid())
		{
			bFoundValidTransition = true;

			// let the latest transition know it has been interrupted
			if ((ActiveTransitionArray.Num() > 0) && ActiveTransitionArray[ActiveTransitionArray.Num()-1].bActive)
			{
				Context.AnimInstance->AddAnimNotifyFromGeneratedClass(ActiveTransitionArray[ActiveTransitionArray.Num()-1].InterruptNotify);
			}

			const int32 PreviousState = CurrentState;
			const int32 NextState = PotentialTransition.TargetState;

			// Fire off Notifies for state transition
			if (!bFirstUpdate)
			{
				Context.AnimInstance->AddAnimNotifyFromGeneratedClass(GetStateInfo(PreviousState).EndNotify);
				Context.AnimInstance->AddAnimNotifyFromGeneratedClass(GetStateInfo(NextState).StartNotify);
			}
			
			// Get the current weight of the next state, which may be non-zero
			const float ExistingWeightOfNextState = GetStateWeight(NextState);

			// Push the transition onto the stack
			const FAnimationTransitionBetweenStates& ReferenceTransition = GetTransitionInfo(PotentialTransition.TransitionRule->TransitionIndex);
			FAnimationActiveTransitionEntry* NewTransition = new (ActiveTransitionArray) FAnimationActiveTransitionEntry(NextState, ExistingWeightOfNextState, PreviousState, ReferenceTransition);
			NewTransition->InitializeCustomGraphLinks(Context, *(PotentialTransition.TransitionRule));

#if WITH_EDITORONLY_DATA
			NewTransition->SourceTransitionIndices = PotentialTransition.SourceTransitionIndices;
#endif

			if (!bFirstUpdate)
			{
				Context.AnimInstance->AddAnimNotifyFromGeneratedClass(NewTransition->StartNotify);
			}
			
			SetState(Context, NextState);

			TransitionCountThisFrame++;
		}
	}
	while (bFoundValidTransition && (TransitionCountThisFrame < MaxTransitionsPerFrame));

	if (bFirstUpdate)
	{
		//Handle enter notify for "first" (after initial transitions) state
		Context.AnimInstance->AddAnimNotifyFromGeneratedClass(GetStateInfo().StartNotify);
		// in the first update, we don't like to transition from entry state
		// so we throw out any transition data at the first update
		ActiveTransitionArray.Reset();
		bFirstUpdate = false;
	}

	StatesUpdated.Empty(StatesUpdated.Num());

	// Tick the individual state/states that are active
	if (ActiveTransitionArray.Num() > 0)
	{
		for (int32 Index = 0; Index < ActiveTransitionArray.Num(); ++Index)
		{
			// The custom graph will tick the needed states
			bool bFinishedTrans = false;

			// The custom graph will tick the needed states
			ActiveTransitionArray[Index].Update(Context, CurrentState, /*out*/ bFinishedTrans);
			
			if (bFinishedTrans)
			{
				// only play these events if it is the last transition (most recent, going to current state)
				if (Index == (ActiveTransitionArray.Num() - 1))
				{
					Context.AnimInstance->AddAnimNotifyFromGeneratedClass(ActiveTransitionArray[Index].EndNotify);
					Context.AnimInstance->AddAnimNotifyFromGeneratedClass(GetStateInfo().FullyBlendedNotify);
				}
			}
			else
			{
				// transition is still active, so tick the required states
				UpdateTransitionStates(Context, ActiveTransitionArray[Index]);
			}
		}
		
		// remove finished transitions here, newer transitions ending means any older ones must complete as well
		for (int32 Index = (ActiveTransitionArray.Num()-1); Index >= 0; --Index)
		{
			// if we find an inactive one, remove all older transitions and break out
			if (!ActiveTransitionArray[Index].bActive)
			{
				ActiveTransitionArray.RemoveAt(0, Index+1);
				break;
			}
		}
	}

	//@TODO: StatesUpdated.Contains is a linear search
	// Update the only active state if there are no transitions still in flight
	if (ActiveTransitionArray.Num() == 0 && !IsAConduitState(CurrentState) && !StatesUpdated.Contains(CurrentState))
	{
		StatePoseLinks[CurrentState].Update(Context);
	}

	ElapsedTime += Context.GetDeltaTime();
}

bool FAnimNode_StateMachine::FindValidTransition(const FAnimationUpdateContext& Context, const FBakedAnimationState& StateInfo, /*out*/ FAnimationPotentialTransition& OutPotentialTransition, /*out*/ TArray<int32, TInlineAllocator<4>>& OutVisitedStateIndices)
{
	// There is a possibility we'll revisit states connected through conduits,
	// so we can avoid doing unnecessary work (and infinite loops) by caching off states we have already checked
	const int32 CheckingStateIndex = GetStateIndex(StateInfo);
	if (OutVisitedStateIndices.Contains(CheckingStateIndex))
	{
		return false;
	}
	OutVisitedStateIndices.Add(CheckingStateIndex);

	const UAnimBlueprintGeneratedClass* AnimBlueprintClass = Context.GetAnimBlueprintClass();

	// Conduit 'states' have an additional entry rule which must be true to consider taking any transitions via the conduit
	//@TODO: It would add flexibility to be able to define this on normal state nodes as well, assuming the dual-graph editing is sorted out
	if (FAnimNode_TransitionResult* StateEntryRuleNode = GetNodeFromPropertyIndex<FAnimNode_TransitionResult>(Context.AnimInstance, AnimBlueprintClass, StateInfo.EntryRuleNodeIndex))
	{
		if (StateEntryRuleNode->NativeTransitionDelegate.IsBound())
		{
			// attempt to evaluate native rule
			StateEntryRuleNode->bCanEnterTransition = StateEntryRuleNode->NativeTransitionDelegate.Execute();
		}
		else
		{
			// Execute it and see if we can take this rule
			StateEntryRuleNode->EvaluateGraphExposedInputs.Execute(Context);
		}

		// not ok, back out
		if (!StateEntryRuleNode->bCanEnterTransition)
		{
			return false;
		}
	}

	const int32 NumTransitions = StateInfo.Transitions.Num();
	for (int32 TransitionIndex = 0; TransitionIndex < NumTransitions; ++TransitionIndex)
	{
		const FBakedStateExitTransition& TransitionRule = StateInfo.Transitions[TransitionIndex];
		if (TransitionRule.CanTakeDelegateIndex == INDEX_NONE)
		{
			continue;
		}

		FAnimNode_TransitionResult* ResultNode = GetNodeFromPropertyIndex<FAnimNode_TransitionResult>(Context.AnimInstance, AnimBlueprintClass, TransitionRule.CanTakeDelegateIndex);

		if (ResultNode->NativeTransitionDelegate.IsBound())
		{
			// attempt to evaluate native rule
			ResultNode->bCanEnterTransition = ResultNode->NativeTransitionDelegate.Execute();
		}
		else
		{
			bool bStillCallEvaluate = true;

			if (TransitionRule.StateSequencePlayerToQueryIndex != INDEX_NONE)
			{
				// Simple automatic rule
				FAnimNode_SequencePlayer* SequencePlayer = GetNodeFromPropertyIndex<FAnimNode_SequencePlayer>(Context.AnimInstance, AnimBlueprintClass, TransitionRule.StateSequencePlayerToQueryIndex);
				if ((SequencePlayer != nullptr) && (SequencePlayer->Sequence != nullptr))
				{
					const float SequenceLength = SequencePlayer->Sequence->GetMaxCurrentTime();
					const float PlayerTime = SequencePlayer->InternalTimeAccumulator;
					const float PlayerTimeLeft = SequenceLength - PlayerTime;

					const FAnimationTransitionBetweenStates& TransitionInfo = GetTransitionInfo(TransitionRule.TransitionIndex);

					ResultNode->bCanEnterTransition = (PlayerTimeLeft <= TransitionInfo.CrossfadeDuration);

					bStillCallEvaluate = false;
				}
			}

			if (bStillCallEvaluate)
			{
				// Execute it and see if we can take this rule
				ResultNode->EvaluateGraphExposedInputs.Execute(Context);
			}
		}

		if (ResultNode->bCanEnterTransition == TransitionRule.bDesiredTransitionReturnValue)
		{
			int32 NextState = GetTransitionInfo(TransitionRule.TransitionIndex).NextState;
			const FBakedAnimationState& NextStateInfo = GetStateInfo(NextState);

			// if next state is a conduit we want to check for transitions using that state as the root
			if (NextStateInfo.bIsAConduit)
			{
				if (FindValidTransition(Context, NextStateInfo, /*out*/ OutPotentialTransition, /*out*/ OutVisitedStateIndices))
				{
#if WITH_EDITORONLY_DATA	
					OutPotentialTransition.SourceTransitionIndices.Add(TransitionRule.TransitionIndex);
#endif		
					return true;
				}					
			}
			// otherwise we have found a content state, so we can record our potential transition
			else
			{
				// clear out any potential transition we already have
				OutPotentialTransition.Clear();

				// fill out the potential transition information
				OutPotentialTransition.TransitionRule = &TransitionRule;
				OutPotentialTransition.TargetState = NextState;

#if WITH_EDITORONLY_DATA	
				OutPotentialTransition.SourceTransitionIndices.Add(TransitionRule.TransitionIndex);
#endif
				return true;
			}
		}
	}

	return false;
}

void FAnimNode_StateMachine::UpdateTransitionStates(const FAnimationUpdateContext& Context, FAnimationActiveTransitionEntry& Transition)
{
	if (Transition.bActive)
	{
		switch (Transition.LogicType)
		{
		case ETransitionLogicType::TLT_StandardBlend:
			{
				// update both states
				UpdateState(Transition.PreviousState, Context.FractionalWeight(1.0f - Transition.Alpha));
				UpdateState(Transition.NextState, Context.FractionalWeight(Transition.Alpha));
			}
			break;

		case ETransitionLogicType::TLT_Custom:
			{
				if (Transition.CustomTransitionGraph.LinkID != INDEX_NONE)
				{
					Transition.CustomTransitionGraph.Update(Context);

					for (TArray<FAnimNode_TransitionPoseEvaluator*>::TIterator PoseEvaluatorListIt = Transition.PoseEvaluators.CreateIterator(); PoseEvaluatorListIt; ++PoseEvaluatorListIt)
					{
						FAnimNode_TransitionPoseEvaluator* Evaluator = *PoseEvaluatorListIt;
						if (Evaluator->InputNodeNeedsUpdate())
						{
							const bool bUsePreviousState = (Evaluator->DataSource == EEvaluatorDataSource::EDS_SourcePose);
							const int32 EffectiveStateIndex = bUsePreviousState ? Transition.PreviousState : Transition.NextState;
							UpdateState(EffectiveStateIndex, Context);
						}
					}
				}
			}
			break;

		default:
			break;
		}
	}
}

void FAnimNode_StateMachine::Evaluate(FPoseContext& Output)
{
	if (FBakedAnimationStateMachine* Machine = GetMachineDescription())
	{
		if (Machine->States.Num() == 0 || !Machine->States.IsValidIndex(CurrentState))
		{
			FAnimationRuntime::FillWithRefPose(Output.Pose.Bones, Output.AnimInstance->RequiredBones);
			return;
		}
	}
	else
	{
		FAnimationRuntime::FillWithRefPose(Output.Pose.Bones, Output.AnimInstance->RequiredBones);
		return;
	}

	SCOPE_CYCLE_COUNTER(STAT_AnimStateMachineEvaluate);
	
	if (ActiveTransitionArray.Num() > 0)
	{
		check(Output.AnimInstance->CurrentSkeleton);

		// Create an accumulator pose
		const int32 NumBones = Output.AnimInstance->RequiredBones.GetNumBones();

		Output.Pose.Bones.Empty(NumBones);
		Output.Pose.Bones.AddUninitialized(NumBones);

		//each transition stomps over the last because they will already include the output from the transition before it
		for (int32 Index = 0; Index < ActiveTransitionArray.Num(); ++Index)
		{
			// if there is any source pose, blend it here
			FAnimationActiveTransitionEntry& ActiveTransition = ActiveTransitionArray[Index];
			
			// when evaluating multiple transitions we need to store the pose from previous results
			// so we can feed the next transitions
			const bool bIntermediatePoseIsValid = Index > 0;

			if (ActiveTransition.bActive)
			{
				switch (ActiveTransition.LogicType)
				{
				case ETransitionLogicType::TLT_StandardBlend:
					EvaluateTransitionStandardBlend(Output, ActiveTransition, bIntermediatePoseIsValid);
					break;
				case ETransitionLogicType::TLT_Custom:
					EvaluateTransitionCustomBlend(Output, ActiveTransition, bIntermediatePoseIsValid);
					break;
				default:
					break;
				}
			}
		}

		// Ensure that all of the resulting rotations are normalized
		FAnimationRuntime::NormalizeRotations(Output.AnimInstance->RequiredBones, Output.Pose.Bones);
	}
	else if (!IsAConduitState(CurrentState))
	{
		// Evaluate the current state
		StatePoseLinks[CurrentState].Evaluate(Output);
	}
}

void FAnimNode_StateMachine::EvaluateTransitionStandardBlend(FPoseContext& Output,FAnimationActiveTransitionEntry& Transition, bool bIntermediatePoseIsValid)
{	
	FPoseContext PreviouseStateResult(Output);
	FPoseContext NextStateResult(Output);

	if (bIntermediatePoseIsValid)
	{
		Output.AnimInstance->CopyPose(Output.Pose, PreviouseStateResult.Pose);
	}
	else
	{
		EvaluateState(Transition.PreviousState, PreviouseStateResult);
	}

	// evaluate the next state
	EvaluateState(Transition.NextState, NextStateResult);

	// Blend it in
	const ScalarRegister VPreviousWeight(1.0f - Transition.Alpha);
	const ScalarRegister VWeight(Transition.Alpha);
	const TArray<FBoneIndexType> & RequiredBoneIndices = Output.AnimInstance->RequiredBones.GetBoneIndicesArray();
	for (int32 j = 0; j < RequiredBoneIndices.Num(); ++j)
	{
		const int32 BoneIndex = RequiredBoneIndices[j];
		Output.Pose.Bones[BoneIndex] = PreviouseStateResult.Pose.Bones[BoneIndex] * VPreviousWeight;
		Output.Pose.Bones[BoneIndex].AccumulateWithShortestRotation(NextStateResult.Pose.Bones[BoneIndex], VWeight);
	}
}

void FAnimNode_StateMachine::EvaluateTransitionCustomBlend(FPoseContext& Output, FAnimationActiveTransitionEntry& Transition, bool bIntermediatePoseIsValid)
{
	if (Transition.CustomTransitionGraph.LinkID != INDEX_NONE)
	{
		for (TArray<FAnimNode_TransitionPoseEvaluator*>::TIterator PoseEvaluatorListIt(Transition.PoseEvaluators); PoseEvaluatorListIt; ++PoseEvaluatorListIt)
		{
			FAnimNode_TransitionPoseEvaluator* Evaluator = *PoseEvaluatorListIt;
			if (Evaluator->InputNodeNeedsEvaluate())
			{
				// All input evaluators that use the intermediate pose can grab it from the current output.
				const bool bUseIntermediatePose = bIntermediatePoseIsValid && (Evaluator->DataSource == EEvaluatorDataSource::EDS_SourcePose);

				// otherwise we need to evaluate the nodes they reference
				if (!bUseIntermediatePose)
				{
					FPoseContext PoseEvalResult(Output);
					
					const bool bUsePreviousState = (Evaluator->DataSource == EEvaluatorDataSource::EDS_SourcePose);
					const int32 EffectiveStateIndex = bUsePreviousState ? Transition.PreviousState : Transition.NextState;
					EvaluateState(EffectiveStateIndex, PoseEvalResult);

					// push transform to node.
					Evaluator->CachePose(Output, PoseEvalResult.Pose);
				}
				else
				{
					// push transform to node.
					Evaluator->CachePose(Output, Output.Pose);
				}
			}
		}

		FPoseContext StatePoseResult(Output);
		Transition.CustomTransitionGraph.Evaluate(StatePoseResult);

		// First pose will just overwrite the destination
		const TArray<FBoneIndexType> & RequiredBoneIndices = Output.AnimInstance->RequiredBones.GetBoneIndicesArray();
		for (int32 j = 0; j < RequiredBoneIndices.Num(); ++j)
		{
			const int32 BoneIndex = RequiredBoneIndices[j];
			Output.Pose.Bones[BoneIndex] = StatePoseResult.Pose.Bones[BoneIndex];
		}
	}
}

void AddStateWeight(TMap<int32, float>& StateWeightMap, int32 StateIndex, float Weight)
{
	if (!StateWeightMap.Find(StateIndex))
	{
		StateWeightMap.Add(StateIndex) = Weight;
	}
}

void FAnimNode_StateMachine::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugLine += FString::Printf(TEXT("(%s->%s)"), *GetMachineDescription()->MachineName.ToString(), *GetStateInfo().StateName.ToString());

	TMap<int32, float> StateWeightMap;

	if (ActiveTransitionArray.Num() > 0)
	{
		for (int32 Index = 0; Index < ActiveTransitionArray.Num(); ++Index)
		{
			// if there is any source pose, blend it here
			FAnimationActiveTransitionEntry& ActiveTransition = ActiveTransitionArray[Index];

			if (ActiveTransition.bActive)
			{
				switch (ActiveTransition.LogicType)
				{
					case ETransitionLogicType::TLT_StandardBlend:
					{
						AddStateWeight(StateWeightMap, ActiveTransition.PreviousState, (1.0f - ActiveTransition.Alpha));
						AddStateWeight(StateWeightMap, ActiveTransition.NextState, (ActiveTransition.Alpha));
						break;
					}
					case ETransitionLogicType::TLT_Custom:
					{
						if (ActiveTransition.CustomTransitionGraph.LinkID != INDEX_NONE)
						{
							for (TArray<FAnimNode_TransitionPoseEvaluator*>::TIterator PoseEvaluatorListIt = ActiveTransition.PoseEvaluators.CreateIterator(); PoseEvaluatorListIt; ++PoseEvaluatorListIt)
							{
								FAnimNode_TransitionPoseEvaluator* Evaluator = *PoseEvaluatorListIt;
								if (Evaluator->InputNodeNeedsUpdate())
								{
									const bool bUsePreviousState = (Evaluator->DataSource == EEvaluatorDataSource::EDS_SourcePose);
									const int32 EffectiveStateIndex = bUsePreviousState ? ActiveTransition.PreviousState : ActiveTransition.NextState;
									AddStateWeight(StateWeightMap, EffectiveStateIndex, 1.f);
								}
							}
						}
						break;
					}
					default:
					{
						break;
					}
				}
			}
		}
	}
	else if (!IsAConduitState(CurrentState))
	{
		StateWeightMap.Add(CurrentState) = 1.0f;
	}

	DebugData.AddDebugItem(DebugLine);
	for (int32 PoseIndex = 0; PoseIndex < StatePoseLinks.Num(); ++PoseIndex)
	{
		float* WeightPtr = StateWeightMap.Find(PoseIndex);
		float Weight = WeightPtr ? *WeightPtr : 0.f;

		StatePoseLinks[PoseIndex].GatherDebugData(DebugData.BranchFlow(Weight));
	}
}

void FAnimNode_StateMachine::SetStateInternal(int32 NewStateIndex)
{
	checkSlow(PRIVATE_MachineDescription);
	ensure(!IsAConduitState(NewStateIndex));
	CurrentState = FMath::Clamp<int32>(NewStateIndex, 0, PRIVATE_MachineDescription->States.Num() - 1);
	ElapsedTime = 0.0f;
}

void FAnimNode_StateMachine::SetState(const FAnimationBaseContext& Context, int32 NewStateIndex)
{
	if (NewStateIndex != CurrentState)
	{
		const int32 PrevStateIndex = CurrentState;
		if(CurrentState != INDEX_NONE && CurrentState < OnGraphStatesExited.Num())
		{
			OnGraphStatesExited[CurrentState].ExecuteIfBound(*this, CurrentState, NewStateIndex);
		}

		// Determine if the new state is active or not
		const bool bAlreadyActive = GetStateWeight(NewStateIndex) > 0.0f;

		SetStateInternal(NewStateIndex);

		if (!bAlreadyActive && !IsAConduitState(NewStateIndex))
		{
			// Initialize the new state since it's not part of an active transition (and thus not still initialized)
			FAnimationInitializeContext InitContext(Context.AnimInstance);
			StatePoseLinks[NewStateIndex].Initialize(InitContext);

			// Also update BoneCaching.
			FAnimationCacheBonesContext CacheBoneContext(Context.AnimInstance);
			StatePoseLinks[NewStateIndex].CacheBones(CacheBoneContext);
		}

		if(CurrentState != INDEX_NONE && CurrentState < OnGraphStatesEntered.Num())
		{
			OnGraphStatesEntered[CurrentState].ExecuteIfBound(*this, PrevStateIndex, CurrentState);
		}
	}
}

float FAnimNode_StateMachine::GetStateWeight(int32 StateIndex) const
{
	const int32 NumTransitions = ActiveTransitionArray.Num();
	if (NumTransitions > 0)
	{
		// Determine the overall weight of the state here.
		float TotalWeight = 0.0f;
		for (int32 Index = 0; Index < NumTransitions; ++Index)
		{
			const FAnimationActiveTransitionEntry& Transition = ActiveTransitionArray[Index];

			float SourceWeight = (1.0f - Transition.Alpha);

			// After the first transition, so source weight is the fraction of how much all previous transitions contribute to the final weight.
			// So if our second transition is 50% complete, and our target state was 80% of the first transition, then that number will be multiplied by this weight
			if (Index > 0)
			{
				TotalWeight *= SourceWeight;
			}
			//during the first transition the source weight represents the actual state weight
			else if (Transition.PreviousState == StateIndex)
			{
				TotalWeight += SourceWeight;
			}

			// The next state weight is the alpha of this transition. We always just add the value, it will be reduced down if there are any newer transitions
			if (Transition.NextState == StateIndex)
			{
				TotalWeight += Transition.Alpha;
			}

		}

		return FMath::Clamp<float>(TotalWeight, 0.0f, 1.0f);
	}
	else
	{
		return (StateIndex == CurrentState) ? 1.0f : 0.0f;
	}
}

#if WITH_EDITORONLY_DATA
bool FAnimNode_StateMachine::IsTransitionActive(int32 TransIndex) const
{
	for (int32 Index = 0; Index < ActiveTransitionArray.Num(); ++Index)
	{
		if (ActiveTransitionArray[Index].SourceTransitionIndices.Contains(TransIndex))
		{
			return true;
		}
	}

	return false;
}
#endif

void FAnimNode_StateMachine::UpdateState(int32 StateIndex, const FAnimationUpdateContext& Context)
{
	if ((StateIndex != INDEX_NONE) && !StatesUpdated.Contains(StateIndex) && !IsAConduitState(StateIndex))
	{
		StatesUpdated.Add(StateIndex);
		StatePoseLinks[StateIndex].Update(Context);
	}
}


void FAnimNode_StateMachine::EvaluateState(int32 StateIndex, FPoseContext& Output)
{
	const int32 NumBones = Output.AnimInstance->RequiredBones.GetNumBones();
	Output.Pose.Bones.Empty(NumBones);
	Output.Pose.Bones.AddUninitialized(NumBones);

	if (!IsAConduitState(StateIndex))
	{
		StatePoseLinks[StateIndex].Evaluate(Output);
	}
}

bool FAnimNode_StateMachine::IsAConduitState(int32 StateIndex) const
{
	return ((PRIVATE_MachineDescription != NULL) && (StateIndex < PRIVATE_MachineDescription->States.Num())) ? GetStateInfo(StateIndex).bIsAConduit : false;
}

