// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "K2Node_BitmaskLiteral.h"
#include "Kismet/KismetSystemLibrary.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "EditorCategoryUtils.h"
#include "KismetCompiler.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintActionDatabaseRegistrar.h"

#define LOCTEXT_NAMESPACE "UK2Node_BitmaskLiteral"

const FString& UK2Node_BitmaskLiteral::GetBitmaskInputPinName()
{
	static const FString PinName(TEXT("Bitmask"));
	return PinName;
}

UK2Node_BitmaskLiteral::UK2Node_BitmaskLiteral(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, BitflagsEnum(nullptr)
{
	
}

void UK2Node_BitmaskLiteral::ValidateBitflagsEnumType()
{
	// Reset enum type reference if it no longer has the proper meta data.
	if (BitflagsEnum != nullptr && (BitflagsEnum->IsPendingKill() || !BitflagsEnum->HasMetaData(TEXT("Bitflags"))))
	{
		BitflagsEnum = nullptr;

		// Note: The input pin's default value is intentionally not reset here. This ensures that we preserve the default value even if the enum type representing the bitflags is removed.
	}
}

void UK2Node_BitmaskLiteral::PostLoad()
{
	Super::PostLoad();

	// Post-load validation of the enum type.
	ValidateBitflagsEnumType();
}

void UK2Node_BitmaskLiteral::AllocateDefaultPins()
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	UEdGraphPin* InputPin = CreatePin(EGPD_Input, Schema->PC_Int, Schema->PSC_Bitmask, BitflagsEnum, false, false, GetBitmaskInputPinName());
	Schema->SetPinDefaultValueBasedOnType(InputPin);

	CreatePin(EGPD_Output, Schema->PC_Int, Schema->PSC_Bitmask, BitflagsEnum, false, false, Schema->PN_ReturnValue);

	Super::AllocateDefaultPins();
}

void UK2Node_BitmaskLiteral::ReconstructNode()
{
	// Validate the enum type prior to reconstruction so that the input pin's default value is reset first (if needed).
	ValidateBitflagsEnumType();

	Super::ReconstructNode();
}

void UK2Node_BitmaskLiteral::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();
	check(nullptr != Schema);

	const FName FunctionName = GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, MakeLiteralInt);
	UK2Node_CallFunction* MakeLiteralInt = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	MakeLiteralInt->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(FunctionName));
	MakeLiteralInt->AllocateDefaultPins();

	UEdGraphPin* OrgInputPin = FindPinChecked(GetBitmaskInputPinName());
	UEdGraphPin* NewInputPin = MakeLiteralInt->FindPinChecked(TEXT("Value"));
	check(nullptr != NewInputPin);
	CompilerContext.MovePinLinksToIntermediate(*OrgInputPin, *NewInputPin);

	UEdGraphPin* OrgReturnPin = FindPinChecked(Schema->PN_ReturnValue);
	UEdGraphPin* NewReturnPin = MakeLiteralInt->GetReturnValuePin();
	check(nullptr != NewReturnPin);
	CompilerContext.MovePinLinksToIntermediate(*OrgReturnPin, *NewReturnPin);
}

FText UK2Node_BitmaskLiteral::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("NodeTitle", "Make Bitmask");
}

void UK2Node_BitmaskLiteral::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

FText UK2Node_BitmaskLiteral::GetMenuCategory() const
{
	return FEditorCategoryUtils::GetCommonCategory(FCommonEditorCategory::Math);
}

#undef LOCTEXT_NAMESPACE
