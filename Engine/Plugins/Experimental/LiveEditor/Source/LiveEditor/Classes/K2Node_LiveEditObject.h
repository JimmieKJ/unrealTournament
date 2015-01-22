// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveEditorTypes.h"
#include "EdGraph/EdGraphNodeUtils.h" // for FNodeTextCache
#include "K2Node_LiveEditObject.generated.h"

UCLASS(MinimalAPI)
class UK2Node_LiveEditObject : public UK2Node
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITOR
	// Begin UEdGraphNode interface.
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	// End UEdGraphNode interface.

	// Begin UK2Node interface
	virtual bool IsNodeSafeToIgnore() const override { return true; }
	virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual void GetNodeAttributes( TArray<TKeyValuePair<FString, FString>>& OutNodeAttributes ) const override;
	// End UK2Node interface

	/** Create new pins to show LiveEditable properties on archetype */
	void CreatePinsForClass(UClass* InClass);
	/** See if this is a spawn variable pin, or a 'default' pin */
	bool IsSpawnVarPin(UEdGraphPin* Pin);

	/** Get the class that we are going to spawn */
	UClass* GetClassToSpawn(const TArray<UEdGraphPin*>* InPinsToSearch=NULL) const;

	FString GetEventName() const;

public:
	static FString LiveEditableVarPinCategory;

private:

	/** Get the blueprint input pin */
	UEdGraphPin* GetBlueprintPin(const TArray<UEdGraphPin*>* InPinsToSearch=NULL) const;
	UEdGraphPin *GetBaseClassPin(const TArray<UEdGraphPin*>* InPinsToSearch=NULL) const;

	UEdGraphPin *GetThenPin() const;
	UEdGraphPin *GetVariablePin() const;
	UEdGraphPin *GetDescriptionPin() const;
	UEdGraphPin *GetPermittedBindingsPin() const;
	UEdGraphPin *GetOnMidiInputPin() const;
	UEdGraphPin *GetDeltaMultPin() const;
	UEdGraphPin *GetShouldClampPin() const;
	UEdGraphPin *GetClampMinPin() const;
	UEdGraphPin *GetClampMaxPin() const;
	UFunction *GetEventMIDISignature() const;

	/**
	 * Takes the specified "MutatablePin" and sets its 'PinToolTip' field (according
	 * to the specified description)
	 * 
	 * @param   MutatablePin	The pin you want to set tool-tip text on
	 * @param   PinDescription	A string describing the pin's purpose
	 */
	void SetPinToolTip(UEdGraphPin& MutatablePin, const FText& PinDescription) const;

	/** Constructing FText strings can be costly, so we cache the node's title */
	FNodeTextCache CachedNodeTitle;
#endif
};
