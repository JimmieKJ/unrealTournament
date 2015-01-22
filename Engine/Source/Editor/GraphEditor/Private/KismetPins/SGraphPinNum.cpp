// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "GraphEditorCommon.h"
#include "SGraphPin.h"
#include "SGraphPinString.h"
#include "SGraphPinNum.h"
#include "DefaultValueHelper.h"

void SGraphPinNum::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);
}

void SGraphPinNum::SetTypeInValue(const FText& NewTypeInValue, ETextCommit::Type /*CommitInfo*/)
{
	FString TypeValueString = NewTypeInValue.ToString();
	if (FDefaultValueHelper::IsStringValidFloat(TypeValueString))
	{
		//This is rubbish.
		//TODO: Make not rubbish.
		const UEdGraphSchema_K2* K2Schema = Cast<const UEdGraphSchema_K2>(GraphPinObj->GetSchema());
		if ((K2Schema && (GraphPinObj->PinType.PinCategory == K2Schema->PC_Int || GraphPinObj->PinType.PinCategory == K2Schema->PC_Byte)))
		{
			int32 IntValue = FCString::Atoi(*TypeValueString);
			GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, FString::FromInt(IntValue));
		}
		else
		{
			GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, *TypeValueString);
		}
	}
}
