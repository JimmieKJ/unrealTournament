// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "BlueprintUtilities.h"
#include "Engine/InputDelegateBinding.h"
#include "Engine/TimelineTemplate.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Engine/LevelScriptActor.h"
#include "Engine/InheritableComponentHandler.h"
#include "CoreNet.h"

#if WITH_EDITOR
#include "BlueprintEditorUtils.h"
#include "KismetEditorUtilities.h"
#endif //WITH_EDITOR

DEFINE_STAT(STAT_PersistentUberGraphFrameMemory);

UBlueprintGeneratedClass::UBlueprintGeneratedClass(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NumReplicatedProperties = 0;
	bHasInstrumentation = false;
}

void UBlueprintGeneratedClass::PostInitProperties()
{
	Super::PostInitProperties();
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		// Default__BlueprintGeneratedClass uses its own AddReferencedObjects function.
		ClassAddReferencedObjects = &UBlueprintGeneratedClass::AddReferencedObjects;
	}
}

void UBlueprintGeneratedClass::PostLoad()
{
	Super::PostLoad();

	// Make sure the class CDO has been generated
	UObject* ClassCDO = GetDefaultObject();

	// Go through the CDO of the class, and make sure we don't have any legacy components that aren't instanced hanging on.
	TArray<UObject*> SubObjects;
	GetObjectsWithOuter(ClassCDO, SubObjects);

	struct FCheckIfComponentChildHelper
	{
		static bool IsComponentChild(UObject* CurrObj, const UObject* CDO)
		{
			UObject*  OuterObject = CurrObj ? CurrObj->GetOuter() : nullptr;
			const bool bValidOuter = OuterObject && (OuterObject != CDO);
			return bValidOuter ? (OuterObject->IsDefaultSubobject() || IsComponentChild(OuterObject, CDO)) : false;
		};
	};

	for( auto SubObjIt = SubObjects.CreateIterator(); SubObjIt; ++SubObjIt )
	{
		UObject* CurrObj = *SubObjIt;
		const bool bComponentChild = FCheckIfComponentChildHelper::IsComponentChild(CurrObj, ClassCDO);
		if (!CurrObj->IsDefaultSubobject() && !CurrObj->IsRooted() && !bComponentChild)
		{
			CurrObj->MarkPendingKill();
		}
	}

#if WITH_EDITORONLY_DATA
	if (GetLinkerUE4Version() < VER_UE4_CLASS_NOTPLACEABLE_ADDED)
	{
		// Make sure the placeable flag is correct for all blueprints
		UBlueprint* Blueprint = Cast<UBlueprint>(ClassGeneratedBy);
		if (ensure(Blueprint) && Blueprint->BlueprintType != BPTYPE_MacroLibrary)
		{
			ClassFlags &= ~CLASS_NotPlaceable;
		}
	}

	UPackage* Package = GetOutermost();
	if (Package != nullptr)
	{
		if (Package->HasAnyPackageFlags(PKG_ForDiffing))
		{
			ClassFlags |= CLASS_Deprecated;
		}
	}
#endif // WITH_EDITORONLY_DATA

#if UE_BLUEPRINT_EVENTGRAPH_FASTCALLS
	// Patch the fast calls (needed as we can't bump engine version to serialize it directly in UFunction right now)
	for (const FEventGraphFastCallPair& Pair : FastCallPairs_DEPRECATED)
	{
		Pair.FunctionToPatch->EventGraphFunction = UberGraphFunction;
		Pair.FunctionToPatch->EventGraphCallOffset = Pair.EventGraphCallOffset;
	}
#endif

	// Generate "fast path" instancing data for UCS/AddComponent node templates.
	if (CookedComponentInstancingData.Num() > 0)
	{
		for (int32 Index = ComponentTemplates.Num() - 1; Index >= 0; --Index)
		{
			if (UActorComponent* ComponentTemplate = ComponentTemplates[Index])
			{
				if (FBlueprintCookedComponentInstancingData* ComponentInstancingData = CookedComponentInstancingData.Find(ComponentTemplate->GetFName()))
				{
					ComponentInstancingData->LoadCachedPropertyDataForSerialization(ComponentTemplate);
				}
			}
		}
	}

	AssembleReferenceTokenStream(true);
}

void UBlueprintGeneratedClass::GetRequiredPreloadDependencies(TArray<UObject*>& DependenciesOut)
{
	Super::GetRequiredPreloadDependencies(DependenciesOut);

	// the component templates are no longer needed as Preload() dependencies 
	// (FLinkerLoad now handles these with placeholder export objects instead)...
	// this change was prompted by a cyclic case, where creating the first
	// component-template tripped the serialization of its class outer, before 
	// another second component-template could be created (even though the 
	// second component was listed in the ExportMap before the class)
// 	for (UActorComponent* Component : ComponentTemplates)
// 	{
// 		// because of the linker's way of handling circular dependencies (with 
// 		// placeholder blueprint classes), we need to ensure that class owned 
// 		// blueprint components are created before the class's  is 
// 		// ComponentTemplates member serialized in (otherwise, the component 
// 		// would be created as a ULinkerPlaceholderClass instance)
// 		//
// 		// by returning these in the DependenciesOut array, we're making it 
// 		// known that they should be prioritized in the package's ExportMap 
// 		// before this class
// 		if (Component->GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint))
// 		{
// 			DependenciesOut.Add(Component);
// 		}
// 	}
}

#if WITH_EDITOR

UClass* UBlueprintGeneratedClass::GetAuthoritativeClass()
{
	if (nullptr == ClassGeneratedBy) // to track UE-11597 and UE-11595
	{
		UE_LOG(LogBlueprint, Fatal, TEXT("UBlueprintGeneratedClass::GetAuthoritativeClass: ClassGeneratedBy is null. class '%s'"), *GetPathName());
	}

	UBlueprint* GeneratingBP = CastChecked<UBlueprint>(ClassGeneratedBy);

	check(GeneratingBP);

	return (GeneratingBP->GeneratedClass != NULL) ? GeneratingBP->GeneratedClass : this;
}

struct FConditionalRecompileClassHepler
{
	enum ENeededAction
	{
		None,
		StaticLink,
		Recompile,
	};

	static bool HasTheSameLayoutAsParent(const UStruct* Struct)
	{
		const UStruct* Parent = Struct ? Struct->GetSuperStruct() : NULL;
		return FStructUtils::TheSameLayout(Struct, Parent);
	}

	static ENeededAction IsConditionalRecompilationNecessary(const UBlueprint* GeneratingBP)
	{
		if (FBlueprintEditorUtils::IsInterfaceBlueprint(GeneratingBP))
		{
			return ENeededAction::None;
		}

		if (FBlueprintEditorUtils::IsDataOnlyBlueprint(GeneratingBP))
		{
			// If my parent is native, my layout wasn't changed.
			const UClass* ParentClass = *GeneratingBP->ParentClass;
			if (!GeneratingBP->GeneratedClass || (GeneratingBP->GeneratedClass->GetSuperClass() != ParentClass))
			{
				return ENeededAction::Recompile;
			}

			if (ParentClass && ParentClass->HasAllClassFlags(CLASS_Native))
			{
				return ENeededAction::None;
			}

			if (HasTheSameLayoutAsParent(*GeneratingBP->GeneratedClass))
			{
				return ENeededAction::StaticLink;
			}
			else
			{
				UE_LOG(LogBlueprint, Log, TEXT("During ConditionalRecompilation the layout of DataOnly BP should not be changed. It will be handled, but it's bad for performence. Blueprint %s"), *GeneratingBP->GetName());
			}
		}

		return ENeededAction::Recompile;
	}
};

