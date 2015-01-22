// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Class.h"

const UBlackboardKeyType_Class::FDataType UBlackboardKeyType_Class::InvalidValue = nullptr;

UBlackboardKeyType_Class::UBlackboardKeyType_Class(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ValueSize = sizeof(TWeakObjectPtr<UClass>);
	BaseClass = UObject::StaticClass();
	SupportedOp = EBlackboardKeyOperation::Basic;
}

UClass* UBlackboardKeyType_Class::GetValue(const uint8* RawData)
{
	TWeakObjectPtr<UClass> WeakObjPtr = GetValueFromMemory< TWeakObjectPtr<UClass> >(RawData);
	return WeakObjPtr.Get();
}

bool UBlackboardKeyType_Class::SetValue(uint8* RawData, UClass* Value)
{
	TWeakObjectPtr<UClass> WeakObjPtr(Value);
	return SetWeakObjectInMemory<UClass>(RawData, WeakObjPtr);
}

FString UBlackboardKeyType_Class::DescribeValue(const uint8* RawData) const
{
	return *GetNameSafe(GetValue(RawData));
}

FString UBlackboardKeyType_Class::DescribeSelf() const
{
	return *GetNameSafe(BaseClass);
}

bool UBlackboardKeyType_Class::IsAllowedByFilter(UBlackboardKeyType* FilterOb) const
{
	UBlackboardKeyType_Class* FilterClass = Cast<UBlackboardKeyType_Class>(FilterOb);
	return (FilterClass && (FilterClass->BaseClass == BaseClass || BaseClass->IsChildOf(FilterClass->BaseClass)));
}

EBlackboardCompare::Type UBlackboardKeyType_Class::Compare(const uint8* MemoryBlockA, const uint8* MemoryBlockB) const
{
	return (FMemory::Memcmp(MemoryBlockA, MemoryBlockB, ValueSize) == 0) ? EBlackboardCompare::Equal : EBlackboardCompare::NotEqual;
}

bool UBlackboardKeyType_Class::TestBasicOperation(const uint8* MemoryBlock, EBasicKeyOperation::Type Op) const
{
	TWeakObjectPtr<UClass> WeakObjPtr = GetValueFromMemory< TWeakObjectPtr<UClass> >(MemoryBlock);
	return (Op == EBasicKeyOperation::Set) ? WeakObjPtr.IsValid() : !WeakObjPtr.IsValid();
}
