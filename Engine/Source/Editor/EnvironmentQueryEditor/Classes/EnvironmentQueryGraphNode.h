// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "EnvironmentQueryGraphNode.generated.h"

UCLASS()
class UEnvironmentQueryGraphNode : public UEdGraphNode
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	UObject* NodeInstance;

	/** subnode index assigned during copy operation to connect nodes again on paste */
	UPROPERTY()
	int32 CopySubNodeIndex;

	/** Get the BT graph that owns this node */
	virtual class UEnvironmentQueryGraph* GetEnvironmentQueryGraph();

	virtual void AutowireNewNode(UEdGraphPin* FromPin) override;
	virtual void PostEditImport() override;
	virtual void PrepareForCopying() override;
	virtual void PostCopyNode();

	// @return the input pin for this state
	virtual UEdGraphPin* GetInputPin(int32 InputIndex=0) const;
	// @return the output pin for this state
	virtual UEdGraphPin* GetOutputPin(int32 InputIndex=0) const;
	//
	virtual UEdGraph* GetBoundGraph() const { return NULL; }

	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetDescription() const;

	virtual void NodeConnectionListChanged() override;

	virtual bool CanCreateUnderSpecifiedSchema(const UEdGraphSchema* DesiredSchema) const override;

	virtual bool IsSubNode() const { return false; }

	UClass* EnvQueryNodeClass;

protected:

	virtual void ResetNodeOwner();
};