extern UNREALED_API FSecondsCounterData BlueprintCompileAndLoadTimerData;

void UBlueprintGeneratedClass::ConditionalRecompileClass(TArray<UObject*>* ObjLoaded)
{
	FSecondsCounterScope Timer(BlueprintCompileAndLoadTimerData);

	UBlueprint* GeneratingBP = Cast<UBlueprint>(ClassGeneratedBy);
	if (GeneratingBP && (GeneratingBP->SkeletonGeneratedClass != this))
	{
		const auto NecessaryAction = FConditionalRecompileClassHepler::IsConditionalRecompilationNecessary(GeneratingBP);
		if (FConditionalRecompileClassHepler::Recompile == NecessaryAction)
		{
			const bool bWasRegenerating = GeneratingBP->bIsRegeneratingOnLoad;
			GeneratingBP->bIsRegeneratingOnLoad = true;

			{
				UPackage* const Package = Cast<UPackage>(GeneratingBP->GetOutermost());
				const bool bStartedWithUnsavedChanges = Package != nullptr ? Package->IsDirty() : true;

				// Make sure that nodes are up to date, so that we get any updated blueprint signatures
				FBlueprintEditorUtils::RefreshExternalBlueprintDependencyNodes(GeneratingBP);

				// Normal blueprints get their status reset by RecompileBlueprintBytecode, but macros will not:
				if ((GeneratingBP->Status != BS_Error) && (GeneratingBP->BlueprintType == EBlueprintType::BPTYPE_MacroLibrary))
				{
					GeneratingBP->Status = BS_UpToDate;
				}

				if (Package != nullptr && Package->IsDirty() && !bStartedWithUnsavedChanges)
				{
					Package->SetDirtyFlag(false);
				}
			}
			if ((GeneratingBP->Status != BS_Error) && (GeneratingBP->BlueprintType != EBlueprintType::BPTYPE_MacroLibrary))
			{
				FKismetEditorUtilities::RecompileBlueprintBytecode(GeneratingBP, ObjLoaded);
			}

			GeneratingBP->bIsRegeneratingOnLoad = bWasRegenerating;
		}
		else if (FConditionalRecompileClassHepler::StaticLink == NecessaryAction)
		{
			StaticLink(true);
			if (*GeneratingBP->SkeletonGeneratedClass)
			{
				GeneratingBP->SkeletonGeneratedClass->StaticLink(true);
			}
		}
	}
}

UObject* UBlueprintGeneratedClass::GetArchetypeForCDO() const
{
	if (OverridenArchetypeForCDO)
	{
		ensure(OverridenArchetypeForCDO->IsA(GetSuperClass()));
		return OverridenArchetypeForCDO;
	}

	return Super::GetArchetypeForCDO();
}
#endif //WITH_EDITOR

void UBlueprintGeneratedClass::SerializeDefaultObject(UObject* Object, FArchive& Ar)
{
	Super::SerializeDefaultObject(Object, Ar);

	if (Ar.IsLoading() && !Ar.IsObjectReferenceCollector() && Object == ClassDefaultObject)
	{
		// On load, build the custom property list used in post-construct initialization logic. Note that in the editor, this will be refreshed during compile-on-load.
		// @TODO - Potentially make this serializable (or cooked data) to eliminate the slight load time cost we'll incur below to generate this list in a cooked build. For now, it's not serialized since the raw UProperty references cannot be saved out.
		UpdateCustomPropertyListForPostConstruction();
	}
}

void UBlueprintGeneratedClass::PostLoadDefaultObject(UObject* Object)
{
	Super::PostLoadDefaultObject(Object);

	if (Object == ClassDefaultObject)
	{
		// Rebuild the custom property list used in post-construct initialization logic. Note that PostLoad() may have altered some serialized properties.
		UpdateCustomPropertyListForPostConstruction();
	}
}

bool UBlueprintGeneratedClass::BuildCustomPropertyListForPostConstruction(FCustomPropertyListNode*& InPropertyList, UStruct* InStruct, const uint8* DataPtr, const uint8* DefaultDataPtr)
{
	const UClass* OwnerClass = Cast<UClass>(InStruct);
	FCustomPropertyListNode** CurrentNodePtr = &InPropertyList;

	for (UProperty* Property = InStruct->PropertyLink; Property; Property = Property->PropertyLinkNext)
	{
		const bool bIsConfigProperty = Property->HasAnyPropertyFlags(CPF_Config) && !(OwnerClass && OwnerClass->HasAnyClassFlags(CLASS_PerObjectConfig));
		const bool bIsTransientProperty = Property->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient | CPF_NonPIEDuplicateTransient);

		// Skip config properties as they're already in the PostConstructLink chain. Also skip transient properties if they contain a reference to an instanced subobjects (as those should not be initialized from defaults).
		if (!bIsConfigProperty && (!bIsTransientProperty || !Property->ContainsInstancedObjectProperty()))
		{
			for (int32 Idx = 0; Idx < Property->ArrayDim; Idx++)
			{
				const uint8* PropertyValue = Property->ContainerPtrToValuePtr<uint8>(DataPtr, Idx);
				const uint8* DefaultPropertyValue = Property->ContainerPtrToValuePtrForDefaults<uint8>(InStruct, DefaultDataPtr, Idx);

				// If this is a struct property, recurse to pull out any fields that differ from the native CDO.
				if (UStructProperty* StructProperty = Cast<UStructProperty>(Property))
				{
					// Create a new node for the struct property.
					*CurrentNodePtr = new(CustomPropertyListForPostConstruction) FCustomPropertyListNode(Property, Idx);

					// Recursively gather up all struct fields that differ and assign to the current node's sub property list.
					if (BuildCustomPropertyListForPostConstruction((*CurrentNodePtr)->SubPropertyList, StructProperty->Struct, PropertyValue, DefaultPropertyValue))
					{
						// Advance to the next node in the list.
						CurrentNodePtr = &(*CurrentNodePtr)->PropertyListNext;
					}
					else
					{
						// Remove the node for the struct property since it does not differ from the native CDO.
						CustomPropertyListForPostConstruction.RemoveAt(CustomPropertyListForPostConstruction.Num() - 1);

						// Clear the current node ptr since the array will have freed up the memory it referenced.
						*CurrentNodePtr = nullptr;
					}
				}
				else if (UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Property))
				{
					// Create a new node for the array property.
					*CurrentNodePtr = new(CustomPropertyListForPostConstruction) FCustomPropertyListNode(Property, Idx);

					// Recursively gather up all array item indices that differ and assign to the current node's sub property list.
					if (BuildCustomArrayPropertyListForPostConstruction(ArrayProperty, (*CurrentNodePtr)->SubPropertyList, PropertyValue, DefaultPropertyValue))
					{
						// Advance to the next node in the list.
						CurrentNodePtr = &(*CurrentNodePtr)->PropertyListNext;
					}
					else
					{
						// Remove the node for the array property since it does not differ from the native CDO.
						CustomPropertyListForPostConstruction.RemoveAt(CustomPropertyListForPostConstruction.Num() - 1);

						// Clear the current node ptr since the array will have freed up the memory it referenced.
						*CurrentNodePtr = nullptr;
					}
				}
				else if (!Property->Identical(PropertyValue, DefaultPropertyValue))
				{
					// Create a new node, link it into the chain and add it into the array.
					*CurrentNodePtr = new(CustomPropertyListForPostConstruction) FCustomPropertyListNode(Property, Idx);

					// Advance to the next node ptr.
					CurrentNodePtr = &(*CurrentNodePtr)->PropertyListNext;
				}
			}
		}
	}

	// This will be non-NULL if the above found at least one property value that differs from the native CDO.
	return (InPropertyList != nullptr);
}

