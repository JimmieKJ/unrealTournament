// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BlueprintCompilerCppBackendModulePrivatePCH.h"
#include "BlueprintCompilerCppBackendGatherDependencies.h"
#include "IBlueprintCompilerCppBackendModule.h"
#include "BlueprintEditorUtils.h"
#include "K2Node_EnumLiteral.h"

struct FGatherConvertedClassDependenciesHelperBase : public FReferenceCollector
{
	TSet<UObject*> SerializedObjects;
	FGatherConvertedClassDependencies& Dependencies;

	FGatherConvertedClassDependenciesHelperBase(FGatherConvertedClassDependencies& InDependencies)
		: Dependencies(InDependencies)
	{ }

	virtual bool IsIgnoringArchetypeRef() const override { return false; }
	virtual bool IsIgnoringTransient() const override { return true; }

	void FindReferences(UObject* Object)
	{
		UProperty* Property = Cast<UProperty>(Object);
		if (Property && Property->HasAnyPropertyFlags(CPF_DevelopmentAssets))
		{
			return;
		}

		{
			FSimpleObjectReferenceCollectorArchive CollectorArchive(Object, *this);
			CollectorArchive.SetSerializedProperty(nullptr);
			CollectorArchive.SetFilterEditorOnly(true);
			if (UClass* AsBPGC = Cast<UBlueprintGeneratedClass>(Object))
			{
				Object = Dependencies.FindOriginalClass(AsBPGC);
			}
			Object->Serialize(CollectorArchive);
		}
	}

	void FindReferencesForNewObject(UObject* Object)
	{
		bool AlreadyAdded = false;
		SerializedObjects.Add(Object, &AlreadyAdded);
		if (!AlreadyAdded)
		{
			FindReferences(Object);
		}
	}

	void IncludeTheHeaderInBody(UField* InField)
	{
		if (InField && !Dependencies.IncludeInHeader.Contains(InField))
		{
			Dependencies.IncludeInBody.Add(InField);
		}
	}
};

struct FFindAssetsToInclude : public FGatherConvertedClassDependenciesHelperBase
{
	FFindAssetsToInclude(FGatherConvertedClassDependencies& InDependencies)
		: FGatherConvertedClassDependenciesHelperBase(InDependencies)
	{
		FindReferences(Dependencies.GetActualStruct());
	}

	virtual void HandleObjectReference(UObject*& InObject, const UObject* InReferencingObject, const UProperty* InReferencingProperty) override
	{
		UObject* Object = InObject;
		if (!Object || Object->IsA<UBlueprint>())
		{
			return;
		}

		UClass* ActualClass = Cast<UClass>(Dependencies.GetActualStruct());
		UStruct* CurrentlyConvertedStruct = ActualClass ? Dependencies.FindOriginalClass(ActualClass) : Dependencies.GetActualStruct();
		ensure(CurrentlyConvertedStruct);
		if (Object == CurrentlyConvertedStruct)
		{
			return;
		}

		const bool bUseZConstructorInGeneratedCode = false;

		//TODO: What About Delegates?
		auto ObjAsBPGC = Cast<UBlueprintGeneratedClass>(Object);
		const bool bWillBeConvetedAsBPGC = ObjAsBPGC && Dependencies.WillClassBeConverted(ObjAsBPGC);
		if (bWillBeConvetedAsBPGC)
		{
			if (ObjAsBPGC != CurrentlyConvertedStruct)
			{
				Dependencies.ConvertedClasses.Add(ObjAsBPGC);
				if(!bUseZConstructorInGeneratedCode)
				{
					IncludeTheHeaderInBody(ObjAsBPGC);
				}
			}
		}
		else if (UUserDefinedStruct* UDS = Cast<UUserDefinedStruct>(Object))
		{
			if (!UDS->HasAnyFlags(RF_ClassDefaultObject))
			{
				Dependencies.ConvertedStructs.Add(UDS);
				if(!bUseZConstructorInGeneratedCode)
				{
					IncludeTheHeaderInBody(UDS);
				}
			}
		}
		else if (UUserDefinedEnum* UDE = Cast<UUserDefinedEnum>(Object))
		{
			if (!UDE->HasAnyFlags(RF_ClassDefaultObject))
			{
				Dependencies.ConvertedEnum.Add(UDE);
			}
		}
		else if ((Object->IsAsset() || ObjAsBPGC) && !Object->IsIn(CurrentlyConvertedStruct))
		{
			// include all not converted super classes
			for (auto SuperBPGC = ObjAsBPGC ? Cast<UBlueprintGeneratedClass>(ObjAsBPGC->GetSuperClass()) : nullptr;
				SuperBPGC && !Dependencies.WillClassBeConverted(SuperBPGC);
				SuperBPGC = Cast<UBlueprintGeneratedClass>(SuperBPGC->GetSuperClass()))
			{
				Dependencies.Assets.AddUnique(SuperBPGC);
			}

			Dependencies.Assets.AddUnique(Object);
			return;
		}
		else if (auto ObjAsClass = Cast<UClass>(Object))
		{
			if (ObjAsClass->HasAnyClassFlags(CLASS_Native))
			{
				return;
			}
		}
		else if (Object->IsA<UScriptStruct>())
		{
			return;
		}

		FindReferencesForNewObject(Object);
	}
};

