// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"

const UBlackboardKeyType_Object::FDataType UBlackboardKeyType_Object::InvalidValue = nullptr;

UBlackboardKeyType_Object::UBlackboardKeyType_Object(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ValueSize = sizeof(FWeakObjectPtr);
	BaseClass = UObject::StaticClass();
	SupportedOp = EBlackboardKeyOperation::Basic;
}

UObject* UBlackboardKeyType_Object::GetValue(const uint8* RawData)
{
	FWeakObjectPtr WeakObjPtr = GetValueFromMemory<FWeakObjectPtr>(RawData);
	return WeakObjPtr.Get();
}

bool UBlackboardKeyType_Object::SetValue(uint8* RawData, UObject* Value)
{
	TWeakObjectPtr<UObject> WeakObjPtr(Value);
	return SetWeakObjectInMemory<UObject>(RawData, WeakObjPtr);
}

FString UBlackboardKeyType_Object::DescribeValue(const uint8* RawData) const
{
	return *GetNameSafe(GetValue(RawData));
}

FString UBlackboardKeyType_Object::DescribeSelf() const
{
	return *GetNameSafe(BaseClass);
}

bool UBlackboardKeyType_Object::IsAllowedByFilter(UBlackboardKeyType* FilterOb) const
{
	UBlackboardKeyType_Object* FilterObject = Cast<UBlackboardKeyType_Object>(FilterOb);
	return (FilterObject && (FilterObject->BaseClass == BaseClass || BaseClass->IsChildOf(FilterObject->BaseClass)));
}

bool UBlackboardKeyType_Object::GetLocation(const uint8* RawData, FVector& Location) const
{
	AActor* MyActor = Cast<AActor>(GetValue(RawData));
	if (MyActor)
	{
		Location = MyActor->GetActorLocation();
		return true;
	}

	return false;
}

bool UBlackboardKeyType_Object::GetRotation(const uint8* RawData, FRotator& Rotation) const
{
	AActor* MyActor = Cast<AActor>(GetValue(RawData));
	if (MyActor)
	{
		Rotation = MyActor->GetActorRotation();
		return true;
	}

	return false;
}

EBlackboardCompare::Type UBlackboardKeyType_Object::Compare(const uint8* MemoryBlockA, const uint8* MemoryBlockB) const
{
	return (FMemory::Memcmp(MemoryBlockA, MemoryBlockB, ValueSize) == 0) ? EBlackboardCompare::Equal : EBlackboardCompare::NotEqual;
}

bool UBlackboardKeyType_Object::TestBasicOperation(const uint8* MemoryBlock, EBasicKeyOperation::Type Op) const
{
	FWeakObjectPtr WeakObjPtr = GetValueFromMemory<FWeakObjectPtr>(MemoryBlock);
	return (Op == EBasicKeyOperation::Set) ? WeakObjPtr.IsValid() : !WeakObjPtr.IsValid();
}