bool UBlueprintGeneratedClass::BuildCustomArrayPropertyListForPostConstruction(UArrayProperty* ArrayProperty, FCustomPropertyListNode*& InPropertyList, const uint8* DataPtr, const uint8* DefaultDataPtr, int32 StartIndex)
{
	FCustomPropertyListNode** CurrentArrayNodePtr = &InPropertyList;

	FScriptArrayHelper ArrayValueHelper(ArrayProperty, DataPtr);
	FScriptArrayHelper DefaultArrayValueHelper(ArrayProperty, DefaultDataPtr);

	for (int32 ArrayValueIndex = StartIndex; ArrayValueIndex < ArrayValueHelper.Num(); ++ArrayValueIndex)
	{
		const int32 DefaultArrayValueIndex = ArrayValueIndex - StartIndex;
		if (DefaultArrayValueIndex < DefaultArrayValueHelper.Num())
		{
			const uint8* ArrayPropertyValue = ArrayValueHelper.GetRawPtr(ArrayValueIndex);
			const uint8* DefaultArrayPropertyValue = DefaultArrayValueHelper.GetRawPtr(DefaultArrayValueIndex);

			if (UStructProperty* InnerStructProperty = Cast<UStructProperty>(ArrayProperty->Inner))
			{
				// Create a new node for the item value at this index.
				*CurrentArrayNodePtr = new(CustomPropertyListForPostConstruction) FCustomPropertyListNode(ArrayProperty, ArrayValueIndex);

				// Recursively gather up all struct fields that differ and assign to the array item value node's sub property list.
				if (BuildCustomPropertyListForPostConstruction((*CurrentArrayNodePtr)->SubPropertyList, InnerStructProperty->Struct, ArrayPropertyValue, DefaultArrayPropertyValue))
				{
					// Advance to the next node in the list.
					CurrentArrayNodePtr = &(*CurrentArrayNodePtr)->PropertyListNext;
				}
				else
				{
					// Remove the node for the struct property since it does not differ from the native CDO.
					CustomPropertyListForPostConstruction.RemoveAt(CustomPropertyListForPostConstruction.Num() - 1);

					// Clear the current array item node ptr
					*CurrentArrayNodePtr = nullptr;
				}
			}
			else if (UArrayProperty* InnerArrayProperty = Cast<UArrayProperty>(ArrayProperty->Inner))
			{
				// Create a new node for the item value at this index.
				*CurrentArrayNodePtr = new(CustomPropertyListForPostConstruction) FCustomPropertyListNode(ArrayProperty, ArrayValueIndex);

				// Recursively gather up all array item indices that differ and assign to the array item value node's sub property list.
				if (BuildCustomArrayPropertyListForPostConstruction(InnerArrayProperty, (*CurrentArrayNodePtr)->SubPropertyList, ArrayPropertyValue, DefaultArrayPropertyValue))
				{
					// Advance to the next node in the list.
					CurrentArrayNodePtr = &(*CurrentArrayNodePtr)->PropertyListNext;
				}
				else
				{
					// Remove the node for the array property since it does not differ from the native CDO.
					CustomPropertyListForPostConstruction.RemoveAt(CustomPropertyListForPostConstruction.Num() - 1);

					// Clear the current array item node ptr
					*CurrentArrayNodePtr = nullptr;
				}
			}
			else if (!ArrayProperty->Inner->Identical(ArrayPropertyValue, DefaultArrayPropertyValue))
			{
				// Create a new node, link it into the chain and add it into the array.
				*CurrentArrayNodePtr = new(CustomPropertyListForPostConstruction) FCustomPropertyListNode(ArrayProperty, ArrayValueIndex);

				// Advance to the next array item node ptr.
				CurrentArrayNodePtr = &(*CurrentArrayNodePtr)->PropertyListNext;
			}
		}
		else
		{
			// Create a temp default array as a placeholder to compare against the remaining elements in the value.
			FScriptArray TempDefaultArray;
			const int32 Count = ArrayValueHelper.Num() - DefaultArrayValueHelper.Num();
			TempDefaultArray.Add(Count, ArrayProperty->Inner->ElementSize);
			uint8 *Dest = (uint8*)TempDefaultArray.GetData();
			if (ArrayProperty->Inner->PropertyFlags & CPF_ZeroConstructor)
			{
				FMemory::Memzero(Dest, Count * ArrayProperty->Inner->ElementSize);
			}
			else
			{
				for (int32 i = 0; i < Count; i++, Dest += ArrayProperty->Inner->ElementSize)
				{
					ArrayProperty->Inner->InitializeValue(Dest);
				}
			}

			// Recursively fill out the property list for the remainder of the elements in the value that extend beyond the size of the default value.
			BuildCustomArrayPropertyListForPostConstruction(ArrayProperty, *CurrentArrayNodePtr, DataPtr, (uint8*)&TempDefaultArray, ArrayValueIndex);

			// Don't need to record anything else.
			break;
		}
	}

	// Return true if the above found at least one array element that differs from the native CDO, or otherwise if the array sizes are different.
	return (InPropertyList != nullptr || ArrayValueHelper.Num() != DefaultArrayValueHelper.Num());
}

void UBlueprintGeneratedClass::UpdateCustomPropertyListForPostConstruction()
{
	// Empty the current list.
	CustomPropertyListForPostConstruction.Empty();

	// Find the first native antecedent. All non-native decendant properties are attached to the PostConstructLink chain (see UStruct::Link), so we only need to worry about properties owned by native super classes here.
	UClass* SuperClass = GetSuperClass();
	while (SuperClass && !SuperClass->HasAnyClassFlags(CLASS_Native | CLASS_Intrinsic))
	{
		SuperClass = SuperClass->GetSuperClass();
	}

	if (SuperClass)
	{
		check(ClassDefaultObject != nullptr);

		// Recursively gather native class-owned property values that differ from defaults.
		FCustomPropertyListNode* PropertyList = nullptr;
		BuildCustomPropertyListForPostConstruction(PropertyList, SuperClass, (uint8*)ClassDefaultObject, (uint8*)SuperClass->GetDefaultObject(false));
	}
}

