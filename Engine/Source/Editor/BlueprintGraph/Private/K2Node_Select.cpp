// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintGraphPrivatePCH.h"
#include "../../../Runtime/Engine/Classes/Kismet/KismetMathLibrary.h"
#include "../../../Runtime/Engine/Classes/Kismet/KismetSystemLibrary.h"
#include "KismetCompiler.h"
#include "BlueprintNodeSpawner.h"
#include "EditorCategoryUtils.h"
#include "BlueprintActionDatabaseRegistrar.h"

#define LOCTEXT_NAMESPACE "K2Node_Select"

//////////////////////////////////////////////////////////////////////////
// FKCHandler_Select

class FKCHandler_Select : public FNodeHandlingFunctor
{
protected:
	TMap<UEdGraphNode*, FBPTerminal*> BoolTermMap;

public:
	FKCHandler_Select(FKismetCompilerContext& InCompilerContext)
		: FNodeHandlingFunctor(InCompilerContext)
	{
	}

	virtual void RegisterNets(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		FNodeHandlingFunctor::RegisterNets(Context, Node);

		// Create the net for the return value manually as it's a special case Output Direction pin
		UK2Node_Select* SelectNode = Cast<UK2Node_Select>(Node);
		UEdGraphPin* ReturnPin = SelectNode->GetReturnValuePin();

		FBPTerminal* Term = Context.CreateLocalTerminalFromPinAutoChooseScope(ReturnPin, Context.NetNameMap->MakeValidName(ReturnPin));
		Context.NetMap.Add(SelectNode->GetReturnValuePin(), Term);

		// Create a term to determine if the compare was successful or not
		FBPTerminal* BoolTerm = Context.CreateLocalTerminal();
		BoolTerm->Type.PinCategory = CompilerContext.GetSchema()->PC_Boolean;
		BoolTerm->Source = Node;
		BoolTerm->Name = Context.NetNameMap->MakeValidName(Node) + TEXT("_CmpSuccess");
		BoolTermMap.Add(Node, BoolTerm);
	}

