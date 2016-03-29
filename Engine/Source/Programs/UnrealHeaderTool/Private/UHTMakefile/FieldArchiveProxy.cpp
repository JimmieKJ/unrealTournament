// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealHeaderTool.h"
#include "UHTMakefile/FieldArchiveProxy.h"
#include "UHTMakefile/UHTMakefile.h"

FFieldArchiveProxy::FFieldArchiveProxy(FUHTMakefile& UHTMakefile, const UField* Field)
	: FObjectBaseArchiveProxy(UHTMakefile, Field)
{
	NextIndex = UHTMakefile.GetFieldIndex(Field->Next);
}

void FFieldArchiveProxy::Resolve(UField* Field, const FUHTMakefile& UHTMakefile) const
{
	FObjectBaseArchiveProxy::Resolve(Field, UHTMakefile);
	Field->Next = UHTMakefile.GetFieldByIndex(NextIndex);
}

void FFieldArchiveProxy::AddReferencedNames(const UField* Field, FUHTMakefile& UHTMakefile)
{
	FObjectBaseArchiveProxy::AddReferencedNames(Field, UHTMakefile);
}

FArchive& operator<<(FArchive& Ar, FFieldArchiveProxy& FieldArchiveProxy)
{
	Ar << static_cast<FObjectBaseArchiveProxy&>(FieldArchiveProxy);
	Ar << FieldArchiveProxy.NextIndex;

	return Ar;
}
