// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Animation/AnimNodeBase.h"

/////////////////////////////////////////////////////
// FAnimNode_Base

void FAnimNode_Base::Initialize(const FAnimationInitializeContext& Context)
{
	EvaluateGraphExposedInputs.Initialize(this, Context.AnimInstance);
}

/////////////////////////////////////////////////////
// FPoseLinkBase

void FPoseLinkBase::AttemptRelink(const FAnimationBaseContext& Context)
{
	// Do the linkage
	if ((LinkedNode == NULL) && (LinkID != INDEX_NONE))
	{
		UAnimBlueprintGeneratedClass* AnimBlueprintClass = Context.GetAnimBlueprintClass();
		check(AnimBlueprintClass);

		// adding ensure. We had a crash on here
		if ( ensure(AnimBlueprintClass->AnimNodeProperties.IsValidIndex(LinkID)) )
		{
			UProperty* LinkedProperty = AnimBlueprintClass->AnimNodeProperties[LinkID];
			void* LinkedNodePtr = LinkedProperty->ContainerPtrToValuePtr<void>(Context.AnimInstance);
			LinkedNode = (FAnimNode_Base*)LinkedNodePtr;
		}
	}
}

void FPoseLinkBase::Initialize(const FAnimationInitializeContext& Context)
{
#if DO_CHECK
	checkf( !bProcessed, TEXT( "Initialize already in progress, circular link for AnimInstance [%s] Blueprint [%s]" ), \
		Context.AnimInstance ? *Context.AnimInstance->GetFullName() : TEXT( "None" ), Context.GetAnimBlueprintClass() ? *Context.GetAnimBlueprintClass()->GetFullName() : TEXT( "None" ) );
	TGuardValue<bool> CircularGuard(bProcessed, true);
#endif

	AttemptRelink(Context);

#if ENABLE_ANIMGRAPH_TRAVERSAL_DEBUG
	// 	checkf(!InitializationCounter.IsSynchronizedWith(Context.AnimInstance->InitializationCounter), TEXT("Already called Initialize on this node!"));
	InitializationCounter.SynchronizeWith(Context.AnimInstance->InitializationCounter);
#endif

	// Do standard initialization
	if (LinkedNode != NULL)
	{
		LinkedNode->Initialize(Context);
	}
}

void FPoseLinkBase::CacheBones(const FAnimationCacheBonesContext& Context) 
{
#if DO_CHECK
	checkf( !bProcessed, TEXT( "CacheBones already in progress, circular link for AnimInstance [%s] Blueprint [%s]" ), \
		Context.AnimInstance ? *Context.AnimInstance->GetFullName() : TEXT( "None" ), Context.GetAnimBlueprintClass() ? *Context.GetAnimBlueprintClass()->GetFullName() : TEXT( "None" ) );
	TGuardValue<bool> CircularGuard(bProcessed, true);
#endif

#if ENABLE_ANIMGRAPH_TRAVERSAL_DEBUG
	// 	checkf(InitializationCounter.IsSynchronizedWith(Context.AnimInstance->InitializationCounter), TEXT("Calling CachedBones without initialization!"));
	// 	checkf(!CachedBonesCounter.IsSynchronizedWith(Context.AnimInstance->CachedBonesCounter), TEXT("Already called CachedBones for this node!"));
	CachedBonesCounter.SynchronizeWith(Context.AnimInstance->CachedBonesCounter);
#endif

	if (LinkedNode != NULL)
	{
		LinkedNode->CacheBones(Context);
	}
}

void FPoseLinkBase::Update(const FAnimationUpdateContext& Context)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FPoseLinkBase_Update);
#if DO_CHECK
	checkf( !bProcessed, TEXT( "Update already in progress, circular link for AnimInstance [%s] Blueprint [%s]" ), \
		Context.AnimInstance ? *Context.AnimInstance->GetFullName() : TEXT( "None" ), Context.GetAnimBlueprintClass() ? *Context.GetAnimBlueprintClass()->GetFullName() : TEXT( "None" ) );
	TGuardValue<bool> CircularGuard(bProcessed, true);
