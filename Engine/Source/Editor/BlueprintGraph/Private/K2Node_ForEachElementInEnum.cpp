// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "BlueprintGraphPrivatePCH.h"
#include "KismetCompiler.h"
#include "Kismet/KismetNodeHelperLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "K2Node_CastByteToEnum.h"
#include "K2Node_ForEachElementInEnum.h"
#include "K2Node_GetNumEnumEntries.h"
#include "BlueprintFieldNodeSpawner.h"
#include "EditorCategoryUtils.h"
#include "BlueprintActionDatabaseRegistrar.h"

#define LOCTEXT_NAMESPACE "K2Node"

struct FForExpandNodeHelper
{
	UEdGraphPin* StartLoopExecInPin;
	UEdGraphPin* InsideLoopExecOutPin;
	UEdGraphPin* LoopCompleteOutExecPin;

	UEdGraphPin* IndexOutPin;
	// for(Index = 0; Index < IndexLimit; ++Index)
	UEdGraphPin* IndexLimitInPin;

	FForExpandNodeHelper()
		: StartLoopExecInPin(NULL)
		, InsideLoopExecOutPin(NULL)
		, LoopCompleteOutExecPin(NULL)
		, IndexOutPin(NULL)
		, IndexLimitInPin(NULL)
	{ }

	bool BuildLoop(UK2Node* Node, FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
	{
		const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();
		check(Node && SourceGraph && Schema);

		bool bResult = true;

		// Create int Iterator
		UK2Node_TemporaryVariable* IndexNode = CompilerContext.SpawnIntermediateNode<UK2Node_TemporaryVariable>(Node, SourceGraph);
		IndexNode->VariableType.PinCategory = Schema->PC_Int;
		IndexNode->AllocateDefaultPins();
		IndexOutPin = IndexNode->GetVariablePin();
		check(IndexOutPin);

		// Initialize iterator
		UK2Node_AssignmentStatement* IteratorInitialize = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(Node, SourceGraph);
		IteratorInitialize->AllocateDefaultPins();
		IteratorInitialize->GetValuePin()->DefaultValue = TEXT("0");
		bResult &= Schema->TryCreateConnection(IndexOutPin, IteratorInitialize->GetVariablePin());
		StartLoopExecInPin = IteratorInitialize->GetExecPin();
		check(StartLoopExecInPin);

		// Do loop branch
		UK2Node_IfThenElse* Branch = CompilerContext.SpawnIntermediateNode<UK2Node_IfThenElse>(Node, SourceGraph);
		Branch->AllocateDefaultPins();
		bResult &= Schema->TryCreateConnection(IteratorInitialize->GetThenPin(), Branch->GetExecPin());
		LoopCompleteOutExecPin = Branch->GetElsePin();
		check(LoopCompleteOutExecPin);

		// Do loop condition
		UK2Node_CallFunction* Condition = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(Node, SourceGraph); 
		Condition->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(TEXT("Less_IntInt")));
		Condition->AllocateDefaultPins();
		bResult &= Schema->TryCreateConnection(Condition->GetReturnValuePin(), Branch->GetConditionPin());
		bResult &= Schema->TryCreateConnection(Condition->FindPinChecked(TEXT("A")), IndexOutPin);
		IndexLimitInPin = Condition->FindPinChecked(TEXT("B"));
		check(IndexLimitInPin);

		// body sequence
		UK2Node_ExecutionSequence* Sequence = CompilerContext.SpawnIntermediateNode<UK2Node_ExecutionSequence>(Node, SourceGraph);
		Sequence->AllocateDefaultPins();
		bResult &= Schema->TryCreateConnection(Branch->GetThenPin(), Sequence->GetExecPin());
		InsideLoopExecOutPin = Sequence->GetThenPinGivenIndex(0);
		check(InsideLoopExecOutPin);

		// Iterator increment
		UK2Node_CallFunction* Increment = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(Node, SourceGraph); 
		Increment->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(TEXT("Add_IntInt")));
		Increment->AllocateDefaultPins();
		bResult &= Schema->TryCreateConnection(Increment->FindPinChecked(TEXT("A")), IndexOutPin);
		Increment->FindPinChecked(TEXT("B"))->DefaultValue = TEXT("1");

