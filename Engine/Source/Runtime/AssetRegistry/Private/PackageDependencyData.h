// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Linker.h"

class FPackageDependencyData : public FLinkerTables
{
public:
	/** The name of the package that dependency data is gathered from */
	FName PackageName;

	/**
	 * Return the package name of the UObject represented by the specified import. 
	 * 
	 * @param	PackageIndex	package index for the resource to get the name for
	 *
	 * @return	the path name of the UObject represented by the resource at PackageIndex, or the empty string if this isn't an import
	 */
	FName GetImportPackageName(int32 ImportIndex);

	/** Operator for serialization */
	friend FArchive& operator<<(FArchive& Ar, FPackageDependencyData& DependencyData)
	{
		// serialize out the asset info
		Ar << DependencyData.PackageName;
		Ar << DependencyData.ImportMap;
		Ar << DependencyData.StringAssetReferencesMap;

		if (Ar.UE4Ver() >= VER_UE4_ADDED_SEARCHABLE_NAMES)
		{
			Ar << DependencyData.SearchableNamesMap;
		}

		return Ar;
	}
};
