// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "Tests/CircularDependencyTestActor.h"
#include "EdGraphSchema_K2.h"
#include "EdGraph/EdGraph.h"
#include "K2Node.h"
#include "EdGraph/EdGraphPin.h"
#include "TokenizedMessage.h"

ACircularDependencyTestActor::ACircularDependencyTestActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, TestState(ETestResult::Unknown)
{
}

bool ACircularDependencyTestActor::SetTestState(ETestResult::Type NewState)
{
	VisualizeNewTestState(TestState, NewState);
	if (TestState == ETestResult::Failed)
	{
		return (NewState == ETestResult::Failed);
	}
	
	TestState = NewState;
	return true;
}

static bool IsPlaceholderClass(UClass* TestClass)
{
	//return Cast<ULinkerPlaceholderClass>(TestClass) != nullptr;
	return TestClass->GetName().Contains(TEXT("PLACEHOLDER"));
}

static bool IsValidClass(UClass* ClassToVerify)
{
	return (ClassToVerify != nullptr) && !IsPlaceholderClass(ClassToVerify);
};

bool ACircularDependencyTestActor::TestVerifyClass(UClass* Class, bool bCheckPropertyType, bool bCheckPropertyVals)
{
	UObject* ClassInst = this;
	if (Class && !GetClass()->IsChildOf(Class))
	{
		ClassInst = Class->GetDefaultObject();
	}
	return TestVerifyClass(Class, ClassInst, bCheckPropertyType, bCheckPropertyVals);
}

bool ACircularDependencyTestActor::TestVerifyClass(UClass* BPClass, UObject* ClassInst, bool bCheckPropertyType, bool bCheckPropertyVals)
{
	if (ClassInst == nullptr || !ClassInst->GetClass()->IsChildOf(BPClass))
	{
		return false;
	}

	if (Cast<UBlueprintGeneratedClass>(BPClass) == nullptr)
	{
		return false;
	}

// 	if (ULinkerLoad* Linker = BPClass->GetLinker())
// 	{
// // 		if (Linker->HasUnresolvedDependencies() || Linker->IsBlueprintFinalizationPending())
// // 		{
// // 			return false;
// // 		}
// 	}
// 	else
// 	{
// 		return false;
// 	}

	for (UProperty* ClassProp = BPClass->PropertyLink; ClassProp != nullptr; ClassProp = ClassProp->PropertyLinkNext)
	{
		if (!TestVerifyProperty(ClassProp, BPClass, (uint8*)ClassInst, bCheckPropertyType, bCheckPropertyVals))
		{
			//SetTestState(ETestResult::Failed);
			return false;
		}

		if (UStructProperty* StructProp = Cast<UStructProperty>(ClassProp))
		{
			uint8* StructInst = StructProp->ContainerPtrToValuePtr<uint8>(ClassInst);
			if ((StructProp->Struct == nullptr) || !TestVerifyStructMember(StructProp->Struct, BPClass, (uint8*)StructInst, bCheckPropertyType))
			{
				//SetTestState(ETestResult::Failed);
				return false;
			}
		}
	}
	return true;
}

bool ACircularDependencyTestActor::TestVerifyBlueprint(UClass* BPClass)
{
#if WITH_EDITOR
	if (UBlueprint* Blueprint = Cast<UBlueprint>(BPClass->ClassGeneratedBy))
	{
		TArray<UEdGraph*> BpGraphs;
		Blueprint->GetAllGraphs(BpGraphs);

		bool bAllValidNodes = true;
		for (UEdGraph* Graph : BpGraphs)
		{
			TArray<UObject*> GraphObjects;
			GetObjectsWithOuter(Graph, GraphObjects, /*bIncludeNestedObjects =*/true);

			for (UObject* GraphObj : GraphObjects)
			{
				if (UProperty* GraphProperty = Cast<UProperty>(GraphObj))
				{
					// @TODO: do we even have graph owned properties? no?
				}
				else if (UEdGraphNode* EdNode = Cast<UEdGraphNode>(GraphObj))
				{
					if (EdNode->bHasCompilerMessage && EdNode->ErrorType >= EMessageSeverity::Warning)
					{
						SetTestState(ETestResult::Failed);
						return false;
					}
				}
				else if (UEdGraphPin* NodePin = Cast<UEdGraphPin>(GraphObj))
				{
					FEdGraphPinType& PinType = NodePin->PinType;
					if (PinType.PinCategory == UEdGraphSchema_K2::PC_Class)
					{
						if (!IsValidClass(Cast<UClass>(PinType.PinSubCategoryObject.Get())) || 
							(NodePin->DefaultObject && !IsValidClass(Cast<UClass>(NodePin->DefaultObject))) )
						{
							SetTestState(ETestResult::Failed);
							return false;
						}
					}
					else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Object)
					{
						if ((PinType.PinSubCategory != UEdGraphSchema_K2::PSC_Self) &&
							!IsValidClass(Cast<UClass>(PinType.PinSubCategoryObject.Get())) )
						{
							SetTestState(ETestResult::Failed);
							return false;
						}
						else if ( NodePin->DefaultObject && !IsValidClass(NodePin->DefaultObject->GetClass()) )
						{
							SetTestState(ETestResult::Failed);
							return false;
						}
					}
					else if ( (PinType.PinCategory == UEdGraphSchema_K2::PC_Interface)
						&& !IsValidClass(Cast<UClass>(PinType.PinSubCategoryObject.Get())) )
					{
						SetTestState(ETestResult::Failed);
						return false;
					}
				}
			}
		}
	}
