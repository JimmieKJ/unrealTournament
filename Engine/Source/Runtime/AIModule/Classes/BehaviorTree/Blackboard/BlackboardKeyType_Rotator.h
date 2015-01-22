// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "BlackboardKeyType_Rotator.generated.h"

UCLASS(EditInlineNew, meta=(DisplayName="Rotator"))
class AIMODULE_API UBlackboardKeyType_Rotator : public UBlackboardKeyType
{
	GENERATED_UCLASS_BODY()
	
	typedef FRotator FDataType; 
	static const FDataType InvalidValue;

	static FRotator GetValue(const uint8* RawData);
	static bool SetValue(uint8* RawData, const FRotator& Value);

	virtual void Initialize(uint8* RawData) const override;
	virtual FString DescribeValue(const uint8* RawData) const override;
	virtual bool GetRotation(const uint8* RawData, FRotator& Rotation) const override;
	virtual bool Clear(uint8* MemoryBlock) const override;
	virtual EBlackboardCompare::Type Compare(const uint8* MemoryBlockA, const uint8* MemoryBlockB) const override;
	virtual bool TestBasicOperation(const uint8* MemoryBlock, EBasicKeyOperation::Type Op) const override;
};
