// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "BehaviorTree/Blackboard/BlackboardKeyType.h"
#include "BlackboardKeyType_Float.generated.h"

UCLASS(EditInlineNew, meta=(DisplayName="Float"))
class AIMODULE_API UBlackboardKeyType_Float : public UBlackboardKeyType
{
	GENERATED_UCLASS_BODY()

	typedef float FDataType;
	static const FDataType InvalidValue;

	static float GetValue(const uint8* RawData);
	static bool SetValue(uint8* RawData, float Value);

	virtual FString DescribeValue(const uint8* RawData) const override;
	virtual EBlackboardCompare::Type Compare(const uint8* MemoryBlockA, const uint8* MemoryBlockB) const override;
	virtual bool TestArithmeticOperation(const uint8* MemoryBlock, EArithmeticKeyOperation::Type Op, int32 OtherIntValue, float OtherFloatValue) const override;
	virtual FString DescribeArithmeticParam(int32 IntValue, float FloatValue) const override;
};
