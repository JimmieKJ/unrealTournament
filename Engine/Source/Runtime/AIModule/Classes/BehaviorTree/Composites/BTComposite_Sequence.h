// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "BehaviorTree/BTCompositeNode.h"
#include "BTComposite_Sequence.generated.h"

/** 
 * Sequence composite node.
 * A sequence node runs each child in turn until one fails, in which case the sequence succeeds.
 */
UCLASS()
class AIMODULE_API UBTComposite_Sequence : public UBTCompositeNode
{
	GENERATED_UCLASS_BODY()

	int32 GetNextChildHandler(struct FBehaviorTreeSearchData& SearchData, int32 PrevChild, EBTNodeResult::Type LastResult) const;

#if WITH_EDITOR
	virtual bool CanAbortLowerPriority() const override;
	virtual FName GetNodeIconName() const override;
#endif
};
