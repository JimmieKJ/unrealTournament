// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnumArchiveProxy.h"
#include "UnrealHeaderTool.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Class.h"
#include "UHTMakefile.h"

FEnumArchiveProxy::FEnumArchiveProxy(FUHTMakefile& UHTMakefile, const UEnum* Enum)
	: FFieldArchiveProxy(UHTMakefile, Enum)
{
	EnumFlags = static_cast<uint32>(Enum->GetFlags());
	CppForm = static_cast<uint32>(Enum->GetCppForm());

	EnumValuesX.Empty(Enum->NumEnums());
	for (int32 i = 0; i < Enum->NumEnums(); ++i)
	{
		FName EnumName = Enum->GetNameByIndex(i);
		int64 Value = Enum->GetValueByIndex(i);
		EnumValuesX.Add(TPairInitializer<FNameArchiveProxy, int64>(FNameArchiveProxy(UHTMakefile, EnumName), Value));
	}

	CppType = Enum->CppType;
}

UEnum* FEnumArchiveProxy::CreateEnum(const FUHTMakefile& UHTMakefile) const
{
	UObject* Outer = UHTMakefile.GetObjectByIndex(OuterIndex);

	// Create enum definition.
	UEnum* Enum = new(EC_InternalUseOnlyConstructor, Outer, Name.CreateName(UHTMakefile), (EObjectFlags)ObjectFlagsUint32) UEnum(FObjectInitializer());
	PostConstruct(Enum, UHTMakefile);
	return Enum;
}

void FEnumArchiveProxy::PostConstruct(UEnum* Enum, const FUHTMakefile& UHTMakefile) const
{
	TArray<TPair<FName, int64>> Names;
	Names.Empty(EnumValuesX.Num());
	for (auto& Kvp : EnumValuesX)
	{
		Names.Add(TPairInitializer<FName, int64>(Kvp.Key.CreateName(UHTMakefile), Kvp.Value));
	}

	Enum->SetEnums(Names, (UEnum::ECppForm)CppForm);
	Enum->CppType = CppType;
}

void FEnumArchiveProxy::AddReferencedNames(const UEnum* Enum, FUHTMakefile& UHTMakefile)
{
	FFieldArchiveProxy::AddReferencedNames(Enum, UHTMakefile);
	for (int32 i = 0; i < Enum->NumEnums(); ++i)
	{
		UHTMakefile.AddName(Enum->GetNameByIndex(i));
	}
}

FArchive& operator<<(FArchive& Ar, FEnumArchiveProxy& EnumArchiveProxy)
{
	Ar << static_cast<FFieldArchiveProxy&>(EnumArchiveProxy);
	Ar << EnumArchiveProxy.EnumFlags;
	Ar << EnumArchiveProxy.CppForm;
	Ar << EnumArchiveProxy.EnumValuesX;
	Ar << EnumArchiveProxy.CppType;

	return Ar;
}