		// Iterator assigned
		UK2Node_AssignmentStatement* IteratorAssign = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(Node, SourceGraph);
		IteratorAssign->AllocateDefaultPins();
		bResult &= Schema->TryCreateConnection(IteratorAssign->GetExecPin(), Sequence->GetThenPinGivenIndex(1));
		bResult &= Schema->TryCreateConnection(IteratorAssign->GetVariablePin(), IndexOutPin);
		bResult &= Schema->TryCreateConnection(IteratorAssign->GetValuePin(), Increment->GetReturnValuePin());
		bResult &= Schema->TryCreateConnection(IteratorAssign->GetThenPin(), Branch->GetExecPin());

		return bResult;
	}
};

UK2Node_ForEachElementInEnum::UK2Node_ForEachElementInEnum(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

const FString UK2Node_ForEachElementInEnum::InsideLoopPinName(TEXT("LoopBody"));
const FString UK2Node_ForEachElementInEnum::EnumOuputPinName(TEXT("EnumValue"));

void UK2Node_ForEachElementInEnum::AllocateDefaultPins()
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	check(K2Schema);

	CreatePin(EGPD_Input, K2Schema->PC_Exec, TEXT(""), NULL, false, false, K2Schema->PN_Execute);

	if (Enum)
	{
		CreatePin(EGPD_Output, K2Schema->PC_Exec, TEXT(""), NULL, false, false, InsideLoopPinName);
		CreatePin(EGPD_Output, K2Schema->PC_Byte, TEXT(""), Enum, false, false, EnumOuputPinName);
	}

	if (auto CompletedPin = CreatePin(EGPD_Output, K2Schema->PC_Exec, TEXT(""), NULL, false, false, K2Schema->PN_Then))
	{
		CompletedPin->PinFriendlyName = LOCTEXT("Completed", "Completed");
	}
}

void UK2Node_ForEachElementInEnum::ValidateNodeDuringCompilation(FCompilerResultsLog& MessageLog) const
{
	if (!Enum)
	{
		MessageLog.Error(*NSLOCTEXT("K2Node", "ForEachElementInEnum_NoEnumError", "No Enum in @@").ToString(), this);
	}
}

FText UK2Node_ForEachElementInEnum::GetTooltipText() const
{
	return GetNodeTitle(ENodeTitleType::FullTitle);
}

FText UK2Node_ForEachElementInEnum::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (Enum == nullptr)
	{
		return LOCTEXT("ForEachElementInUnknownEnum_Title", "ForEach UNKNOWN");
	}
	else if (CachedNodeTitle.IsOutOfDate(this))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("EnumName"), FText::FromName(Enum->GetFName()));
		// FText::Format() is slow, so we cache this to save on performance
		CachedNodeTitle.SetCachedText(FText::Format(LOCTEXT("ForEachElementInEnum_Title", "ForEach {EnumName}"), Args), this);
	}
	return CachedNodeTitle;
}