bool UBlueprintGeneratedClass::IsFunctionImplementedInBlueprint(FName InFunctionName) const
{
	UFunction* Function = FindFunctionByName(InFunctionName);
	return Function && Function->GetOuter() && Function->GetOuter()->IsA(UBlueprintGeneratedClass::StaticClass());
}

UInheritableComponentHandler* UBlueprintGeneratedClass::GetInheritableComponentHandler(const bool bCreateIfNecessary)
{
	static const FBoolConfigValueHelper EnableInheritableComponents(TEXT("Kismet"), TEXT("bEnableInheritableComponents"), GEngineIni);
	if (!EnableInheritableComponents)
	{
		return nullptr;
	}
	
	if (InheritableComponentHandler)
	{
		InheritableComponentHandler->PreloadAll();
	}

	if (!InheritableComponentHandler && bCreateIfNecessary)
	{
		InheritableComponentHandler = NewObject<UInheritableComponentHandler>(this, FName(TEXT("InheritableComponentHandler")));
	}

	return InheritableComponentHandler;
}


UObject* UBlueprintGeneratedClass::FindArchetype(UClass* ArchetypeClass, const FName ArchetypeName) const
{
	UObject* Archetype = nullptr;

	// There are some rogue LevelScriptActors that still have a SimpleConstructionScript
	// and since preloading the SCS of a script in a world package is bad news, we need to filter them out
	if (SimpleConstructionScript && !IsChildOf<ALevelScriptActor>())
	{
		UBlueprintGeneratedClass* Class = const_cast<UBlueprintGeneratedClass*>(this);
		while (Class)
		{
			USimpleConstructionScript* ClassSCS = Class->SimpleConstructionScript;
			USCS_Node* SCSNode = nullptr;
			if (ClassSCS)
			{
				if (ClassSCS->HasAnyFlags(RF_NeedLoad))
				{
					ClassSCS->PreloadChain();
				}

				SCSNode = ClassSCS->FindSCSNode(ArchetypeName);
			}

			if (SCSNode)
			{
				// Ensure that the stored template is of the same type as the serialized object. Since
				// we match these by name, this handles the case where the Blueprint class was updated
				// after having previously serialized an instanced into another package (e.g. map). In
				// that case, the Blueprint class might contain an SCS node with the same name as the
				// previously-serialized object, but it might also have been switched to a different type.
				if (SCSNode->ComponentTemplate && SCSNode->ComponentTemplate->IsA(ArchetypeClass))
				{
					Archetype = SCSNode->ComponentTemplate;
				}
			}
			else if (UInheritableComponentHandler* ICH = Class->GetInheritableComponentHandler())
			{
				Archetype = ICH->GetOverridenComponentTemplate(ICH->FindKey(ArchetypeName));
			}

			if (Archetype == nullptr)
			{
				Archetype = static_cast<UObject*>(FindObjectWithOuter(Class, ArchetypeClass, ArchetypeName));
				Class = (Archetype ? nullptr : Cast<UBlueprintGeneratedClass>(Class->GetSuperClass()));
			}
			else
			{
				Class = nullptr;
			}
		}
	}


	return Archetype;
}

UDynamicBlueprintBinding* UBlueprintGeneratedClass::GetDynamicBindingObject(const UClass* ThisClass, UClass* BindingClass)
{
	check(ThisClass);
	UDynamicBlueprintBinding* DynamicBlueprintBinding = nullptr;
	if (auto BPGC = Cast<UBlueprintGeneratedClass>(ThisClass))
	{
		for (auto DynamicBindingObject : BPGC->DynamicBindingObjects)
		{
			if (DynamicBindingObject && (DynamicBindingObject->GetClass() == BindingClass))
			{
				DynamicBlueprintBinding = DynamicBindingObject;
				break;
			}
		}
	}
	else if (auto DynamicClass = Cast<UDynamicClass>(ThisClass))
	{
		for (auto MiscObj : DynamicClass->DynamicBindingObjects)
		{
			auto DynamicBindingObject = Cast<UDynamicBlueprintBinding>(MiscObj);
			if (DynamicBindingObject && (DynamicBindingObject->GetClass() == BindingClass))
			{
				DynamicBlueprintBinding = DynamicBindingObject;
				break;
			}
		}
	}
	return DynamicBlueprintBinding;
}

void UBlueprintGeneratedClass::BindDynamicDelegates(const UClass* ThisClass, UObject* InInstance)
{
	check(ThisClass && InInstance);
	if (!InInstance->IsA(ThisClass))
	{
		UE_LOG(LogBlueprint, Warning, TEXT("BindComponentDelegates: '%s' is not an instance of '%s'."), *InInstance->GetName(), *ThisClass->GetName());
		return;
	}

	if (auto BPGC = Cast<UBlueprintGeneratedClass>(ThisClass))
	{
		for (auto DynamicBindingObject : BPGC->DynamicBindingObjects)
		{
			if (ensure(DynamicBindingObject))
			{
				DynamicBindingObject->BindDynamicDelegates(InInstance);
			}
		}
	}
	else if (auto DynamicClass = Cast<UDynamicClass>(ThisClass))
	{
		for (auto MiscObj : DynamicClass->DynamicBindingObjects)
		{
			auto DynamicBindingObject = Cast<UDynamicBlueprintBinding>(MiscObj);
			if (DynamicBindingObject)
			{
				DynamicBindingObject->BindDynamicDelegates(InInstance);
			}
		}
	}

	if (auto TheSuperClass = ThisClass->GetSuperClass())
	{
		BindDynamicDelegates(TheSuperClass, InInstance);
	}
}

#if WITH_EDITOR
void UBlueprintGeneratedClass::UnbindDynamicDelegates(const UClass* ThisClass, UObject* InInstance)
{
	check(ThisClass && InInstance);
	if (!InInstance->IsA(ThisClass))
	{
		UE_LOG(LogBlueprint, Warning, TEXT("UnbindDynamicDelegates: '%s' is not an instance of '%s'."), *InInstance->GetName(), *ThisClass->GetName());
		return;
	}

	if (auto BPGC = Cast<UBlueprintGeneratedClass>(ThisClass))
	{
		for (auto DynamicBindingObject : BPGC->DynamicBindingObjects)
		{
			if (ensure(DynamicBindingObject))
			{
				DynamicBindingObject->UnbindDynamicDelegates(InInstance);
			}
		}
	}
	else if (auto DynamicClass = Cast<UDynamicClass>(ThisClass))
	{
		for (auto MiscObj : DynamicClass->DynamicBindingObjects)
		{
			auto DynamicBindingObject = Cast<UDynamicBlueprintBinding>(MiscObj);
			if (DynamicBindingObject)
			{
				DynamicBindingObject->UnbindDynamicDelegates(InInstance);
			}
		}
	}

	if (auto TheSuperClass = ThisClass->GetSuperClass())
	{
		UnbindDynamicDelegates(TheSuperClass, InInstance);
	}
}

