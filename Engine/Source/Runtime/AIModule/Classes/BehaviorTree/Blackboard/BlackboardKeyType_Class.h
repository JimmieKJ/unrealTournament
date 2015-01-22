// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "BehaviorTree/Blackboard/BlackboardKeyType.h"
#include "BlackboardKeyType_Class.generated.h"

UCLASS(EditInlineNew, meta=(DisplayName="Class"))
class AIMODULE_API UBlackboardKeyType_Class : public UBlackboardKeyType
{
	GENERATED_UCLASS_BODY()

	typedef UClass* FDataType;
	static const FDataType InvalidValue;

	UPROPERTY(Category=Blackboard, EditDefaultsOnly, meta=(AllowAbstract="1"))
	UClass* BaseClass;
	
	static UClass* GetValue(const uint8* RawData);
	static bool SetValue(uint8* RawData, UClass* Value);

	virtual FString DescribeValue(const uint8* RawData) const override;
	virtual FString DescribeSelf() const override;
	virtual bool IsAllowedByFilter(UBlackboardKeyType* FilterOb) const override;
	virtual EBlackboardCompare::Type Compare(const uint8* MemoryBlockA, const uint8* MemoryBlockB) const override;
	virtual bool TestBasicOperation(const uint8* MemoryBlock, EBasicKeyOperation::Type Op) const override;
};
