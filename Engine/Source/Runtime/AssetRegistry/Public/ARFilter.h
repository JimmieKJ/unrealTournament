// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/** A struct to serve as a filter for Asset Registry queries. Each component element is processed as an 'OR' operation while all the components are processed together as an 'AND' operation. */
struct FARFilter
{
	/** The filter component for package names */
	TArray<FName> PackageNames;
	/** The filter component for package paths */
	TArray<FName> PackagePaths;
	/** The filter component containing specific object paths */
	TArray<FName> ObjectPaths;
	/** The filter component for class names */
	TArray<FName> ClassNames;
	/** The filter component for properties marked with the AssetRegistrySearchable flag */
	TMultiMap<FName, FString> TagsAndValues;
	/** If bRecursiveClasses is true, this results will exclude classes (including subclasses) in this list */
	TSet<FName> RecursiveClassesExclusionSet;
	/** If true, PackagePath components will be recursive */
	bool bRecursivePaths;
	/** If true, Classes will include subclasses */
	bool bRecursiveClasses;
	/** If true, only on-disk assets will be returned. Be warned that this is rarely what you want and should only be used for performance reasons */
	bool bIncludeOnlyOnDiskAssets;

	FARFilter()
	{
		bRecursivePaths = false;
		bRecursiveClasses = false;
		bIncludeOnlyOnDiskAssets = false;
	}

	/** Appends the other filter to this one */
	void Append(const FARFilter& Other)
	{
		PackageNames.Append(Other.PackageNames);
		PackagePaths.Append(Other.PackagePaths);
		ObjectPaths.Append(Other.ObjectPaths);
		ClassNames.Append(Other.ClassNames);

		for (auto TagIt = Other.TagsAndValues.CreateConstIterator(); TagIt; ++TagIt)
		{
			TagsAndValues.Add(TagIt.Key(), TagIt.Value());
		}

		RecursiveClassesExclusionSet.Append(Other.RecursiveClassesExclusionSet);

		bRecursivePaths |= Other.bRecursivePaths;
		bRecursiveClasses |= Other.bRecursiveClasses;
		bIncludeOnlyOnDiskAssets |= Other.bIncludeOnlyOnDiskAssets;
	}

	/** Returns true if this filter has no entries */
	bool IsEmpty() const
	{
		return PackageNames.Num() + PackagePaths.Num() + ObjectPaths.Num() + ClassNames.Num() + TagsAndValues.Num() == 0;
	}

	/** Clears this filter of all entries */
	void Clear()
	{
		PackageNames.Empty();
		PackagePaths.Empty();
		ObjectPaths.Empty();
		ClassNames.Empty();
		TagsAndValues.Empty();
		RecursiveClassesExclusionSet.Empty();

		bRecursivePaths = false;
		bRecursiveClasses = false;
		bIncludeOnlyOnDiskAssets = false;

		ensure(IsEmpty());
	}
};