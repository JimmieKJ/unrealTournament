// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "K2Node_CallFunction.h"
#include "EdGraph/EdGraphNodeUtils.h" // for FNodeTitleTextTable
#include "K2Node_Message.generated.h"

UCLASS(MinimalAPI)
class UK2Node_Message : public UK2Node_CallFunction
{
	GENERATED_UCLASS_BODY()


	// Begin UEdGraphNode interface.
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual void AllocateDefaultPins() override;
	// End UEdGraphNode interface.

	// Begin K2Node interface.
	virtual bool IsNodeSafeToIgnore() const override { return true; }
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual FName GetCornerIcon() const override;
	virtual bool IsNodePure() const override { return false; }
	// End K2Node interface.

	// Begin K2Node_CallFunction Interface.
	virtual UEdGraphPin* CreateSelfPin(const UFunction* Function) override;

protected:
	virtual void EnsureFunctionIsInBlueprint() override;
	// End K2Node_CallFunction Interface.

	/** Constructing FText strings can be costly, so we cache the node's title */
	FNodeTitleTextTable CachedNodeTitles;
};

