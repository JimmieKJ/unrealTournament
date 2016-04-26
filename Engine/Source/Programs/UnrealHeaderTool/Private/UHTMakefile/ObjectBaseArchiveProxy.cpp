// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealHeaderTool.h"
#include "UHTMakefile/UHTMakefile.h"
#include "UHTMakefile/ObjectBaseArchiveProxy.h"

FObjectBaseArchiveProxy::FObjectBaseArchiveProxy(FUHTMakefile& UHTMakefile, const UObjectBase* ObjectBase)
{
	ObjectFlagsUint32 = static_cast<uint32>(ObjectBase->ObjectFlags);
	ClassIndex = UHTMakefile.GetClassIndex(ObjectBase->Class);
	Name = FNameArchiveProxy(UHTMakefile, ObjectBase->Name);
	OuterIndex = UHTMakefile.GetObjectIndex(ObjectBase->Outer);
}

UObjectBase* FObjectBaseArchiveProxy::CreateObjectBase(const FUHTMakefile& UHTMakefile) const
{
	UObjectBase* ObjectBase = new UObjectBase(nullptr, EObjectFlags(ObjectFlagsUint32), EInternalObjectFlags::Native, nullptr, Name.CreateName(UHTMakefile));
	PostConstruct(ObjectBase);
	return ObjectBase;
}

void FObjectBaseArchiveProxy::Resolve(UObjectBase* ObjectBase, const FUHTMakefile& UHTMakefile) const
{
	ObjectBase->Class = UHTMakefile.GetClassByIndex(ClassIndex);
	ObjectBase->Outer = UHTMakefile.GetObjectByIndex(OuterIndex);
}

void FObjectBaseArchiveProxy::AddReferencedNames(const UObjectBase* ObjectBase, FUHTMakefile& UHTMakefile)
{
	UHTMakefile.AddName(ObjectBase->Name);
}

void FObjectBaseArchiveProxy::PostConstruct(UObjectBase* ObjectBase) const
{
	ObjectBase->ObjectFlags = static_cast<EObjectFlags>(ObjectFlagsUint32);
}

FArchive& operator<<(FArchive& Ar, FObjectBaseArchiveProxy& ObjectBaseArchiveProxy)
{
	Ar << ObjectBaseArchiveProxy.ObjectFlagsUint32;
	Ar << ObjectBaseArchiveProxy.ClassIndex;
	Ar << ObjectBaseArchiveProxy.Name;
	Ar << ObjectBaseArchiveProxy.OuterIndex;

	return Ar;
}