	virtual void Compile(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		// Cast the node and get all the input pins
		UK2Node_Select* SelectNode = Cast<UK2Node_Select>(Node);
		TArray<UEdGraphPin*> OptionPins;
		SelectNode->GetOptionPins(OptionPins);
		UEdGraphPin* IndexPin = SelectNode->GetIndexPin();

		// Get the kismet term for the (Condition or Index) that will determine which option to use
		UEdGraphPin* PinToTry = FEdGraphUtilities::GetNetFromPin(IndexPin);
		FBPTerminal** ConditionTerm = Context.NetMap.Find(PinToTry);

		// Get the kismet term for the return value
		UEdGraphPin* ReturnPin = SelectNode->GetReturnValuePin();
		FBPTerminal** ReturnTerm = Context.NetMap.Find(ReturnPin);

		// Don't proceed if there is no return value or there is no selection
		if (ConditionTerm != NULL && ReturnTerm != NULL)
		{
			FName ConditionalFunctionName = "";
			UClass* ConditionalFunctionClass = NULL;
			SelectNode->GetConditionalFunction(ConditionalFunctionName, &ConditionalFunctionClass);
			UFunction* ConditionFunction = FindField<UFunction>(ConditionalFunctionClass, ConditionalFunctionName);

			// Find the local boolean for use in the equality call function below (BoolTerm = result of EqualEqual_IntInt or NotEqual_BoolBool)
			FBPTerminal* BoolTerm = BoolTermMap.FindRef(SelectNode);

			// We need to keep a pointer to the previous IfNot statement so it can be linked to the next conditional statement
			FBlueprintCompiledStatement* PrevIfNotStatement = NULL;

			// Keep an array of all the unconditional goto statements so we can clean up their jumps after the noop statement is created
			TArray<FBlueprintCompiledStatement*> GotoStatementList;

			// Loop through all the options
			for (int32 OptionIdx = 0; OptionIdx < OptionPins.Num(); OptionIdx++)
			{
				// Create a CallFunction statement with the condition function from the Select class
				FBlueprintCompiledStatement& Statement = Context.AppendStatementForNode(Node);
				Statement.Type = KCST_CallFunction;
				Statement.FunctionToCall = ConditionFunction;
				Statement.FunctionContext = NULL;
				Statement.bIsParentContext = false;
				// BoolTerm will be the return value of the condition statement
				Statement.LHS = BoolTerm;
				// The condition passed into the Select node
				Statement.RHS.Add(*ConditionTerm);
				// Create a local int for use in the equality call function below (LiteralTerm = the right hand side of the EqualEqual_IntInt or NotEqual_BoolBool statement)
				FBPTerminal* LiteralTerm = Context.CreateLocalTerminal(ETerminalSpecification::TS_Literal);
				LiteralTerm->bIsLiteral = true;
				LiteralTerm->Type.PinCategory = CompilerContext.GetSchema()->PC_Int;
				LiteralTerm->Name = FString::Printf(TEXT("%d"), OptionIdx);
				Statement.RHS.Add(LiteralTerm);
				// If there is a previous IfNot statement, hook this one to that one for jumping
				if (PrevIfNotStatement)
				{
					Statement.bIsJumpTarget = true;
					PrevIfNotStatement->TargetLabel = &Statement;
				}

				// Create a GotoIfNot statement using the BoolTerm from above as the condition
				FBlueprintCompiledStatement* IfNotStatement = &Context.AppendStatementForNode(Node);
				IfNotStatement->Type = KCST_GotoIfNot;
				IfNotStatement->LHS = BoolTerm;

				// Create an assignment statement
				FBlueprintCompiledStatement& AssignStatement = Context.AppendStatementForNode(Node);
				AssignStatement.Type = KCST_Assignment;
				AssignStatement.LHS = *ReturnTerm;
				// Get the kismet term from the option pin
				UEdGraphPin* OptionPinToTry = FEdGraphUtilities::GetNetFromPin(OptionPins[OptionIdx]);
				FBPTerminal** OptionTerm = Context.NetMap.Find(OptionPinToTry);
				if (!OptionTerm)
				{
					Context.MessageLog.Error(*LOCTEXT("Error_UnregisterOptionPin", "Unregister option pin @@").ToString(), OptionPins[OptionIdx]);
					return;
				}
				AssignStatement.RHS.Add(*OptionTerm);

				// Create an unconditional goto to exit the node
				FBlueprintCompiledStatement& GotoStatement = Context.AppendStatementForNode(Node);
				GotoStatement.Type = KCST_UnconditionalGoto;
				GotoStatementList.Add(&GotoStatement);

				// If this is the last IfNot statement, hook the jump to an error message
				if (OptionIdx == OptionPins.Num() - 1)
				{
					// Create a CallFunction statement for doing a print string of our error message
					FBlueprintCompiledStatement& PrintStatement = Context.AppendStatementForNode(Node);
					PrintStatement.Type = KCST_CallFunction;
					PrintStatement.bIsJumpTarget = true;
					FName PrintStringFunctionName = "";
					UClass* PrintStringFunctionClass = NULL;
					SelectNode->GetPrintStringFunction(PrintStringFunctionName, &PrintStringFunctionClass);
					UFunction* PrintFunction = FindField<UFunction>(PrintStringFunctionClass, PrintStringFunctionName);
					PrintStatement.FunctionToCall = PrintFunction;
					PrintStatement.FunctionContext = NULL;
					PrintStatement.bIsParentContext = false;

					// Create a local int for use in the equality call function below (LiteralTerm = the right hand side of the EqualEqual_IntInt or NotEqual_BoolBool statement)
					FBPTerminal* LiteralStringTerm = Context.CreateLocalTerminal(ETerminalSpecification::TS_Literal);
					LiteralStringTerm->bIsLiteral = true;
					LiteralStringTerm->Type.PinCategory = CompilerContext.GetSchema()->PC_String;

					FString SelectionNodeType(TEXT("NONE"));
					if (IndexPin)
					{
						UEnum* EnumObject = Cast<UEnum>(IndexPin->PinType.PinSubCategoryObject.Get());
						if (EnumObject != NULL)
						{
							SelectionNodeType = EnumObject->GetName();
						}
						else
						{
							// Not an enum, so just use the basic type
							SelectionNodeType = IndexPin->PinType.PinCategory;
						}
					}

					const UEdGraph* OwningGraph = Context.MessageLog.FindSourceObjectTypeChecked<UEdGraph>( SelectNode->GetGraph() );
					LiteralStringTerm->Name =
						FString::Printf(*LOCTEXT("SelectNodeIndexWarning", "Graph %s: Selection Node of type %s failed! Out of bounds indexing of the options. There are only %d options available.").ToString(),
						(OwningGraph) ? *OwningGraph->GetFullName() : TEXT("NONE"),
						*SelectionNodeType,
						OptionPins.Num());
					PrintStatement.RHS.Add(LiteralStringTerm);

					// Hook the IfNot statement's jump target to this statement
					IfNotStatement->TargetLabel = &PrintStatement;
				}

				PrevIfNotStatement = IfNotStatement;
			}

			// Create a noop to jump to so the unconditional goto statements can exit the node after successful assignment
			FBlueprintCompiledStatement& NopStatement = Context.AppendStatementForNode(Node);
			NopStatement.Type = KCST_Nop;
			NopStatement.bIsJumpTarget = true;
			// Loop through the unconditional goto statements and fix their jump targets
			for (auto It = GotoStatementList.CreateConstIterator(); It; It++)
			{
				(*It)->TargetLabel = &NopStatement;
			}
		}
	}
};

