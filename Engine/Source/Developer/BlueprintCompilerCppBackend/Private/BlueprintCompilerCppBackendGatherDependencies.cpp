// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintCompilerCppBackendModulePrivatePCH.h"
#include "BlueprintCompilerCppBackendGatherDependencies.h"
#include "IBlueprintCompilerCppBackendModule.h"

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
		{
			FSimpleObjectReferenceCollectorArchive CollectorArchive(Object, *this);
			CollectorArchive.SetSerializedProperty(nullptr);
			CollectorArchive.SetFilterEditorOnly(true);
			Object->SerializeScriptProperties(CollectorArchive);
		}
		Object->CallAddReferencedObjects(*this);
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
		if (!InObject || InObject->IsA<UBlueprint>() || (InObject == Dependencies.GetActualStruct()))
		{
			return;
		}

		const bool bUseZConstructorInGeneratedCode = true;

		//TODO: What About Delegates?
		auto ObjAsBPGC = Cast<UBlueprintGeneratedClass>(InObject);
		const bool bWillBeConvetedAsBPGC = ObjAsBPGC && Dependencies.WillClassBeConverted(ObjAsBPGC);
		if (bWillBeConvetedAsBPGC)
		{
			if (ObjAsBPGC != Dependencies.GetActualStruct())
			{
				Dependencies.ConvertedClasses.Add(ObjAsBPGC);
				if(!bUseZConstructorInGeneratedCode)
				{
					IncludeTheHeaderInBody(ObjAsBPGC);
				}
			}
		}
		else if (UUserDefinedStruct* UDS = Cast<UUserDefinedStruct>(InObject))
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
		else if (UUserDefinedEnum* UDE = Cast<UUserDefinedEnum>(InObject))
		{
			if (!UDE->HasAnyFlags(RF_ClassDefaultObject))
			{
				Dependencies.ConvertedEnum.Add(UDE);
			}
		}
		else if ((InObject->IsAsset() || ObjAsBPGC) && !InObject->IsIn(Dependencies.GetActualStruct()))
		{
			// include all not converted super classes
			for (auto SuperBPGC = ObjAsBPGC ? Cast<UBlueprintGeneratedClass>(ObjAsBPGC->GetSuperClass()) : nullptr;
				SuperBPGC && !Dependencies.WillClassBeConverted(SuperBPGC);
				SuperBPGC = Cast<UBlueprintGeneratedClass>(SuperBPGC->GetSuperClass()))
			{
				Dependencies.Assets.AddUnique(SuperBPGC);
			}

			Dependencies.Assets.AddUnique(InObject);
			return;
		}
		else if (auto ObjAsClass = Cast<UClass>(InObject))
		{
			if (ObjAsClass->HasAnyClassFlags(CLASS_Native))
			{
				return;
			}
		}
		else if (InObject->IsA<UScriptStruct>())
		{
			return;
		}

		FindReferencesForNewObject(InObject);
	}
};

struct FFindHeadersToInclude : public FGatherConvertedClassDependenciesHelperBase
{
	FFindHeadersToInclude(FGatherConvertedClassDependencies& InDependencies)
		: FGatherConvertedClassDependenciesHelperBase(InDependencies)
	{
		FindReferences(Dependencies.GetActualStruct());
	}

	virtual void HandleObjectReference(UObject*& InObject, const UObject* InReferencingObject, const UProperty* InReferencingProperty) override
	{
		const bool bIncludeMembersFromParentClass = true;

		if (!InObject || InObject->IsA<UBlueprint>() || (InObject == Dependencies.GetActualStruct()))
		{
			return;
		}

		auto ObjAsField = Cast<UField>(InObject);
		if (ObjAsField)
		{
			IncludeTheHeaderInBody(ObjAsField);
		}

		if ((InObject->IsAsset() || InObject->IsA<UBlueprintGeneratedClass>()) && !InObject->IsIn(Dependencies.GetActualStruct()))
		{
			return;
		}
		else if (auto ObjAsClass = Cast<UClass>(InObject))
		{
			if (bIncludeMembersFromParentClass && (ObjAsClass != UObject::StaticClass()) && Dependencies.GetActualStruct()->IsChildOf(ObjAsClass))
			{
				FindReferencesForNewObject(InObject);
			}
			if (ObjAsClass->HasAnyClassFlags(CLASS_Native))
			{
				return;
			}
		}
		else if (InObject->IsA<UScriptStruct>())
		{
			return;
		}

		if ((InObject->GetOutermost() == Dependencies.GetActualStruct()->GetOutermost()) || 
			(bIncludeMembersFromParentClass && ObjAsField && Dependencies.GetActualStruct()->IsChildOf(ObjAsField->GetOwnerClass())))
		{
			FindReferencesForNewObject(InObject);
		}
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
}

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
		auto& WillBeConvertedQuery = BackEndModule.OnIsTargetedForConversionQuery();

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
		auto Property = Cast<const UProperty>(Obj);
		auto OwnerProperty = IsValid(Property) ? Property->GetOwnerProperty() : nullptr;
		const bool bIsParam = OwnerProperty && (0 != (OwnerProperty->PropertyFlags & CPF_Parm)) && OwnerProperty->IsIn(OriginalStruct);
		const bool bIsMemberVariable = OwnerProperty && (OwnerProperty->GetOuter() == OriginalStruct);
		if (bIsParam || bIsMemberVariable)
		{
			if (auto ObjectProperty = Cast<const UObjectPropertyBase>(Property))
			{
				if (ShouldIncludeHeaderFor(ObjectProperty->PropertyClass))
				{
					DeclareInHeader.Add(ObjectProperty->PropertyClass);
				}
			}
			else if (auto InterfaceProperty = Cast<const UInterfaceProperty>(Property))
			{
				IncludeInHeader.Add(InterfaceProperty->InterfaceClass);
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
}