struct FFindHeadersToInclude : public FGatherConvertedClassDependenciesHelperBase
{
	FFindHeadersToInclude(FGatherConvertedClassDependencies& InDependencies)
		: FGatherConvertedClassDependenciesHelperBase(InDependencies)
	{
		FindReferences(Dependencies.GetActualStruct());

		// special case - literal enum
		UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Dependencies.GetActualStruct());
		UBlueprint* BP = BPGC ? Cast<UBlueprint>(BPGC->ClassGeneratedBy) : nullptr;
		if (BP)
		{
			TArray<UEdGraph*> Graphs;
			BP->GetAllGraphs(Graphs);
			for (UEdGraph* Graph : Graphs)
			{
				if (Graph)
				{
					TArray<UK2Node_EnumLiteral*> LiteralEnumNodes;
					Graph->GetNodesOfClass<UK2Node_EnumLiteral>(LiteralEnumNodes);
					for (UK2Node_EnumLiteral* LiteralEnumNode : LiteralEnumNodes)
					{
						UEnum* Enum = LiteralEnumNode ? LiteralEnumNode->Enum : nullptr;
						IncludeTheHeaderInBody(Enum);
					}
				}
			}
		}

		// Include classes of native subobjects
		if (BPGC)
		{
			UClass* NativeSuperClass = BPGC->GetSuperClass();
			for (; NativeSuperClass && !NativeSuperClass->HasAnyClassFlags(CLASS_Native); NativeSuperClass = NativeSuperClass->GetSuperClass())
			{}
			UObject* NativeCDO = NativeSuperClass ? NativeSuperClass->GetDefaultObject(false) : nullptr;
			if (NativeCDO)
			{
				TArray<UObject*> DefaultSubobjects;
				NativeCDO->GetDefaultSubobjects(DefaultSubobjects);
				for (UObject* DefaultSubobject : DefaultSubobjects)
				{
					IncludeTheHeaderInBody(DefaultSubobject ? DefaultSubobject->GetClass() : nullptr);
				}
			}
		}
	}

	virtual void HandleObjectReference(UObject*& InObject, const UObject* InReferencingObject, const UProperty* InReferencingProperty) override
	{
		UObject* Object = InObject;
		if (!Object || Object->IsA<UBlueprint>())
		{
			return;
		}

		UClass* ActualClass = Cast<UClass>(Dependencies.GetActualStruct());
		UStruct* CurrentlyConvertedStruct = ActualClass ? Dependencies.FindOriginalClass(ActualClass) : Dependencies.GetActualStruct();
		ensure(CurrentlyConvertedStruct);
		if (Object == CurrentlyConvertedStruct)
		{
			return;
		}

		{
			auto ObjAsField = Cast<UField>(Object);
			if (!ObjAsField)
			{
				const bool bTransientObject = (Object->HasAnyFlags(RF_Transient) && !Object->IsIn(CurrentlyConvertedStruct)) || Object->IsIn(GetTransientPackage());
				if (bTransientObject)
				{
					return;
				}

				ObjAsField = Object->GetClass();
			}

			if (ObjAsField && !ObjAsField->HasAnyFlags(RF_ClassDefaultObject))
			{
				if (ObjAsField->IsA<UProperty>())
				{
					ObjAsField = ObjAsField->GetOwnerStruct();
				}
				if (ObjAsField->IsA<UFunction>())
				{
					ObjAsField = ObjAsField->GetOwnerClass();
				}
				IncludeTheHeaderInBody(ObjAsField);
			}
		}

		if ((Object->IsAsset() || Object->IsA<UBlueprintGeneratedClass>()) && !Object->IsIn(CurrentlyConvertedStruct))
		{
			return;
		}

		auto OwnedByAnythingInHierarchy = [&]()->bool
		{
			for (UStruct* IterStruct = CurrentlyConvertedStruct; IterStruct; IterStruct = IterStruct->GetSuperStruct())
			{
				if (Object->IsIn(IterStruct))
				{
					return true;
				}
				UClass* IterClass = Cast<UClass>(IterStruct);
				UObject* CDO = IterClass ? IterClass->GetDefaultObject(false) : nullptr;
				if (CDO && Object->IsIn(CDO))
				{
					return true;
				}
			}
			return false;
		};
		if (!Object->IsA<UField>() && !Object->HasAnyFlags(RF_ClassDefaultObject) && !OwnedByAnythingInHierarchy())
		{
			Object = Object->GetClass();
		}

		FindReferencesForNewObject(Object);
	}
};

