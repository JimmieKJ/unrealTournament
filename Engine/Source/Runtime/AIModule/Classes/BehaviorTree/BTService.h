// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BehaviorTreeTypes.h"
#include "BehaviorTree/BTAuxiliaryNode.h"
#include "BTService.generated.h"

/** 
 *	Behavior Tree service nodes is designed to perform "background" tasks that update AI's knowledge.
 *
 *  First tick can be called when parent node is activated during task search (default).
 *  It's meant to be used by services that periodically check condition, which can affect
 *  execution flow in underlying branch. Check OnSearchStart() for details.
 *  Keep in mind that any checks performed there have to be instantaneous!
 *	
 *	BT service should never modify the parent BT's state or flow. From BT point of view
 *  services are fire-and-forget calls and no service-state-checking is being performed. 
 *  If you need this data create a BTDecorator to check this information.
 *
 *  Because some of them can be instanced for specific AI, following virtual functions are not marked as const:
 *   - OnBecomeRelevant (from UBTAuxiliaryNode)
 *   - OnCeaseRelevant (from UBTAuxiliaryNode)
 *   - TickNode (from UBTAuxiliaryNode)
 *   - OnSearchStart
 *
 *  If your node is not being instanced (default behavior), DO NOT change any properties of object within those functions!
 *  Template nodes are shared across all behavior tree components using the same tree asset and must store
 *  their runtime properties in provided NodeMemory block (allocation size determined by GetInstanceMemorySize() )
 *
 */
 
UCLASS(Abstract)
class AIMODULE_API UBTService : public UBTAuxiliaryNode
{
	GENERATED_UCLASS_BODY()

	virtual FString GetStaticDescription() const override;

	void NotifyParentActivation(FBehaviorTreeSearchData& SearchData);

protected:

	// Gets the description of our tick interval
	FString GetStaticTickIntervalDescription() const;

	// Gets the description for our service
	virtual FString GetStaticServiceDescription() const;

	/** defines time span between subsequent ticks of the service */
	UPROPERTY(Category=Service, EditAnywhere, meta=(ClampMin="0.001"))
	float Interval;

	/** allows adding random time to service's Interval */
	UPROPERTY(Category=Service, EditAnywhere, meta=(ClampMin="0.0"))
	float RandomDeviation;

	/** if set, service will be notified about search entering underlying branch */
	uint32 bNotifyOnSearch : 1;

	/** update next tick interval
	 * this function should be considered as const (don't modify state of object) if node is not instanced! */
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	/** called when search enters underlying branch */
	virtual void OnSearchStart(FBehaviorTreeSearchData& SearchData);

#if WITH_EDITOR
	virtual FName GetNodeIconName() const override;
#endif // WITH_EDITOR
};
