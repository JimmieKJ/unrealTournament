// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "BehaviorTree/Blackboard/BlackboardKeyType.h"
#include "BlackboardKeyType_Bool.generated.h"

UCLASS(EditInlineNew, meta=(DisplayName="Bool"))
class AIMODULE_API UBlackboardKeyType_Bool : public UBlackboardKeyType
{
	GENERATED_UCLASS_BODY()

	typedef bool FDataType;
	static const FDataType InvalidValue;

	static bool GetValue(const uint8* RawData);
	static bool SetValue(uint8* RawData, bool bValue);

	virtual FString DescribeValue(const uint8* RawData) const override;
	virtual EBlackboardCompare::Type Compare(const uint8* MemoryBlockA, const uint8* MemoryBlockB) const override;
	virtual bool TestBasicOperation(const uint8* MemoryBlock, EBasicKeyOperation::Type Op) const override;
};
