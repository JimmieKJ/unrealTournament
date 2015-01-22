// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "BehaviorTree/Blackboard/BlackboardKeyType.h"
#include "BlackboardKeyType_String.generated.h"

UCLASS(EditInlineNew, meta=(DisplayName="String"))
class AIMODULE_API UBlackboardKeyType_String : public UBlackboardKeyType
{
	GENERATED_UCLASS_BODY()

	typedef FString FDataType;
	static const FDataType InvalidValue;

	static FString GetValue(const uint8* RawData);
	static bool SetValue(uint8* RawData, const FString& Value);

	virtual FString DescribeValue(const uint8* RawData) const override;
	virtual EBlackboardCompare::Type Compare(const uint8* MemoryBlockA, const uint8* MemoryBlockB) const override;
	virtual bool TestTextOperation(const uint8* MemoryBlock, ETextKeyOperation::Type Op, const FString& OtherString) const override;
};
