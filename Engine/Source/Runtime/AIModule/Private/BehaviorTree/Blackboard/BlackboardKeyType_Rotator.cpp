// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Rotator.h"

const UBlackboardKeyType_Rotator::FDataType UBlackboardKeyType_Rotator::InvalidValue = FAISystem::InvalidRotation;

UBlackboardKeyType_Rotator::UBlackboardKeyType_Rotator(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ValueSize = sizeof(FRotator);
	SupportedOp = EBlackboardKeyOperation::Basic;
}

FRotator UBlackboardKeyType_Rotator::GetValue(const uint8* RawData)
{
	return GetValueFromMemory<FRotator>(RawData);
}

bool UBlackboardKeyType_Rotator::SetValue(uint8* RawData, const FRotator& Value)
{
	return SetValueInMemory<FRotator>(RawData, Value);
}

bool UBlackboardKeyType_Rotator::Clear(uint8* RawData) const 
{
	return SetValueInMemory<FRotator>(RawData, FAISystem::InvalidRotation);
}

FString UBlackboardKeyType_Rotator::DescribeValue(const uint8* RawData) const
{
	const FRotator Rotation = GetValue(RawData);
	return FAISystem::IsValidRotation(Rotation) ? Rotation.ToString() : TEXT("(invalid)");
}

bool UBlackboardKeyType_Rotator::GetRotation(const uint8* RawData, FRotator& Rotation) const
{
	Rotation = GetValue(RawData);
	return FAISystem::IsValidRotation(Rotation);
}

void UBlackboardKeyType_Rotator::Initialize(uint8* RawData) const
{
	SetValue(RawData, FAISystem::InvalidRotation);
}

EBlackboardCompare::Type UBlackboardKeyType_Rotator::Compare(const uint8* MemoryBlockA, const uint8* MemoryBlockB) const
{
	return GetValueFromMemory<FRotator>(MemoryBlockA).Equals(GetValueFromMemory<FRotator>(MemoryBlockB))
		? EBlackboardCompare::Equal : EBlackboardCompare::NotEqual;
}

bool UBlackboardKeyType_Rotator::TestBasicOperation(const uint8* MemoryBlock, EBasicKeyOperation::Type Op) const
{
	const FRotator Rotation = GetValue(MemoryBlock);
	return (Op == EBasicKeyOperation::Set) ? FAISystem::IsValidRotation(Rotation) : !FAISystem::IsValidRotation(Rotation);
}