FGatherConvertedClassDependencies::FGatherConvertedClassDependencies(UStruct* InStruct) : OriginalStruct(InStruct)
{
	check(OriginalStruct);

	// Gather headers and type declarations for header.
	DependenciesForHeader();
	// Gather headers (only from the class hierarchy) to include in body.
	FFindHeadersToInclude FindHeadersToInclude(*this);
	// Gather assets that must be referenced.
	FFindAssetsToInclude FindAssetsToInclude(*this);

	static const FBoolConfigValueHelper DontNativizeDataOnlyBP(TEXT("BlueprintNativizationSettings"), TEXT("bDontNativizeDataOnlyBP"));
	if (DontNativizeDataOnlyBP)
	{
		auto RemoveFieldsFromDataOnlyBP = [&](TSet<UField*>& FieldSet)
		{
			TSet<UField*> FieldsToAdd;
			for (auto Iter = FieldSet.CreateIterator(); Iter; ++Iter)
			{
				UClass* CurrentClass = (*Iter) ? (*Iter)->GetOwnerClass() : nullptr;
				UBlueprint* CurrentBP = CurrentClass ? Cast<UBlueprint>(CurrentClass->ClassGeneratedBy) : nullptr;
				if (CurrentBP && FBlueprintEditorUtils::IsDataOnlyBlueprint(CurrentBP))
				{
					Iter.RemoveCurrent();
					FieldsToAdd.Add(GetFirstNativeOrConvertedClass(CurrentClass, true));
				}
			}

			FieldSet.Append(FieldsToAdd);
		};
		RemoveFieldsFromDataOnlyBP(IncludeInHeader);
		RemoveFieldsFromDataOnlyBP(DeclareInHeader);
		RemoveFieldsFromDataOnlyBP(IncludeInBody);
	}
}

