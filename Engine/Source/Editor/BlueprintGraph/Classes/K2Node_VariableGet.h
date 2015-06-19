// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "K2Node_Variable.h"
#include "EdGraph/EdGraphNodeUtils.h" // for FNodeTextCache
#include "K2Node_VariableGet.generated.h"

UCLASS()
class BLUEPRINTGRAPH_API UK2Node_VariableGet : public UK2Node_Variable
{
	GENERATED_UCLASS_BODY()

	// Begin UEdGraphNode interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual void GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const override;
	// End UEdGraphNode interface

	// Begin K2Node interface
	virtual bool IsNodePure() const override { return bIsPureGet; }
	virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;
	virtual bool ShouldShowNodeProperties() const override { return true; }
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	// End K2Node interface

	static FText GetPropertyTooltip(UProperty const* VariableProperty);
	static FText GetBlueprintVarTooltip(FBPVariableDescription const& VarDesc);

	/**
	 * Will change the node's purity, and reallocate pins accordingly (adding/
	 * removing exec pins).
	 * 
	 * @param  bNewPurity  The new value for bIsPureCast.
	 */
	void SetPurity(bool bNewPurity);

private:
	/** Checks if the pin type is valid to be a non-pure node */
	static bool IsValidTypeForNonPure(const FEdGraphPinType& InPinType);

	/** Adds pins required for the node to function in a non-pure manner */
	void CreateNonPurePins(TArray<UEdGraphPin*>* InOldPinsPtr);

	/** Flips the node's purity (adding/removing exec pins as needed). */
	void TogglePurity();

	/** Constructing FText strings can be costly, so we cache the node's title/tooltip */
	FNodeTextCache CachedTooltip;
	FNodeTextCache CachedNodeTitle;

	/** TRUE if the node should function as a pure node, without exec pins */
	UPROPERTY()
	bool bIsPureGet;
};

