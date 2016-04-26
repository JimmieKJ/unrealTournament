// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealHeaderTool.h"
#include "UHTMakefile/UHTMakefile.h"
#include "UHTMakefile/EnumArchiveProxy.h"

FEnumArchiveProxy::FEnumArchiveProxy(FUHTMakefile& UHTMakefile, const UEnum* Enum)
	: FFieldArchiveProxy(UHTMakefile, Enum)
{
	EnumFlags = static_cast<uint32>(Enum->GetFlags());
	CppForm = static_cast<uint32>(Enum->GetCppForm());

	EnumValues.Empty(Enum->NumEnums());
	for (int32 i = 0; i < Enum->NumEnums(); ++i)
	{
		FName EnumName = Enum->GetNameByIndex(i);
		uint8 Value = Enum->GetValueByIndex(i);
		EnumValues.Add(TPairInitializer<FNameArchiveProxy, uint8>(FNameArchiveProxy(UHTMakefile, EnumName), Value));
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
	TArray<TPair<FName, uint8>> Names;
	Names.Empty(EnumValues.Num());
	for (auto& Kvp : EnumValues)
	{
		Names.Add(TPairInitializer<FName, uint8>(Kvp.Key.CreateName(UHTMakefile), Kvp.Value));
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
	Ar << EnumArchiveProxy.EnumValues;
	Ar << EnumArchiveProxy.CppType;

	return Ar;
}
