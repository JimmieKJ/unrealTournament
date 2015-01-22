// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"

const UBlackboardKeyType_Float::FDataType UBlackboardKeyType_Float::InvalidValue = UBlackboardKeyType_Float::FDataType(0);

UBlackboardKeyType_Float::UBlackboardKeyType_Float(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ValueSize = sizeof(float);
	SupportedOp = EBlackboardKeyOperation::Arithmetic;
}

float UBlackboardKeyType_Float::GetValue(const uint8* RawData)
{
	return GetValueFromMemory<float>(RawData);
}

bool UBlackboardKeyType_Float::SetValue(uint8* RawData, float Value)
{
	return SetValueInMemory<float>(RawData, Value);
}

FString UBlackboardKeyType_Float::DescribeValue(const uint8* RawData) const
{
	return FString::Printf(TEXT("%f"), GetValue(RawData));
}

EBlackboardCompare::Type UBlackboardKeyType_Float::Compare(const uint8* MemoryBlockA, const uint8* MemoryBlockB) const
{
	const float Diff = GetValueFromMemory<float>(MemoryBlockA) - GetValueFromMemory<float>(MemoryBlockB);
	return FMath::Abs(Diff) < KINDA_SMALL_NUMBER ? EBlackboardCompare::Equal 
		: (Diff > 0 ? EBlackboardCompare::Greater : EBlackboardCompare::Less);
}

bool UBlackboardKeyType_Float::TestArithmeticOperation(const uint8* MemoryBlock, EArithmeticKeyOperation::Type Op, int32 OtherIntValue, float OtherFloatValue) const
{
	const float Value = GetValue(MemoryBlock);
	switch (Op)
	{
	case EArithmeticKeyOperation::Equal:			return (Value == OtherFloatValue);
	case EArithmeticKeyOperation::NotEqual:			return (Value != OtherFloatValue);
	case EArithmeticKeyOperation::Less:				return (Value < OtherFloatValue);
	case EArithmeticKeyOperation::LessOrEqual:		return (Value <= OtherFloatValue);
	case EArithmeticKeyOperation::Greater:			return (Value > OtherFloatValue);
	case EArithmeticKeyOperation::GreaterOrEqual:	return (Value >= OtherFloatValue);
	default: break;
	}

	return false;
}

FString UBlackboardKeyType_Float::DescribeArithmeticParam(int32 IntValue, float FloatValue) const
{
	return FString::Printf(TEXT("%f"), FloatValue);
}
