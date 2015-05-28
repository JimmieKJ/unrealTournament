// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "BehaviorTreeGraphNode_Service.generated.h"

UCLASS()
class UBehaviorTreeGraphNode_Service : public UBehaviorTreeGraphNode
{
	GENERATED_UCLASS_BODY()

	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual void AllocateDefaultPins() override;
};

