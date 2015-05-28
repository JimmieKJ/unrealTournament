// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_NativeEnum.h"

const UBlackboardKeyType_NativeEnum::FDataType UBlackboardKeyType_NativeEnum::InvalidValue = UBlackboardKeyType_NativeEnum::FDataType(0);

UBlackboardKeyType_NativeEnum::UBlackboardKeyType_NativeEnum(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

UBlackboardKeyType* UBlackboardKeyType_NativeEnum::UpdateDeprecatedKey()
{
	UBlackboardKeyType_Enum* KeyOb = NewObject<UBlackboardKeyType_Enum>(GetOuter());
	KeyOb->EnumName = EnumName;
	KeyOb->EnumType = EnumType;
	KeyOb->bIsEnumNameValid = EnumType && !EnumName.IsEmpty();

	return KeyOb;
}

uint8 UBlackboardKeyType_NativeEnum::GetValue(const UBlackboardKeyType_NativeEnum* KeyOb, const uint8* RawData)
{
	return GetValueFromMemory<uint8>(RawData);
}

bool UBlackboardKeyType_NativeEnum::SetValue(UBlackboardKeyType_NativeEnum* KeyOb, uint8* RawData, uint8 Value)
{
	return SetValueInMemory<uint8>(RawData, Value);
}
