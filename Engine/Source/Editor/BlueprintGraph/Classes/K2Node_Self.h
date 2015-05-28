// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "K2Node.h"
#include "K2Node_Self.generated.h"

UCLASS(MinimalAPI)
class UK2Node_Self : public UK2Node
{
	GENERATED_UCLASS_BODY()

	// Begin UEdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FText GetKeywords() const override;
	virtual void AllocateDefaultPins() override;
	virtual void ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const override;
	// End UEdGraphNode interface

	// Begin K2Node interface
	virtual bool IsNodePure() const override { return true; }
	virtual bool DrawNodeAsVariable() const override { return true; }
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	// End K2Node interface
};

