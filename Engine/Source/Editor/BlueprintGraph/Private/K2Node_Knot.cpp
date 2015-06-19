// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintGraphPrivatePCH.h"
#include "Kismet2NameValidators.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "K2Node_Knot.h"

#define LOCTEXT_NAMESPACE "K2Node_Knot"

/////////////////////////////////////////////////////
// UK2Node_Knot

UK2Node_Knot::UK2Node_Knot(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCanRenameNode = true;
}

void UK2Node_Knot::AllocateDefaultPins()
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	const FString InputPinName(TEXT("InputPin"));
	const FString OutputPinName(TEXT("OutputPin"));

	UEdGraphPin* MyInputPin = CreatePin(EGPD_Input, Schema->PC_Wildcard, FString(), nullptr, /*bIsArray=*/ false, /*bIsReference=*/ false, InputPinName);
	MyInputPin->bDefaultValueIsIgnored = true;

	UEdGraphPin* MyOutputPin = CreatePin(EGPD_Output, Schema->PC_Wildcard, FString(), nullptr, /*bIsArray=*/ false, /*bIsReference=*/ false, OutputPinName);
}

FText UK2Node_Knot::GetTooltipText() const
{
	//@TODO: Should pull the tooltip from the source pin
	return LOCTEXT("KnotTooltip", "Reroute Node (reroutes wires)");
}

FText UK2Node_Knot::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (TitleType == ENodeTitleType::EditableTitle)
	{
		return FText::FromString(NodeComment);
	}
	else if (TitleType == ENodeTitleType::MenuTitle)
	{
		return LOCTEXT("KnotListTitle", "Add Reroute Node...");
	}
	else
	{
		return LOCTEXT("KnotTitle", "Reroute Node");
	}
}

void UK2Node_Knot::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	UEdGraphPin* MyInputPin = GetInputPin();
	UEdGraphPin* MyOutputPin = GetOutputPin();

	K2Schema->CombineTwoPinNetsAndRemoveOldPins(MyInputPin, MyOutputPin);
}

bool UK2Node_Knot::IsNodeSafeToIgnore() const
{
	return true;
}

bool UK2Node_Knot::AllowSplitPins() const
{
	return false;
}

void UK2Node_Knot::NotifyPinConnectionListChanged(UEdGraphPin* Pin)
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	UEdGraphPin* MyInputPin = GetInputPin();
	UEdGraphPin* MyOutputPin = GetOutputPin();

	const int32 NumLinks = MyInputPin->LinkedTo.Num() + MyOutputPin->LinkedTo.Num();

	if (Pin->LinkedTo.Num() > 0)
	{
		// Just made a connection, was it the first?
		if (NumLinks == 1)
		{
			UEdGraphPin* TypeSource = Pin->LinkedTo[0];

			MyInputPin->PinType = TypeSource->PinType;
			MyOutputPin->PinType = TypeSource->PinType;
		}
	}
	else
	{
		// Just broke a connection, was it the last?
		if (NumLinks == 0)
		{
			// Revert to wildcard
			MyInputPin->BreakAllPinLinks();
			MyInputPin->PinType.ResetToDefaults();
			MyInputPin->PinType.PinCategory = K2Schema->PC_Wildcard;

			MyOutputPin->BreakAllPinLinks();
			MyOutputPin->PinType.ResetToDefaults();
			MyOutputPin->PinType.PinCategory = K2Schema->PC_Wildcard;
		}
	}
}

void UK2Node_Knot::PostReconstructNode()
{
	UEdGraphPin* MyInputPin = GetInputPin();
	UEdGraphPin* MyOutputPin = GetOutputPin();

	// Find a pin that has connections to use to jumpstart the wildcard process
	for (int32 PinIndex = 0; PinIndex < Pins.Num(); ++PinIndex)
	{
		if (Pins[PinIndex]->LinkedTo.Num() > 0)
		{
			// The pin is linked, continue to use its type as the type for all pins.
			UEdGraphPin* TypeSource = Pins[PinIndex]->LinkedTo[0];

			MyInputPin->PinType = TypeSource->PinType;
			MyOutputPin->PinType = TypeSource->PinType;
			break;
		}
	}
}

void UK2Node_Knot::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	// actions get registered under specific object-keys; the idea is that 
	// actions might have to be updated (or deleted) if their object-key is  
	// mutated (or removed)... here we use the node's class (so if the node 
	// type disappears, then the action should go with it)
	UClass* ActionKey = GetClass();
	// to keep from needlessly instantiating a UBlueprintNodeSpawner, first   
	// check to make sure that the registrar is looking for actions of this type
	// (could be regenerating actions for a specific asset, and therefore the 
	// registrar would only accept actions corresponding to that asset)
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

bool UK2Node_Knot::ShouldOverridePinNames() const
{
	return true;
}

FText UK2Node_Knot::GetPinNameOverride(const UEdGraphPin& Pin) const
{
	// Keep the pin size tiny
	return FText::GetEmpty();
}

void UK2Node_Knot::OnRenameNode(const FString& NewName)
{
	NodeComment = NewName;
}

TSharedPtr<class INameValidatorInterface> UK2Node_Knot::MakeNameValidator() const
{
	// Comments can be duplicated, etc...
	return MakeShareable(new FDummyNameValidator(EValidatorResult::Ok));
}

UEdGraphPin* UK2Node_Knot::GetPassThroughPin(const UEdGraphPin* FromPin) const
{
	if(FromPin && Pins.Contains(FromPin))
	{
		return FromPin == Pins[0] ? Pins[1] : Pins[0];
	}

	return nullptr;
}

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
