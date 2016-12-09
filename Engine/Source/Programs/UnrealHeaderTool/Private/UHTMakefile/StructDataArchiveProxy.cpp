// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "StructDataArchiveProxy.h"
#include "UnrealHeaderTool.h"

FStructDataArchiveProxy::FStructDataArchiveProxy(const FUHTMakefile& UHTMakefile, const FStructData* InStructData)
{
	StructData = FTokenArchiveProxy(UHTMakefile, &InStructData->StructData);
	StructPropertyData = FPropertyDataArchiveProxy(UHTMakefile, &InStructData->StructPropertyData);
}

void FStructDataArchiveProxy::AddReferencedNames(const FStructData* InStructData, FUHTMakefile& UHTMakefile)
{
	FTokenArchiveProxy::AddReferencedNames(&InStructData->StructData, UHTMakefile);
	FPropertyDataArchiveProxy::AddReferencedNames(&InStructData->StructPropertyData, UHTMakefile);
}

FStructData* FStructDataArchiveProxy::CreateStructData(FUHTMakefile& UHTMakefile)
{
	FToken* Token = StructData.CreateToken();
	FStructData* Result = new FStructData(*Token);
	StructPropertyData.Resolve(&Result->StructPropertyData, UHTMakefile);
	return Result;
}

FArchive& operator<<(FArchive& Ar, FStructDataArchiveProxy& StructDataArchiveProxy)
{
	Ar << StructDataArchiveProxy.StructData;
	Ar << StructDataArchiveProxy.StructPropertyData;

	return Ar;
}
