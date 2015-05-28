// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "BlueprintGraphPrivatePCH.h"

#include "SlateBasics.h"
#include "../../../Runtime/Engine/Classes/Kismet/KismetArrayLibrary.h"
#include "ScopedTransaction.h"
#include "KismetCompiler.h"
#include "BlueprintNodeSpawner.h"
#include "EditorCategoryUtils.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "EdGraph/EdGraphNodeUtils.h" // for FNodeTextCache

static const FString OutputPinName = FString(TEXT("Array"));

#define LOCTEXT_NAMESPACE "MakeArrayNode"

/////////////////////////////////////////////////////
// FKCHandler_MakeArray
class FKCHandler_MakeArray : public FNodeHandlingFunctor
{
public:
	FKCHandler_MakeArray(FKismetCompilerContext& InCompilerContext)
		: FNodeHandlingFunctor(InCompilerContext)
	{
	}

	virtual void RegisterNets(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
 		UK2Node_MakeArray* ArrayNode = CastChecked<UK2Node_MakeArray>(Node);
 		UEdGraphPin* OutputPin = ArrayNode->GetOutputPin();

		FNodeHandlingFunctor::RegisterNets(Context, Node);

		// Create a local term to drop the array into
		FBPTerminal* Term = Context.CreateLocalTerminalFromPinAutoChooseScope(OutputPin, Context.NetNameMap->MakeValidName(OutputPin));
		Term->bPassedByReference = false;
		Term->Source = Node;
 		Context.NetMap.Add(OutputPin, Term);
	}

	virtual void Compile(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		UK2Node_MakeArray* ArrayNode = CastChecked<UK2Node_MakeArray>(Node);
		UEdGraphPin* OutputPin = ArrayNode->GetOutputPin();

		FBPTerminal** ArrayTerm = Context.NetMap.Find(OutputPin);
		check(ArrayTerm);

		FBlueprintCompiledStatement& CreateArrayStatement = Context.AppendStatementForNode(Node);
		CreateArrayStatement.Type = KCST_CreateArray;
		CreateArrayStatement.LHS = *ArrayTerm;

		for(auto PinIt = Node->Pins.CreateIterator(); PinIt; ++PinIt)
		{
			UEdGraphPin* Pin = *PinIt;
			if(Pin && Pin->Direction == EGPD_Input)
			{
				FBPTerminal** InputTerm = Context.NetMap.Find(FEdGraphUtilities::GetNetFromPin(Pin));
				if( InputTerm )
				{
					CreateArrayStatement.RHS.Add(*InputTerm);
				}
			}
		}
	}
};

/////////////////////////////////////////////////////
// UK2Node_MakeArray

UK2Node_MakeArray::UK2Node_MakeArray(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

	NumInputs = 1;
}

FNodeHandlingFunctor* UK2Node_MakeArray::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_MakeArray(CompilerContext);
}

FText UK2Node_MakeArray::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("NodeTitle", "Make Array");
}

UEdGraphPin* UK2Node_MakeArray::GetOutputPin() const
{
	return FindPin(OutputPinName);
}

void UK2Node_MakeArray::AllocateDefaultPins()
{
	// Create the output pin
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Wildcard, TEXT(""), NULL, true, false, *OutputPinName);

	// Create the input pins to create the arrays from
	for (int32 i = 0; i < NumInputs; ++i)
	{
		CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Wildcard, TEXT(""), NULL, false, false, *FString::Printf(TEXT("[%d]"), i));
	}
}

bool UK2Node_MakeArray::CanResetToWildcard() const
{
	bool bClearPinsToWildcard = true;

	// Check to see if we want to clear the wildcards.
	for (const UEdGraphPin* Pin : Pins)
	{
		if( Pin->LinkedTo.Num() > 0 )
		{
			// One of the pins is still linked, we will not be clearing the types.
			bClearPinsToWildcard = false;
			break;
		}
	}

	return bClearPinsToWildcard;
}

void UK2Node_MakeArray::ClearPinTypeToWildcard()
{
	if( CanResetToWildcard() )
	{
		UEdGraphPin* OutputPin = GetOutputPin();
		OutputPin->PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
		OutputPin->PinType.PinSubCategory = TEXT("");
		OutputPin->PinType.PinSubCategoryObject = NULL;

		PropagatePinType();
	}
}