UK2Node_Select::UK2Node_Select(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

	NumOptionPins = 2;

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	IndexPinType.PinCategory = Schema->PC_Wildcard;
	IndexPinType.PinSubCategory = Schema->PSC_Index;
	IndexPinType.PinSubCategoryObject = NULL;
}

void UK2Node_Select::AllocateDefaultPins()
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	// To refresh, just in case it changed
	SetEnum(Enum, true);

	if (Enum)
	{
		NumOptionPins = EnumEntries.Num();
	}

	// Create the option pins
	for (int32 Idx = 0; Idx < NumOptionPins; Idx++)
	{
		UEdGraphPin* NewPin = NULL;

		if (Enum)
		{
			const FString PinName = EnumEntries[Idx].ToString();
			UEdGraphPin* TempPin = FindPin(PinName);
			if (!TempPin)
			{
				NewPin = CreatePin(EGPD_Input, Schema->PC_Wildcard, TEXT(""), NULL, false, false, PinName);
			}
		}
		else
		{
			const FString PinName = FString::Printf(TEXT("Option %d"), Idx);
			NewPin = CreatePin(EGPD_Input, Schema->PC_Wildcard, TEXT(""), NULL, false, false, PinName);
		}

		if (NewPin)
		{
			if (IndexPinType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
			{
				NewPin->PinFriendlyName = (Idx == 0 ? GFalse : GTrue);
			}
			else if (Idx < EnumEntryFriendlyNames.Num())
			{
				if (EnumEntryFriendlyNames[Idx] != NAME_None)
				{
					NewPin->PinFriendlyName = FText::FromName(EnumEntryFriendlyNames[Idx]);
				}
				else
				{
					NewPin->PinFriendlyName = FText::GetEmpty();
				}
			}
		}
	}

	// Create the index wildcard pin
	CreatePin(EGPD_Input, IndexPinType.PinCategory, IndexPinType.PinSubCategory, IndexPinType.PinSubCategoryObject.Get(), false, false, "Index");

	// Create the return value
	CreatePin(EGPD_Output, Schema->PC_Wildcard, TEXT(""), NULL, false, false, Schema->PN_ReturnValue);

	Super::AllocateDefaultPins();
}

void UK2Node_Select::AutowireNewNode(UEdGraphPin* FromPin)
{
	if (FromPin)
	{
		// Attempt to autowire to the index pin as users generally drag off of something intending to use
		// it as an index in a select statement rather than an arbitrary entry:
		const UEdGraphSchema_K2* K2Schema = CastChecked<UEdGraphSchema_K2>(GetSchema());
		UEdGraphPin* IndexPin = GetIndexPin();
		ECanCreateConnectionResponse ConnectResponse = K2Schema->CanCreateConnection(FromPin, IndexPin).Response;
		if (ConnectResponse == ECanCreateConnectionResponse::CONNECT_RESPONSE_MAKE)
		{
			if (K2Schema->TryCreateConnection(FromPin, IndexPin))
			{
				FromPin->GetOwningNode()->NodeConnectionListChanged();
				this->NodeConnectionListChanged();
				return;
			}
		}
	}

	// No connection made, just use default autowire logic:
	Super::AutowireNewNode(FromPin);
}

FText UK2Node_Select::GetTooltipText() const
{
	return LOCTEXT("SelectNodeTooltip", "Return the option at Index, (first option is indexed at 0)");
}

FText UK2Node_Select::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Select", "Select");
}

