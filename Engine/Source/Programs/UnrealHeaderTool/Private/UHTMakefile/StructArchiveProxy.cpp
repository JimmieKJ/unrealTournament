// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "StructArchiveProxy.h"
#include "UnrealHeaderTool.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Class.h"
#include "UHTMakefile.h"

FStructArchiveProxy::FStructArchiveProxy(FUHTMakefile& UHTMakefile, const UStruct* Struct)
	: FFieldArchiveProxy(UHTMakefile, Struct)
{
	SuperStructIndex = UHTMakefile.GetStructIndex(Struct->SuperStruct);
	ChildrenIndex = UHTMakefile.GetFieldIndex(Struct->Children);
	PropertiesSize = Struct->PropertiesSize;
	MinAlignment = Struct->PropertiesSize;
	Script = Struct->Script;
	PropertyLinkIndex = UHTMakefile.GetPropertyIndex(Struct->PropertyLink);
	RefLinkIndex = UHTMakefile.GetPropertyIndex(Struct->RefLink);
	DestructorLinkIndex = UHTMakefile.GetPropertyIndex(Struct->DestructorLink);
	PostConstructLinkIndex = UHTMakefile.GetPropertyIndex(Struct->PostConstructLink);
	ScriptObjectReferenceIndexes.Empty(Struct->ScriptObjectReferences.Num());
	for (UObject* ScriptObjectReference : Struct->ScriptObjectReferences)
	{
		ScriptObjectReferenceIndexes.Add(UHTMakefile.GetObjectIndex(ScriptObjectReference));
	}
}

UStruct* FStructArchiveProxy::CreateStruct(const FUHTMakefile& UHTMakefile) const
{
	UObject* Outer = UHTMakefile.GetObjectByIndex(OuterIndex);
	UStruct* Struct = new (EC_InternalUseOnlyConstructor, Outer, Name.CreateName(UHTMakefile), (EObjectFlags)ObjectFlagsUint32) UStruct(FObjectInitializer());
	PostConstruct(Struct);


	return Struct;
}

void FStructArchiveProxy::PostConstruct(UStruct* Struct) const
{
	Struct->PropertiesSize = PropertiesSize;
	Struct->MinAlignment = MinAlignment;
	Struct->Script = Script;
}

void FStructArchiveProxy::Resolve(UStruct* Struct, const FUHTMakefile& UHTMakefile) const
{
	FFieldArchiveProxy::Resolve(Struct, UHTMakefile);
	if (!Struct->IsA<UClass>())
	{
		Struct->SuperStruct = UHTMakefile.GetStructByIndex(SuperStructIndex);
	}
	Struct->Children = UHTMakefile.GetFieldByIndex(ChildrenIndex);
	Struct->PropertyLink = UHTMakefile.GetPropertyByIndex(PropertyLinkIndex);
	Struct->RefLink = UHTMakefile.GetPropertyByIndex(RefLinkIndex);
	Struct->DestructorLink = UHTMakefile.GetPropertyByIndex(DestructorLinkIndex);
	Struct->PostConstructLink = UHTMakefile.GetPropertyByIndex(PostConstructLinkIndex);
	for (FSerializeIndex ScriptObjectReferenceIndex : ScriptObjectReferenceIndexes)
	{
		Struct->ScriptObjectReferences.Add(UHTMakefile.GetObjectByIndex(ScriptObjectReferenceIndex));
	}
}

FArchive& operator<<(FArchive& Ar, FStructArchiveProxy& StructArchiveProxy)
{
	Ar << static_cast<FFieldArchiveProxy&>(StructArchiveProxy);
	Ar << StructArchiveProxy.SuperStructIndex;
	Ar << StructArchiveProxy.ChildrenIndex;
	Ar << StructArchiveProxy.PropertiesSize;
	Ar << StructArchiveProxy.MinAlignment;
	Ar << StructArchiveProxy.Script;
	Ar << StructArchiveProxy.PropertyLinkIndex;
	Ar << StructArchiveProxy.RefLinkIndex;
	Ar << StructArchiveProxy.DestructorLinkIndex;
	Ar << StructArchiveProxy.PostConstructLinkIndex;
	Ar << StructArchiveProxy.ScriptObjectReferenceIndexes;

	return Ar;
}
