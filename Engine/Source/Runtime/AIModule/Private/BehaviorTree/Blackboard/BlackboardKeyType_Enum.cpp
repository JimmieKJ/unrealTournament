// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Enum.h"

const UBlackboardKeyType_Enum::FDataType UBlackboardKeyType_Enum::InvalidValue = UBlackboardKeyType_Enum::FDataType(0);

UBlackboardKeyType_Enum::UBlackboardKeyType_Enum(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ValueSize = sizeof(uint8);
	SupportedOp = EBlackboardKeyOperation::Arithmetic;
}

uint8 UBlackboardKeyType_Enum::GetValue(const uint8* RawData)
{
	return GetValueFromMemory<uint8>(RawData);
}

bool UBlackboardKeyType_Enum::SetValue(uint8* RawData, uint8 Value)
{
	return SetValueInMemory<uint8>(RawData, Value);
}

FString UBlackboardKeyType_Enum::DescribeValue(const uint8* RawData) const
{
	return EnumType ? EnumType->GetEnumText(GetValue(RawData)).ToString() : FString("UNKNOWN!");
}

FString UBlackboardKeyType_Enum::DescribeSelf() const
{
	return *GetNameSafe(EnumType);
}

bool UBlackboardKeyType_Enum::IsAllowedByFilter(UBlackboardKeyType* FilterOb) const
{
	UBlackboardKeyType_Enum* FilterEnum = Cast<UBlackboardKeyType_Enum>(FilterOb);
	return (FilterEnum && FilterEnum->EnumType == EnumType);
}

EBlackboardCompare::Type UBlackboardKeyType_Enum::Compare(const uint8* MemoryBlockA, const uint8* MemoryBlockB) const
{
	const int8 Diff = GetValueFromMemory<uint8>(MemoryBlockA) - GetValueFromMemory<uint8>(MemoryBlockB);
	return Diff > 0 ? EBlackboardCompare::Greater : (Diff < 0 ? EBlackboardCompare::Less : EBlackboardCompare::Equal);
}

bool UBlackboardKeyType_Enum::TestArithmeticOperation(const uint8* MemoryBlock, EArithmeticKeyOperation::Type Op, int32 OtherIntValue, float OtherFloatValue) const
{
	const uint8 Value = GetValue(MemoryBlock);
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

FString UBlackboardKeyType_Enum::DescribeArithmeticParam(int32 IntValue, float FloatValue) const
{
	return EnumType ? EnumType->GetEnumText(IntValue).ToString() : FString("UNKNOWN!");
}
