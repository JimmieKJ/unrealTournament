// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "EnvironmentQueryGraphNode_Option.generated.h"

UCLASS()
class UEnvironmentQueryGraphNode_Option : public UEnvironmentQueryGraphNode
{
	GENERATED_UCLASS_BODY()

	virtual void AllocateDefaultPins() override;
	virtual void PostPlacedNewNode() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetDescription() const override;

	virtual void PrepareForCopying() override;
	virtual void UpdateNodeClassData() override;

	virtual void GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const override;
	void CreateAddTestSubMenu(class FMenuBuilder& MenuBuilder, UEdGraph* Graph) const;

	void CalculateWeights();
	void UpdateNodeData();

protected:

	virtual void ResetNodeOwner() override;
};
