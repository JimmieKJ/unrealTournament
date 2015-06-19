// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "K2Node.h"
#include "NodeDependingOnEnumInterface.h"
#include "K2Node_Select.generated.h"

UCLASS(MinimalAPI, meta=(Keywords = "Ternary If"))
class UK2Node_Select : public UK2Node, public INodeDependingOnEnumInterface
{
	GENERATED_UCLASS_BODY()

	/** The number of selectable options this node currently has */
	UPROPERTY()
	int32 NumOptionPins;

	/** The pin type of the index pin */
	UPROPERTY()
	FEdGraphPinType IndexPinType;

	/** Name of the enum being switched on */
	UPROPERTY()
	UEnum* Enum;

	/** List of the current entries in the enum (Pin Names) */
	UPROPERTY()
	TArray<FName> EnumEntries;

	/** List of the current entries in the enum (Pin Friendly Names) */
	UPROPERTY()
	TArray<FName> EnumEntryFriendlyNames;

	/** Whether we need to reconstruct the node after the pins have changed */
	UPROPERTY(Transient)
	bool bReconstructNode;

	// Begin UEdGraphNode interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual void NodeConnectionListChanged() override;
	virtual void PinTypeChanged(UEdGraphPin* Pin) override;
	virtual void AutowireNewNode(UEdGraphPin* FromPin) override;
	virtual void PostPasteNode() override;
	virtual FName GetPaletteIcon(FLinearColor& OutColor) const override{ return TEXT("GraphEditor.Select_16x"); }
	// End UEdGraphNode interface

	// Begin UK2Node interface
	virtual ERedirectType DoPinsMatchForReconstruction(const UEdGraphPin* NewPin, int32 NewPinIndex, const UEdGraphPin* OldPin, int32 OldPinIndex) const override;
	virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;
	virtual void PostReconstructNode() override;
	virtual bool IsNodeSafeToIgnore() const override { return true; }
	virtual bool IsNodePure() const override { return true; }
	virtual void NotifyPinConnectionListChanged(UEdGraphPin* Pin) override;
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	virtual bool IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	virtual int32 GetNodeRefreshPriority() const override { return EBaseNodeRefreshPriority::Low_UsesDependentWildcard; }
	// End UK2Node interface

	// INodeDependingOnEnumInterface
	virtual class UEnum* GetEnum() const override { return Enum; }
	virtual bool ShouldBeReconstructedAfterEnumChanged() const override { return true; }
	// End of INodeDependingOnEnumInterface

	/** Get the return value pin */
	BLUEPRINTGRAPH_API UEdGraphPin* GetReturnValuePin() const;
	/** Get the condition pin */
	BLUEPRINTGRAPH_API virtual UEdGraphPin* GetIndexPin() const;
	/** Returns a list of pins that represent the selectable options */
	BLUEPRINTGRAPH_API virtual void GetOptionPins(TArray<UEdGraphPin*>& OptionPins) const;

	/** Gets the name and class of the EqualEqual_IntInt function */
	BLUEPRINTGRAPH_API void GetConditionalFunction(FName& FunctionName, UClass** FunctionClass);
	/** Gets the name and class of the PrintString function */
	BLUEPRINTGRAPH_API static void GetPrintStringFunction(FName& FunctionName, UClass** FunctionClass);

	/** Adds a new option pin to the node */
	BLUEPRINTGRAPH_API void AddOptionPinToNode();
	/** Removes the last option pin from the node */
	BLUEPRINTGRAPH_API void RemoveOptionPinToNode();
	/** Return whether an option pin can be added to the node */
	BLUEPRINTGRAPH_API virtual bool CanAddOptionPinToNode() const;
	/** Return whether an option pin can be removed to the node */
	BLUEPRINTGRAPH_API virtual bool CanRemoveOptionPinToNode() const;

	/**
	 * Notification from the editor that the user wants to change the PinType on a selected pin
	 *
	 * @param Pin	The pin the user wants to change the type for
	 */
	BLUEPRINTGRAPH_API void ChangePinType(UEdGraphPin* Pin);
	/**
	 * Whether the user can change the pintype on a selected pin
	 *
	 * @param Pin	The pin in question
	 */
	BLUEPRINTGRAPH_API bool CanChangePinType(UEdGraphPin* Pin) const;

	// Bind the options to a named enum 
	virtual void SetEnum(UEnum* InEnum, bool bForceRegenerate = false);
};

