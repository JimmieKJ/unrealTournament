// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"

const UBlackboardKeyType_Vector::FDataType UBlackboardKeyType_Vector::InvalidValue = FAISystem::InvalidLocation;

UBlackboardKeyType_Vector::UBlackboardKeyType_Vector(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ValueSize = sizeof(FVector);
	SupportedOp = EBlackboardKeyOperation::Basic;
}

FVector UBlackboardKeyType_Vector::GetValue(const uint8* RawData)
{
	return GetValueFromMemory<FVector>(RawData);
}

bool UBlackboardKeyType_Vector::SetValue(uint8* RawData, const FVector& Value)
{
	return SetValueInMemory<FVector>(RawData, Value);
}

bool UBlackboardKeyType_Vector::Clear(uint8* RawData) const 
{
	return SetValueInMemory<FVector>(RawData, FAISystem::InvalidLocation);
}

FString UBlackboardKeyType_Vector::DescribeValue(const uint8* RawData) const
{
	const FVector Location = GetValue(RawData);
	return FAISystem::IsValidLocation(Location) ? Location.ToString() : TEXT("(invalid)");
}

bool UBlackboardKeyType_Vector::GetLocation(const uint8* RawData, FVector& Location) const
{
	Location = GetValue(RawData);
	return FAISystem::IsValidLocation(Location);
}

void UBlackboardKeyType_Vector::Initialize(uint8* RawData) const
{
	SetValue(RawData, FAISystem::InvalidLocation);
}

EBlackboardCompare::Type UBlackboardKeyType_Vector::Compare(const uint8* MemoryBlockA, const uint8* MemoryBlockB) const
{
	return GetValueFromMemory<FVector>(MemoryBlockA).Equals(GetValueFromMemory<FVector>(MemoryBlockB))
		? EBlackboardCompare::Equal : EBlackboardCompare::NotEqual;
}

bool UBlackboardKeyType_Vector::TestBasicOperation(const uint8* MemoryBlock, EBasicKeyOperation::Type Op) const
{
	const FVector Location = GetValue(MemoryBlock);
	return (Op == EBasicKeyOperation::Set) ? FAISystem::IsValidLocation(Location) : !FAISystem::IsValidLocation(Location);
}
