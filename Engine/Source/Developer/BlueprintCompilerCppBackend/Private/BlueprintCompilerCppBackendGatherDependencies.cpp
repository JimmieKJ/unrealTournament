// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintCompilerCppBackendModulePrivatePCH.h"
#include "BlueprintCompilerCppBackendGatherDependencies.h"
#include "BlueprintSupport.h"

FGatherConvertedClassDependencies::FGatherConvertedClassDependencies(UStruct* InStruct) : OriginalStruct(InStruct)
{
	check(OriginalStruct);
	DependenciesForHeader();
	FindReferences(OriginalStruct);
}

bool FGatherConvertedClassDependencies::WillClassBeConverted(const UBlueprintGeneratedClass* InClass) const
{
	return FReplaceCookedBPGC::Get().CouldBeConverted(InClass);
}

void FGatherConvertedClassDependencies::DependenciesForHeader()
{
	TArray<UObject*> ObjectsToCheck;
	GetObjectsWithOuter(OriginalStruct, ObjectsToCheck, true);

	TArray<UObject*> NeededObjects;
	FReferenceFinder HeaderReferenceFinder(NeededObjects, nullptr, false, false, true, false);

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

bool FGatherConvertedClassDependencies::ShouldIncludeHeaderFor(UObject* Obj) const
{
	if (Obj
		&& (Obj->IsA<UClass>() || Obj->IsA<UEnum>() || Obj->IsA<UScriptStruct>())
		&& !Obj->HasAnyFlags(RF_ClassDefaultObject))
	{
		auto ObjAsBPGC = Cast<UBlueprintGeneratedClass>(Obj);
		const bool bWillBeConvetedAsBPGC = ObjAsBPGC && WillClassBeConverted(ObjAsBPGC);
		const bool bRemainAsUnconvertedBPGC = ObjAsBPGC && !bWillBeConvetedAsBPGC;
		if (!bRemainAsUnconvertedBPGC && (Obj->GetOutermost() != OriginalStruct->GetOutermost()))
		{
			return true;
		}
	}
	return false;
}

void FGatherConvertedClassDependencies::HandleObjectReference(UObject*& InObject, const UObject* InReferencingObject, const UProperty* InReferencingProperty)
{
	if (!InObject || InObject->IsA<UBlueprint>() || (InObject == OriginalStruct))
	{
		return;
	}

	auto ObjAsField = Cast<UField>(InObject);
	if (ObjAsField && !IncludeInHeader.Contains(ObjAsField))
	{
		IncludeInBody.Add(ObjAsField);
	}

	//TODO: What About Delegates?
	auto ObjAsBPGC = Cast<UBlueprintGeneratedClass>(InObject);
	const bool bWillBeConvetedAsBPGC = ObjAsBPGC && WillClassBeConverted(ObjAsBPGC);
	if (bWillBeConvetedAsBPGC)
	{
		ConvertedClasses.Add(ObjAsBPGC);
	}
	else if (InObject->IsA<UUserDefinedStruct>())
	{
		if (!InObject->HasAnyFlags(RF_ClassDefaultObject))
		{
			ConvertedStructs.Add(CastChecked<UUserDefinedStruct>(InObject));
		}
	}
	else if (InObject->IsA<UUserDefinedEnum>())
	{
		if (!InObject->HasAnyFlags(RF_ClassDefaultObject))
		{
			ConvertedEnum.Add(CastChecked<UUserDefinedEnum>(InObject));
		}
	}
	else if ((InObject->IsAsset() || ObjAsBPGC) && !InObject->IsIn(OriginalStruct))
	{
		// include all not converted super classes
		for (auto SuperBPGC = ObjAsBPGC ? Cast<UBlueprintGeneratedClass>(ObjAsBPGC->GetSuperClass()) : nullptr; 
			SuperBPGC && !WillClassBeConverted(SuperBPGC);
			SuperBPGC = Cast<UBlueprintGeneratedClass>(SuperBPGC->GetSuperClass()))
		{
			Assets.AddUnique(SuperBPGC);
		}

		Assets.AddUnique(InObject);
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

	bool AlreadyAdded = false;
	SerializedObjects.Add(InObject, &AlreadyAdded);
	if (!AlreadyAdded)
	{
		FindReferences(InObject);
	}
}

void FGatherConvertedClassDependencies::FindReferences(UObject* Object)
{
	{
		FSimpleObjectReferenceCollectorArchive CollectorArchive(Object, *this);
		CollectorArchive.SetSerializedProperty(nullptr);
		CollectorArchive.SetFilterEditorOnly(true);
		Object->SerializeScriptProperties(CollectorArchive);
	}
	Object->CallAddReferencedObjects(*this);
}