// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "BehaviorTree/Decorators/BTDecorator_ForceSuccess.h"

UBTDecorator_ForceSuccess::UBTDecorator_ForceSuccess(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NodeName = TEXT("Force Success");
	bNotifyProcessed = true;

	bAllowAbortNone = false;
	bAllowAbortLowerPri = false;
	bAllowAbortChildNodes = false;
}

void UBTDecorator_ForceSuccess::OnNodeProcessed(FBehaviorTreeSearchData& SearchData, EBTNodeResult::Type& NodeResult)
{
	NodeResult = EBTNodeResult::Succeeded;
	BT_SEARCHLOG(SearchData, Log, TEXT("Forcing Success: %s"), *UBehaviorTreeTypes::DescribeNodeHelper(this));
}

#if WITH_EDITOR

FName UBTDecorator_ForceSuccess::GetNodeIconName() const
{
	return FName("BTEditor.Graph.BTNode.Decorator.ForceSuccess.Icon");
}

#endif	// WITH_EDITOR