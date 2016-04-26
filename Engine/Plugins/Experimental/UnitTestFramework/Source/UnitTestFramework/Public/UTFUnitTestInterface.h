// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTFUnitTestInterface.generated.h"

/**  */
UENUM(BlueprintType)
enum class EUTFUnitTestResult
{
	UTF_Unresolved,
	UTF_Success,
	UTF_Failure,
};

/**  */
UINTERFACE(BlueprintType, Category="Unit Test Framework")
class UNITTESTFRAMEWORK_API UUTFUnitTestInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY() 
};

/**  */
class UNITTESTFRAMEWORK_API IUTFUnitTestInterface
{
	GENERATED_IINTERFACE_BODY()

public:
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Unit Test Framework", meta=(CallInEditor = "true"))
	FString GetTestName();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Unit Test Framework", meta=(CallInEditor = "true"))
	void ResetTest();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Unit Test Framework", meta=(CallInEditor = "true"))
	EUTFUnitTestResult GetTestResult();

protected:
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Unit Test Framework", meta=(CallInEditor = "true"))
	EUTFUnitTestResult RunUnitTest();
};
