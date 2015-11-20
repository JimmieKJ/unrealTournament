// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "BehaviorTree/BTFunctionLibrary.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyAllTypes.h"
#include "BehaviorTree/BTFunctionLibrary.h"

//----------------------------------------------------------------------//
// 
//----------------------------------------------------------------------//
namespace FBTNodeBPImplementationHelper
{
	int32 CheckEventImplementationVersion(FName GenericEventName, FName AIEventName, const UObject* Ob, const UClass* StopAtClass)
	{
		const bool bGeneric = BlueprintNodeHelpers::HasBlueprintFunction(GenericEventName, Ob, StopAtClass);
		const bool bAI = BlueprintNodeHelpers::HasBlueprintFunction(AIEventName, Ob, StopAtClass);
		
		return (bGeneric ? Generic : NoImplementation) | (bAI ? AISpecific : NoImplementation);
	}
}
//----------------------------------------------------------------------//
// 
//----------------------------------------------------------------------//
namespace
{
	FORCEINLINE UBlackboardComponent* GetBlackboard(UBTNode& BTNode)
	{
		check(BTNode.GetOuter());
		check(BTNode.GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint) && "This function call is valid only for BP-implemented BT nodes");
		check(Cast<UBehaviorTreeComponent>(BTNode.GetOuter()));

		return ((UBehaviorTreeComponent*)BTNode.GetOuter())->GetBlackboardComponent();
	}
}

UBehaviorTreeComponent* UBTFunctionLibrary::GetOwnerComponent(UBTNode* NodeOwner)
{
	check(NodeOwner);
	check(NodeOwner->GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint) && "This function call is valid only for BP-implemented BT nodes");
	check(NodeOwner->GetOuter());

	UBehaviorTreeComponent* OwnerComp = Cast<UBehaviorTreeComponent>(NodeOwner->GetOuter());
	check(OwnerComp);

	return OwnerComp;
}

UBlackboardComponent* UBTFunctionLibrary::GetOwnersBlackboard(UBTNode* NodeOwner)
{
	check(NodeOwner != NULL);
	return GetBlackboard(*NodeOwner);
}

UObject* UBTFunctionLibrary::GetBlackboardValueAsObject(UBTNode* NodeOwner, const FBlackboardKeySelector& Key)
{
	check(NodeOwner != NULL);
	const UBlackboardComponent* BlackboardComp = GetBlackboard(*NodeOwner);
	return BlackboardComp ? BlackboardComp->GetValueAsObject(Key.SelectedKeyName) : NULL;
}

AActor* UBTFunctionLibrary::GetBlackboardValueAsActor(UBTNode* NodeOwner, const FBlackboardKeySelector& Key)
{
	check(NodeOwner != NULL);
	const UBlackboardComponent* BlackboardComp = GetBlackboard(*NodeOwner);
	return BlackboardComp ? Cast<AActor>(BlackboardComp->GetValueAsObject(Key.SelectedKeyName)) : NULL;
}

UClass* UBTFunctionLibrary::GetBlackboardValueAsClass(UBTNode* NodeOwner, const FBlackboardKeySelector& Key)
{
	check(NodeOwner != NULL);
	const UBlackboardComponent* BlackboardComp = GetBlackboard(*NodeOwner);
	return BlackboardComp ? BlackboardComp->GetValueAsClass(Key.SelectedKeyName) : NULL;
}

uint8 UBTFunctionLibrary::GetBlackboardValueAsEnum(UBTNode* NodeOwner, const FBlackboardKeySelector& Key)
{
	check(NodeOwner != NULL);
	const UBlackboardComponent* BlackboardComp = GetBlackboard(*NodeOwner);
	return BlackboardComp ? BlackboardComp->GetValueAsEnum(Key.SelectedKeyName) : 0;
}

int32 UBTFunctionLibrary::GetBlackboardValueAsInt(UBTNode* NodeOwner, const FBlackboardKeySelector& Key)
{
	check(NodeOwner != NULL);
	const UBlackboardComponent* BlackboardComp = GetBlackboard(*NodeOwner);
	return BlackboardComp ? BlackboardComp->GetValueAsInt(Key.SelectedKeyName) : 0;
}

