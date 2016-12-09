// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "RepRecordArchiveProxy.h"
#include "UnrealHeaderTool.h"
#include "UHTMakefile.h"

FRepRecordArchiveProxy::FRepRecordArchiveProxy(const FUHTMakefile& UHTMakefile, const FRepRecord& RepRecord)
{
	PropertyIndex = UHTMakefile.GetPropertyIndex(RepRecord.Property);
	Index = RepRecord.Index;
}

FRepRecord FRepRecordArchiveProxy::CreateRepRecord(const FUHTMakefile& UHTMakefile) const
{
	return FRepRecord(nullptr, Index);
}

FArchive& operator<<(FArchive& Ar, FRepRecordArchiveProxy& RepRecordArchiveProxy)
{
	Ar << RepRecordArchiveProxy.PropertyIndex;
	Ar << RepRecordArchiveProxy.Index;

	return Ar;
}

void FRepRecordArchiveProxy::Resolve(FRepRecord& RepRecord, const FUHTMakefile& UHTMakefile) const
{
	RepRecord.Property = UHTMakefile.GetPropertyByIndex(PropertyIndex);
}

