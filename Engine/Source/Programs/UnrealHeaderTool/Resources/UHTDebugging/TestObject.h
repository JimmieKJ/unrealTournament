// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EnumOnlyHeader.h"
#include "TestObject.generated.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

DECLARE_DYNAMIC_DELEGATE_OneParam(FRegularDelegate, int32, SomeArgument);
DECLARE_DYNAMIC_DELEGATE_OneParam(FDelegateWithDelegateParam, FRegularDelegate const &, RegularDelegate);

UCLASS()
class UTestObject : public UObject
{
	GENERATED_BODY()

public:
	UTestObject(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category="Random")
	void TestForNullPtrDefaults(UObject* Obj1 = NULL, UObject* Obj2 = nullptr, UObject* Obj3 = 0);

	UPROPERTY()
	int32 Cpp11Init = 123;

	UPROPERTY()
	int RawInt;

	UPROPERTY()
	unsigned int RawUint;

	UFUNCTION()
	void FuncTakingRawInts(int Signed, unsigned int Unsigned);

	UPROPERTY()
	ECppEnum EnumProperty;

	UPROPERTY()
	TMap<int32, bool> TestMap;

	UPROPERTY()
	TSet<int32> TestSet;

	UFUNCTION()
	void CodeGenTestForEnumClasses(ECppEnum Val);

	UFUNCTION(Category="Xyz", BlueprintCallable)
	TArray<UClass*> ReturnArrayOfUClassPtrs();

	UFUNCTION()
	inline int32 InlineFunc1()
	{
		return FString("Hello").Len();
	}

	UFUNCTION()
	FORCEINLINE int32 InlineFunc2()
	{
		return FString("Hello").Len();
	}

	UFUNCTION()
	FORCEINLINE_WHATEVER int32 InlineFunc3()
	{
		return FString("Hello").Len();
	}

	UFUNCTION()
	FORCENOINLINE int32 NoInlineFunc()
	{
		return FString("Hello").Len();
	}

	UFUNCTION()
	int32 InlineFuncWithCppMacros()
#if CPP
	{
		return FString("Hello").Len();
	}
#endif

	UFUNCTION(BlueprintNativeEvent, Category="Game")
	UClass* BrokenReturnTypeForFunction();

#if 0
	UPROPERTY()
	ITestInterface* ShouldGiveAMeaningfulErrorAboutTScriptInterface;
#endif

#if BLAH
	UFUNCTION() int x; // This should not compile if UHT parses it, which it shouldn't
#elif BLAH2
	UFUNCTION() int x;
#else
	UFUNCTION() int x;
#endif

#ifdef X
	UFUNCTION() int x;
	#ifndef X
		UFUNCTION() int x;
	#elif BLAH4
		UFUNCTION() int x;
	#else
		UFUNCTION() int x;
	#endif
	UFUNCTION() int x;
#elif BLAH3
	UFUNCTION() int x;
	#ifndef X
		UFUNCTION() int x;
	#elif BLAH4
		UFUNCTION() int x;
	#else
		UFUNCTION() int x;
	#endif
	UFUNCTION() int x;
#else
	UFUNCTION() int x;
	#ifndef X
		UFUNCTION() int x;
	#elif BLAH4
		UFUNCTION() int x;
	#else
		UFUNCTION() int x;
	#endif
	UFUNCTION() int x;
#endif

#ifndef X
	UFUNCTION() int x;
#elif BLAH4
	UFUNCTION() int x;
#else
	UFUNCTION() int x;
#endif
};

PRAGMA_ENABLE_DEPRECATION_WARNINGS