float UBTFunctionLibrary::GetBlackboardValueAsFloat(UBTNode* NodeOwner, const FBlackboardKeySelector& Key)
{
	check(NodeOwner != NULL);
	const UBlackboardComponent* BlackboardComp = GetBlackboard(*NodeOwner);
	return BlackboardComp ? BlackboardComp->GetValueAsFloat(Key.SelectedKeyName) : 0.0f;
}

bool UBTFunctionLibrary::GetBlackboardValueAsBool(UBTNode* NodeOwner, const FBlackboardKeySelector& Key)
{
	check(NodeOwner != NULL);
	const UBlackboardComponent* BlackboardComp = GetBlackboard(*NodeOwner);
	return BlackboardComp ? BlackboardComp->GetValueAsBool(Key.SelectedKeyName) : false;
}

FString UBTFunctionLibrary::GetBlackboardValueAsString(UBTNode* NodeOwner, const FBlackboardKeySelector& Key)
{
	check(NodeOwner != NULL);
	const UBlackboardComponent* BlackboardComp = GetBlackboard(*NodeOwner);
	return BlackboardComp ? BlackboardComp->GetValueAsString(Key.SelectedKeyName) : FString();
}

FName UBTFunctionLibrary::GetBlackboardValueAsName(UBTNode* NodeOwner, const FBlackboardKeySelector& Key)
{
	check(NodeOwner != NULL);
	const UBlackboardComponent* BlackboardComp = GetBlackboard(*NodeOwner);
	return BlackboardComp ? BlackboardComp->GetValueAsName(Key.SelectedKeyName) : NAME_None;
}

FVector UBTFunctionLibrary::GetBlackboardValueAsVector(UBTNode* NodeOwner, const FBlackboardKeySelector& Key)
{
	check(NodeOwner != NULL);
	const UBlackboardComponent* BlackboardComp = GetBlackboard(*NodeOwner);
	return BlackboardComp ? BlackboardComp->GetValueAsVector(Key.SelectedKeyName) : FVector::ZeroVector;
}

FRotator UBTFunctionLibrary::GetBlackboardValueAsRotator(UBTNode* NodeOwner, const FBlackboardKeySelector& Key)
{
	check(NodeOwner != NULL);
	const UBlackboardComponent* BlackboardComp = GetBlackboard(*NodeOwner);
	return BlackboardComp ? BlackboardComp->GetValueAsRotator(Key.SelectedKeyName) : FRotator::ZeroRotator;
}

void UBTFunctionLibrary::SetBlackboardValueAsObject(UBTNode* NodeOwner, const FBlackboardKeySelector& Key, UObject* Value)
{
	check(NodeOwner != NULL);
	UBlackboardComponent* BlackboardComp = GetBlackboard(*NodeOwner);
	if (BlackboardComp != NULL)
	{
		BlackboardComp->SetValue<UBlackboardKeyType_Object>(Key.SelectedKeyName, Value);
	}
}

void UBTFunctionLibrary::SetBlackboardValueAsClass(UBTNode* NodeOwner, const FBlackboardKeySelector& Key, UClass* Value)
{
	check(NodeOwner != NULL);
	UBlackboardComponent* BlackboardComp = GetBlackboard(*NodeOwner);
	if (BlackboardComp != NULL)
	{
		BlackboardComp->SetValue<UBlackboardKeyType_Class>(Key.SelectedKeyName, Value);
	}
}

void UBTFunctionLibrary::SetBlackboardValueAsEnum(UBTNode* NodeOwner, const FBlackboardKeySelector& Key, uint8 Value)
{
	check(NodeOwner != NULL);
	UBlackboardComponent* BlackboardComp = GetBlackboard(*NodeOwner);
	if (BlackboardComp != NULL)
	{
		BlackboardComp->SetValue<UBlackboardKeyType_Enum>(Key.SelectedKeyName, Value);
	}
}

void UBTFunctionLibrary::SetBlackboardValueAsInt(UBTNode* NodeOwner, const FBlackboardKeySelector& Key, int32 Value)
{
	check(NodeOwner != NULL);
	UBlackboardComponent* BlackboardComp = GetBlackboard(*NodeOwner);
	if (BlackboardComp != NULL)
	{
		BlackboardComp->SetValue<UBlackboardKeyType_Int>(Key.SelectedKeyName, Value);
	}
}

