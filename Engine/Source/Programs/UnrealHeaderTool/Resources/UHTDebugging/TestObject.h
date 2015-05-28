// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EnumOnlyHeader.h"
#include "TestObject.generated.h"

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
	ECppEnum EnumProperty;

	UPROPERTY()
	TMap<int32, FString> TestMap;

	UFUNCTION()
	void CodeGenTestForEnumClasses(ECppEnum Val);

	UFUNCTION(Category="Xyz", BlueprintCallable)
	TArray<UClass*> ReturnArrayOfUClassPtrs();

	UFUNCTION()
	int32 InlineFunc()
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
