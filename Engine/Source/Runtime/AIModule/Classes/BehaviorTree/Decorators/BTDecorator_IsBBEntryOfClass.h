// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "BehaviorTree/BTDecorator.h"
#include "BTDecorator_IsBBEntryOfClass.generated.h"

UCLASS(HideCategories=(Condition))
class AIMODULE_API UBTDecorator_IsBBEntryOfClass : public UBTDecorator_BlackboardBase
{
	GENERATED_BODY()
		
public:
	UBTDecorator_IsBBEntryOfClass(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:

	UPROPERTY(Category = Blackboard, EditAnywhere)
	TSubclassOf<UObject> TestClass;

	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
	virtual EBlackboardNotificationResult OnBlackboardKeyValueChange(const UBlackboardComponent& Blackboard, FBlackboard::FKey ChangedKeyID) override;
	virtual void DescribeRuntimeValues(const UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity, TArray<FString>& Values) const override;
	virtual FString GetStaticDescription() const override;
};
