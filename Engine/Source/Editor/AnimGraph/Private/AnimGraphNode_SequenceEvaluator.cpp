// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphPrivatePCH.h"

#include "CompilerResultsLog.h"
#include "GraphEditorActions.h"
#include "AnimGraphNode_SequenceEvaluator.h"

/////////////////////////////////////////////////////
// UAnimGraphNode_SequenceEvaluator

#define LOCTEXT_NAMESPACE "A3Nodes"

UAnimGraphNode_SequenceEvaluator::UAnimGraphNode_SequenceEvaluator(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UAnimGraphNode_SequenceEvaluator::PreloadRequiredAssets()
{
	PreloadObject(Node.Sequence);

	Super::PreloadRequiredAssets();
}

void UAnimGraphNode_SequenceEvaluator::GetAllAnimationSequencesReferred(TArray<UAnimationAsset*>& ComplexAnims, TArray<UAnimSequence*>& AnimationSequences) const
{
	if(Node.Sequence)
	{
		HandleAnimReferenceCollection(Node.Sequence, ComplexAnims, AnimationSequences);
	}
}

void UAnimGraphNode_SequenceEvaluator::ReplaceReferredAnimations(const TMap<UAnimationAsset*, UAnimationAsset*>& ComplexAnimsMap, const TMap<UAnimSequence*, UAnimSequence*>& AnimSequenceMap)
{
	HandleAnimReferenceReplacement(Node.Sequence, ComplexAnimsMap, AnimSequenceMap);
}

FText UAnimGraphNode_SequenceEvaluator::GetTooltipText() const
{
	// FText::Format() is slow, so we utilize the cached list title
	return GetNodeTitle(ENodeTitleType::ListView);
}

FText UAnimGraphNode_SequenceEvaluator::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (Node.Sequence == nullptr)
	{
		return LOCTEXT("EvaluateSequence_TitleNONE", "Evaluate (None)");
	}
	// @TODO: don't know enough about this node type to comfortably assert that
	//        the CacheName won't change after the node has spawned... until
	//        then, we'll leave this optimization off
	else //if (CachedNodeTitle.IsOutOfDate(this))
	{
		const FText SequenceName = FText::FromString(Node.Sequence->GetName());

		FFormatNamedArguments Args;
		Args.Add(TEXT("SequenceName"), SequenceName);

		// FText::Format() is slow, so we cache this to save on performance
		if (Node.Sequence->IsValidAdditive())
		{
			CachedNodeTitle.SetCachedText(FText::Format(LOCTEXT("EvaluateSequence_Additive", "Evaluate {SequenceName} (additive)"), Args), this);
		}
		else
		{
			CachedNodeTitle.SetCachedText(FText::Format(LOCTEXT("EvaluateSequence", "Evaluate {SequenceName}"), Args), this);
		}
	}

	return CachedNodeTitle;
}

void UAnimGraphNode_SequenceEvaluator::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	// Intentionally empty; you can drop down a regular sequence player and convert into a sequence evaluator in the right-click menu.
}

void UAnimGraphNode_SequenceEvaluator::ValidateAnimNodeDuringCompilation(class USkeleton* ForSkeleton, class FCompilerResultsLog& MessageLog)
{
	if (Node.Sequence == NULL)
	{
		MessageLog.Error(TEXT("@@ references an unknown sequence"), this);
	}
	else
	{
		USkeleton* SeqSkeleton = Node.Sequence->GetSkeleton();
		if (SeqSkeleton&& // if anim sequence doesn't have skeleton, it might be due to anim sequence not loaded yet, @todo: wait with anim blueprint compilation until all assets are loaded?
			!SeqSkeleton->IsCompatible(ForSkeleton))
		{
			MessageLog.Error(TEXT("@@ references sequence that uses different skeleton @@"), this, SeqSkeleton);
		}
	}
}

void UAnimGraphNode_SequenceEvaluator::GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const
{
	if (!Context.bIsDebugging)
	{
		// add an option to convert to a regular sequence player
		Context.MenuBuilder->BeginSection("AnimGraphNodeSequenceEvaluator", NSLOCTEXT("A3Nodes", "SequenceEvaluatorHeading", "Sequence Evaluator"));
		{
			Context.MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().OpenRelatedAsset);
			Context.MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().ConvertToSeqPlayer);
		}
		Context.MenuBuilder->EndSection();
	}
}

bool UAnimGraphNode_SequenceEvaluator::DoesSupportTimeForTransitionGetter() const
{
	return true;
}

UAnimationAsset* UAnimGraphNode_SequenceEvaluator::GetAnimationAsset() const 
{
	return Node.Sequence;
}

const TCHAR* UAnimGraphNode_SequenceEvaluator::GetTimePropertyName() const 
{
	return TEXT("ExplicitTime");
}

UScriptStruct* UAnimGraphNode_SequenceEvaluator::GetTimePropertyStruct() const 
{
	return FAnimNode_SequenceEvaluator::StaticStruct();
}

#undef LOCTEXT_NAMESPACE