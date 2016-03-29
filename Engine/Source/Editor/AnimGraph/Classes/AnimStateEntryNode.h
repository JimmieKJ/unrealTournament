// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "EdGraph/EdGraphNode.h"
#include "AnimStateEntryNode.generated.h"

UCLASS(MinimalAPI)
class UAnimStateEntryNode : public UEdGraphNode
{
	GENERATED_UCLASS_BODY()


	//~ Begin UEdGraphNode Interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	//~ End UEdGraphNode Interface
	
	ANIMGRAPH_API UEdGraphNode* GetOutputNode() const;

};
