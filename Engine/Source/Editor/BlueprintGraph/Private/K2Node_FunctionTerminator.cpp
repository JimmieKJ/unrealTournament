// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


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

	return bIsEditable;
}

#undef LOCTEXT_NAMESPACE