#endif

#if WITH_EDITOR
	if (GIsEditor)
	{
		if (LinkedNode == NULL)
		{
			//@TODO: Should only do this when playing back
			AttemptRelink(Context);
		}

		// Record the node line activation
		if (LinkedNode != NULL)
		{
			//@TODO: Might like a cheaper way to check this, like a bool on the Anim Blueprint or the context
			if (UAnimBlueprint* BP = Context.GetAnimBlueprint())
			{
				if( Context.AnimInstance && (BP->GetObjectBeingDebugged() == Context.AnimInstance))
				{
					Context.GetAnimBlueprintClass()->GetAnimBlueprintDebugData().RecordNodeVisit(LinkID, SourceLinkID, Context.GetFinalBlendWeight());
				}
			}
		}
	}
#endif

#if ENABLE_ANIMGRAPH_TRAVERSAL_DEBUG
	checkf(InitializationCounter.IsSynchronizedWith(Context.AnimInstance->InitializationCounter), TEXT("Calling Update without initialization!"));
	checkf(!UpdateCounter.IsSynchronizedWith(Context.AnimInstance->UpdateCounter), TEXT("Already called Update for this node!"));
	UpdateCounter.SynchronizeWith(Context.AnimInstance->UpdateCounter);
#endif

	if (LinkedNode != NULL)
	{
		LinkedNode->Update(Context);
	}
}

void FPoseLinkBase::GatherDebugData(FNodeDebugData& DebugData)
{
	if(LinkedNode != NULL)
	{
		LinkedNode->GatherDebugData(DebugData);
	}
}

/////////////////////////////////////////////////////
// FPoseLink

void FPoseLink::Evaluate(FPoseContext& Output)
{
#if DO_CHECK
	checkf( !bProcessed, TEXT( "Evaluate already in progress, circular link for AnimInstance [%s] Blueprint [%s]" ), \
		Output.AnimInstance ? *Output.AnimInstance->GetFullName() : TEXT( "None" ), Output.GetAnimBlueprintClass() ? *Output.GetAnimBlueprintClass()->GetFullName() : TEXT( "None" ) );
	TGuardValue<bool> CircularGuard(bProcessed, true);
#endif

#if WITH_EDITOR
	if ((LinkedNode == NULL) && GIsEditor)
	{
		//@TODO: Should only do this when playing back
		AttemptRelink(Output);
	}
#endif

#if ENABLE_ANIMGRAPH_TRAVERSAL_DEBUG
	checkf(InitializationCounter.IsSynchronizedWith(Output.AnimInstance->InitializationCounter), TEXT("Calling Evaluate without initialization!"));
	checkf(CachedBonesCounter.IsSynchronizedWith(Output.AnimInstance->CachedBonesCounter), TEXT("Calling Evaluate without CachedBones!"));
	checkf(UpdateCounter.IsSynchronizedWith(Output.AnimInstance->UpdateCounter), TEXT("Calling Evaluate without Update for this node!"));
	checkf(!EvaluationCounter.IsSynchronizedWith(Output.AnimInstance->EvaluationCounter), TEXT("Already called Evaluate for this node!"));
	EvaluationCounter.SynchronizeWith(Output.AnimInstance->EvaluationCounter);
#endif

	if (LinkedNode != NULL)
	{
#if ENABLE_ANIMNODE_POSE_DEBUG
		CurrentPose.ResetToIdentity();
#endif
		LinkedNode->Evaluate(Output);
#if ENABLE_ANIMNODE_POSE_DEBUG
		CurrentPose = Output.Pose;
#endif
	}
	else
	{
		//@TODO: Warning here?
		Output.ResetToRefPose();
	}

	// Detect non valid output
	checkSlow( !Output.ContainsNaN() );
	checkSlow( Output.IsNormalized() );
}