void UK2Node_MakeArray::NotifyPinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::NotifyPinConnectionListChanged(Pin);

	// Was this the first or last connection?
	int32 NumPinsWithLinks = 0;
	// Array to cache the input pins we might want to find these if we are removing the last link
	TArray< UEdGraphPin* > InputPins;
	for (int32 PinIndex = 0; PinIndex < Pins.Num(); ++PinIndex)
	{
		NumPinsWithLinks += (Pins[PinIndex]->LinkedTo.Num() > 0) ? 1 : 0;
		if( Pins[PinIndex]->Direction == EGPD_Input )
		{
			InputPins.Add(Pins[PinIndex]);
		}
	}

	UEdGraphPin* OutputPin = GetOutputPin();

	if (Pin->LinkedTo.Num() > 0)
	{
		// Just made a connection, was it the first?
		if (NumPinsWithLinks == 1)
		{
			// Update the types on all the pins
			OutputPin->PinType = Pin->LinkedTo[0]->PinType;
			OutputPin->PinType.bIsArray = true;
			PropagatePinType();
		}
	}
	else
	{
		// Just broke a connection, was it the last?
		if (NumPinsWithLinks == 0)
		{
			// Return to wildcard if theres nothing in any of the input pins
			bool bResetOutputPin = true;
			for (int32 PinIndex = 0; PinIndex < InputPins.Num(); ++PinIndex)
			{
				if( InputPins[PinIndex]->GetDefaultAsString().IsEmpty() == false )
				{
					bResetOutputPin = false;
				}
			}

			if( bResetOutputPin == true )
			{
				OutputPin->PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
				OutputPin->PinType.PinSubCategory = TEXT("");
				OutputPin->PinType.PinSubCategoryObject = NULL;

				PropagatePinType();
			}
		}
	}
}

void UK2Node_MakeArray::PropagatePinType()
{
	const UEdGraphPin* OutputPin = GetOutputPin();

	if (OutputPin)
	{
		UClass const* CallingContext = NULL;
		if (UBlueprint const* Blueprint = GetBlueprint())
		{
			CallingContext = Blueprint->GeneratedClass;
			if (CallingContext == NULL)
			{
				CallingContext = Blueprint->ParentClass;
			}
		}

		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		bool bWantRefresh = false;
		// Propagate pin type info (except for array info!) to pins with dependent types
		for (TArray<UEdGraphPin*>::TIterator it(Pins); it; ++it)
		{
			UEdGraphPin* CurrentPin = *it;

			if (CurrentPin != OutputPin)
			{
				// sub pins will be updated by their parent pin, so if we have a parent pin just do nothing
				if (CurrentPin->ParentPin == nullptr)
				{
					bWantRefresh = true;

					// if we've reset to wild card or the parentpin no longer matches we need to collapse the split pin(s)
					// otherwise everything should be OK:
					if (CurrentPin->SubPins.Num() != 0 &&
						(	CurrentPin->PinType.PinCategory != OutputPin->PinType.PinCategory ||
							CurrentPin->PinType.PinSubCategory != OutputPin->PinType.PinSubCategory ||
							CurrentPin->PinType.PinSubCategoryObject != OutputPin->PinType.PinSubCategoryObject )
						)
					{
						// this is a little dicey, but should be fine.. relies on the fact that RecombinePin will only remove pins that
						// are placed after CurrentPin in the Pins member:
						Schema->RecombinePin(CurrentPin->SubPins[0]);
					}

					CurrentPin->PinType.PinCategory = OutputPin->PinType.PinCategory;
					CurrentPin->PinType.PinSubCategory = OutputPin->PinType.PinSubCategory;
					CurrentPin->PinType.PinSubCategoryObject = OutputPin->PinType.PinSubCategoryObject;
				}

				if (CurrentPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard)
				{
					CurrentPin->ResetDefaultValue();
				}
				else if (CurrentPin->GetDefaultAsString().IsEmpty() == true)
				{
					// Only reset default value if there isn't one set. Otherwise this deletes data!
					Schema->SetPinDefaultValueBasedOnType(CurrentPin);
				}

				// Verify that all previous connections to this pin are still valid with the new type
				for (TArray<UEdGraphPin*>::TIterator ConnectionIt(CurrentPin->LinkedTo); ConnectionIt; ++ConnectionIt)
				{
					UEdGraphPin* ConnectedPin = *ConnectionIt;
					if (!Schema->ArePinsCompatible(CurrentPin, ConnectedPin, CallingContext))
					{
						CurrentPin->BreakLinkTo(ConnectedPin);
					}
				}
			}
		}
		// If we have a valid graph we should refresh it now to refelect any changes we made
		if( (bWantRefresh == true ) && ( OutputPin->GetOwningNode() != NULL ) && ( OutputPin->GetOwningNode()->GetGraph() != NULL ) )
		{
			OutputPin->GetOwningNode()->GetGraph()->NotifyGraphChanged();
		}
	}
}

