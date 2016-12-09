// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PropertyDataArchiveProxy.h"
#include "UnrealHeaderTool.h"
#include "ParserHelper.h"
#include "UHTMakefile.h"
#include "TokenDataArchiveProxy.h"

FPropertyDataArchiveProxy::FPropertyDataArchiveProxy(const FUHTMakefile& UHTMakefile, const FPropertyData* PropertyData)
{
	for (const auto& Kvp : *PropertyData)
	{
		EntriesIndexes.Add(UHTMakefile.GetPropertyDataEntryIndex(&Kvp));
	}
}

void FPropertyDataArchiveProxy::AddReferencedNames(const FPropertyData* PropertyData, FUHTMakefile& UHTMakefile)
{ }

FArchive& operator<<(FArchive& Ar, FPropertyDataArchiveProxy& PropertyDataArchiveProxy)
{
	Ar << PropertyDataArchiveProxy.EntriesIndexes;

	return Ar;
}

FPropertyData* FPropertyDataArchiveProxy::CreatePropertyData(const FUHTMakefile& UHTMakefile) const
{
	return new FPropertyData();
}

void FPropertyDataArchiveProxy::Resolve(FPropertyData* PropertyData, FUHTMakefile& UHTMakefile)
{
	PropertyData->Empty(EntriesIndexes.Num());
	for (int32 Index : EntriesIndexes)
	{
		const TPair<UProperty*, TSharedPtr<FTokenData>>* Entry = UHTMakefile.GetPropertyDataEntryByIndex(Index);
		UHTMakefile.ResolvePropertyDataEntry(Index);
		PropertyData->Add(Entry->Key, Entry->Value);
	}
}

