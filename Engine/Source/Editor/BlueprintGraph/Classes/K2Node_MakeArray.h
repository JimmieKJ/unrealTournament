// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "K2Node.h"
#include "K2Node_MakeArray.generated.h"

UCLASS(MinimalAPI)
class UK2Node_MakeArray : public UK2Node
{
	GENERATED_UCLASS_BODY()

	/** The number of input pins to generate for this node */
	UPROPERTY()
	int32 NumInputs;

public:
	BLUEPRINTGRAPH_API void AddInputPin();
	BLUEPRINTGRAPH_API void RemoveInputPin(UEdGraphPin* Pin);

	/** returns a reference to the output array pin of this node, which is responsible for defining the type */
	BLUEPRINTGRAPH_API UEdGraphPin* GetOutputPin() const;

public:
	// UEdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual void AllocateDefaultPins() override;
	virtual void PostReconstructNode() override;
	virtual FText GetTooltipText() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual void ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const override;
	// End of UEdGraphNode interface

	// UK2Node interface
	virtual bool IsNodePure() const override { return true; }
	virtual void NotifyPinConnectionListChanged(UEdGraphPin* Pin) override;
	virtual void GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const override;
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	virtual bool IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	virtual int32 GetNodeRefreshPriority() const override { return EBaseNodeRefreshPriority::Low_UsesDependentWildcard; }
	virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins);
	// End of UK2Node interface



protected:
	friend class FKismetCompilerContext;

	/** If needed, will clear all pins to be wildcards */
	void ClearPinTypeToWildcard();

	bool CanResetToWildcard() const;

	/** Propagates the pin type from the output (array) pin to the inputs, to make sure types are consistent */
	void PropagatePinType();

	/** Returns the function to be used to clear the array */
	BLUEPRINTGRAPH_API UFunction* GetArrayClearFunction() const;

	/** Returns the function to be used to add a function to the array */
	BLUEPRINTGRAPH_API UFunction* GetArrayAddFunction() const;

	void SyncPinNames();
};