void UBTFunctionLibrary::SetBlackboardValueAsFloat(UBTNode* NodeOwner, const FBlackboardKeySelector& Key, float Value)
{
	check(NodeOwner != NULL);
	UBlackboardComponent* BlackboardComp = GetBlackboard(*NodeOwner);
	if (BlackboardComp != NULL)
	{
		BlackboardComp->SetValue<UBlackboardKeyType_Float>(Key.SelectedKeyName, Value);
	}
}

void UBTFunctionLibrary::SetBlackboardValueAsBool(UBTNode* NodeOwner, const FBlackboardKeySelector& Key, bool Value)
{
	check(NodeOwner != NULL);
	UBlackboardComponent* BlackboardComp = GetBlackboard(*NodeOwner);
	if (BlackboardComp != NULL)
	{
		BlackboardComp->SetValue<UBlackboardKeyType_Bool>(Key.SelectedKeyName, Value);
	}
}

void UBTFunctionLibrary::SetBlackboardValueAsString(UBTNode* NodeOwner, const FBlackboardKeySelector& Key, FString Value)
{
	check(NodeOwner != NULL);
	UBlackboardComponent* BlackboardComp = GetBlackboard(*NodeOwner);
	if (BlackboardComp != NULL)
	{
		BlackboardComp->SetValue<UBlackboardKeyType_String>(Key.SelectedKeyName, Value);
	}
}

void UBTFunctionLibrary::SetBlackboardValueAsName(UBTNode* NodeOwner, const FBlackboardKeySelector& Key, FName Value)
{
	check(NodeOwner != NULL);
	UBlackboardComponent* BlackboardComp = GetBlackboard(*NodeOwner);
	if (BlackboardComp != NULL)
	{
		BlackboardComp->SetValue<UBlackboardKeyType_Name>(Key.SelectedKeyName, Value);
	}
}

void UBTFunctionLibrary::SetBlackboardValueAsVector(UBTNode* NodeOwner, const FBlackboardKeySelector& Key, FVector Value)
{
	check(NodeOwner != NULL);
	UBlackboardComponent* BlackboardComp = GetBlackboard(*NodeOwner);
	if (BlackboardComp != NULL)
	{
		BlackboardComp->SetValue<UBlackboardKeyType_Vector>(Key.SelectedKeyName, Value);
	}
}

void UBTFunctionLibrary::ClearBlackboardValueAsVector(UBTNode* NodeOwner, const FBlackboardKeySelector& Key)
{
	check(NodeOwner != NULL);
	UBlackboardComponent* BlackboardComp = GetBlackboard(*NodeOwner);
	if (BlackboardComp != NULL)
	{
		BlackboardComp->ClearValue(Key.SelectedKeyName);
	}
}

void UBTFunctionLibrary::SetBlackboardValueAsRotator(UBTNode* NodeOwner, const FBlackboardKeySelector& Key, FRotator Value)
{
	check(NodeOwner != NULL);
	UBlackboardComponent* BlackboardComp = GetBlackboard(*NodeOwner);
	if (BlackboardComp != NULL)
	{
		BlackboardComp->SetValue<UBlackboardKeyType_Rotator>(Key.SelectedKeyName, Value);
	}
}

void UBTFunctionLibrary::ClearBlackboardValue(UBTNode* NodeOwner, const FBlackboardKeySelector& Key)
{
	check(NodeOwner != NULL);
	UBlackboardComponent* BlackboardComp = GetBlackboard(*NodeOwner);
	if (BlackboardComp != NULL)
	{
		BlackboardComp->ClearValue(Key.SelectedKeyName);
	}
}

void UBTFunctionLibrary::StartUsingExternalEvent(UBTNode* NodeOwner, AActor* OwningActor)
{
	// deprecated, not removed yet
}

void UBTFunctionLibrary::StopUsingExternalEvent(UBTNode* NodeOwner)
{
	// deprecated, not removed yet
}
