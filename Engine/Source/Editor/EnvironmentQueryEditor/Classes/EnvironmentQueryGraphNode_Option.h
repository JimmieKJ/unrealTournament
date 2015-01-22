// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "EnvironmentQueryGraphNode_Option.generated.h"

UCLASS()
class UEnvironmentQueryGraphNode_Option : public UEnvironmentQueryGraphNode
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	TArray<class UEnvironmentQueryGraphNode_Test*> Tests;

	virtual void AllocateDefaultPins() override;
	virtual void PostPlacedNewNode() override;
	virtual void GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const override;
	virtual void PrepareForCopying() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetDescription() const override;

	void AddSubNode(class UEnvironmentQueryGraphNode_Test* NodeTemplate, class UEdGraph* ParentGraph);
	void CreateAddTestSubMenu(class FMenuBuilder& MenuBuilder, UEdGraph* Graph) const;

	void CalculateWeights();

protected:

	virtual void ResetNodeOwner() override;
};
