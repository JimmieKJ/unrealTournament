// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "BehaviorTree/Decorators/BTDecorator_BlackboardBase.h"
#include "BTDecorator_IsAtLocation.generated.h"

/**
 * Is At Location decorator node.
 * A decorator node that checks if AI controlled pawn is at given location.
 */
UCLASS()
class AIMODULE_API UBTDecorator_IsAtLocation : public UBTDecorator_BlackboardBase
{
	GENERATED_UCLASS_BODY()

	/** distance threshold to accept as being at location */
	UPROPERTY(EditAnywhere, Category = Condition, meta = (ClampMin = "0.0"))
	float AcceptableRadius;

	/** if moving to an actor and this actor is a nav agent, then we will move to their nav agent location */
	UPROPERTY(EditAnywhere, Category = Condition)
	bool bUseNavAgentGoalLocation;

	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
	
	virtual FString GetStaticDescription() const override;

#if WITH_EDITOR
	virtual FName GetNodeIconName() const override;
#endif // WITH_EDITOR
};
