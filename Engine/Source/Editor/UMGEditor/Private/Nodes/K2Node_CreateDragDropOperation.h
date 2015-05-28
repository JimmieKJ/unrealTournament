// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "K2Node.h"
#include "K2Node_ConstructObjectFromClass.h"
#include "K2Node_CreateDragDropOperation.generated.h"

UCLASS()
class UMGEDITOR_API UK2Node_CreateDragDropOperation : public UK2Node_ConstructObjectFromClass
{
	GENERATED_UCLASS_BODY()

	// Begin UEdGraphNode interface.
	virtual void AllocateDefaultPins() override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	// End UEdGraphNode interface.

	// Begin UK2Node interface
	virtual FText GetMenuCategory() const override;
	virtual FName GetCornerIcon() const override;
	// End UK2Node interface.

protected:
	/** Gets the default node title when no class is selected */
	virtual FText GetBaseNodeTitle() const override;
	/** Gets the node title when a class has been selected. */
	virtual FText GetNodeTitleFormat() const override;
	/** Gets base class to use for the 'class' pin.  UObject by default. */
	virtual UClass* GetClassPinBaseClass() const override;
};