void UBlueprintGeneratedClass::UnbindDynamicDelegatesForProperty(UObject* InInstance, const UObjectProperty* InObjectProperty)
{
	for (int32 Index = 0; Index < DynamicBindingObjects.Num(); ++Index)
	{
		if ( ensure(DynamicBindingObjects[Index] != NULL) )
		{
			DynamicBindingObjects[Index]->UnbindDynamicDelegatesForProperty(InInstance, InObjectProperty);
		}
	}
}
#endif

bool UBlueprintGeneratedClass::GetGeneratedClassesHierarchy(const UClass* InClass, TArray<const UBlueprintGeneratedClass*>& OutBPGClasses)
{
	OutBPGClasses.Empty();
	bool bNoErrors = true;
	while(const UBlueprintGeneratedClass* BPGClass = Cast<const UBlueprintGeneratedClass>(InClass))
	{
#if WITH_EDITORONLY_DATA
		const UBlueprint* BP = Cast<const UBlueprint>(BPGClass->ClassGeneratedBy);
		bNoErrors &= (NULL != BP) && (BP->Status != BS_Error);
#endif
		OutBPGClasses.Add(BPGClass);
		InClass = BPGClass->GetSuperClass();
	}
	return bNoErrors;
}

UActorComponent* UBlueprintGeneratedClass::FindComponentTemplateByName(const FName& TemplateName) const
{
	for(int32 i = 0; i < ComponentTemplates.Num(); i++)
	{
		UActorComponent* Template = ComponentTemplates[i];
		if(Template != NULL && Template->GetFName() == TemplateName)
		{
			return Template;
		}
	}

	return NULL;
}

void UBlueprintGeneratedClass::CreateTimelineComponent(AActor* Actor, const UTimelineTemplate* TimelineTemplate)
{
	if (!Actor
		|| !TimelineTemplate
		|| !TimelineTemplate->bValidatedAsWired
		|| Actor->IsTemplate()
		|| Actor->IsPendingKill())
	{
		return;
	}

	FName NewName(*UTimelineTemplate::TimelineTemplateNameToVariableName(TimelineTemplate->GetFName()));
	UTimelineComponent* NewTimeline = NewObject<UTimelineComponent>(Actor, NewName);
	NewTimeline->CreationMethod = EComponentCreationMethod::UserConstructionScript; // Indicate it comes from a blueprint so it gets cleared when we rerun construction scripts
	Actor->BlueprintCreatedComponents.Add(NewTimeline); // Add to array so it gets saved
	NewTimeline->SetNetAddressable();	// This component has a stable name that can be referenced for replication

	NewTimeline->SetPropertySetObject(Actor); // Set which object the timeline should drive properties on
	NewTimeline->SetDirectionPropertyName(TimelineTemplate->GetDirectionPropertyName());

	NewTimeline->SetTimelineLength(TimelineTemplate->TimelineLength); // copy length
	NewTimeline->SetTimelineLengthMode(TimelineTemplate->LengthMode);

	// Find property with the same name as the template and assign the new Timeline to it
	UClass* ActorClass = Actor->GetClass();
	UObjectPropertyBase* Prop = FindField<UObjectPropertyBase>(ActorClass, *UTimelineTemplate::TimelineTemplateNameToVariableName(TimelineTemplate->GetFName()));
	if (Prop)
	{
		Prop->SetObjectPropertyValue_InContainer(Actor, NewTimeline);
	}

	// Event tracks
	// In the template there is a track for each function, but in the runtime Timeline each key has its own delegate, so we fold them together
	for (int32 TrackIdx = 0; TrackIdx < TimelineTemplate->EventTracks.Num(); TrackIdx++)
	{
		const FTTEventTrack* EventTrackTemplate = &TimelineTemplate->EventTracks[TrackIdx];
		if (EventTrackTemplate->CurveKeys != NULL)
		{
			// Create delegate for all keys in this track
			FScriptDelegate EventDelegate;
			EventDelegate.BindUFunction(Actor, TimelineTemplate->GetEventTrackFunctionName(TrackIdx));

			// Create an entry in Events for each key of this track
			for (auto It(EventTrackTemplate->CurveKeys->FloatCurve.GetKeyIterator()); It; ++It)
			{
				NewTimeline->AddEvent(It->Time, FOnTimelineEvent(EventDelegate));
			}
		}
	}

	// Float tracks
	for (int32 TrackIdx = 0; TrackIdx < TimelineTemplate->FloatTracks.Num(); TrackIdx++)
	{
		const FTTFloatTrack* FloatTrackTemplate = &TimelineTemplate->FloatTracks[TrackIdx];
		if (FloatTrackTemplate->CurveFloat != NULL)
		{
			NewTimeline->AddInterpFloat(FloatTrackTemplate->CurveFloat, FOnTimelineFloat(), TimelineTemplate->GetTrackPropertyName(FloatTrackTemplate->TrackName));
		}
	}

	// Vector tracks
	for (int32 TrackIdx = 0; TrackIdx < TimelineTemplate->VectorTracks.Num(); TrackIdx++)
	{
		const FTTVectorTrack* VectorTrackTemplate = &TimelineTemplate->VectorTracks[TrackIdx];
		if (VectorTrackTemplate->CurveVector != NULL)
		{
			NewTimeline->AddInterpVector(VectorTrackTemplate->CurveVector, FOnTimelineVector(), TimelineTemplate->GetTrackPropertyName(VectorTrackTemplate->TrackName));
		}
	}

	// Linear color tracks
	for (int32 TrackIdx = 0; TrackIdx < TimelineTemplate->LinearColorTracks.Num(); TrackIdx++)
	{
		const FTTLinearColorTrack* LinearColorTrackTemplate = &TimelineTemplate->LinearColorTracks[TrackIdx];
		if (LinearColorTrackTemplate->CurveLinearColor != NULL)
		{
			NewTimeline->AddInterpLinearColor(LinearColorTrackTemplate->CurveLinearColor, FOnTimelineLinearColor(), TimelineTemplate->GetTrackPropertyName(LinearColorTrackTemplate->TrackName));
		}
	}

	// Set up delegate that gets called after all properties are updated
	FScriptDelegate UpdateDelegate;
	UpdateDelegate.BindUFunction(Actor, TimelineTemplate->GetUpdateFunctionName());
	NewTimeline->SetTimelinePostUpdateFunc(FOnTimelineEvent(UpdateDelegate));

	// Set up finished delegate that gets called after all properties are updated
	FScriptDelegate FinishedDelegate;
	FinishedDelegate.BindUFunction(Actor, TimelineTemplate->GetFinishedFunctionName());
	NewTimeline->SetTimelineFinishedFunc(FOnTimelineEvent(FinishedDelegate));

	NewTimeline->RegisterComponent();

	// Start playing now, if desired
	if (TimelineTemplate->bAutoPlay)
	{
		// Needed for autoplay timelines in cooked builds, since they won't have Activate() called via the Play call below
		NewTimeline->bAutoActivate = true;
		NewTimeline->Play();
	}

	// Set to loop, if desired
	if (TimelineTemplate->bLoop)
	{
		NewTimeline->SetLooping(true);
	}

	// Set replication, if desired
	if (TimelineTemplate->bReplicated)
	{
		NewTimeline->SetIsReplicated(true);
	}
}

