// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Decorators/BTDecorator_BlackboardBase.h"

UBTDecorator_BlackboardBase::UBTDecorator_BlackboardBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NodeName = "BlackboardBase";

	bNotifyBecomeRelevant = true;
	bNotifyCeaseRelevant = true;

	// empty KeySelector = allow everything
}

void UBTDecorator_BlackboardBase::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);
	BlackboardKey.CacheSelectedKey(GetBlackboardAsset());
}

void UBTDecorator_BlackboardBase::OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (BlackboardComp)
	{
		auto KeyID = BlackboardKey.GetSelectedKeyID();
		BlackboardComp->RegisterObserver(KeyID, this, FOnBlackboardChangeNotification::CreateUObject(this, &UBTDecorator_BlackboardBase::OnBlackboardKeyValueChange));
	}
}

void UBTDecorator_BlackboardBase::OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (BlackboardComp)
	{
		BlackboardComp->UnregisterObserversFrom(this);
	}
}

EBlackboardNotificationResult UBTDecorator_BlackboardBase::OnBlackboardKeyValueChange(const UBlackboardComponent& Blackboard, FBlackboard::FKey ChangedKeyID)
{
	UBehaviorTreeComponent* BehaviorComp = (UBehaviorTreeComponent*)Blackboard.GetBrainComponent();
	if (BehaviorComp == nullptr)
	{
		return EBlackboardNotificationResult::RemoveObserver;
	}

	if (BlackboardKey.GetSelectedKeyID() == ChangedKeyID)
	{
		BehaviorComp->RequestExecution(this);		
	}
	return EBlackboardNotificationResult::ContinueObserving;
}

#if WITH_EDITOR

FName UBTDecorator_BlackboardBase::GetNodeIconName() const
{
	return FName("BTEditor.Graph.BTNode.Decorator.Blackboard.Icon");
}

#endif	// WITH_EDITOR

//----------------------------------------------------------------------//
// DEPRECATED
//----------------------------------------------------------------------//
void UBTDecorator_BlackboardBase::OnBlackboardChange(const UBlackboardComponent& Blackboard, FBlackboard::FKey ChangedKeyID)
{
	OnBlackboardKeyValueChange(Blackboard, ChangedKeyID);
}