UK2Node::ERedirectType UK2Node_Select::DoPinsMatchForReconstruction(const UEdGraphPin* NewPin, int32 NewPinIndex, const UEdGraphPin* OldPin, int32 OldPinIndex) const
{
	// Check to see if the new pin name matches the old pin name (case insensitive - since the base uses Stricmp() to compare pin names, we also ignore case here).
	if(Enum != nullptr && NewPinIndex < NumOptionPins && !NewPin->PinName.Equals(OldPin->PinName, ESearchCase::IgnoreCase))
	{
		// The names don't match, so check for an enum redirect from the old pin name.
		int32 EnumIndex = UEnum::FindEnumRedirects(Enum, FName(*OldPin->PinName));
		if(EnumIndex != INDEX_NONE)
		{
			// Found a redirect. Attempt to match it to the new pin name.
			FString NewPinName = Enum->GetEnumName(EnumIndex);
			if(NewPinName.Equals(NewPin->PinName, ESearchCase::IgnoreCase))
			{
				// The redirect is a match, so we can reconstruct this pin using the old pin's state.
				return UK2Node::ERedirectType_Name;
			}
		}
	}

	// Fall back to base class functionality for all other cases.
	return Super::DoPinsMatchForReconstruction(NewPin, NewPinIndex, OldPin, OldPinIndex);
}

void UK2Node_Select::ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins)
{
	Super::ReallocatePinsDuringReconstruction(OldPins);

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	// See if this node was saved in the old version with a boolean as the condition
	UEdGraphPin* OldConditionPin = NULL;
	UEdGraphPin* OldIndexPin = NULL;
	for (auto It = OldPins.CreateConstIterator(); It; It++)
	{
		if ((*It)->PinName == TEXT("bPickOption0"))
		{
			OldConditionPin = (*It);
		}
		else if ((*It)->PinName == TEXT("Index"))
		{
			OldIndexPin = (*It);
		}
	}

	UEdGraphPin* IndexPin = GetIndexPin();

	// If we are fixing up an old bool node (swap the options and copy the condition links)
	if (OldConditionPin)
	{
		// Set the index pin type
		IndexPinType.PinCategory = Schema->PC_Boolean;
		IndexPinType.PinSubCategory = TEXT("");
		IndexPinType.PinSubCategoryObject = NULL;

		// Set the pin type and Copy the pin
		IndexPin->PinType = IndexPinType;
		Schema->CopyPinLinks(*OldConditionPin, *IndexPin);
		// If we copy links, we need to send a notification
		if (IndexPin->LinkedTo.Num() > 0)
		{
			PinConnectionListChanged(IndexPin);
		}

		UEdGraphPin* OptionPin0 = FindPin("Option 0");
		UEdGraphPin* OptionPin1 = FindPin("Option 1");

		for (auto It = OldPins.CreateConstIterator(); It; It++)
		{
			UEdGraphPin* OldPin = (*It);
			if (OldPin->PinName == OptionPin0->PinName)
			{
				Schema->MovePinLinks(*OldPin, *OptionPin1);
			}
			else if (OldPin->PinName == OptionPin1->PinName)
			{
				Schema->MovePinLinks(*OldPin, *OptionPin0);
			}
		}
	}

	// If the index pin has links or a default value but is a wildcard, this is an old int pin so convert it
	if (OldIndexPin &&
		IndexPinType.PinCategory == Schema->PC_Wildcard &&
		(OldIndexPin->LinkedTo.Num() > 0 || OldIndexPin->DefaultValue != TEXT("")))
	{
		IndexPinType.PinCategory = Schema->PC_Int;
		IndexPinType.PinSubCategory = TEXT("");
		IndexPinType.PinSubCategoryObject = NULL;
		IndexPin->PinType = IndexPinType;
	}
}

void UK2Node_Select::PostReconstructNode()
{
	bReconstructNode = false;

	const UEdGraphSchema_K2* Schema = Cast<UEdGraphSchema_K2>(GetSchema());

	UEdGraphPin* ReturnPin = GetReturnValuePin();
	PinConnectionListChanged(ReturnPin);
	const bool bFillTypeFromReturn = Schema && ReturnPin && (ReturnPin->PinType.PinCategory != Schema->PC_Wildcard);

	TArray<UEdGraphPin*> OptionPins;
	GetOptionPins(OptionPins);
	for (auto It = OptionPins.CreateConstIterator(); It; It++)
	{
		UEdGraphPin* Pin = *It;
		const bool bTypeShouldBeFilled = Schema && Pin && (Pin->PinType.PinCategory == Schema->PC_Wildcard);
		if (bTypeShouldBeFilled && bFillTypeFromReturn)
		{
			Pin->Modify();
			Pin->PinType = ReturnPin->PinType;
			UEdGraphSchema_K2::ValidateExistingConnections(Pin);
		}

		PinConnectionListChanged(*It);
	}

	//After ReconstructNode we must be sure, that no additional reconstruction is required
	bReconstructNode = false;
}

