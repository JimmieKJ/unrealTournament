// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

class UBTNode;
class UBehaviorTreeComponent;
class UBlackboardData;

namespace BlueprintNodeHelpers
{
	FString CollectPropertyDescription(const UObject* Ob, const UClass* StopAtClass, const TArray<UProperty*>& PropertyData);
	void CollectPropertyData(const UObject* Ob, const UClass* StopAtClass, TArray<UProperty*>& PropertyData);
	uint16 GetPropertiesMemorySize(const TArray<UProperty*>& PropertyData);

	void CollectBlackboardSelectors(const UObject* Ob, const UClass* StopAtClass, TArray<FName>& KeyNames);
	void ResolveBlackboardSelectors(UObject& Ob, const UClass& StopAtClass, const UBlackboardData& BlackboardAsset);
	bool HasAnyBlackboardSelectors(const UObject* Ob, const UClass* StopAtClass);

	AIMODULE_API FString DescribeProperty(const UProperty* Prop, const uint8* PropertyAddr);
	void DescribeRuntimeValues(const UObject* Ob, const TArray<UProperty*>& PropertyData, TArray<FString>& RuntimeValues);

	void CopyPropertiesToContext(const TArray<UProperty*>& PropertyData, uint8* ObjectMemory, uint8* ContextMemory);
	void CopyPropertiesFromContext(const TArray<UProperty*>& PropertyData, uint8* ObjectMemory, uint8* ContextMemory);

	bool FindNodeOwner(AActor* OwningActor, UBTNode* Node, UBehaviorTreeComponent*& OwningComp, int32& OwningInstanceIdx);

	void AbortLatentActions(UActorComponent& OwnerOb, const UObject& Ob);

	FORCEINLINE bool HasBlueprintFunction(FName FuncName, const UObject& Object, const UClass& StopAtClass)
	{
		const UFunction* Function = Object.GetClass()->FindFunctionByName(FuncName);
		ensure(Function);
		return (Function != nullptr) && (Function->GetOuter() != &StopAtClass);
	}

	FORCEINLINE FString GetNodeName(const UObject& NodeObject)
	{
		return NodeObject.GetClass()->GetName().LeftChop(2);
	}

	//----------------------------------------------------------------------//
	// DEPRECATED
	//----------------------------------------------------------------------//
	DEPRECATED(4.7, "This version is deprecated. Please use the one taking reference to UActorComponent rather than a pointer.")
	void AbortLatentActions(UActorComponent* OwnerOb, const UObject* Ob);

	DEPRECATED(4.11, "This version of HasBlueprintFunction is deprecated. Please use the one taking reference to UObject and StopAtClass rather than a pointers.")
	AIMODULE_API bool HasBlueprintFunction(FName FuncName, const UObject* Object, const UClass* StopAtClass);
}
