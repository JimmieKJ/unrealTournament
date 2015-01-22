// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "BehaviorTree/BTCompositeNode.h"
#include "BTComposite_Selector.generated.h"

/** 
 * Selector composite node.
 * A selector node runs each child in turn until the first child node succeeds, in which case the selector fails.
 */
UCLASS()
class AIMODULE_API UBTComposite_Selector: public UBTCompositeNode
{
	GENERATED_UCLASS_BODY()

	int32 GetNextChildHandler(struct FBehaviorTreeSearchData& SearchData, int32 PrevChild, EBTNodeResult::Type LastResult) const;

#if WITH_EDITOR
	virtual FName GetNodeIconName() const override;
#endif
};