/** Determine if any pins are connected, if so make all the other pins the same type, if not, make sure pins are switched back to wildcards */
void UK2Node_Select::NotifyPinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::NotifyPinConnectionListChanged(Pin);

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	// If this is the Enum pin we need to set the enum and reconstruct the node
	if (Pin == GetIndexPin())
	{
		// If the index pin was just linked to another pin
		if (Pin->LinkedTo.Num() > 0)
		{
			UEdGraphPin* LinkPin = Pin->LinkedTo[0];
			IndexPinType = LinkPin->PinType;
			Pin->PinType = IndexPinType;

			// See if it was an enum pin
			if (LinkPin->PinType.PinCategory == Schema->PC_Byte &&
				LinkPin->PinType.PinSubCategoryObject != NULL &&
				LinkPin->PinType.PinSubCategoryObject->IsA(UEnum::StaticClass()))
			{
				UEnum* EnumPtr = Cast<UEnum>(LinkPin->PinType.PinSubCategoryObject.Get());
				SetEnum(EnumPtr);
			}
			else
			{
				SetEnum(NULL);
			}

			Schema->SetPinDefaultValueBasedOnType(Pin);

			GetGraph()->NotifyGraphChanged();
			UBlueprint* Blueprint = GetBlueprint();
			if(!Blueprint->bBeingCompiled)
			{
				FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
				Blueprint->BroadcastChanged();
			}

			// If the index pin is a boolean, we need to remove all but 2 options
			if (IndexPinType.PinCategory == Schema->PC_Boolean &&
				NumOptionPins != 2)
			{
				NumOptionPins = 2;
				bReconstructNode = true;
			}
		}
	}
	else
	{
		// Grab references to all option pins and the return pin
		TArray<UEdGraphPin*> OptionPins;
		GetOptionPins(OptionPins);
		UEdGraphPin* ReturnPin = FindPin(Schema->PN_ReturnValue);

		// See if this pin is one of the wildcard pins
		bool bIsWildcardPin = (Pin == ReturnPin || OptionPins.Find(Pin) != INDEX_NONE) && Pin->PinType.PinCategory == Schema->PC_Wildcard;

		// If the pin was one of the wildcards we have to handle it specially
		if (bIsWildcardPin)
		{
			// If the pin is linked, make sure the other wildcard pins match
			if (Pin->LinkedTo.Num() > 0)
			{
				// Set pin type on the pin
				Pin->PinType = Pin->LinkedTo[0]->PinType;

				// Make sure the return pin is the same pin type
				if (ReturnPin != Pin)
				{
					ReturnPin->Modify();

					ReturnPin->PinType = Pin->PinType;
					UEdGraphSchema_K2::ValidateExistingConnections(ReturnPin);
				}

				// Make sure all options are of the same pin type
				for (auto It = OptionPins.CreateConstIterator(); It; It++)
				{
					UEdGraphPin* OptionPin = (*It);
					if (*It && *It != Pin)
					{
						(*It)->Modify();

						(*It)->PinType = Pin->PinType;
						UEdGraphSchema_K2::ValidateExistingConnections(*It);
					}
				}

				bReconstructNode = true;
			}
		}
	}
}

UEdGraphPin* UK2Node_Select::GetReturnValuePin() const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	UEdGraphPin* Pin = FindPin(K2Schema->PN_ReturnValue);
	check(Pin != NULL);
	return Pin;
}

UEdGraphPin* UK2Node_Select::GetIndexPin() const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	UEdGraphPin* Pin = FindPin("Index");
	check(Pin != NULL);
	return Pin;
}

