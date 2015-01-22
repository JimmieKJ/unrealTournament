// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Name.h"

const UBlackboardKeyType_Name::FDataType UBlackboardKeyType_Name::InvalidValue = NAME_None;

UBlackboardKeyType_Name::UBlackboardKeyType_Name(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ValueSize = sizeof(FName);
	SupportedOp = EBlackboardKeyOperation::Text;
}

FName UBlackboardKeyType_Name::GetValue(const uint8* RawData)
{
	return GetValueFromMemory<FName>(RawData);
}

bool UBlackboardKeyType_Name::SetValue(uint8* RawData, const FName& Value)
{
	return SetValueInMemory<FName>(RawData, Value);
}

FString UBlackboardKeyType_Name::DescribeValue(const uint8* RawData) const
{
	return GetValue(RawData).ToString();
}

EBlackboardCompare::Type UBlackboardKeyType_Name::Compare(const uint8* MemoryBlockA, const uint8* MemoryBlockB) const
{
	return GetValueFromMemory<FName>(MemoryBlockA) == GetValueFromMemory<FName>(MemoryBlockB)
		? EBlackboardCompare::Equal : EBlackboardCompare::NotEqual;
}

bool UBlackboardKeyType_Name::TestTextOperation(const uint8* MemoryBlock, ETextKeyOperation::Type Op, const FString& OtherString) const
{
	const FString Value = GetValue(MemoryBlock).ToString();
	switch (Op)
	{
	case ETextKeyOperation::Equal:			return (Value == OtherString);
	case ETextKeyOperation::NotEqual:		return (Value != OtherString);
	case ETextKeyOperation::Contain:		return (Value.Contains(OtherString) == true);
	case ETextKeyOperation::NotContain:		return (Value.Contains(OtherString) == false);
	default: break;
	}

	return false;
}