void UK2Node_ForEachElementInEnum::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	if (!Enum)
	{
		ValidateNodeDuringCompilation(CompilerContext.MessageLog);
		return;
	}

	FForExpandNodeHelper ForLoop;
	if (!ForLoop.BuildLoop(this, CompilerContext, SourceGraph))
	{
		CompilerContext.MessageLog.Error(*NSLOCTEXT("K2Node", "ForEachElementInEnum_ForError", "For Expand error in @@").ToString(), this);
	}

	const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();

	CompilerContext.MovePinLinksToIntermediate(*GetExecPin(), *ForLoop.StartLoopExecInPin);
	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(Schema->PN_Then), *ForLoop.LoopCompleteOutExecPin);
	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(InsideLoopPinName), *ForLoop.InsideLoopExecOutPin);

	UK2Node_GetNumEnumEntries* GetNumEnumEntries = CompilerContext.SpawnIntermediateNode<UK2Node_GetNumEnumEntries>(this, SourceGraph);
	GetNumEnumEntries->Enum = Enum;
	GetNumEnumEntries->AllocateDefaultPins();
	bool bResult = Schema->TryCreateConnection(GetNumEnumEntries->FindPinChecked(Schema->PN_ReturnValue), ForLoop.IndexLimitInPin);

	UK2Node_CallFunction* Conv_Func = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	FName Conv_Func_Name = GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Conv_IntToByte);
	Conv_Func->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(Conv_Func_Name));
	Conv_Func->AllocateDefaultPins();
	bResult &= Schema->TryCreateConnection(Conv_Func->FindPinChecked(TEXT("InInt")), ForLoop.IndexOutPin);

	UK2Node_CastByteToEnum* CastByteToEnum = CompilerContext.SpawnIntermediateNode<UK2Node_CastByteToEnum>(this, SourceGraph);
	CastByteToEnum->Enum = Enum;
	CastByteToEnum->bSafe = true;
	CastByteToEnum->AllocateDefaultPins();
	bResult &= Schema->TryCreateConnection(Conv_Func->FindPinChecked(Schema->PN_ReturnValue), CastByteToEnum->FindPinChecked(UK2Node_CastByteToEnum::ByteInputPinName));
	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(EnumOuputPinName), *CastByteToEnum->FindPinChecked(Schema->PN_ReturnValue));

	if (!bResult)
	{
		CompilerContext.MessageLog.Error(*NSLOCTEXT("K2Node", "ForEachElementInEnum_ExpandError", "Expand error in @@").ToString(), this);
	}

	BreakAllNodeLinks();
}

void UK2Node_ForEachElementInEnum::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	auto SetNodeEnumLambda = [](UEdGraphNode* NewNode, UField const* /*EnumField*/, TWeakObjectPtr<UEnum> NonConstEnumPtr)
	{
		UK2Node_ForEachElementInEnum* EnumNode = CastChecked<UK2Node_ForEachElementInEnum>(NewNode);
		EnumNode->Enum = NonConstEnumPtr.Get();
	};

	for (TObjectIterator<UEnum> EnumIt; EnumIt; ++EnumIt)
	{
		UEnum const* EnumToConsider = (*EnumIt);
		if (!UEdGraphSchema_K2::IsAllowableBlueprintVariableType(EnumToConsider))
		{
			continue;
		}

		// to keep from needlessly instantiating a UBlueprintNodeSpawners, first   
		// check to make sure that the registrar is looking for actions of this type
		// (could be regenerating actions for a specific asset, and therefore the 
		// registrar would only accept actions corresponding to that asset)
		if (!ActionRegistrar.IsOpenForRegistration(EnumToConsider))
		{
			continue;
		}

		UBlueprintFieldNodeSpawner* NodeSpawner = UBlueprintFieldNodeSpawner::Create(GetClass(), EnumToConsider);
		check(NodeSpawner != nullptr);
		TWeakObjectPtr<UEnum> NonConstEnumPtr = EnumToConsider;
		NodeSpawner->SetNodeFieldDelegate = UBlueprintFieldNodeSpawner::FSetNodeFieldDelegate::CreateStatic(SetNodeEnumLambda, NonConstEnumPtr);

		// this enum could belong to a class, or is a user defined enum (asset), 
		// that's why we want to make sure to register it along with the action 
		// (so the action can be refreshed when the class/asset is).
		ActionRegistrar.AddBlueprintAction(EnumToConsider, NodeSpawner);
	}
}

FText UK2Node_ForEachElementInEnum::GetMenuCategory() const
{
	return FEditorCategoryUtils::GetCommonCategory(FCommonEditorCategory::Enum);
}

#undef LOCTEXT_NAMESPACE