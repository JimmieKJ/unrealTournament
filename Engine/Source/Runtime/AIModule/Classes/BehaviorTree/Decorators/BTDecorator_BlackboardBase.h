// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "BehaviorTree/BTDecorator.h"
#include "BTDecorator_BlackboardBase.generated.h"

class UBlackboardComponent;

UCLASS(Abstract)
class AIMODULE_API UBTDecorator_BlackboardBase : public UBTDecorator
{
	GENERATED_UCLASS_BODY()

	/** initialize any asset related data */
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

	DEPRECATED(4.8, "OnBlackboardChange has been deprecated. Please switch over to using OnBlackboardKeyValueChange, it's different just by a return type.")
	virtual void OnBlackboardChange(const UBlackboardComponent& Blackboard, FBlackboard::FKey ChangedKeyID);

	/** notify about change in blackboard keys */
	virtual EBlackboardNotificationResult OnBlackboardKeyValueChange(const UBlackboardComponent& Blackboard, FBlackboard::FKey ChangedKeyID);

#if WITH_EDITOR
	virtual FName GetNodeIconName() const override;
#endif

	/** get name of selected blackboard key */
	FName GetSelectedBlackboardKey() const;

protected:

	/** blackboard key selector */
	UPROPERTY(EditAnywhere, Category=Blackboard)
	FBlackboardKeySelector BlackboardKey;

	/** called when execution flow controller becomes active */
	virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	/** called when execution flow controller becomes inactive */
	virtual void OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};

//////////////////////////////////////////////////////////////////////////
// Inlines

FORCEINLINE FName UBTDecorator_BlackboardBase::GetSelectedBlackboardKey() const
{
	return BlackboardKey.SelectedKeyName;
}
