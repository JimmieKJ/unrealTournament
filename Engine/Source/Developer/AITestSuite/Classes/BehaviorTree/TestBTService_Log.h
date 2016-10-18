// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "BehaviorTree/BTService.h"
#include "TestBTService_Log.generated.h"

UCLASS(meta = (HiddenNode))
class UTestBTService_Log : public UBTService
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	int32 LogActivation;

	UPROPERTY()
	int32 LogDeactivation;

	virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