void UK2Node_Select::GetOptionPins(TArray<UEdGraphPin*>& OptionPins) const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	OptionPins.Empty();

	// If the select node is currently dealing with an enum
	if (IndexPinType.PinCategory == K2Schema->PC_Byte &&
		IndexPinType.PinSubCategory == TEXT("") &&
		IndexPinType.PinSubCategoryObject != NULL &&
		IndexPinType.PinSubCategoryObject->IsA(UEnum::StaticClass()))
	{
		for (auto It = Pins.CreateConstIterator(); It; It++)
		{
			UEdGraphPin* Pin = (*It);
			if (EnumEntries.Contains(FName(*Pin->PinName)))
			{
				OptionPins.Add(Pin);
			}
		}
	}
	else
	{
		for (auto It = Pins.CreateConstIterator(); It; It++)
		{
			UEdGraphPin* Pin = (*It);
			if (Pin->PinName.Left(6) == "Option")
			{
				OptionPins.Add(Pin);
			}
		}
	}
}

void UK2Node_Select::GetConditionalFunction(FName& FunctionName, UClass** FunctionClass)
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	if (IndexPinType.PinCategory == K2Schema->PC_Boolean)
	{
		FunctionName = GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, EqualEqual_BoolBool);
	}
	else if (IndexPinType.PinCategory == K2Schema->PC_Byte)
	{
		FunctionName = GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, EqualEqual_ByteByte);
	}
	else if (IndexPinType.PinCategory == K2Schema->PC_Int)
	{
		FunctionName = GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, EqualEqual_IntInt);
	}

	*FunctionClass = UKismetMathLibrary::StaticClass();
}

void UK2Node_Select::GetPrintStringFunction(FName& FunctionName, UClass** FunctionClass)
{
	FunctionName = GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, PrintWarning);
	*FunctionClass = UKismetSystemLibrary::StaticClass();
}

void UK2Node_Select::AddOptionPinToNode()
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	// Increment the pin count
	NumOptionPins++;
	// We guarantee at least 2 options by default and since we just increased the count
	// to more than 2, we need to make sure we're now dealing with an index for selection
	// instead of the default boolean check
	if (IndexPinType.PinCategory == K2Schema->PC_Boolean)
	{
		IndexPinType.PinCategory = K2Schema->PC_Int;
		GetIndexPin()->BreakAllPinLinks();
	}
	// We will let the AllocateDefaultPins call handle the actual addition via ReconstructNode
	ReconstructNode();
}

void UK2Node_Select::RemoveOptionPinToNode()
{
	// Increment the pin count
	NumOptionPins--;
	// We will let the AllocateDefaultPins call handle the actual subtraction via ReconstructNode
	ReconstructNode();
}

void UK2Node_Select::SetEnum(UEnum* InEnum, bool bForceRegenerate)
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	UEnum* PrevEnum = Enum;
	Enum = InEnum;

	if ((PrevEnum != Enum) || bForceRegenerate)
	{
		// regenerate enum name list
		EnumEntries.Empty();
		EnumEntryFriendlyNames.Empty();

		if (Enum)
		{
			for (int32 EnumIndex = 0; EnumIndex < Enum->NumEnums() - 1; ++EnumIndex)
			{
				bool const bShouldBeHidden = Enum->HasMetaData(TEXT("Hidden"), EnumIndex ) || Enum->HasMetaData(TEXT("Spacer"), EnumIndex );
				if (!bShouldBeHidden)
				{
					FString EnumValueName = Enum->GetEnumName(EnumIndex);
					FString EnumFriendlyName = Enum->GetDisplayNameText(EnumIndex).ToString();
					EnumEntries.Add(FName(*EnumValueName));
					EnumEntryFriendlyNames.Add(FName(*EnumFriendlyName));
				}
			}
		}

		bReconstructNode = true;
	}
}

void UK2Node_Select::NodeConnectionListChanged()
{
	Super::NodeConnectionListChanged();

	if (bReconstructNode)
	{
		ReconstructNode();

		UBlueprint* Blueprint = GetBlueprint();
		if(!Blueprint->bBeingCompiled)
		{
			FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
			Blueprint->BroadcastChanged();
		}
	}
}

bool UK2Node_Select::CanAddOptionPinToNode() const
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	if (IndexPinType.PinCategory == Schema->PC_Byte &&
		IndexPinType.PinSubCategoryObject.IsValid() &&
		IndexPinType.PinSubCategoryObject.Get()->IsA(UEnum::StaticClass()))
	{
		return false;
	}
	else if (IndexPinType.PinCategory == Schema->PC_Boolean)
	{
		return false;
	}

	return true;
}

