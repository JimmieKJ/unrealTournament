// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_NativeEnum.h"

const UBlackboardKeyType_NativeEnum::FDataType UBlackboardKeyType_NativeEnum::InvalidValue = UBlackboardKeyType_NativeEnum::FDataType(0);

UBlackboardKeyType_NativeEnum::UBlackboardKeyType_NativeEnum(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ValueSize = sizeof(uint8);
	SupportedOp = EBlackboardKeyOperation::Arithmetic;
}

uint8 UBlackboardKeyType_NativeEnum::GetValue(const uint8* RawData)
{
	return GetValueFromMemory<uint8>(RawData);
}

bool UBlackboardKeyType_NativeEnum::SetValue(uint8* RawData, uint8 Value)
{
	return SetValueInMemory<uint8>(RawData, Value);
}

FString UBlackboardKeyType_NativeEnum::DescribeValue(const uint8* RawData) const
{
	return EnumType ? EnumType->GetEnumName(GetValue(RawData)) : FString("UNKNOWN!");
}

FString UBlackboardKeyType_NativeEnum::DescribeSelf() const
{
	return *GetNameSafe(EnumType);
}

bool UBlackboardKeyType_NativeEnum::IsAllowedByFilter(UBlackboardKeyType* FilterOb) const
{
	UBlackboardKeyType_NativeEnum* FilterEnum = Cast<UBlackboardKeyType_NativeEnum>(FilterOb);
	return (FilterEnum && FilterEnum->EnumName == EnumName);
}

#if WITH_EDITOR
void UBlackboardKeyType_NativeEnum::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property &&
		PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UBlackboardKeyType_NativeEnum, EnumName))
	{
		EnumType = FindObject<UEnum>(ANY_PACKAGE, *EnumName, true);
		bIsEnumNameValid =  (EnumType != NULL);
	}
}
#endif

EBlackboardCompare::Type UBlackboardKeyType_NativeEnum::Compare(const uint8* MemoryBlockA, const uint8* MemoryBlockB) const
{
	const int8 Diff = GetValueFromMemory<uint8>(MemoryBlockA) - GetValueFromMemory<uint8>(MemoryBlockB);
	return Diff > 0 ? EBlackboardCompare::Greater : (Diff < 0 ? EBlackboardCompare::Less : EBlackboardCompare::Equal);
}

bool UBlackboardKeyType_NativeEnum::TestArithmeticOperation(const uint8* MemoryBlock, EArithmeticKeyOperation::Type Op, int32 OtherIntValue, float OtherFloatValue) const
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

FString UBlackboardKeyType_NativeEnum::DescribeArithmeticParam(int32 IntValue, float FloatValue) const
{
	return EnumType ? EnumType->GetEnumName(IntValue) : FString("UNKNOWN!");
}
