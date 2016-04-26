// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealHeaderTool.h"
#include "UHTMakefile/UHTMakefile.h"
#include "UHTMakefile/NameArchiveProxy.h"

FNameArchiveProxy::FNameArchiveProxy(const FUHTMakefile& UHTMakefile, FName Name)
{
	Number = Name.GetNumber();
	NameMapIndex = UHTMakefile.GetNameIndex(Name);
}

FName FNameArchiveProxy::CreateName(const FUHTMakefile& UHTMakefile) const
{
	// if the name wasn't loaded (because it wasn't valid in this context)
	FName MappedName = UHTMakefile.GetNameByIndex(NameMapIndex);
	if (MappedName.IsNone())
	{
		return NAME_None;
	}

	return FName(MappedName, Number);
}

FArchive& operator<<(FArchive& Ar, FNameArchiveProxy& NameArchiveProxy)
{
	Ar << NameArchiveProxy.NameMapIndex;
	Ar << NameArchiveProxy.Number;

	return Ar;
}
