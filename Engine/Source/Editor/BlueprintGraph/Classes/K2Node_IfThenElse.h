// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "K2Node.h"
#include "K2Node_IfThenElse.generated.h"

UCLASS(MinimalAPI, meta=(Keywords = "if bool"))
class UK2Node_IfThenElse : public UK2Node
{
	GENERATED_UCLASS_BODY()

	// Begin UEdGraphNode interface
	virtual void AllocateDefaultPins() override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FName GetPaletteIcon(FLinearColor& OutColor) const override{ return TEXT("GraphEditor.Branch_16x"); }
	// End UEdGraphNode interface

	// Begin K2Node interface.
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	// End K2Node interface.

	/** Get the then output pin */
	BLUEPRINTGRAPH_API UEdGraphPin* GetThenPin() const;
	/** Get the return value pin */
	BLUEPRINTGRAPH_API UEdGraphPin* GetElsePin() const;
	/** Get the condition pin */
	BLUEPRINTGRAPH_API UEdGraphPin* GetConditionPin() const;
};

