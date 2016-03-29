// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "AnimationGraphSchema.h"
#include "AnimationStateGraphSchema.generated.h"

UCLASS(MinimalAPI)
class UAnimationStateGraphSchema : public UAnimationGraphSchema
{
	GENERATED_UCLASS_BODY()

	//~ Begin UEdGraphSchema Interface.
	virtual void CreateDefaultNodesForGraph(UEdGraph& Graph) const override;
	virtual void GetGraphDisplayInformation(const UEdGraph& Graph, /*out*/ FGraphDisplayInfo& DisplayInfo) const override;
	//~ End UEdGraphSchema Interface.
};
