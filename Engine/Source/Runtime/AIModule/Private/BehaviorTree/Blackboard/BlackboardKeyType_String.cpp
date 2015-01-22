// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_String.h"

const UBlackboardKeyType_String::FDataType UBlackboardKeyType_String::InvalidValue = FString();

UBlackboardKeyType_String::UBlackboardKeyType_String(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ValueSize = sizeof(FString);
	SupportedOp = EBlackboardKeyOperation::Text;
}

FString UBlackboardKeyType_String::GetValue(const uint8* RawData)
{
	return GetValueFromMemory<FString>(RawData);
}

bool UBlackboardKeyType_String::SetValue(uint8* RawData, const FString& Value)
{
	return SetValueInMemory<FString>(RawData, Value);
}

FString UBlackboardKeyType_String::DescribeValue(const uint8* RawData) const
{
	return GetValue(RawData);
}

EBlackboardCompare::Type UBlackboardKeyType_String::Compare(const uint8* MemoryBlockA, const uint8* MemoryBlockB) const
{
	return GetValueFromMemory<FString>(MemoryBlockA) == GetValueFromMemory<FString>(MemoryBlockB)
		? EBlackboardCompare::Equal : EBlackboardCompare::NotEqual;
}

bool UBlackboardKeyType_String::TestTextOperation(const uint8* MemoryBlock, ETextKeyOperation::Type Op, const FString& OtherString) const
{
	const FString Value = GetValue(MemoryBlock);
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
