// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "BlueprintGraphPrivatePCH.h"
#include "KismetCompiler.h"
#include "Kismet/KismetNodeHelperLibrary.h"
#include "K2Node_CastByteToEnum.h"
#include "BlueprintFieldNodeSpawner.h"
#include "EditorCategoryUtils.h"
#include "BlueprintActionDatabaseRegistrar.h"

const FString UK2Node_CastByteToEnum::ByteInputPinName = TEXT("Byte");

UK2Node_CastByteToEnum::UK2Node_CastByteToEnum(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UK2Node_CastByteToEnum::ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const
{
	Super::ValidateNodeDuringCompilation(MessageLog);
	if (!Enum)
	{
		MessageLog.Error(*FString::Printf(*NSLOCTEXT("K2Node", "CastByteToNullEnumError", "Undefined Enum in @@").ToString()), this);
	}
}

void UK2Node_CastByteToEnum::AllocateDefaultPins()
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	
	CreatePin(EGPD_Input, Schema->PC_Byte, TEXT(""), NULL, false, false, ByteInputPinName);
	CreatePin(EGPD_Output, Schema->PC_Byte, TEXT(""), Enum, false, false, Schema->PN_ReturnValue);
}

FText UK2Node_CastByteToEnum::GetTooltipText() const
{
	if (Enum == nullptr)
	{
		return NSLOCTEXT("K2Node", "CastByteToEnum_NullTooltip", "Byte to Enum (bad enum)");
	}
	else if(CachedTooltip.IsOutOfDate(this))
	{
		// FText::Format() is slow, so we cache this to save on performance
		CachedTooltip.SetCachedText(FText::Format(
			NSLOCTEXT("K2Node", "CastByteToEnum_Tooltip", "Byte to Enum {0}"),
			FText::FromName(Enum->GetFName())
		), this);
	}
	return CachedTooltip;
}

FText UK2Node_CastByteToEnum::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return GetTooltipText();
}

FText UK2Node_CastByteToEnum::GetCompactNodeTitle() const
{
	return NSLOCTEXT("K2Node", "CastSymbol", "\x2022");
}

FName UK2Node_CastByteToEnum::GetFunctionName() const
{
	const FName FunctionName = GET_FUNCTION_NAME_CHECKED(UKismetNodeHelperLibrary, GetValidIndex);
	return FunctionName;
}

void UK2Node_CastByteToEnum::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	if (bSafe && Enum)
	{
		const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();

		// FUNCTION NODE
		const FName FunctionName = GetFunctionName();
		const UFunction* Function = UKismetNodeHelperLibrary::StaticClass()->FindFunctionByName(FunctionName);
		check(NULL != Function);
		UK2Node_CallFunction* CallValidation = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph); 
		CallValidation->SetFromFunction(Function);
		CallValidation->AllocateDefaultPins();
		check(CallValidation->IsNodePure());

		// FUNCTION ENUM PIN
		UEdGraphPin* FunctionEnumPin = CallValidation->FindPinChecked(TEXT("Enum"));
		Schema->TrySetDefaultObject(*FunctionEnumPin, Enum);
		check(FunctionEnumPin->DefaultObject == Enum);

		// FUNCTION INPUT BYTE PIN
		UEdGraphPin* OrgInputPin = FindPinChecked(ByteInputPinName);
		UEdGraphPin* FunctionIndexPin = CallValidation->FindPinChecked(TEXT("EnumeratorIndex"));
		check(EGPD_Input == FunctionIndexPin->Direction && Schema->PC_Byte == FunctionIndexPin->PinType.PinCategory);
		CompilerContext.MovePinLinksToIntermediate(*OrgInputPin, *FunctionIndexPin);

		// UNSAFE CAST NODE
		UK2Node_CastByteToEnum* UsafeCast = CompilerContext.SpawnIntermediateNode<UK2Node_CastByteToEnum>(this, SourceGraph); 
		UsafeCast->Enum = Enum;
		UsafeCast->bSafe = false;
		UsafeCast->AllocateDefaultPins();

		// UNSAFE CAST INPUT
		UEdGraphPin* CastInputPin = UsafeCast->FindPinChecked(ByteInputPinName);
		UEdGraphPin* FunctionReturnPin = CallValidation->GetReturnValuePin();
		check(NULL != FunctionReturnPin);
		Schema->TryCreateConnection(CastInputPin, FunctionReturnPin);

		// OPUTPUT PIN
		UEdGraphPin* OrgReturnPin = FindPinChecked(Schema->PN_ReturnValue);
		UEdGraphPin* NewReturnPin = UsafeCast->FindPinChecked(Schema->PN_ReturnValue);
		CompilerContext.MovePinLinksToIntermediate(*OrgReturnPin, *NewReturnPin);

		BreakAllNodeLinks();
	}
}

class FKCHandler_CastByteToEnum : public FNodeHandlingFunctor
{
public:
	FKCHandler_CastByteToEnum(FKismetCompilerContext& InCompilerContext)
		: FNodeHandlingFunctor(InCompilerContext)
	{
	}

	virtual void RegisterNets(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		FNodeHandlingFunctor::RegisterNets(Context, Node); //handle literals

		UEdGraphPin* InPin = Node->FindPinChecked(UK2Node_CastByteToEnum::ByteInputPinName);
		UEdGraphPin* Net = FEdGraphUtilities::GetNetFromPin(InPin);
		if (Context.NetMap.Find(Net) == NULL)
		{
			FBPTerminal* Term = Context.CreateLocalTerminalFromPinAutoChooseScope(Net, Context.NetNameMap->MakeValidName(Net));
			Context.NetMap.Add(Net, Term);
		}

		FBPTerminal** ValueSource = Context.NetMap.Find(Net);
		check(ValueSource && *ValueSource);
		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		UEdGraphPin* OutPin = Node->FindPinChecked(Schema->PN_ReturnValue);
		if (ensure(Context.NetMap.Find(OutPin) == NULL))
		{
			// We need to copy here to avoid passing in a reference to an element inside the map. The array
			// that owns the map members could be reallocated, causing the reference to become stale.
			FBPTerminal* ValueSourceCopy = *ValueSource;
			Context.NetMap.Add(OutPin, ValueSourceCopy);
		}
	}
};

FNodeHandlingFunctor* UK2Node_CastByteToEnum::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	if (!bSafe)
	{
		return new FKCHandler_CastByteToEnum(CompilerContext);
	}
	return new FNodeHandlingFunctor(CompilerContext);;
}

bool UK2Node_CastByteToEnum::IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const
{
	const UEnum* SubCategoryObject = Cast<UEnum>(OtherPin->PinType.PinSubCategoryObject.Get());
	if (SubCategoryObject)
	{
		if (SubCategoryObject != Enum)
		{
			return true;
		}
	}
	return false;
}

void UK2Node_CastByteToEnum::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	auto SetNodeEnumLambda = [](UEdGraphNode* NewNode, UField const* /*EnumField*/, TWeakObjectPtr<UEnum> NonConstEnumPtr)
	{
		UK2Node_CastByteToEnum* EnumNode = CastChecked<UK2Node_CastByteToEnum>(NewNode);
		EnumNode->Enum  = NonConstEnumPtr.Get();
		EnumNode->bSafe = true;
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

FText UK2Node_CastByteToEnum::GetMenuCategory() const
{
	return FEditorCategoryUtils::GetCommonCategory(FCommonEditorCategory::Enum);
}
