// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "BlueprintGraphPrivatePCH.h"
#include "K2Node_FunctionTerminator.h"
#include "GraphEditorSettings.h"

#define LOCTEXT_NAMESPACE "K2Node"

UK2Node_FunctionTerminator::UK2Node_FunctionTerminator(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FLinearColor UK2Node_FunctionTerminator::GetNodeTitleColor() const
{
	return GetDefault<UGraphEditorSettings>()->FunctionTerminatorNodeTitleColor;
}

FString UK2Node_FunctionTerminator::CreateUniquePinName(FString InSourcePinName) const
{
	const UFunction* FoundFunction = FFunctionFromNodeHelper::FunctionFromNode(this);

	FString ResultName = InSourcePinName;
	int UniqueNum = 0;
	// Prevent the unique name from being the same as another of the UFunction's properties
	while(FindPin(ResultName) || FindField<const UProperty>(FoundFunction, *ResultName) != NULL)
	{
		ResultName = FString::Printf(TEXT("%s%d"), *InSourcePinName, ++UniqueNum);
	}
	return ResultName;
}

bool UK2Node_FunctionTerminator::CanCreateUserDefinedPin(const FEdGraphPinType& InPinType, EEdGraphPinDirection InDesiredDirection, FText& OutErrorMessage)
{
	const bool bIsEditable = IsEditable();

	// Make sure that if this is an exec node we are allowed one.
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	if (bIsEditable && InPinType.PinCategory == Schema->PC_Exec && !CanModifyExecutionWires())
	{
		OutErrorMessage = LOCTEXT("MultipleExecPinError", "Cannot support more exec pins!");
		return false;
	}
	else if (!bIsEditable)
	{
		OutErrorMessage = LOCTEXT("NotEditableError", "Cannot edit this node!");
	}

	return bIsEditable;
}

bool UK2Node_FunctionTerminator::HasExternalDependencies(TArray<class UStruct*>* OptionalOutput) const
{
	const UBlueprint* SourceBlueprint = GetBlueprint();

	UClass* SourceClass = *SignatureClass;
	bool bResult = (SourceClass != nullptr) && (SourceClass->ClassGeneratedBy != SourceBlueprint);
	if (bResult && OptionalOutput)
	{
		OptionalOutput->AddUnique(SourceClass);
	}

	// All structures, that are required for the BP compilation, should be gathered
	for (auto Pin : Pins)
	{
		UStruct* DepStruct = Pin ? Cast<UStruct>(Pin->PinType.PinSubCategoryObject.Get()) : nullptr;

		UClass* DepClass = Cast<UClass>(DepStruct);
		if (DepClass && (DepClass->ClassGeneratedBy == SourceBlueprint))
		{
			//Don't include self
			continue;
		}

		if (DepStruct && !DepStruct->IsNative())
		{
			if (OptionalOutput)
			{
				OptionalOutput->AddUnique(DepStruct);
			}
			bResult = true;
		}
	}

	const bool bSuperResult = Super::HasExternalDependencies(OptionalOutput);
	return bSuperResult || bResult;
}

void UK2Node_FunctionTerminator::PromoteFromInterfaceOverride(bool bIsPrimaryTerminator)
{
	// Remove the signature class, that is not relevant.
	SignatureClass = nullptr;
	TArray<UEdGraphPin*> OriginalPins = Pins;
	for (const UEdGraphPin* Pin : OriginalPins)
	{
		if (Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
		{
			CreateUserDefinedPin(Pin->PinName, Pin->PinType, (Pin->Direction == EGPD_Input)? EGPD_Output : EGPD_Input, false);
		}
	}
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	Schema->ReconstructNode(*this, true);
}

#undef LOCTEXT_NAMESPACE