#endif // WITH_EDITOR
	return true;
}

bool ACircularDependencyTestActor::TestVerifyStructMember(UScriptStruct* Struct, UClass* AncestorClass, uint8* StructInst, bool bCheckPropertyType, bool bCheckPropertyVals)
{
	for (UProperty* StructProp = Struct->PropertyLink; StructProp != nullptr; StructProp = StructProp->NextRef)
	{
		if (!TestVerifyProperty(StructProp, AncestorClass, StructInst, bCheckPropertyType, bCheckPropertyVals))
		{
			SetTestState(ETestResult::Failed);
			return false;
		}

		if (UStructProperty* StructProp2 = Cast<UStructProperty>(StructProp))
		{
			uint8* StructInst2 = StructProp2->ContainerPtrToValuePtr<uint8>(StructInst);
			if ((StructProp2->Struct == nullptr) || !TestVerifyStructMember(StructProp2->Struct, AncestorClass, StructInst2, bCheckPropertyType))
			{
				SetTestState(ETestResult::Failed);
				return false;
			}
		}
	}
	return true;
}

bool ACircularDependencyTestActor::TestVerifySubObjects(UObject* ObjInstace)
{
	TArray<UObject*> SubObjects;
	GetObjectsWithOuter(ObjInstace, SubObjects);

	TArray<FString> Errors;

	for (auto SubObjIt : SubObjects)
	{
		const UObject* SubObj = SubObjIt;
		const UClass*  SubObjClass = SubObj->GetClass();
		const FString  SubObjName  = SubObj->GetName();

		if (SubObj->IsPendingKill())
		{
			continue;
		}

		if (SubObjClass->HasAnyClassFlags(CLASS_NewerVersionExists))
		{
			return false;
		}

		if (SubObjClass->GetOutermost() == GetTransientPackage())
		{
			return false;
		}

		if (SubObj->GetOutermost() != ObjInstace->GetOutermost())
		{
			return false;
		}

		if (SubObj->GetName().Find(TEXT("TRASH_")) != INDEX_NONE)
		{
			return false;
		}

		if (SubObj->GetName().Find(TEXT("REINST_")) != INDEX_NONE)
		{
			return false;
		}

		if (SubObj->GetName().Find(TEXT("PLACEHOLDER-")) != INDEX_NONE)
		{
			return false;
		}
	}

	if (AActor* AsActor = Cast<AActor>(ObjInstace))
	{
		for (UActorComponent* Component : AsActor->GetComponents())
		{
			if (!TestVerifySubObjects(Component))
			{
				return false;
			}
		}
	}
	return true;
}

