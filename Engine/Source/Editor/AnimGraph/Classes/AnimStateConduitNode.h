// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "AnimStateNodeBase.h"
#include "AnimStateConduitNode.generated.h"

UCLASS(MinimalAPI)
class UAnimStateConduitNode : public UAnimStateNodeBase
{
	GENERATED_UCLASS_BODY()
public:

	// The transition graph for this conduit; it's a logic graph, not an animation graph
	UPROPERTY()
	class UEdGraph* BoundGraph;

	// Begin UEdGraphNode interface
	virtual void AllocateDefaultPins() override;
	virtual void AutowireNewNode(UEdGraphPin* FromPin) override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual bool CanDuplicateNode() const override { return true; }
	virtual void PostPasteNode() override;
	virtual void PostPlacedNewNode() override;
	virtual void DestroyNode() override;
	virtual void ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const override;
	// End UEdGraphNode interface

	// Begin UAnimStateNodeBase interface
	virtual UEdGraphPin* GetInputPin() const override;
	virtual UEdGraphPin* GetOutputPin() const override;
	virtual FString GetStateName() const override;
	virtual void GetTransitionList(TArray<class UAnimStateTransitionNode*>& OutTransitions, bool bWantSortedList = true) override;
	virtual FString GetDesiredNewNodeName() const override;
	virtual UEdGraph* GetBoundGraph() const override { return BoundGraph; }
	// End of UAnimStateNodeBase interface
};
