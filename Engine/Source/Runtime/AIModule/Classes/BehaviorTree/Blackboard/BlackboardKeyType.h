// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "BlackboardKeyType.generated.h"

class UBlackboardComponent;

namespace EBlackboardCompare
{
	enum Type
	{
		Less = -1, 	
		Equal = 0, 
		Greater = 1,

		NotEqual = 1,	// important, do not change
	};
}

namespace EBlackboardKeyOperation
{
	enum Type
	{
		Basic,
		Arithmetic,
		Text,
	};
}

UENUM()
namespace EBasicKeyOperation
{
	enum Type
	{
		Set				UMETA(DisplayName="Is Set"),
		NotSet			UMETA(DisplayName="Is Not Set"),
	};
}

UENUM()
namespace EArithmeticKeyOperation
{
	enum Type
	{
		Equal			UMETA(DisplayName="Is Equal To"),
		NotEqual		UMETA(DisplayName="Is Not Equal To"),
		Less			UMETA(DisplayName="Is Less Than"),
		LessOrEqual		UMETA(DisplayName="Is Less Than Or Equal To"),
		Greater			UMETA(DisplayName="Is Greater Than"),
		GreaterOrEqual	UMETA(DisplayName="Is Greater Than Or Equal To"),
	};
}

UENUM()
namespace ETextKeyOperation
{
	enum Type
	{
		Equal			UMETA(DisplayName="Is Equal To"),
		NotEqual		UMETA(DisplayName="Is Not Equal To"),
		Contain			UMETA(DisplayName="Contains"),
		NotContain		UMETA(DisplayName="Not Contains"),
	};
}

UCLASS(EditInlineNew, Abstract, CollapseCategories, AutoExpandCategories=(Blackboard))
class AIMODULE_API UBlackboardKeyType : public UObject
{
	GENERATED_UCLASS_BODY()
		
	/** get ValueSize */
	FORCEINLINE uint16 GetValueSize() const { return ValueSize; }

	/** get test supported by this type */
	FORCEINLINE EBlackboardKeyOperation::Type GetTestOperation() const { return SupportedOp; }

	/** initialize memory */
	virtual void Initialize(uint8* MemoryBlock) const;

	/** does it match settings in filter? */
	virtual bool IsAllowedByFilter(UBlackboardKeyType* FilterOb) const;

	/** extract location from entry */
	virtual bool GetLocation(const uint8* MemoryBlock, FVector& Location) const;

	/** extract rotation from entry */
	virtual bool GetRotation(const uint8* MemoryBlock, FRotator& Rotation) const;

	/** sets value to the default */
	virtual bool Clear(uint8* MemoryBlock) const;

	/** interprets given two memory blocks as "my" type and compares them */
	virtual EBlackboardCompare::Type Compare(const uint8* MemoryBlockA, const uint8* MemoryBlockB) const;

	/** various value testing, used by decorators */
	virtual bool TestBasicOperation(const uint8* MemoryBlock, EBasicKeyOperation::Type Op) const;
	virtual bool TestArithmeticOperation(const uint8* MemoryBlock, EArithmeticKeyOperation::Type Op, int32 OtherIntValue, float OtherFloatValue) const;
	virtual bool TestTextOperation(const uint8* MemoryBlock, ETextKeyOperation::Type Op, const FString& OtherString) const;

	/** describe params of arithmetic test */
	virtual FString DescribeArithmeticParam(int32 IntValue, float FloatValue) const;

	/** convert value to text */
	virtual FString DescribeValue(const uint8* MemoryBlock) const;

	/** description of params for property view */
	virtual FString DescribeSelf() const;

protected:

	/** size of value for this type */
	uint16 ValueSize;

	/** decorator operation supported with this type */
	TEnumAsByte<EBlackboardKeyOperation::Type> SupportedOp;

	/** helper function for reading typed data from memory block */
	template<typename T>
	static T GetValueFromMemory(const uint8* MemoryBlock)
	{
		return *((T*)MemoryBlock);
	}

	/** helper function for writing typed data to memory block, returns true if value has changed */
	template<typename T>
	static bool SetValueInMemory(uint8* MemoryBlock, const T& Value)
	{
		const bool bChanged = *((T*)MemoryBlock) != Value;
		*((T*)MemoryBlock) = Value;
		
		return bChanged;
	}

	/** helper function for witting weak object data to memory block, returns true if value has changed */
	template<typename T>
	static bool SetWeakObjectInMemory(uint8* MemoryBlock, const TWeakObjectPtr<T>& Value)
	{
		TWeakObjectPtr<T>* PrevValue = (TWeakObjectPtr<T>*)MemoryBlock;
		const bool bChanged =
			(Value.IsValid(false, true) != PrevValue->IsValid(false, true)) ||
			(Value.IsStale(false, true) != PrevValue->IsStale(false, true)) ||
			(*PrevValue) != Value;

		*((TWeakObjectPtr<T>*)MemoryBlock) = Value;

		return bChanged;
	}

	friend UBlackboardComponent;
	void CopyValue(uint8* To, const uint8* From) const
	{
		FMemory::Memcpy(To, From, GetValueSize());
	}
};
