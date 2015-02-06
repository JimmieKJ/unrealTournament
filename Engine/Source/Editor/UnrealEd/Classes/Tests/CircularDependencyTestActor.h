// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "CircularDependencyTestActor.generated.h"

UENUM(BlueprintType)
namespace ETestResult
{
	enum Type
	{
		Unknown,
		Failed,
		Succeeded
	};
}

UCLASS(Abstract, Blueprintable, BlueprintType, meta=(ChildCanTick))
class ACircularDependencyTestActor : public AActor
{
	GENERATED_UCLASS_BODY()
public:

	UFUNCTION(BlueprintCallable, Category="Development|Tests")
	bool SetTestState(ETestResult::Type NewState);

	UFUNCTION(BlueprintImplementableEvent, Category="Development|Tests")
	void VisualizeNewTestState(ETestResult::Type OldState, ETestResult::Type NewState);

	UFUNCTION(BlueprintCallable, Category="Development|Tests")
	bool TestVerifyClass(UClass* Class, bool bCheckPropertyType = true, bool bCheckPropertyVals = true);

	UFUNCTION(BlueprintCallable, Category = "Development|Tests")
	bool TestVerifyBlueprint(UClass* BPClass);

	//UFUNCTION(BlueprintCallable, Category="Development|Tests")
	bool TestVerifyStructMember(UScriptStruct* Struct, UClass* AncestorClass, uint8* Container, bool bCheckPropertyType = true, bool bCheckPropertyVals = true);

	UFUNCTION(BlueprintCallable, Category = "Development|Tests")
	bool TestVerifySubObjects(UObject* ObjInstace);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Development|Tests")
	bool RunVerificationTests();

	UFUNCTION(BlueprintCallable, Category = "Development|Tests")
	bool TestObjectValidity(UObject* ObjInst, bool bCheckPropertyType = true, bool bCheckPropertyVals = true);

	UFUNCTION(BlueprintCallable, Category="Development|Tests")
	bool TestVerifyIsBlueprintTypeVar(FName VarName, UClass* ClassOuter, bool bCheckPropertyType, bool bCheckPropertyVal);

	UFUNCTION(BlueprintCallable, Category="Development|Tests")
	void ForceResetFailure();

	UFUNCTION(BlueprintCallable, Category="Development|Tests")
	bool CheckObjectValIsBlueprint(UObject* BPObject);

protected:
	UPROPERTY(BlueprintReadOnly, Category="Development|Tests")
	TEnumAsByte<ETestResult::Type> TestState;

private: 
	bool TestVerifyClass(UClass* Class, UObject* ObjInst, bool bCheckPropertyType, bool bCheckPropertyVals);
	bool TestVerifyProperty(UProperty* Property, UClass* PropOwner, uint8* Container, bool bCheckPropertyType, bool bCheckPropertyVal);
};