UFunction* UK2Node_MakeArray::GetArrayClearFunction() const
{
	UClass* ArrayLibClass = UKismetArrayLibrary::StaticClass();
	UFunction* ReturnFunction = ArrayLibClass->FindFunctionByName(FName(TEXT("Array_Clear")));
	check(ReturnFunction);
	return ReturnFunction;
}

UFunction* UK2Node_MakeArray::GetArrayAddFunction() const
{
	UClass* ArrayLibClass = UKismetArrayLibrary::StaticClass();
	UFunction* ReturnFunction = ArrayLibClass->FindFunctionByName(FName(TEXT("Array_Add")));
	check(ReturnFunction);
	return ReturnFunction;
}

void UK2Node_MakeArray::PostReconstructNode()
{
	// Find a pin that has connections to use to jumpstart the wildcard process
	for (int32 PinIndex = 0; PinIndex < Pins.Num(); ++PinIndex)
	{
		if (Pins[PinIndex]->LinkedTo.Num() > 0)
		{
			// The pin is linked, continue to use its type as the type for all pins.

			// Update the types on all the pins
			UEdGraphPin* OutputPin = GetOutputPin();
			OutputPin->PinType = Pins[PinIndex]->LinkedTo[0]->PinType;
			OutputPin->PinType.bIsArray = true;
			PropagatePinType();
			break;
		}
		else if(!Pins[PinIndex]->GetDefaultAsString().IsEmpty())
		{
			// The pin has user data in it, continue to use its type as the type for all pins.

			// Update the types on all the pins
			UEdGraphPin* OutputPin = GetOutputPin();
			OutputPin->PinType = Pins[PinIndex]->PinType;
			OutputPin->PinType.bIsArray = true;
			PropagatePinType();
			break;
		}
	}
}

FText UK2Node_MakeArray::GetTooltipText() const
{
	return LOCTEXT("MakeArrayTooltip", "Create an array from a series of items.");
}

void UK2Node_MakeArray::AddInputPin()
{
	FScopedTransaction Transaction( LOCTEXT("AddPinTx", "AddPin") );
	Modify();

	++NumInputs;
	FEdGraphPinType OutputPinType = GetOutputPin()->PinType;
	CreatePin(EGPD_Input, OutputPinType.PinCategory, OutputPinType.PinSubCategory, OutputPinType.PinSubCategoryObject.Get(), false, false, *FString::Printf(TEXT("[%d]"), (NumInputs-1)));
	
	const bool bIsCompiling = GetBlueprint()->bBeingCompiled;
	if( !bIsCompiling )
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprint());
	}
}

void UK2Node_MakeArray::RemoveInputPin(UEdGraphPin* Pin)
{
	FScopedTransaction Transaction( LOCTEXT("RemovePinTx", "RemovePin") );
	Modify();

	check(Pin->Direction == EGPD_Input);

	int32 PinRemovalIndex = INDEX_NONE;
	if (Pins.Find(Pin, /*out*/ PinRemovalIndex))
	{
		for (int32 PinIndex = PinRemovalIndex + 1; PinIndex < Pins.Num(); ++PinIndex)
		{
			Pins[PinIndex]->Modify();
			Pins[PinIndex]->PinName = FString::Printf(TEXT("[%d]"), PinIndex - 2); // -1 to shift back one, -1 to account for the output pin at the 0th position
		}

		Pins.RemoveAt(PinRemovalIndex);
		Pin->Modify();
		Pin->BreakAllPinLinks();

		--NumInputs;

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprint());
	}
}