void UBlueprintGeneratedClass::CreateComponentsForActor(const UClass* ThisClass, AActor* Actor)
{
	check(ThisClass && Actor);
	check(!Actor->IsTemplate());
	check(!Actor->IsPendingKill());

	if (auto BPGC = Cast<const UBlueprintGeneratedClass>(ThisClass))
	{
		for (auto TimelineTemplate : BPGC->Timelines)
		{
			// Not fatal if NULL, but shouldn't happen and ignored if not wired up in graph
			if (TimelineTemplate && TimelineTemplate->bValidatedAsWired)
			{
				CreateTimelineComponent(Actor, TimelineTemplate);
			}
		}
	}
	else if (auto DynamicClass = Cast<UDynamicClass>(ThisClass))
	{
		for (auto MiscObj : DynamicClass->Timelines)
		{
			auto TimelineTemplate = Cast<const UTimelineTemplate>(MiscObj);
			// Not fatal if NULL, but shouldn't happen and ignored if not wired up in graph
			if (TimelineTemplate && TimelineTemplate->bValidatedAsWired)
			{
				CreateTimelineComponent(Actor, TimelineTemplate);
			}
		}
	}
}

uint8* UBlueprintGeneratedClass::GetPersistentUberGraphFrame(UObject* Obj, UFunction* FuncToCheck) const
{
	if (Obj && UsePersistentUberGraphFrame() && UberGraphFramePointerProperty && UberGraphFunction)
	{
		if (UberGraphFunction == FuncToCheck)
		{
			auto PointerToUberGraphFrame = UberGraphFramePointerProperty->ContainerPtrToValuePtr<FPointerToUberGraphFrame>(Obj);
			checkSlow(PointerToUberGraphFrame);
			ensure(PointerToUberGraphFrame->RawPointer);
			return PointerToUberGraphFrame->RawPointer;
		}
	}
	auto ParentClass = GetSuperClass();
	checkSlow(ParentClass);
	return ParentClass->GetPersistentUberGraphFrame(Obj, FuncToCheck);
}

void UBlueprintGeneratedClass::CreatePersistentUberGraphFrame(UObject* Obj, bool bCreateOnlyIfEmpty, bool bSkipSuperClass, UClass* OldClass) const
{
	ensure(!UberGraphFramePointerProperty == !UberGraphFunction);
	if (Obj && UsePersistentUberGraphFrame() && UberGraphFramePointerProperty && UberGraphFunction)
	{
		auto PointerToUberGraphFrame = UberGraphFramePointerProperty->ContainerPtrToValuePtr<FPointerToUberGraphFrame>(Obj);
		check(PointerToUberGraphFrame);

		if ( !ensureMsgf(bCreateOnlyIfEmpty || !PointerToUberGraphFrame->RawPointer
			, TEXT("Attempting to recreate an object's UberGraphFrame when the previous one was not properly destroyed (transitioning '%s' from '%s' to '%s'). We'll attempt to free the frame memory, but cannot clean up its properties (this may result in leaks and undesired side effects).")
			, *Obj->GetPathName()
			, (OldClass == nullptr) ? TEXT("<NULL>") : *OldClass->GetName()
			, *GetName()) )
		{
			FMemory::Free(PointerToUberGraphFrame->RawPointer);
			PointerToUberGraphFrame->RawPointer = nullptr;
		}
		
		if (!PointerToUberGraphFrame->RawPointer)
		{
			uint8* FrameMemory = NULL;
			const bool bUberGraphFunctionIsReady = UberGraphFunction->HasAllFlags(RF_LoadCompleted); // is fully loaded
			if (bUberGraphFunctionIsReady)
			{
				INC_MEMORY_STAT_BY(STAT_PersistentUberGraphFrameMemory, UberGraphFunction->GetStructureSize());
				FrameMemory = (uint8*)FMemory::Malloc(UberGraphFunction->GetStructureSize());

				FMemory::Memzero(FrameMemory, UberGraphFunction->GetStructureSize());
				for (UProperty* Property = UberGraphFunction->PropertyLink; Property; Property = Property->PropertyLinkNext)
				{
					Property->InitializeValue_InContainer(FrameMemory);
				}
			}
			else
			{
				UE_LOG(LogBlueprint, Verbose, TEXT("Function '%s' is not ready to create frame for '%s'"),
					*GetPathNameSafe(UberGraphFunction), *GetPathNameSafe(Obj));
			}
			PointerToUberGraphFrame->RawPointer = FrameMemory;
		}
	}

	if (!bSkipSuperClass)
	{
		auto ParentClass = GetSuperClass();
		checkSlow(ParentClass);
		ParentClass->CreatePersistentUberGraphFrame(Obj, bCreateOnlyIfEmpty);
	}
}

void UBlueprintGeneratedClass::DestroyPersistentUberGraphFrame(UObject* Obj, bool bSkipSuperClass) const
{
	ensure(!UberGraphFramePointerProperty == !UberGraphFunction);
	if (Obj && UsePersistentUberGraphFrame() && UberGraphFramePointerProperty && UberGraphFunction)
	{
		auto PointerToUberGraphFrame = UberGraphFramePointerProperty->ContainerPtrToValuePtr<FPointerToUberGraphFrame>(Obj);
		checkSlow(PointerToUberGraphFrame);
		auto FrameMemory = PointerToUberGraphFrame->RawPointer;
		PointerToUberGraphFrame->RawPointer = NULL;
		if (FrameMemory)
		{
			for (UProperty* Property = UberGraphFunction->PropertyLink; Property; Property = Property->PropertyLinkNext)
			{
				Property->DestroyValue_InContainer(FrameMemory);
			}
			FMemory::Free(FrameMemory);
			DEC_MEMORY_STAT_BY(STAT_PersistentUberGraphFrameMemory, UberGraphFunction->GetStructureSize());
		}
		else
		{
			UE_LOG(LogBlueprint, Log, TEXT("Object '%s' had no Uber Graph Persistent Frame"), *GetPathNameSafe(Obj));
		}
	}

	if (!bSkipSuperClass)
	{
		auto ParentClass = GetSuperClass();
		checkSlow(ParentClass);
		ParentClass->DestroyPersistentUberGraphFrame(Obj);
	}
}

