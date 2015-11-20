// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/BTDecorator.h"

UBTDecorator::UBTDecorator(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	FlowAbortMode = EBTFlowAbortMode::None;
	bAllowAbortNone = true;
	bAllowAbortLowerPri = true;
	bAllowAbortChildNodes = true;
	bNotifyActivation = false;
	bNotifyDeactivation = false;
	bNotifyProcessed = false;

	bShowInverseConditionDesc = true;
	bInverseCondition = false;
}

void UBTDecorator::InitializeDecorator(uint8 MyChildIndex)
{
	ChildIndex = MyChildIndex;
}

bool UBTDecorator::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	return true;
}

void UBTDecorator::SetIsInversed(bool bShouldBeInversed)
{
	bInverseCondition = bShouldBeInversed;
}

void UBTDecorator::OnNodeActivation(FBehaviorTreeSearchData& SearchData)
{
}

void UBTDecorator::OnNodeDeactivation(FBehaviorTreeSearchData& SearchData, EBTNodeResult::Type NodeResult)
{
}

void UBTDecorator::OnNodeProcessed(FBehaviorTreeSearchData& SearchData, EBTNodeResult::Type& NodeResult)
{
}

bool UBTDecorator::WrappedCanExecute(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	const UBTDecorator* NodeOb = bCreateNodeInstance ? (const UBTDecorator*)GetNodeInstance(OwnerComp, NodeMemory) : this;
	return NodeOb ? (IsInversed() != NodeOb->CalculateRawConditionValue(OwnerComp, NodeMemory)) : false;
}

void UBTDecorator::WrappedOnNodeActivation(FBehaviorTreeSearchData& SearchData) const
{
	if (bNotifyActivation)
	{
		const UBTNode* NodeOb = bCreateNodeInstance ? GetNodeInstance(SearchData) : this;
		if (NodeOb)
		{
			((UBTDecorator*)NodeOb)->OnNodeActivation(SearchData);
		}		
	}
};

void UBTDecorator::WrappedOnNodeDeactivation(FBehaviorTreeSearchData& SearchData, EBTNodeResult::Type NodeResult) const
{
	if (bNotifyDeactivation)
	{
		const UBTNode* NodeOb = bCreateNodeInstance ? GetNodeInstance(SearchData) : this;
		if (NodeOb)
		{
			((UBTDecorator*)NodeOb)->OnNodeDeactivation(SearchData, NodeResult);
		}		
	}
}

void UBTDecorator::WrappedOnNodeProcessed(FBehaviorTreeSearchData& SearchData, EBTNodeResult::Type& NodeResult) const
{
	if (bNotifyProcessed)
	{
		const UBTNode* NodeOb = bCreateNodeInstance ? GetNodeInstance(SearchData) : this;
		if (NodeOb)
		{
			((UBTDecorator*)NodeOb)->OnNodeProcessed(SearchData, NodeResult);
		}		
	}
}

const UBTNode* UBTDecorator::GetMyNode() const
{
	return GetParentNode() ? GetParentNode()->GetChildNode(ChildIndex) : NULL;
}

FString UBTDecorator::GetStaticDescription() const
{
	FString FlowAbortDesc;
	if (FlowAbortMode != EBTFlowAbortMode::None)
	{
		FlowAbortDesc = FString::Printf(TEXT("aborts %s"), *UBehaviorTreeTypes::DescribeFlowAbortMode(FlowAbortMode).ToLower());
	}

	FString InversedDesc;
	if (bShowInverseConditionDesc && IsInversed())
	{
		InversedDesc = TEXT("inversed");
	}

	FString AdditionalDesc;
	if (FlowAbortDesc.Len() || InversedDesc.Len())
	{
		AdditionalDesc = FString::Printf(TEXT("( %s%s%s )\n"), *FlowAbortDesc, 
			(FlowAbortDesc.Len() > 0) && (InversedDesc.Len() > 0) ? TEXT(", ") : TEXT(""),
			*InversedDesc);
	}

	return FString::Printf(TEXT("%s%s"), *AdditionalDesc, *UBehaviorTreeTypes::GetShortTypeName(this));
}

bool UBTDecorator::IsFlowAbortModeValid() const
{
#if WITH_EDITOR
	if (GetParentNode() == NULL ||
		(GetParentNode()->CanAbortLowerPriority() == false && GetParentNode()->CanAbortSelf() == false))
	{
		return (FlowAbortMode == EBTFlowAbortMode::None);
	}

	if (GetParentNode()->CanAbortLowerPriority() == false)
	{
		return (FlowAbortMode == EBTFlowAbortMode::None || FlowAbortMode == EBTFlowAbortMode::Self);
	}

	if (GetParentNode()->CanAbortSelf() == false)
	{
		return (FlowAbortMode == EBTFlowAbortMode::None || FlowAbortMode == EBTFlowAbortMode::LowerPriority);
	}
#endif

	return true;
}

void UBTDecorator::UpdateFlowAbortMode()
{
#if WITH_EDITOR
	if (GetParentNode() == NULL)
	{
		FlowAbortMode = EBTFlowAbortMode::None;
		return;
	}

	if (GetParentNode()->CanAbortLowerPriority() == false)
	{
		if (FlowAbortMode == EBTFlowAbortMode::Both)
		{
			FlowAbortMode = GetParentNode()->CanAbortSelf() ? EBTFlowAbortMode::Self : EBTFlowAbortMode::None;
		}
		else if (FlowAbortMode == EBTFlowAbortMode::LowerPriority)
		{
			FlowAbortMode = EBTFlowAbortMode::None;
		}
	}

	if (GetParentNode()->CanAbortSelf() == false)
	{
		if (FlowAbortMode == EBTFlowAbortMode::Both)
		{
			FlowAbortMode = GetParentNode()->CanAbortLowerPriority() ? EBTFlowAbortMode::LowerPriority : EBTFlowAbortMode::None;
		}
		else if (FlowAbortMode == EBTFlowAbortMode::Self)
		{
			FlowAbortMode = EBTFlowAbortMode::None;
		}
	}
#endif
}

//----------------------------------------------------------------------//
// DEPRECATED
//----------------------------------------------------------------------//
bool UBTDecorator::CalculateRawConditionValue(UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory) const
{
	return OwnerComp ? CalculateRawConditionValue(*OwnerComp, NodeMemory) : false;
}