UClass* FGatherConvertedClassDependencies::GetFirstNativeOrConvertedClass(UClass* InClass, bool bExcludeBPDataOnly) const
{
	check(InClass);
	for (UClass* ItClass = InClass; ItClass; ItClass = ItClass->GetSuperClass())
	{
		UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(ItClass);
		if (ItClass->HasAnyClassFlags(CLASS_Native) || WillClassBeConverted(BPGC))
		{
			if (bExcludeBPDataOnly)
			{
				UBlueprint* BP = BPGC ? Cast<UBlueprint>(BPGC->ClassGeneratedBy) : nullptr;
				if (BP && FBlueprintEditorUtils::IsDataOnlyBlueprint(BP))
				{
					continue;
				}
			}
			return ItClass;
		}
	}
	check(false);
	return nullptr;
};

UClass* FGatherConvertedClassDependencies::FindOriginalClass(const UClass* InClass) const
{
	if (InClass)
	{
		IBlueprintCompilerCppBackendModule& BackEndModule = (IBlueprintCompilerCppBackendModule&)IBlueprintCompilerCppBackendModule::Get();
		auto ClassWeakPtrPtr = BackEndModule.GetOriginalClassMap().Find(InClass);
		UClass* OriginalClass = ClassWeakPtrPtr ? ClassWeakPtrPtr->Get() : nullptr;
		return OriginalClass ? OriginalClass : const_cast<UClass*>(InClass);
	}
	return nullptr;
}

bool FGatherConvertedClassDependencies::WillClassBeConverted(const UBlueprintGeneratedClass* InClass) const
{
	if (InClass && !InClass->HasAnyFlags(RF_ClassDefaultObject))
	{
		const UClass* ClassToCheck = FindOriginalClass(InClass);

		IBlueprintCompilerCppBackendModule& BackEndModule = (IBlueprintCompilerCppBackendModule&)IBlueprintCompilerCppBackendModule::Get();
		const auto& WillBeConvertedQuery = BackEndModule.OnIsTargetedForConversionQuery();

		if (WillBeConvertedQuery.IsBound())
		{
			return WillBeConvertedQuery.Execute(ClassToCheck);
		}
		return true;
	}
	return false;
}

