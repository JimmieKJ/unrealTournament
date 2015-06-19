// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "GraphEditorCommon.h"
#include "SGraphPin.h"
#include "SGraphPinString.h"
#include "SGraphPinNum.h"
#include "SGraphPinInteger.h"
#include "DefaultValueHelper.h"
#include "Editor/UnrealEd/Public/ScopedTransaction.h"

void SGraphPinInteger::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	SGraphPinNum::Construct(SGraphPinNum::FArguments(), InGraphPinObj);
}

void SGraphPinInteger::SetTypeInValue(const FText& NewTypeInValue, ETextCommit::Type /*CommitInfo*/)
{
	const FString TypeValueString = NewTypeInValue.ToString();
	if (FDefaultValueHelper::IsStringValidFloat(TypeValueString) || FDefaultValueHelper::IsStringValidInteger(TypeValueString))
	{
		if (GraphPinObj->GetDefaultAsString() != TypeValueString)
		{
			const FScopedTransaction Transaction(NSLOCTEXT("GraphEditor", "ChangeNumberPinValue", "Change Number Pin Value"));
			GraphPinObj->Modify();

			// Round-tripped here to allow floating point values to be pasted and truncated rather than failing to be set at all
			const int32 IntValue = FCString::Atoi(*TypeValueString);
			GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, FString::FromInt(IntValue));
		}
	}
}
