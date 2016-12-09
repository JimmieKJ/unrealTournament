// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "K2Node_GetArrayItem.h"
#include "Engine/Blueprint.h"
#include "EdGraphSchema_K2.h"

#include "EdGraphUtilities.h"
#include "BPTerminal.h"
#include "BlueprintCompiledStatement.h"
#include "KismetCompiledFunctionContext.h"
#include "KismetCompilerMisc.h"

#define LOCTEXT_NAMESPACE "GetArrayItem"

/*******************************************************************************
*  FKCHandler_GetArrayItem
******************************************************************************/
class FKCHandler_GetArrayItem : public FNodeHandlingFunctor
{
public:
	FKCHandler_GetArrayItem(FKismetCompilerContext& InCompilerContext)
		: FNodeHandlingFunctor(InCompilerContext)
	{
	}

	virtual void RegisterNets(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		UK2Node_GetArrayItem* ArrayNode = CastChecked<UK2Node_GetArrayItem>(Node);

		// return inline term
		if (Context.NetMap.Find(Node->Pins[2]))
		{
			Context.MessageLog.Error(*LOCTEXT("Error_ReturnTermAlreadyRegistered", "ICE: Return term is already registered @@").ToString(), Node);
			return;
		}

		{
			FBPTerminal* Term = new (Context.InlineGeneratedValues) FBPTerminal();
			Term->CopyFromPin(Node->Pins[2], Context.NetNameMap->MakeValidName(Node->Pins[2]));
			Context.NetMap.Add(Node->Pins[2], Term);
		}

		FNodeHandlingFunctor::RegisterNets(Context, Node);
	}

	virtual void Compile(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		UK2Node_GetArrayItem* ArrayNode = CastChecked<UK2Node_GetArrayItem>(Node);

		FBlueprintCompiledStatement* SelectStatementPtr = new FBlueprintCompiledStatement();
		FBlueprintCompiledStatement& ArrayGetFunction = *SelectStatementPtr;
		ArrayGetFunction.Type = KCST_ArrayGetByRef;

		UEdGraphPin* ArrayPinNet = FEdGraphUtilities::GetNetFromPin(Node->Pins[0]);
		UEdGraphPin* ReturnValueNet = FEdGraphUtilities::GetNetFromPin(Node->Pins[2]);

		FBPTerminal** ArrayTerm = Context.NetMap.Find(ArrayPinNet);

		UEdGraphPin* IndexPin = ArrayNode->GetIndexPin();
		check(IndexPin);
		UEdGraphPin* IndexPinNet = FEdGraphUtilities::GetNetFromPin(IndexPin);
		FBPTerminal** IndexTermPtr = Context.NetMap.Find(IndexPinNet);

		FBPTerminal** ReturnValue = Context.NetMap.Find(ReturnValueNet);
		FBPTerminal** ReturnValueOrig = Context.NetMap.Find(Node->Pins[2]);
		ArrayGetFunction.RHS.Add(*ArrayTerm);
		ArrayGetFunction.RHS.Add(*IndexTermPtr);

		(*ReturnValue)->InlineGeneratedParameter = &ArrayGetFunction;
	}
};

/*******************************************************************************
*  UK2Node_GetArrayItem
******************************************************************************/
UK2Node_GetArrayItem::UK2Node_GetArrayItem(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UK2Node_GetArrayItem::AllocateDefaultPins()
{
	// Create the output pin
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Wildcard, TEXT(""), NULL, true, false, TEXT("Array"));
	UEdGraphPin* IndexPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Int, TEXT(""), NULL, false, false, TEXT("Dimension 1"));
	GetDefault<UEdGraphSchema_K2>()->SetPinDefaultValueBasedOnType(IndexPin);

	// Create the input pins to create the arrays from
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Wildcard, TEXT(""), NULL, false, true, TEXT("Output"));
}

void UK2Node_GetArrayItem::PostReconstructNode()
{
	if (GetTargetArrayPin()->LinkedTo.Num() > 0)
	{
		PropagatePinType(GetTargetArrayPin()->LinkedTo[0]->PinType);
	}
	else if (GetResultPin()->LinkedTo.Num() > 0)
	{
		PropagatePinType(GetResultPin()->LinkedTo[0]->PinType);
	}
}

FText UK2Node_GetArrayItem::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (TitleType == ENodeTitleType::FullTitle)
	{
		return LOCTEXT("GetArrayItemByRef_FullTitle", "GET");
	}
	return LOCTEXT("GetArrayItemByRef", "Get");
}