void UBlueprintGeneratedClass::Link(FArchive& Ar, bool bRelinkExistingProperties)
{
	// Ensure that function netflags equate to any super function in a parent BP prior to linking; it may have been changed by the user
	// and won't be reflected in the child class until it is recompiled. Without this, UClass::Link() will assert if they are out of sync.
	for(UField* Field = Children; Field; Field = Field->Next)
	{
		Ar.Preload(Field);

		UFunction* Function = dynamic_cast<UFunction*>(Field);
		if(Function != nullptr)
		{
			UFunction* ParentFunction = Function->GetSuperFunction();
			if(ParentFunction != nullptr)
			{
				const uint32 ParentNetFlags = (ParentFunction->FunctionFlags & FUNC_NetFuncFlags);
				if(ParentNetFlags != (Function->FunctionFlags & FUNC_NetFuncFlags))
				{
					Function->FunctionFlags &= ~FUNC_NetFuncFlags;
					Function->FunctionFlags |= ParentNetFlags;
				}
			}
		}
	}

	Super::Link(Ar, bRelinkExistingProperties);

	if (UsePersistentUberGraphFrame() && UberGraphFunction)
	{
		Ar.Preload(UberGraphFunction);

		for (auto Property : TFieldRange<UStructProperty>(this, EFieldIteratorFlags::ExcludeSuper))
		{
			if (Property->GetFName() == GetUberGraphFrameName())
			{
				UberGraphFramePointerProperty = Property;
				break;
			}
		}
		checkSlow(UberGraphFramePointerProperty);
	}

	AssembleReferenceTokenStream(true);
}

void UBlueprintGeneratedClass::PurgeClass(bool bRecompilingOnLoad)
{
	Super::PurgeClass(bRecompilingOnLoad);

	UberGraphFramePointerProperty = NULL;
	UberGraphFunction = NULL;
#if WITH_EDITORONLY_DATA
	OverridenArchetypeForCDO = NULL;
#endif //WITH_EDITOR

#if UE_BLUEPRINT_EVENTGRAPH_FASTCALLS
	FastCallPairs_DEPRECATED.Empty();
#endif
}

void UBlueprintGeneratedClass::Bind()
{
	Super::Bind();

	if (UsePersistentUberGraphFrame() && UberGraphFunction)
	{
		ClassAddReferencedObjects = &UBlueprintGeneratedClass::AddReferencedObjectsInUbergraphFrame;
	}
}

class FPersistentFrameCollectorArchive : public FSimpleObjectReferenceCollectorArchive
{
public:
	FPersistentFrameCollectorArchive(const UObject* InSerializingObject, FReferenceCollector& InCollector)
		: FSimpleObjectReferenceCollectorArchive(InSerializingObject, InCollector)
	{}

protected:
	virtual FArchive& operator<<(UObject*& Object) override
	{
#if !(UE_BUILD_TEST || UE_BUILD_SHIPPING)
		if (!ensureMsgf( (Object == nullptr) || Object->IsValidLowLevelFast()
			, TEXT("Invalid object referenced by the PersistentFrame: 0x%016llx (Blueprint object: %s, ReferencingProperty: %s) - If you have a reliable repro for this, please contact the development team with it.")
			, (int64)(PTRINT)Object
			, SerializingObject ? *SerializingObject->GetFullName() : TEXT("NULL")
			, GetSerializedProperty() ? *GetSerializedProperty()->GetFullName() : TEXT("NULL") ))
		{
			// clear the property value (it's garbage)... the ubergraph-frame 
			// has just lost a reference to whatever it was attempting to hold onto
			Object = nullptr;
		}
#endif

		const bool bWeakRef = Object ? !Object->HasAnyFlags(RF_StrongRefOnFrame) : false;
		Collector.SetShouldHandleAsWeakRef(bWeakRef); 
		return FSimpleObjectReferenceCollectorArchive::operator<<(Object);
	}
};

void UBlueprintGeneratedClass::AddReferencedObjectsInUbergraphFrame(UObject* InThis, FReferenceCollector& Collector)
{
	checkSlow(InThis);
	for (UClass* CurrentClass = InThis->GetClass(); CurrentClass; CurrentClass = CurrentClass->GetSuperClass())
	{
		if (auto BPGC = Cast<UBlueprintGeneratedClass>(CurrentClass))
		{
			if (BPGC->UberGraphFramePointerProperty)
			{
				auto PointerToUberGraphFrame = BPGC->UberGraphFramePointerProperty->ContainerPtrToValuePtr<FPointerToUberGraphFrame>(InThis);
				checkSlow(PointerToUberGraphFrame)
				if (PointerToUberGraphFrame->RawPointer)
				{
					checkSlow(BPGC->UberGraphFunction);
					FPersistentFrameCollectorArchive ObjectReferenceCollector(InThis, Collector);
					BPGC->UberGraphFunction->SerializeBin(ObjectReferenceCollector, PointerToUberGraphFrame->RawPointer);

					// Reset the ShouldHandleAsWeakRef state, before the collector is used by a different archive.
					Collector.SetShouldHandleAsWeakRef(false);
				}
			}
		}
		else if (CurrentClass->HasAllClassFlags(CLASS_Native))
		{
			CurrentClass->CallAddReferencedObjects(InThis, Collector);
			break;
		}
		else
		{
			checkSlow(false);
		}
	}
}

FName UBlueprintGeneratedClass::GetUberGraphFrameName()
{
	static const FName UberGraphFrameName(TEXT("UberGraphFrame"));
	return UberGraphFrameName;
}

bool UBlueprintGeneratedClass::UsePersistentUberGraphFrame()
{
#if USE_UBER_GRAPH_PERSISTENT_FRAME
	static const FBoolConfigValueHelper PersistentUberGraphFrame(TEXT("Kismet"), TEXT("bPersistentUberGraphFrame"), GEngineIni);
	return PersistentUberGraphFrame;
#else
	return false;
#endif
}

void UBlueprintGeneratedClass::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.IsLoading() && 0 == (Ar.GetPortFlags() & PPF_Duplicate))
	{
		CreatePersistentUberGraphFrame(ClassDefaultObject, true);
	}
}

void UBlueprintGeneratedClass::GetLifetimeBlueprintReplicationList(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	uint32 PropertiesLeft = NumReplicatedProperties;

	for (TFieldIterator<UProperty> It(this, EFieldIteratorFlags::ExcludeSuper); It && PropertiesLeft > 0; ++It)
	{
		UProperty * Prop = *It;
		if (Prop != NULL && Prop->GetPropertyFlags() & CPF_Net)
		{
			PropertiesLeft--;
			OutLifetimeProps.Add(FLifetimeProperty(Prop->RepIndex));
		}
	}

	UBlueprintGeneratedClass* SuperBPClass = Cast<UBlueprintGeneratedClass>(GetSuperStruct());
	if (SuperBPClass != NULL)
	{
		SuperBPClass->GetLifetimeBlueprintReplicationList(OutLifetimeProps);
	}
}