void FGatherConvertedClassDependencies::DependenciesForHeader()
{
	TArray<UObject*> ObjectsToCheck;
	GetObjectsWithOuter(OriginalStruct, ObjectsToCheck, true);

	TArray<UObject*> NeededObjects;
	FReferenceFinder HeaderReferenceFinder(NeededObjects, nullptr, false, false, true, false);

	auto ShouldIncludeHeaderFor = [&](UObject* InObj)->bool
	{
		if (InObj
			&& (InObj->IsA<UClass>() || InObj->IsA<UEnum>() || InObj->IsA<UScriptStruct>())
			&& !InObj->HasAnyFlags(RF_ClassDefaultObject))
		{
			auto ObjAsBPGC = Cast<UBlueprintGeneratedClass>(InObj);
			const bool bWillBeConvetedAsBPGC = ObjAsBPGC && WillClassBeConverted(ObjAsBPGC);
			const bool bRemainAsUnconvertedBPGC = ObjAsBPGC && !bWillBeConvetedAsBPGC;
			if (!bRemainAsUnconvertedBPGC && (InObj->GetOutermost() != OriginalStruct->GetOutermost()))
			{
				return true;
			}
		}
		return false;
	};

	for (auto Obj : ObjectsToCheck)
	{
		const UProperty* Property = Cast<const UProperty>(Obj);
		if (const UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Property))
		{
			Property = ArrayProperty->Inner;
		}
		const UProperty* OwnerProperty = IsValid(Property) ? Property->GetOwnerProperty() : nullptr;
		const bool bIsParam = OwnerProperty && (0 != (OwnerProperty->PropertyFlags & CPF_Parm)) && OwnerProperty->IsIn(OriginalStruct);
		const bool bIsMemberVariable = OwnerProperty && (OwnerProperty->GetOuter() == OriginalStruct);
		if (bIsParam || bIsMemberVariable)
		{
			if (auto ObjectProperty = Cast<const UObjectPropertyBase>(Property))
			{
				DeclareInHeader.Add(GetFirstNativeOrConvertedClass(ObjectProperty->PropertyClass));
			}
			else if (auto InterfaceProperty = Cast<const UInterfaceProperty>(Property))
			{
				IncludeInHeader.Add(InterfaceProperty->InterfaceClass);
			}
			else if (auto DelegateProperty = Cast<const UDelegateProperty>(Property))
			{
				IncludeInHeader.Add(DelegateProperty->SignatureFunction ? DelegateProperty->SignatureFunction->GetOwnerStruct() : nullptr);
			}
			/* MC Delegate signatures are recreated in local scope anyway.
			else if (auto MulticastDelegateProperty = Cast<const UMulticastDelegateProperty>(Property))
			{
				IncludeInHeader.Add(MulticastDelegateProperty->SignatureFunction ? MulticastDelegateProperty->SignatureFunction->GetOwnerClass() : nullptr);
			}
			*/
			else if (const UByteProperty* ByteProperty = Cast<const UByteProperty>(Property))
			{ 
				// HeaderReferenceFinder.FindReferences(Obj); cannot find this enum..
				IncludeInHeader.Add(ByteProperty->Enum);
			}
			else
			{
				HeaderReferenceFinder.FindReferences(Obj);
			}
		}
	}

	if (auto SuperStruct = OriginalStruct->GetSuperStruct())
	{
		IncludeInHeader.Add(SuperStruct);
	}

	if (auto SourceClass = Cast<UClass>(OriginalStruct))
	{
		for (auto& ImplementedInterface : SourceClass->Interfaces)
		{
			IncludeInHeader.Add(ImplementedInterface.Class);
		}
	}

	for (auto Obj : NeededObjects)
	{
		if (ShouldIncludeHeaderFor(Obj))
		{
			IncludeInHeader.Add(CastChecked<UField>(Obj));
		}
	}

	// DEFAULT VALUES FROM UDS:
	UUserDefinedStruct* UDS = Cast<UUserDefinedStruct>(OriginalStruct);
	if (UDS)
	{
		FStructOnScope StructOnScope(UDS);
		UDS->InitializeDefaultValue(StructOnScope.GetStructMemory());
		for (TFieldIterator<UObjectPropertyBase> PropertyIt(UDS); PropertyIt; ++PropertyIt)
		{
			UObject* DefaultValueObject = ((UObjectPropertyBase*)*PropertyIt)->GetObjectPropertyValue_InContainer(StructOnScope.GetStructMemory());
			UField* ObjAsField = Cast<UField>(DefaultValueObject);
			UField* FieldForHeader = ObjAsField ? ObjAsField : (DefaultValueObject ? DefaultValueObject->GetClass() : nullptr);
			IncludeInHeader.Add(FieldForHeader);
		}
	}

	// REMOVE UNNECESSARY HEADERS
	UClass* AsBPGC = Cast<UBlueprintGeneratedClass>(OriginalStruct);
	UClass* OriginalClassFromOriginalPackage = AsBPGC ? FindOriginalClass(AsBPGC) : nullptr;
	const UPackage* OriginalStructPackage = OriginalStruct ? OriginalStruct->GetOutermost() : nullptr;
	for (auto Iter = IncludeInHeader.CreateIterator(); Iter; ++Iter)
	{
		UField* CurrentField = *Iter;
		if (CurrentField)
		{
			if (CurrentField->GetOutermost() == OriginalStructPackage)
			{
				Iter.RemoveCurrent();
			}
			else if (CurrentField == OriginalStruct)
			{
				Iter.RemoveCurrent();
			}
			else if (OriginalClassFromOriginalPackage && (CurrentField == OriginalClassFromOriginalPackage))
			{
				Iter.RemoveCurrent();
			}
		}
	}
}