class FNodeHandlingFunctor* UK2Node_GetArrayItem::CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_GetArrayItem(CompilerContext);
}

void UK2Node_GetArrayItem::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	// temporarily don't register this slick new node with the database, gotta figure out how to provide old
	// (get by value) behavior alongside new (get by ref) behavior
}

void UK2Node_GetArrayItem::NotifyPinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::NotifyPinConnectionListChanged(Pin);

	if (Pin != GetIndexPin() && Pin->ParentPin == nullptr)
	{
		UEdGraphPin* ArrayPin  = Pins[0];
		UEdGraphPin* ResultPin = Pins[2];

		auto ClearWildcardType = [ArrayPin, ResultPin]()
		{
			ArrayPin->PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
			ArrayPin->PinType.PinSubCategory = TEXT("");
			ArrayPin->PinType.PinSubCategoryObject = NULL;

			ResultPin->PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
			ResultPin->PinType.PinSubCategory = TEXT("");
			ResultPin->PinType.PinSubCategoryObject = NULL;
			ResultPin->PinType.bIsReference = true;

			ArrayPin->BreakAllPinLinks();
			ResultPin->BreakAllPinLinks();
		};

		const int32 NewLinkCount  = Pin->LinkedTo.Num();
		const bool  bPinsHasLinks = (NewLinkCount > 0);
		if (ArrayPin == Pin)
		{
			if (bPinsHasLinks)
			{
				// if we had more than one input, we'd have to find the common base type
				ensure(NewLinkCount == 1); 
				// the input array has authority, change output types, even if they are connected
				PropagatePinType(Pin->LinkedTo[0]->PinType);
			}
			// if the array pin was disconnected from everything, and...
			else if (ResultPin->LinkedTo.Num() == 0)
			{
				ClearWildcardType();
			}
		}
		else if (ArrayPin->LinkedTo.Num() == 0)
		{
			// if we cleared the result pin's connections
			if (!bPinsHasLinks)
			{
				ClearWildcardType();
			}
			// if this is the first connection to the result pin...
			else if (NewLinkCount == 1)
			{
				PropagatePinType(Pin->LinkedTo[0]->PinType);
			}
			// else, the result pin already had a connection and a type, leave 
			// it alone, as it is what facilitated this connection as well
		}
		// else, leave this node alone, the array input is still connected and 
		// it has authority over the wildcard types (the type set should already be good)
	}
}

void UK2Node_GetArrayItem::PropagatePinType(FEdGraphPinType& InType)
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
	UEdGraphPin* ArrayPin = Pins[0];
	UEdGraphPin* ResultPin = Pins[2];

	ArrayPin->PinType = InType;
	ArrayPin->PinType.bIsArray = true;
	ArrayPin->PinType.bIsReference = false;

	ResultPin->PinType = InType;
	ResultPin->PinType.bIsArray = false;
	ResultPin->PinType.bIsReference = !(ResultPin->PinType.PinCategory == Schema->PC_Object || ResultPin->PinType.PinCategory == Schema->PC_Class || ResultPin->PinType.PinCategory == Schema->PC_Asset || ResultPin->PinType.PinCategory == Schema->PC_AssetClass || ResultPin->PinType.PinCategory == Schema->PC_Interface);
	ResultPin->PinType.bIsConst = false;


	// Verify that all previous connections to this pin are still valid with the new type
	for (TArray<UEdGraphPin*>::TIterator ConnectionIt(ArrayPin->LinkedTo); ConnectionIt; ++ConnectionIt)
	{
		UEdGraphPin* ConnectedPin = *ConnectionIt;
		if (!Schema->ArePinsCompatible(ArrayPin, ConnectedPin, CallingContext))
		{
			ArrayPin->BreakLinkTo(ConnectedPin);
		}
	}

	// Verify that all previous connections to this pin are still valid with the new type
	for (TArray<UEdGraphPin*>::TIterator ConnectionIt(ResultPin->LinkedTo); ConnectionIt; ++ConnectionIt)
	{
		UEdGraphPin* ConnectedPin = *ConnectionIt;
		if (!Schema->ArePinsCompatible(ResultPin, ConnectedPin, CallingContext))
		{
			ResultPin->BreakLinkTo(ConnectedPin);
		}
	}
}

#undef LOCTEXT_NAMESPACE
