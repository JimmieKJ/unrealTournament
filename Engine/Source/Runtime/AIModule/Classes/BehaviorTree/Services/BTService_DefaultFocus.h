// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "BehaviorTree/Services/BTService_BlackboardBase.h"
#include "BTService_DefaultFocus.generated.h"

struct FBTFocusMemory
{
	AActor* FocusActorSet;
	FVector FocusLocationSet;
	bool bActorSet;

	void Reset()
	{
		FocusActorSet = nullptr;
		FocusLocationSet = FAISystem::InvalidLocation;
		bActorSet = false;
	}
};

/**
 * Default Focus service node.
 * A service node that automatically sets the AI controller's focus when it becomes active.
 */
UCLASS(hidecategories=(Service))
class AIMODULE_API UBTService_DefaultFocus : public UBTService_BlackboardBase
{
	GENERATED_BODY()
	
	// not exposed to users on purpose. Here to make reusing focus-setting mechanics by derived classes possible
	UPROPERTY()
	uint8 FocusPriority;

	UBTService_DefaultFocus(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual uint16 GetInstanceMemorySize() const override { return sizeof(FBTFocusMemory); }
	virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	virtual FString GetStaticDescription() const override;
	virtual void DescribeRuntimeValues(const UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity, TArray<FString>& Values) const override;

#if WITH_EDITOR
	virtual FName GetNodeIconName() const override;
#endif // WITH_EDITOR
};