/////////////////////////////////////////////////////
// FComponentSpacePoseLink

void FComponentSpacePoseLink::EvaluateComponentSpace(FComponentSpacePoseContext& Output)
{
#if DO_CHECK
	checkf( !bProcessed, TEXT( "EvaluateComponentSpace already in progress, circular link for AnimInstance [%s] Blueprint [%s]" ), \
		Output.AnimInstance ? *Output.AnimInstance->GetFullName() : TEXT( "None" ), Output.GetAnimBlueprintClass() ? *Output.GetAnimBlueprintClass()->GetFullName() : TEXT( "None" ) );
	TGuardValue<bool> CircularGuard(bProcessed, true);
#endif

#if ENABLE_ANIMGRAPH_TRAVERSAL_DEBUG
	checkf(InitializationCounter.IsSynchronizedWith(Output.AnimInstance->InitializationCounter), TEXT("Calling EvaluateComponentSpace without initialization!"));
	checkf(CachedBonesCounter.IsSynchronizedWith(Output.AnimInstance->CachedBonesCounter), TEXT("Calling EvaluateComponentSpace without CachedBones!"));
	checkf(UpdateCounter.IsSynchronizedWith(Output.AnimInstance->UpdateCounter), TEXT("Calling EvaluateComponentSpace without Update for this node!"));
	checkf(!EvaluationCounter.IsSynchronizedWith(Output.AnimInstance->EvaluationCounter), TEXT("Already called EvaluateComponentSpace for this node!"));
	EvaluationCounter.SynchronizeWith(Output.AnimInstance->EvaluationCounter);
#endif

	if (LinkedNode != NULL)
	{
		LinkedNode->EvaluateComponentSpace(Output);
	}
	else
	{
		//@TODO: Warning here?
		Output.ResetToRefPose();
	}

	// Detect non valid output
	checkSlow( !Output.ContainsNaN() );
	checkSlow( Output.IsNormalized() );
}

/////////////////////////////////////////////////////
// FComponentSpacePoseContext

bool FComponentSpacePoseContext::ContainsNaN() const
{
	return Pose.GetPose().ContainsNaN();
}

bool FComponentSpacePoseContext::IsNormalized() const
{
	return Pose.GetPose().IsNormalized();
}

/////////////////////////////////////////////////////
// FNodeDebugData

void FNodeDebugData::AddDebugItem(FString DebugData, bool bPoseSource)
{
	check(NodeChain.Num() == 0 || NodeChain.Last().ChildNodeChain.Num() == 0); //Cannot add to this chain once we have branched

	NodeChain.Add( DebugItem(DebugData, bPoseSource) );
}

FNodeDebugData& FNodeDebugData::BranchFlow(float BranchWeight, FString InNodeDescription)
{
	DebugItem& LatestItem = NodeChain.Last();
	new (LatestItem.ChildNodeChain) FNodeDebugData(AnimInstance, BranchWeight*AbsoluteWeight, InNodeDescription);
	return LatestItem.ChildNodeChain.Last();
}

void FNodeDebugData::GetFlattenedDebugData(TArray<FFlattenedDebugData>& FlattenedDebugData, int32 Indent, int32& ChainID)
{
	int32 CurrChainID = ChainID;
	for(DebugItem& Item : NodeChain)
	{
		FlattenedDebugData.Add( FFlattenedDebugData(Item.DebugData, AbsoluteWeight, Indent, CurrChainID, Item.bPoseSource) );
		bool bMultiBranch = Item.ChildNodeChain.Num() > 1;
		int32 ChildIndent = bMultiBranch ? Indent + 1 : Indent;
		for(FNodeDebugData& Child : Item.ChildNodeChain)
		{
			if(bMultiBranch)
			{
				// If we only have one branch we treat it as the same really
				// as we may have only changed active status
				++ChainID;
			}
			Child.GetFlattenedDebugData(FlattenedDebugData, ChildIndent, ChainID);
		}
	}
}