bool ACircularDependencyTestActor::TestVerifyIsBlueprintTypeVar(FName VarName, UClass* ClassOuter, bool bCheckPropertyType, bool bCheckPropertyVal)
{
	FMemberReference VarRef;
	VarRef.SetExternalMember(VarName, ClassOuter);

	UObject* ClassInst = this;
	if (ClassOuter != GetClass())
	{
		ClassInst = ClassOuter->GetDefaultObject();
	}

	UProperty* VarProperty = VarRef.ResolveMember<UProperty>(GetClass());
	if (UObjectProperty* ObjProp = Cast<UObjectProperty>(VarProperty))
	{
		UClass* ClassToVerify = ObjProp->PropertyClass;
		if (UClassProperty* ClassProp = Cast<UClassProperty>(ObjProp))
		{
			ClassToVerify = Cast<UClass>(ClassProp->GetPropertyValue_InContainer(ClassInst));
		}
		else if (bCheckPropertyVal)
		{
			UObject* ObjVal = ObjProp->GetPropertyValue_InContainer(ClassInst);
			if (UChildActorComponent* AsChildComponent = Cast<UChildActorComponent>(ObjVal))
			{
				ObjVal = AsChildComponent->ChildActor;
			}
			if ((ObjVal != nullptr) && !CheckObjectValIsBlueprint(ObjVal))
			{
				return false;
			}
		}

		bCheckPropertyType &= !ClassToVerify->IsChildOf<UChildActorComponent>();
		if (bCheckPropertyType && Cast<UBlueprintGeneratedClass>(ClassToVerify) != nullptr)
		{
			return true;
		}
	}
	else if (UInterfaceProperty* InterfaceProp = Cast<UInterfaceProperty>(VarProperty))
	{
		if (bCheckPropertyType && Cast<UBlueprintGeneratedClass>(InterfaceProp->InterfaceClass) != nullptr)
		{
			return true;
		}
	}
	return !bCheckPropertyType;
}

bool ACircularDependencyTestActor::RunVerificationTests_Implementation()
{
	return TestObjectValidity(this);
}

bool ACircularDependencyTestActor::TestObjectValidity(UObject* ObjInst, bool bCheckPropertyType, bool bCheckPropertyVals)
{
	return TestVerifyClass(ObjInst->GetClass(), ObjInst, bCheckPropertyType, bCheckPropertyVals) && TestVerifyBlueprint(ObjInst->GetClass()) && TestVerifySubObjects(ObjInst);// &&
		//(ObjInst->GetClass()->GetDefaultObject<ACircularDependencyTestActor>()->TestState != ETestResult::Failed);
}

bool ACircularDependencyTestActor::TestVerifyProperty(UProperty* Property, UClass* PropOwner, uint8* Container, bool bCheckPropertyType, bool bCheckPropertyVal)
{
	bool bBelongsToThisBlueprintClass = false;
	if (UClass* OwnerClass = Property->GetOwnerClass())
	{
		bBelongsToThisBlueprintClass = (OwnerClass == PropOwner) && (Cast<UBlueprintGeneratedClass>(OwnerClass) != nullptr);
		if (UClass* SuperClass = OwnerClass->GetSuperClass())
		{
			FMemberReference VarRef;
			VarRef.SetExternalMember(Property->GetFName(), SuperClass);
			// make sure the parent doesn't have this variable
			bBelongsToThisBlueprintClass &= (VarRef.ResolveMember<UProperty>(PropOwner) == nullptr);
		}
	}
	else if (UStruct* OwnerStruct = Property->GetOwnerStruct())
	{
		// @TODO
	}

	if (UObjectProperty* ObjProp = Cast<UObjectProperty>(Property))
	{
		UClass* ClassToVerify = ObjProp->PropertyClass;
		if (UClassProperty* ClassProp = Cast<UClassProperty>(ObjProp))
		{
			ClassToVerify = Cast<UClass>(ClassProp->GetPropertyValue_InContainer(Container));
		}

		if (!IsValidClass(ClassToVerify))
		{
			return false;
		}		

		if (bBelongsToThisBlueprintClass)
		{
			// assume any object properties that we've added to the BP 
			// directly should be circular refs (and thus, pointers to other blueprint types) 
			return TestVerifyIsBlueprintTypeVar(Property->GetFName(), PropOwner, bCheckPropertyType, bCheckPropertyVal);
		}
	}
	else if (UInterfaceProperty* InterfaceProp = Cast<UInterfaceProperty>(Property))
	{
		if (!IsValidClass(InterfaceProp->InterfaceClass))
		{
			return false;
		}

		if (bBelongsToThisBlueprintClass)
		{
			// assume any object properties that we've added to the BP 
			// directly should be circular refs (and thus, pointers to other blueprint types) 
			return TestVerifyIsBlueprintTypeVar(Property->GetFName(), PropOwner, bCheckPropertyType, bCheckPropertyVal);
		}
	}

	return true;
}

void ACircularDependencyTestActor::ForceResetFailure()
{
	if (TestState == ETestResult::Failed)
	{
		VisualizeNewTestState(TestState, ETestResult::Unknown);
		TestState = ETestResult::Unknown;
	}
}

bool ACircularDependencyTestActor::CheckObjectValIsBlueprint(UObject* BPObject)
{
	return (BPObject != nullptr) && (Cast<UBlueprintGeneratedClass>(BPObject->GetClass()) != nullptr);
}