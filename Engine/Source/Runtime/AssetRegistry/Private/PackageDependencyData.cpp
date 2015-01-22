// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AssetRegistryPCH.h"

FName FPackageDependencyData::GetImportPackageName(int32 ImportIndex)
{
	FName Result;
	for (FPackageIndex LinkerIndex = FPackageIndex::FromImport(ImportIndex); !LinkerIndex.IsNull();)
	{
		FObjectResource* Resource = &ImpExp(LinkerIndex);
		LinkerIndex = Resource->OuterIndex;
		if ( LinkerIndex.IsNull() )
		{
			Result = Resource->ObjectName;
		}
	}

	return Result;
}