// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "K2Node.h"
#include "K2Node_GetArrayItem.generated.h"

UCLASS(MinimalAPI, Category = "Utilities|Array", meta=(Keywords = "array"))
class UK2Node_GetArrayItem : public UK2Node
{
	GENERATED_UCLASS_BODY()

public:
	// UEdGraphNode interface
	virtual bool IsNodePure() const override { return true; }
	virtual void AllocateDefaultPins() override;
	virtual void PostReconstructNode() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	// End of UEdGraphNode interface

	// UK2Node interface
	virtual bool ShouldDrawCompact() const { return true; }
	virtual void NotifyPinConnectionListChanged(UEdGraphPin* Pin) override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual int32 GetNodeRefreshPriority() const override { return EBaseNodeRefreshPriority::Low_UsesDependentWildcard; }
	// End of UK2Node interface

	/** Helper function to return the array pin */
	UEdGraphPin* GetTargetArrayPin() { return Pins[0]; }

	/** Helper function to return the index pin */
	UEdGraphPin* GetIndexPin() { return Pins[1]; }

	/** Helper function to return the result pin */
	UEdGraphPin* GetResultPin() { return Pins[2]; }

	/** Propagates pin type to the Array pin and the Result pin */
	void PropagatePinType(FEdGraphPinType& InType);
};