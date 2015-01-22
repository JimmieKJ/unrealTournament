// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Int.h"

const UBlackboardKeyType_Int::FDataType UBlackboardKeyType_Int::InvalidValue = UBlackboardKeyType_Int::FDataType(0);

UBlackboardKeyType_Int::UBlackboardKeyType_Int(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ValueSize = sizeof(int32);
	SupportedOp = EBlackboardKeyOperation::Arithmetic;
}

int32 UBlackboardKeyType_Int::GetValue(const uint8* RawData)
{
	return GetValueFromMemory<int32>(RawData);
}

bool UBlackboardKeyType_Int::SetValue(uint8* RawData, int32 Value)
{
	return SetValueInMemory<int32>(RawData, Value);
}

FString UBlackboardKeyType_Int::DescribeValue(const uint8* RawData) const
{
	return FString::Printf(TEXT("%d"), GetValue(RawData));
}

EBlackboardCompare::Type UBlackboardKeyType_Int::Compare(const uint8* MemoryBlockA, const uint8* MemoryBlockB) const
{
	const int32 Diff = GetValueFromMemory<int32>(MemoryBlockA) - GetValueFromMemory<int32>(MemoryBlockB);
	return Diff > 0 ? EBlackboardCompare::Greater : (Diff < 0 ? EBlackboardCompare::Less : EBlackboardCompare::Equal);
}

bool UBlackboardKeyType_Int::TestArithmeticOperation(const uint8* MemoryBlock, EArithmeticKeyOperation::Type Op, int32 OtherIntValue, float OtherFloatValue) const
{
	const int32 Value = GetValue(MemoryBlock);
	switch (Op)
	{
	case EArithmeticKeyOperation::Equal:			return (Value == OtherIntValue);
	case EArithmeticKeyOperation::NotEqual:			return (Value != OtherIntValue);
	case EArithmeticKeyOperation::Less:				return (Value < OtherIntValue);
	case EArithmeticKeyOperation::LessOrEqual:		return (Value <= OtherIntValue);
	case EArithmeticKeyOperation::Greater:			return (Value > OtherIntValue);
	case EArithmeticKeyOperation::GreaterOrEqual:	return (Value >= OtherIntValue);
	default: break;
	}

	return false;
}

FString UBlackboardKeyType_Int::DescribeArithmeticParam(int32 IntValue, float FloatValue) const
{
	return FString::Printf(TEXT("%d"), IntValue);
}
