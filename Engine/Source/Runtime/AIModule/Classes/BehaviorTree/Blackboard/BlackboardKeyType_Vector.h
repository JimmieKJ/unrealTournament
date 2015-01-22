// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "BehaviorTree/Blackboard/BlackboardKeyType.h"
#include "BlackboardKeyType_Vector.generated.h"

UCLASS(EditInlineNew, meta=(DisplayName="Vector"))
class AIMODULE_API UBlackboardKeyType_Vector : public UBlackboardKeyType
{
	GENERATED_UCLASS_BODY()

	typedef FVector FDataType; 
	static const FDataType InvalidValue;
	
	static FVector GetValue(const uint8* RawData);
	static bool SetValue(uint8* RawData, const FVector& Value);

	virtual void Initialize(uint8* RawData) const override;
	virtual FString DescribeValue(const uint8* RawData) const override;
	virtual bool GetLocation(const uint8* RawData, FVector& Location) const override;
	virtual bool Clear(uint8* RawData) const override;
	virtual EBlackboardCompare::Type Compare(const uint8* MemoryBlockA, const uint8* MemoryBlockB) const override;
	virtual bool TestBasicOperation(const uint8* MemoryBlock, EBasicKeyOperation::Type Op) const override;
};