bool UK2Node_Select::CanRemoveOptionPinToNode() const
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	if (IndexPinType.PinCategory == Schema->PC_Byte &&
		(NULL != Cast<UEnum>(IndexPinType.PinSubCategoryObject.Get())))
	{
		return false;
	}
	else if (IndexPinType.PinCategory == Schema->PC_Boolean)
	{
		return false;
	}

	return true;
}

void UK2Node_Select::ChangePinType(UEdGraphPin* Pin)
{
	PinTypeChanged(Pin);
}

bool UK2Node_Select::CanChangePinType(UEdGraphPin* Pin) const
{
	// If this is the index pin, only allow type switching if nothing is linked to the pin
	if (Pin == GetIndexPin())
	{
		if (Pin->LinkedTo.Num() > 0)
		{
			return false;
		}
	}
	// Else it's one of the wildcard pins that share their type, so make sure none of them have a link
	else
	{
		if (GetReturnValuePin()->LinkedTo.Num() > 0)
		{
			return false;
		}
		else
		{
			TArray<UEdGraphPin*> OptionPins;
			GetOptionPins(OptionPins);
			for (auto It = OptionPins.CreateConstIterator(); It; It++)
			{
				UEdGraphPin* OptionPin = (*It);
				if (OptionPin && OptionPin->LinkedTo.Num() > 0)
				{
					return false;
				}
			}
		}
	}
	return true;
}

void UK2Node_Select::PinTypeChanged(UEdGraphPin* Pin)
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	if (Pin == GetIndexPin())
	{
		if (IndexPinType != Pin->PinType)
		{
			IndexPinType = Pin->PinType;

			if (IndexPinType.PinSubCategoryObject.IsValid())
			{
				SetEnum(Cast<UEnum>(IndexPinType.PinSubCategoryObject.Get()));
			}
			else if (Enum)
			{
				SetEnum(NULL);
			}

			// Remove all but two options if we switched to a bool index
			if (IndexPinType.PinCategory == Schema->PC_Boolean)
			{
				if (NumOptionPins > 2)
				{
					NumOptionPins = 2;
					bReconstructNode = true;
				}
			}

			// Reset the default value
			Schema->SetPinDefaultValueBasedOnType(Pin);
		}
	}
	else
	{
		// Set the return value
		UEdGraphPin* ReturnPin = GetReturnValuePin();
		if (ReturnPin->PinType != Pin->PinType)
		{
			ReturnPin->PinType = Pin->PinType;
			Schema->SetPinDefaultValueBasedOnType(ReturnPin);
		}

		// Set the options
		TArray<UEdGraphPin*> OptionPins;
		GetOptionPins(OptionPins);
		for (auto It = OptionPins.CreateConstIterator(); It; It++)
		{
			UEdGraphPin* OptionPin = (*It);
			if (OptionPin->PinType != Pin->PinType ||
				OptionPin == Pin)
			{
				OptionPin->PinType = Pin->PinType;
				Schema->SetPinDefaultValueBasedOnType(OptionPin);
			}
		}
	}


	// Reconstruct the node since the options could change
	if (bReconstructNode)
	{
		ReconstructNode();
	}

	// Let the graph know to refresh
	GetGraph()->NotifyGraphChanged();

	UBlueprint* Blueprint = GetBlueprint();
	if(!Blueprint->bBeingCompiled)
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		Blueprint->BroadcastChanged();
	}
}

void UK2Node_Select::PostPasteNode()
{
	Super::PostPasteNode();

	UEdGraphPin* IndexPin = GetIndexPin();

	// This information will be cleared and we want to restore it
	FString OldDefaultValue = IndexPin->DefaultValue;

	// Corrects data in the index pin that is not valid after pasting
	PinTypeChanged(GetIndexPin());

	// Restore the default value of the index pin
	IndexPin->DefaultValue = OldDefaultValue;
}

bool UK2Node_Select::IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	if (OtherPin && (OtherPin->PinType.PinCategory == K2Schema->PC_Exec))
	{
		OutReason = LOCTEXT("ExecConnectionDisallowed", "Cannot connect with Exec pin.").ToString();
		return true;
	}

	return Super::IsConnectionDisallowed(MyPin, OtherPin, OutReason);
}

FNodeHandlingFunctor* UK2Node_Select::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_Select(CompilerContext);
}

void UK2Node_Select::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
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

FText UK2Node_Select::GetMenuCategory() const
{
	return FEditorCategoryUtils::GetCommonCategory(FCommonEditorCategory::Utilities);
}

#undef LOCTEXT_NAMESPACE
