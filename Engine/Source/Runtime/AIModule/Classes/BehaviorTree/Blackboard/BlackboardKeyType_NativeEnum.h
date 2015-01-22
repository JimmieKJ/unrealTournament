// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "BehaviorTree/Blackboard/BlackboardKeyType.h"
#include "BlackboardKeyType_NativeEnum.generated.h"

UCLASS(EditInlineNew, meta=(DisplayName="Native Enum"))
class AIMODULE_API UBlackboardKeyType_NativeEnum : public UBlackboardKeyType
{
	GENERATED_UCLASS_BODY()

	typedef uint8 FDataType;
	static const FDataType InvalidValue;

	UPROPERTY(Category=Blackboard, EditDefaultsOnly)
	FString EnumName;

	UPROPERTY(Category=Blackboard, VisibleDefaultsOnly)
	bool bIsEnumNameValid;

	UPROPERTY()
	UEnum* EnumType;

	static uint8 GetValue(const uint8* RawData);
	static bool SetValue(uint8* RawData, uint8 Value);

	virtual FString DescribeValue(const uint8* RawData) const override;
	virtual FString DescribeSelf() const override;
	virtual bool IsAllowedByFilter(UBlackboardKeyType* FilterOb) const override;
	virtual EBlackboardCompare::Type Compare(const uint8* MemoryBlockA, const uint8* MemoryBlockB) const override;
	virtual bool TestArithmeticOperation(const uint8* MemoryBlock, EArithmeticKeyOperation::Type Op, int32 OtherIntValue, float OtherFloatValue) const override;
	virtual FString DescribeArithmeticParam(int32 IntValue, float FloatValue) const override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