void FBlueprintCookedComponentInstancingData::BuildCachedPropertyList(FCustomPropertyListNode** CurrentNode, const UStruct* CurrentScope, int32* CurrentSourceIdx) const
{
	int32 LocalSourceIdx = 0;

	if (CurrentSourceIdx == nullptr)
	{
		CurrentSourceIdx = &LocalSourceIdx;
	}

	// The serialized list is stored linearly, so stop iterating once we no longer match the scope (this indicates that we've finished parsing out "sub" properties for a UStruct).
	while (*CurrentSourceIdx < ChangedPropertyList.Num() && ChangedPropertyList[*CurrentSourceIdx].PropertyScope == CurrentScope)
	{
		// Find changed property by name/scope.
		const FBlueprintComponentChangedPropertyInfo& ChangedPropertyInfo = ChangedPropertyList[(*CurrentSourceIdx)++];
		UProperty* Property = nullptr;
		const UStruct* PropertyScope = CurrentScope;
		while (!Property && PropertyScope)
		{
			Property = FindField<UProperty>(PropertyScope, ChangedPropertyInfo.PropertyName);
			PropertyScope = PropertyScope->GetSuperStruct();
		}

		// Create a new node to hold property info.
		FCustomPropertyListNode* NewNode = new(CachedPropertyListForSerialization) FCustomPropertyListNode(Property, ChangedPropertyInfo.ArrayIndex);

		// Link the new node into the current property list.
		if (CurrentNode)
		{
			*CurrentNode = NewNode;
		}

		// If this is a UStruct property, recursively build a sub-property list.
		if (const UStructProperty* StructProperty = Cast<UStructProperty>(Property))
		{
			BuildCachedPropertyList(&NewNode->SubPropertyList, StructProperty->Struct, CurrentSourceIdx);
		}
		else if (const UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Property))
		{
			// If this is an array property, recursively build a sub-property list.
			BuildCachedArrayPropertyList(ArrayProperty, &NewNode->SubPropertyList, CurrentSourceIdx);
		}

		// Advance current location to the next linked node.
		CurrentNode = &NewNode->PropertyListNext;
	}
}

void FBlueprintCookedComponentInstancingData::BuildCachedArrayPropertyList(const UArrayProperty* ArrayProperty, FCustomPropertyListNode** ArraySubPropertyNode, int32* CurrentSourceIdx) const
{
	// Build the array property's sub-property list. An empty name field signals the end of the changed array property list.
	while (*CurrentSourceIdx < ChangedPropertyList.Num() &&
		(ChangedPropertyList[*CurrentSourceIdx].PropertyName == NAME_None
			|| ChangedPropertyList[*CurrentSourceIdx].PropertyName == ArrayProperty->GetFName()))
	{
		const FBlueprintComponentChangedPropertyInfo& ChangedArrayPropertyInfo = ChangedPropertyList[(*CurrentSourceIdx)++];
		UProperty* InnerProperty = ChangedArrayPropertyInfo.PropertyName != NAME_None ? ArrayProperty->Inner : nullptr;

		*ArraySubPropertyNode = new(CachedPropertyListForSerialization) FCustomPropertyListNode(InnerProperty, ChangedArrayPropertyInfo.ArrayIndex);

		// If this is a UStruct property, recursively build a sub-property list.
		if (const UStructProperty* InnerStructProperty = Cast<UStructProperty>(InnerProperty))
		{
			BuildCachedPropertyList(&(*ArraySubPropertyNode)->SubPropertyList, InnerStructProperty->Struct, CurrentSourceIdx);
		}
		else if (const UArrayProperty* InnerArrayProperty = Cast<UArrayProperty>(InnerProperty))
		{
			// If this is an array property, recursively build a sub-property list.
			BuildCachedArrayPropertyList(InnerArrayProperty, &(*ArraySubPropertyNode)->SubPropertyList, CurrentSourceIdx);
		}

		ArraySubPropertyNode = &(*ArraySubPropertyNode)->PropertyListNext;
	}
}

const FCustomPropertyListNode* FBlueprintCookedComponentInstancingData::GetCachedPropertyListForSerialization() const
{
	FCustomPropertyListNode* PropertyListRootNode = nullptr;

	// Construct the list if necessary.
	if (CachedPropertyListForSerialization.Num() == 0 && ChangedPropertyList.Num() > 0)
	{
		CachedPropertyListForSerialization.Reserve(ChangedPropertyList.Num());

		// Kick off construction of the cached property list.
		BuildCachedPropertyList(&PropertyListRootNode, ComponentTemplateClass);
	}
	else if (CachedPropertyListForSerialization.Num() > 0)
	{
		PropertyListRootNode = *CachedPropertyListForSerialization.GetData();
	}

	return PropertyListRootNode;
}

void FBlueprintCookedComponentInstancingData::LoadCachedPropertyDataForSerialization(UActorComponent* SourceTemplate)
{
	// Blueprint component instance data writer implementation.
	class FBlueprintComponentInstanceDataWriter : public FObjectWriter
	{
	public:
		FBlueprintComponentInstanceDataWriter(TArray<uint8>& InDstBytes, const FCustomPropertyListNode* InPropertyList)
			:FObjectWriter(InDstBytes)
		{
			ArCustomPropertyList = InPropertyList;
			ArUseCustomPropertyList = true;
			ArWantBinaryPropertySerialization = true;
		}
	};

	if (bIsValid)
	{
		if (SourceTemplate)
		{
			// Make sure the source template has been loaded.
			if (SourceTemplate->HasAnyFlags(RF_NeedLoad))
			{
				if (FLinkerLoad* Linker = SourceTemplate->GetLinker())
				{
					Linker->Preload(SourceTemplate);
				}
			}

			// Cache source template attributes needed for instancing.
			ComponentTemplateName = SourceTemplate->GetFName();
			ComponentTemplateClass = SourceTemplate->GetClass();
			ComponentTemplateFlags = SourceTemplate->GetFlags();

			// This will also load the cached property list, if necessary.
			const FCustomPropertyListNode* PropertyList = GetCachedPropertyListForSerialization();

			// Write template data out to the "fast path" buffer. All dependencies will be loaded at this point.
			FBlueprintComponentInstanceDataWriter InstanceDataWriter(CachedPropertyDataForSerialization, PropertyList);
			SourceTemplate->Serialize(InstanceDataWriter);
		}
		else
		{
			bIsValid = false;
		}
	}
}

bool UBlueprintGeneratedClass::ArePropertyGuidsAvailable() const
{
#if WITH_EDITORONLY_DATA
	// Property guid's are generated during compilation.
	return PropertyGuids.Num() > 0;
#else
	return false;
#endif // WITH_EDITORONLY_DATA
}

FName UBlueprintGeneratedClass::FindPropertyNameFromGuid(const FGuid& PropertyGuid) const
{
	FName RedirectedName = NAME_None;
#if WITH_EDITORONLY_DATA
	if (const FName* Result = PropertyGuids.FindKey(PropertyGuid))
	{
		RedirectedName = *Result;
	}
#endif // WITH_EDITORONLY_DATA
	return RedirectedName;
}

FGuid UBlueprintGeneratedClass::FindPropertyGuidFromName(const FName InName) const
{
	FGuid PropertyGuid;
#if WITH_EDITORONLY_DATA
	if (const FGuid* Result = PropertyGuids.Find(InName))
	{
		PropertyGuid = *Result;
	}
#endif // WITH_EDITORONLY_DATA
	return PropertyGuid;
}