void UK2Node_MakeArray::GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const
{
	Super::GetContextMenuActions(Context);

	if (!Context.bIsDebugging)
	{
		Context.MenuBuilder->BeginSection("K2NodeMakeArray", NSLOCTEXT("K2Nodes", "MakeArrayHeader", "MakeArray"));

		if (Context.Pin != NULL)
		{
			// we only do this for normal BlendList/BlendList by enum, BlendList by Bool doesn't support add/remove pins
			if (Context.Pin->Direction == EGPD_Input)
			{
				//@TODO: Only offer this option on arrayed pins
				Context.MenuBuilder->AddMenuEntry(
					LOCTEXT("RemovePin", "Remove array element pin"),
					LOCTEXT("RemovePinTooltip", "Remove this array element pin"),
					FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateUObject(this, &UK2Node_MakeArray::RemoveInputPin, const_cast<UEdGraphPin*>(Context.Pin))
					)
				);
			}
		}
		else
		{
			Context.MenuBuilder->AddMenuEntry(
				LOCTEXT("AddPin", "Add array element pin"),
				LOCTEXT("AddPinTooltip", "Add another array element pin"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateUObject(this, &UK2Node_MakeArray::AddInputPin)
				)
			);
		}

		Context.MenuBuilder->AddMenuEntry(
			LOCTEXT("ResetToWildcard", "Reset to wildcard"),
			LOCTEXT("ResetToWildcardTooltip", "Reset the node to have wildcard input/outputs. Requires no pins are connected."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateUObject(this, &UK2Node_MakeArray::ClearPinTypeToWildcard),
				FCanExecuteAction::CreateUObject(this, &UK2Node_MakeArray::CanResetToWildcard)
			)
		);

		Context.MenuBuilder->EndSection();
	}
}

bool UK2Node_MakeArray::IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const
{
	if(OtherPin->PinType.bIsArray == true && MyPin->Direction == EGPD_Input)
	{
		OutReason = NSLOCTEXT("K2Node", "MakeArray_InputIsArray", "Cannot make an array with an input of an array!").ToString();
		return true;
	}

	auto Schema = Cast<const UEdGraphSchema_K2>(GetSchema());
	if (!ensure(Schema) || (ensure(OtherPin) && Schema->IsExecPin(*OtherPin)))
	{
		OutReason = NSLOCTEXT("K2Node", "MakeArray_InputIsExec", "Cannot make an array with an execution input!").ToString();
		return true;
	}

	return false;
}

void UK2Node_MakeArray::ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const
{
	auto Schema = Cast<const UEdGraphSchema_K2>(GetSchema());
	auto OutputPin = GetOutputPin();
	if (!ensure(Schema) || !ensure(OutputPin) || Schema->IsExecPin(*OutputPin))
	{
		MessageLog.Error(*NSLOCTEXT("K2Node", "MakeArray_OutputIsExec", "Uaccepted array type in @@").ToString(), this);
	}
}

void UK2Node_MakeArray::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	// actions get registered under specific object-keys; the idea is that 
	// actions might have to be updated (or deleted) if their object-key is  
	// mutated (or removed)... here we use the node's class (so if the node 
	// type disappears, then the action should go with it)
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

FText UK2Node_MakeArray::GetMenuCategory() const
{
	static FNodeTextCache CachedCategory;
	if (CachedCategory.IsOutOfDate(this))
	{
		// FText::Format() is slow, so we cache this to save on performance
		CachedCategory.SetCachedText(FEditorCategoryUtils::BuildCategoryString(FCommonEditorCategory::Utilities, LOCTEXT("ActionMenuCategory", "Array")), this);
	}
	return CachedCategory;
}

#undef LOCTEXT_NAMESPACE
