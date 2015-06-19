// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class UBTNode;
class UBehaviorTreeComponent;

namespace BlueprintNodeHelpers
{
	FString CollectPropertyDescription(const UObject* Ob, const UClass* StopAtClass, const TArray<UProperty*>& PropertyData);
	void CollectPropertyData(const UObject* Ob, const UClass* StopAtClass, TArray<UProperty*>& PropertyData);
	uint16 GetPropertiesMemorySize(const TArray<UProperty*>& PropertyData);

	void CollectBlackboardSelectors(const UObject* Ob, const UClass* StopAtClass, TArray<FName>& KeyNames);

	ENGINE_API FString DescribeProperty(const UProperty* Prop, const uint8* PropertyAddr);
	void DescribeRuntimeValues(const UObject* Ob, const TArray<UProperty*>& PropertyData, TArray<FString>& RuntimeValues);

	void CopyPropertiesToContext(const TArray<UProperty*>& PropertyData, uint8* ObjectMemory, uint8* ContextMemory);
	void CopyPropertiesFromContext(const TArray<UProperty*>& PropertyData, uint8* ObjectMemory, uint8* ContextMemory);

	bool FindNodeOwner(AActor* OwningActor, UBTNode* Node, UBehaviorTreeComponent*& OwningComp, int32& OwningInstanceIdx);

	void AbortLatentActions(UActorComponent& OwnerOb, const UObject& Ob);

	FORCEINLINE bool HasBlueprintFunction(FName FuncName, const UObject* Ob, const UClass* StopAtClass)
	{
		return (Ob->GetClass()->FindFunctionByName(FuncName)->GetOuter() != StopAtClass);
	}

	FORCEINLINE FString GetNodeName(const UObject* Ob)
	{
		return Ob->GetClass()->GetName().LeftChop(2);
	}
}
