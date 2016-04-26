// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealHeaderTool.h"
#include "UHTMakefile/UHTMakefile.h"
#include "UHTMakefile/ClassArchiveProxy.h"

FClassArchiveProxy::FClassArchiveProxy(FUHTMakefile& UHTMakefile, const UClass* Class)
	: FStructArchiveProxy(UHTMakefile, Class)
{
	ClassUnique = Class->ClassUnique;
	ClassFlags = Class->ClassFlags;
	ClassCastFlags = Class->ClassCastFlags;
	ClassWithinIndex = UHTMakefile.GetClassIndex(Class->ClassWithin);
	ClassGeneratedByIndex = UHTMakefile.GetObjectIndex(Class->ClassGeneratedBy);
	ClassConfigName = FNameArchiveProxy(UHTMakefile, Class->ClassConfigName);
	bCooked = Class->bCooked;
	ClassReps.Empty(Class->ClassReps.Num());
	for (FRepRecord RepRecord : Class->ClassReps)
	{
		ClassReps.Add(FRepRecordArchiveProxy(UHTMakefile, RepRecord));
	}

	NetFields.Empty(Class->NetFields.Num());
	for (UField* Field : Class->NetFields)
	{
		NetFields.Add(UHTMakefile.GetFieldIndex(Field));
	}

	FuncMap.Empty(Class->FuncMap.Num());
	for (auto& Kvp : Class->FuncMap)
	{
		FuncMap.Add(TPairInitializer<FNameArchiveProxy, int32>(FNameArchiveProxy(UHTMakefile, Kvp.Key), UHTMakefile.GetFunctionIndex(Kvp.Value)));
	}

	Interfaces.Empty(Class->Interfaces.Num());
	for (FImplementedInterface ImplementedInterface : Class->Interfaces)
	{
		Interfaces.Add(FImplementedInterfaceArchiveProxy(UHTMakefile, ImplementedInterface));
	}
}

void FClassArchiveProxy::Resolve(UClass* Class, const FUHTMakefile& UHTMakefile) const
{
	FStructArchiveProxy::Resolve(Class, UHTMakefile);
	Class->SetSuperStruct(UHTMakefile.GetStructByIndex(SuperStructIndex));
	Class->ClassWithin = UHTMakefile.GetClassByIndex(ClassWithinIndex);
	Class->ClassGeneratedBy = UHTMakefile.GetObjectByIndex(ClassGeneratedByIndex);

	Class->NetFields.Empty(NetFields.Num());
	for (FSerializeIndex FieldIndex : NetFields)
	{
		Class->NetFields.Add(UHTMakefile.GetFieldByIndex(FieldIndex));
	}

	Class->FuncMap.Empty(FuncMap.Num());
	for (auto& Kvp : FuncMap)
	{
		Class->FuncMap.Add(Kvp.Key.CreateName(UHTMakefile), UHTMakefile.GetFunctionByIndex(Kvp.Value));
	}

	Class->Interfaces.Empty(Interfaces.Num());
	for (FImplementedInterfaceArchiveProxy ImplementedInterfaceArchiveProxy : Interfaces)
	{
		FImplementedInterface ImplementedInterface = ImplementedInterfaceArchiveProxy.CreateImplementedInterface(UHTMakefile);
		ImplementedInterfaceArchiveProxy.Resolve(ImplementedInterface, UHTMakefile);
		Class->Interfaces.Add(ImplementedInterface);
	}
	Class->ClassConfigName = ClassConfigName.CreateName(UHTMakefile);
}

void FClassArchiveProxy::AddReferencedNames(const UClass* Class, FUHTMakefile& UHTMakefile)
{
	FStructArchiveProxy::AddReferencedNames(Class, UHTMakefile);
	UHTMakefile.AddName(Class->ClassConfigName);
	for (auto& Kvp : Class->FuncMap)
	{
		UHTMakefile.AddName(Kvp.Key);
	}
}

FArchive& operator<<(FArchive& Ar, FClassArchiveProxy& ClassArchiveProxy)
{
	Ar << static_cast<FStructArchiveProxy&>(ClassArchiveProxy);
	Ar << ClassArchiveProxy.ClassUnique;
	Ar << ClassArchiveProxy.ClassFlags;
	Ar << ClassArchiveProxy.ClassCastFlags;
	Ar << ClassArchiveProxy.ClassWithinIndex;
	Ar << ClassArchiveProxy.ClassGeneratedByIndex;
	Ar << ClassArchiveProxy.ClassConfigName;
	Ar << ClassArchiveProxy.bCooked;
	Ar << ClassArchiveProxy.ClassReps;
	Ar << ClassArchiveProxy.NetFields;
	Ar << ClassArchiveProxy.FuncMap;
	Ar << ClassArchiveProxy.Interfaces;
	Ar << ClassArchiveProxy.ConvertedSubobjectsFromBPGC;

	return Ar;
}

UClass* FClassArchiveProxy::CreateClass(const FUHTMakefile& UHTMakefile) const
{
	UObject* Outer = UHTMakefile.GetObjectByIndex(OuterIndex);
	FName ClassName = Name.CreateName(UHTMakefile);
	UClass* Class = FindObject<UClass>(Outer, *ClassName.ToString());
	if (Class == nullptr || !Class->HasAnyClassFlags(CLASS_Native))
	{
		Class = new(EC_InternalUseOnlyConstructor, Outer, ClassName, (EObjectFlags)ObjectFlagsUint32) UClass(FObjectInitializer(), nullptr);
	}
	PostConstruct(Class);

	return Class;
}

void FClassArchiveProxy::PostConstruct(UClass* Class) const
{
	Class->ClassFlags |= ClassFlags;
	Class->ClassCastFlags |= ClassCastFlags;
}
