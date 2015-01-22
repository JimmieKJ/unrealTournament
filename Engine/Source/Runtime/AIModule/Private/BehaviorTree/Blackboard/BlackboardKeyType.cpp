// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType.h"

UBlackboardKeyType::UBlackboardKeyType(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ValueSize = 0;
	SupportedOp = EBlackboardKeyOperation::Basic;
}

FString UBlackboardKeyType::DescribeValue(const uint8* RawData) const
{
	FString DescBytes;
	for (int32 ValueIndex = 0; ValueIndex < ValueSize; ValueIndex++)
	{
		DescBytes += FString::Printf(TEXT("%X"), RawData[ValueIndex]);
	}

	return DescBytes.Len() ? (FString("0x") + DescBytes) : FString("empty");
}

FString UBlackboardKeyType::DescribeSelf() const
{
	return FString();
};

void UBlackboardKeyType::Initialize(uint8* MemoryBlock) const
{
	// empty in base class
}

bool UBlackboardKeyType::IsAllowedByFilter(UBlackboardKeyType* FilterOb) const
{
	return GetClass() == (FilterOb ? FilterOb->GetClass() : NULL);
}

bool UBlackboardKeyType::GetLocation(const uint8* RawData, FVector& Location) const
{
	return false;
}

bool UBlackboardKeyType::GetRotation(const uint8* MemoryBlock, FRotator& Rotation) const
{
	return false;
}

bool UBlackboardKeyType::Clear(uint8* MemoryBlock) const
{
	for (int32 ByteIndex = 0; ByteIndex < GetValueSize(); ++ByteIndex)
	{
		if (MemoryBlock[ByteIndex] != uint8(0))
		{
			FMemory::Memzero(MemoryBlock, GetValueSize());
			return true;
		}		
	}

	// value already "0", return false to indicate it had not changed
	return false;
}

EBlackboardCompare::Type UBlackboardKeyType::Compare(const uint8* MemoryBlockA, const uint8* MemoryBlockB) const
{ 
	return MemoryBlockA == MemoryBlockB ? EBlackboardCompare::Equal : EBlackboardCompare::NotEqual;
}

bool UBlackboardKeyType::TestBasicOperation(const uint8* MemoryBlock, EBasicKeyOperation::Type Op) const
{
	return false;
}

bool UBlackboardKeyType::TestArithmeticOperation(const uint8* MemoryBlock, EArithmeticKeyOperation::Type Op, int32 OtherIntValue, float OtherFloatValue) const
{
	return false;
}

bool UBlackboardKeyType::TestTextOperation(const uint8* MemoryBlock, ETextKeyOperation::Type Op, const FString& OtherString) const
{
	return false;
}

FString UBlackboardKeyType::DescribeArithmeticParam(int32 IntValue, float FloatValue) const
{
	return FString();
}
